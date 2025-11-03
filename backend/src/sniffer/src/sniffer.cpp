

#include "sniffer.hpp"
#include "rt_utils.hpp"
#include "logger.hpp"
#include "metrics.hpp"

#include <chrono>
#include <vector>
#ifdef REMOVE_UNUSED_INCLUDE_NUMERIC
#endif
#include <thread>

#include <sys/types.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#ifdef __linux__
#include <linux/if_packet.h>
#endif
#ifdef __linux__
#include <linux/net_tstamp.h>
#endif
#include <net/if.h>
#include <ifaddrs.h>          
#include <arpa/inet.h>        
#ifdef __linux__
#include <linux/sockios.h>
#endif
#include <sys/ioctl.h>
#include <unistd.h>

#ifdef __linux__
#include <fftw3.h>
#endif
#include <math.h>

#ifdef REMOVE_UNUSED_INCLUDE_FSTREAM
#endif
#ifdef REMOVE_UNUSED_INCLUDE_SSTREAM
#endif

// Globals removed - moved into SnifferThread as local variables
// std::vector<std::vector<uint8_t>> registeredMACs;
// SnifferClass* sniffer;

int debug_count=0;

struct task_arg{
    uint8_t* pkt;
    ssize_t pkt_len;
    SnifferClass* sniffer; // Add context
    std::vector<std::vector<uint8_t>>* registeredMACs; // Add context
};

void process_GOOSE_packet(uint8_t* frame, ssize_t frameSize, int i, SnifferClass* sniffer){

    // Validate minimum GOOSE header size
    if (i + 14 > frameSize) {
        LOG_ERROR("GOOSE", "Truncated header (frameSize=%zd, position=%d)", frameSize, i);
        METRIC_PARSE_ERROR();
        return;
    }

    // frame[i:i+2] // APPID
    // frame[i+2:i+4] // Length
    // frame[i+4:i+8] // Reserved 1 and 2
    // frame[i+9] // GOOSE TAG
    // frame[i+10] // GOOSE Length

    // Parse BER length with bounds checking
    uint16_t length = 0;
    if (frame[i+11] == 0x82){
        if (i + 14 > frameSize) {
            LOG_ERROR("GOOSE", "Truncated 0x82 length field (frameSize=%zd, position=%d)", frameSize, i);
            METRIC_PARSE_ERROR();
            return;
        }
        length = static_cast<uint16_t>((frame[i+12] << 8) | frame[i+13]);
        i += 14;
    }else if (frame[i+11] == 0x81){
        if (i + 13 > frameSize) {
            LOG_ERROR("GOOSE", "Truncated 0x81 length field (frameSize=%zd, position=%d)", frameSize, i);
            METRIC_PARSE_ERROR();
            return;
        }
        length = frame[i+12];
        i += 13;
    }else{
        length = frame[i+11];
        i += 12;
    }

    // Validate length against remaining frame
    if (i + length > frameSize) {
        LOG_ERROR("GOOSE", "PDU length exceeds frame size (length=%u, frameSize=%zd, position=%d)", 
                  length, frameSize, i);
        METRIC_PARSE_ERROR();
        return;
    }

    int j = 0, goIdx = -1;
    while (j < length){
        // Bounds check for TLV access
        if (i + j + 1 >= frameSize) {
            LOG_ERROR("GOOSE", "TLV truncated (frameSize=%zd, position=%d)", frameSize, i + j);
            METRIC_PARSE_ERROR();
            return;
        }

        uint8_t tag = frame[i+j];
        uint8_t tlv_len = frame[i+j+1];

        // Validate TLV data is within bounds
        if (i + j + 2 + tlv_len > frameSize) {
            LOG_ERROR("GOOSE", "TLV data exceeds frame (frameSize=%zd, position=%d, tlv_len=%u)", 
                      frameSize, i + j, tlv_len);
            METRIC_PARSE_ERROR();
            return;
        }

        if (tag == 0x80){
            for (size_t idx = 0; idx < sniffer->goInfo.size(); idx++)
            if (tlv_len <= sniffer->goInfo[idx].goCbRef.size() &&
                memcmp(&frame[i+j+2], sniffer->goInfo[idx].goCbRef.data(), tlv_len) == 0){
                goIdx = static_cast<int>(idx);
                break;
            }
        }

        if (tag == 0xab){
            break;
        }

        j += tlv_len + 2;
    }
    
    if (goIdx == -1) return;
    i += j;
    
    // Parse allData with bounds checking
    if (i + 1 >= frameSize) {
        LOG_ERROR("GOOSE", "allData truncated (frameSize=%zd, position=%d)", frameSize, i);
        METRIC_PARSE_ERROR();
        return;
    }
    
    std::vector<uint8_t> boolDat;
    j = 2;
    length = frame[i+1];
    
    if (i + length > frameSize) {
        LOG_ERROR("GOOSE", "allData length exceeds frame (length=%u, frameSize=%zd, position=%d)", 
                  length, frameSize, i);
        METRIC_PARSE_ERROR();
        return;
    }
    
    while (j < length){
        if (i + j + 1 >= frameSize) break;
        
        uint8_t tag = frame[i+j];
        uint8_t tlv_len = frame[i+j+1];
        
        if (i + j + 2 + tlv_len > frameSize) break;
        
        if (tag == 0x83){
            boolDat.push_back(frame[i+j+2]);
        }else{
            boolDat.push_back(0);
        }
        j += tlv_len + 2;
    }
    
    for (const auto& dat : sniffer->goInfo[static_cast<size_t>(goIdx)].input){
        if (dat[0] >= boolDat.size()){
            LOG_ERROR("GOOSE", "Data index out of range (dat[0]=%u, boolDat.size=%zu)", 
                      dat[0], boolDat.size());
            METRIC_PARSE_ERROR();
            return;
        }
        if (dat[1] >= boolDat.size()) {
            LOG_ERROR("GOOSE", "GOOSE data index out of range (dat[1]=%u, boolDat.size=%zu)", 
                      dat[1], boolDat.size());
            METRIC_PARSE_ERROR();
            return;
        }
        (*sniffer->digitalInput)[dat[0]].store(boolDat[dat[1]], std::memory_order_release);
    }
    
    // Successfully received and parsed GOOSE packet
    METRIC_RECV_FRAME();
    // std::cout << "GOOSE Received: "<< (boolDat[0] != 0) << std::endl;
}

