

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

std::vector<std::vector<uint8_t>> registeredMACs;

int debug_count=0;

struct task_arg{
    uint8_t* pkt;
    ssize_t pkt_len;
};

void process_GOOSE_packet(uint8_t* frame, ssize_t frameSize, int i){

    i += (frame[i+11] == 0x82) ? 17 : (frame[i+11] == 0x81) ? 16 : 15; 
    
}

void process_pkt(task_arg* arg) {

    // Todo: Chech for PRP Packets, do not duplicate the data from them

    uint8_t* frame = arg->pkt;
    ssize_t frameSize = arg->pkt_len;

    // -------- Process the frame -------- //

    // Check if the mac exist in the registeredMACs
    int mac_found = 0;
    for (int i=0; i<registeredMACs.size(); i++){
        if (memcmp(frame, registeredMACs[i].data(), 6) == 0){ // For SV
            mac_found = 1;
            break;
        }
        if (memcmp(frame+6, registeredMACs[i].data(), 6) == 0){ // For GOOSE
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
        process_GOOSE_packet(frame, frameSize, i);
    }else return;

}

void* SnifferThread(void* arg){

    using namespace std::chrono;

    auto sniffer_conf = static_cast<SnifferClass*>(arg);

    sniffer_conf->running = 1;
    sniffer_conf->stop = 0;

    for (auto mac : sniffer_conf->goInfo){
        registeredMACs.push_back(mac.mac_dst);
    }

    for (int i=0;i<6;i++){
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(registeredMACs[0][i]) << " ";
    }
    std::cout << std::endl;

    RawSocket* raw_socket = sniffer_conf->socket;
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

        if (rx_bytes < 0 || rx_bytes > Sniffer_RxSize) {
            std::cerr << "Failed to receive message" << std::endl;
            continue;
        }
  
        task.pkt = args_buff[idx_task];
        task.pkt_len = rx_bytes;
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