

#ifndef SNIFFER_HPP
#define SNIFFER_HPP

#include <string>
#include <cstdint>
#include <cstring>
#include <atomic>

#include "general_definition.hpp"
#include "raw_socket.hpp"
#include "thread_pool.hpp"

#define WINDOW_STEP 0.2

struct Goose_info{
    std::string goCbRef;
    std::vector<uint8_t> mac_dst;
    std::vector<std::vector<uint8_t>> input;  // {Digital Input POS, GOOSE data pos}
};

void* SnifferThread(void* arg);

class SnifferClass {
public:
    std::atomic<bool> running;
    std::atomic<bool> stop;
    int noThreads;
    int noTasks;
    int priority;

    pthread_t thd;
    bool threadStarted;

    RawSocket socket;
    std::vector<std::atomic<uint8_t>>* digitalInput;
    std::vector<Goose_info> goInfo;

    SnifferClass() : running(false), stop(false), threadStarted(false) {
    }
    
    ~SnifferClass(){
        stopThread();
    }

    void init(){
    }

    void startThread(std::vector<Goose_info> goInfo){
        if (threadStarted) {
            throw std::runtime_error("Sniffer thread already started");
        }

        this->goInfo = goInfo;
        this->noThreads = Sniffer_NoThreads;
        this->noTasks = Sniffer_NoTasks;
        this->priority = Sniffer_ThreadPriority;

        int ret = pthread_create(&this->thd, NULL, SnifferThread, static_cast<void*>(this));
        if (ret != 0) {
            throw std::runtime_error("Failed to create sniffer thread: " + std::string(strerror(ret)));
        }
        threadStarted = true;
        
        struct sched_param param;
        param.sched_priority = this->priority;
        pthread_setschedparam(this->thd, SCHED_FIFO, &param);
    }

    void stopThread(){
        if (!threadStarted) {
            return;
        }
        
        stop.store(true, std::memory_order_release);
        pthread_join(this->thd, NULL);
        threadStarted = false;
    }

};


#endif // SNIFFER_HPP