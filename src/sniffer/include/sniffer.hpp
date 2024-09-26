

#ifndef SNIFFER_HPP
#define SNIFFER_HPP

#include <string>
#include <cstdint>
#include <cstring>

#include "general_definition.hpp"
#include "raw_socket.hpp"
#include "thread_pool.hpp"

#define WINDOW_STEP 0.2

struct Goose_info{
    std::string goCbRef;
    std::vector<uint8_t> mac_dst;
    std::vector<std::vector<uint8_t>> input;
};

void* SnifferThread(void* arg);

class SnifferClass {
public:
    int running, stop;
    int noThreads;
    int noTasks;
    int priority;

    pthread_t thd;

    RawSocket* socket;
    std::vector<uint8_t>* digitalInput;
    std::vector<Goose_info> goInfo;

    SnifferClass(){
    }
    ~SnifferClass(){
    }

    void init(){
    }

    void startThread(std::vector<Goose_info> goInfo){

        this->goInfo = goInfo;
        this->noThreads = Sniffer_NoThreads;
        this->noTasks = Sniffer_NoTasks;
        this->priority = Sniffer_ThreadPriority;

        pthread_create(&this->thd, NULL, SnifferThread, static_cast<void*>(this));
        struct sched_param param;
        param.sched_priority = this->priority;
        pthread_setschedparam(this->thd, SCHED_FIFO, &param);
    }

    void stopThread(){
        this->stop = 1;
        pthread_join(this->thd, NULL);
    }

};


#endif // SNIFFER_HPP