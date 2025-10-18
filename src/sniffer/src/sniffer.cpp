

#include "sniffer.hpp"

#include <chrono>
#include <vector>
#include <numeric>
#include <iostream>
#include <thread>

#include <sys/types.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <linux/if_packet.h> 
#include <linux/net_tstamp.h>
#include <net/if.h>
#include <ifaddrs.h>          
#include <arpa/inet.h>        
#include <linux/sockios.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <fftw3.h>
#include <math.h>

#include <fstream>
#include <sstream>

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
        std::cerr << "GOOSE: truncated header" << std::endl;
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
            std::cerr << "GOOSE: truncated 0x82 length" << std::endl;
            return;
        }
        length = static_cast<uint16_t>((frame[i+12] << 8) | frame[i+13]);
        i += 14;
    }else if (frame[i+11] == 0x81){
        if (i + 13 > frameSize) {
            std::cerr << "GOOSE: truncated 0x81 length" << std::endl;
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
        std::cerr << "GOOSE: PDU length exceeds frame size" << std::endl;
        return;
    }

    int j = 0, goIdx = -1;
    while (j < length){
        // Bounds check for TLV access
        if (i + j + 1 >= frameSize) {
            std::cerr << "GOOSE: TLV truncated" << std::endl;
            return;
        }

        uint8_t tag = frame[i+j];
        uint8_t tlv_len = frame[i+j+1];

        // Validate TLV data is within bounds
        if (i + j + 2 + tlv_len > frameSize) {
            std::cerr << "GOOSE: TLV data exceeds frame" << std::endl;
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
        std::cerr << "GOOSE: allData truncated" << std::endl;
        return;
    }
    
    std::vector<uint8_t> boolDat;
    j = 2;
    length = frame[i+1];
    
    if (i + length > frameSize) {
        std::cerr << "GOOSE: allData length exceeds frame" << std::endl;
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
    
    for (const auto& dat : sniffer->goInfo[goIdx].input){
        if (dat[0] >= boolDat.size()){
            std::cerr << "GOOSE Error: Data out of range" << std::endl;
            return;
        }
        if (dat[1] >= boolDat.size()) {
            std::cerr << "GOOSE Error: GOOSE data index out of range" << std::endl;
            return;
        }
        (*sniffer->digitalInput)[dat[0]].store(boolDat[dat[1]], std::memory_order_release);
    }
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
    int j = 0;
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

    sniffer_conf->running = 1;
    sniffer_conf->stop = 0;

    // Create local MACs list instead of global
    std::vector<std::vector<uint8_t>> registeredMACs;
    for (auto mac : sniffer_conf->goInfo){
        registeredMACs.push_back(mac.mac_dst);
    }

    // for (int i=0;i<6;i++){
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(registeredMACs[0][i]) << " ";
    // }
    // std::cout << std::endl;

    RawSocket* raw_socket = &sniffer_conf->socket;
    
    // Add SO_RCVTIMEO for responsive stop (1 second timeout)
    struct timeval timeout;
    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;
    if (setsockopt(raw_socket->socket_id, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        std::cerr << "Warning: Failed to set SO_RCVTIMEO: " << strerror(errno) << std::endl;
    }
    
    // ThreadPool<void(task_arg*)> pool(sniffer_conf->noThreads, sniffer_conf->noTasks, sniffer_conf->priority);
    // Variables used for decoding and the Thread Pool 
    uint8_t args_buff[Sniffer_NoTasks+1][Sniffer_RxSize];
    ssize_t rx_bytes;
    raw_socket->iov.iov_len = Sniffer_RxSize;

    int32_t idx_task = 0;
    task_arg task;
    while (!sniffer_conf->stop) {

        raw_socket->msg_hdr.msg_iov->iov_base = args_buff[idx_task];
        rx_bytes = recvmsg(raw_socket->socket_id, &raw_socket->msg_hdr, 0);

        if (rx_bytes < 0) {
            // Check for timeout - this allows responsive stop
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Timeout, check stop condition
            }
            std::cerr << "Failed to receive message: " << strerror(errno) << std::endl;
            continue;
        }
        
        if (rx_bytes > Sniffer_RxSize) {
            std::cerr << "Received message too large" << std::endl;
            continue;
        }
  
        task.pkt = args_buff[idx_task];
        task.pkt_len = rx_bytes;
        task.sniffer = sniffer_conf;
        task.registeredMACs = &registeredMACs;
        process_pkt(&task);


        // if (memcmp(args_buff[idx_task], sniffer_conf->sv_info.mac_dst, 6) != 0){
        //     continue; // Check if packet is for this IED
        // }
            
        // Submit task to thread pool
        // pool.submit(
        //     process_pkt,
        //     std::shared_ptr<task_arg*> (
        //         new task_arg*(new task_arg{
        //             .pkt = args_buff[idx_task],
        //             .pkt_len = rx_bytes,
        //             .info = sniffer_conf->sv_info 
        //         }), 
        //         [](task_arg** p) { delete *p; delete p;}
        //     )
        // );

        ++idx_task;

        if (idx_task > Sniffer_NoTasks)  idx_task = 0;
    }


    sniffer_conf->running = 0;
    return nullptr;
}