void process_pkt(task_arg* arg) {

    // Todo: Chech for PRP Packets, do not duplicate the data from them

    uint8_t* frame = arg->pkt;
    ssize_t frameSize = arg->pkt_len;
    std::vector<std::vector<uint8_t>>* registeredMACs = arg->registeredMACs;
    SnifferClass* sniffer = arg->sniffer;

    // -------- Process the frame -------- //

    // Check if the mac exist in the registeredMACs
    int mac_found = 0;
    for (size_t i=0; i<registeredMACs->size(); i++){
        if (memcmp(frame, (*registeredMACs)[i].data(), 6) == 0){ // For SV
            mac_found = 1;
            break;
        }
        if (memcmp(frame+6, (*registeredMACs)[i].data(), 6) == 0){ // For GOOSE
            mac_found = 1;
            break;
        }
    }
    if (!mac_found) return;

    // uint16_t smpCount;
    // int j = 0; // unused variable removed
    int i = (frame[12] == 0x81 && frame[13] == 0x00) ? 16 : 12; // Skip Ethernet and vLAN

    if ((frame[i] == 0x88 && frame[i+1] == 0xba)){ // Check if packet is SV
        // process_SV_packet(frame, frameSize, sv, i);
        return;
    }else if ((frame[i] == 0x88 && frame[i+1] == 0xb8)){
        process_GOOSE_packet(frame, frameSize, i, sniffer);
    }else return;

}

void* SnifferThread(void* arg){

    using namespace std::chrono;

    auto sniffer_conf = static_cast<SnifferClass*>(arg);

    // Phase 7: Real-time setup for critical sniffer thread
    LOG_INFO("SNIFFER", "Thread starting with real-time capabilities...");
    
    // Set real-time priority (high priority for packet capture)
    rt_set_realtime(Sniffer_ThreadPriority);  // Default: 80 (configured in general_definition.hpp)
    
    // Optional: Set CPU affinity to isolate sniffer thread
    // Example: bind to CPUs 2-3 for dedicated packet processing
    // rt_set_affinity({2, 3});
    
    sniffer_conf->running.store(true, std::memory_order_release);

    // Create local MACs list instead of global
    std::vector<std::vector<uint8_t>> registeredMACs;
    for (auto mac : sniffer_conf->goInfo){
        registeredMACs.push_back(mac.mac_dst);
    }


#ifdef __linux__
    RawSocket* raw_socket = &sniffer_conf->socket;
    // Add SO_RCVTIMEO for responsive stop (100ms timeout per spec)
    struct timeval timeout;
    timeout.tv_sec  = 0;
    timeout.tv_usec = 100000; // 100ms
    if (setsockopt(raw_socket->socket_id, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        LOG_WARN("SNIFFER", "Failed to set SO_RCVTIMEO: %s", strerror(errno));
    }
    uint8_t args_buff[Sniffer_NoTasks+1][Sniffer_RxSize];
    ssize_t rx_bytes;
    raw_socket->iov.iov_len = Sniffer_RxSize;
    int32_t idx_task = 0;
    task_arg task;
    while (!sniffer_conf->stop.load(std::memory_order_acquire)) {
        raw_socket->msg_hdr.msg_iov->iov_base = args_buff[idx_task];
        rx_bytes = recvmsg(raw_socket->socket_id, &raw_socket->msg_hdr, 0);
        if (rx_bytes < 0) {
            // Check for timeout - this allows responsive stop
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Timeout, check stop condition
            }
            LOG_ERROR("SNIFFER", "Failed to receive message: %s", strerror(errno));
            continue;
        }
        if (rx_bytes > Sniffer_RxSize) {
            LOG_ERROR("SNIFFER", "Received message too large (rxBytes=%zd, maxSize=%d)", rx_bytes, Sniffer_RxSize);
            continue;
        }
        task.pkt = args_buff[idx_task];
        task.pkt_len = rx_bytes;
        task.sniffer = sniffer_conf;
        task.registeredMACs = &registeredMACs;
        process_pkt(&task);
        ++idx_task;
        if (idx_task > Sniffer_NoTasks)  idx_task = 0;
    }
#else
    LOG_WARN("SNIFFER", "Raw socket packet capture is not supported on this platform. Sniffer thread will idle.");
    while (!sniffer_conf->stop.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
#endif


    sniffer_conf->running.store(false, std::memory_order_release);
    return nullptr;
}