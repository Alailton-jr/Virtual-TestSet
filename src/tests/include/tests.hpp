#ifndef TESTS_HPP
#define TESTS_HPP

#include "transient.hpp"

class Tests{

    std::vector<transient_config> transient_tests;

    RawSocket raw_socket;

    uint8_t priority = 80;

    void start_transient_test(std::vector<transient_config>& configs){

        struct sched_param param;
        param.sched_priority = this->priority;

        for (auto& conf : configs){
            transient_tests.push_back(conf);
        }

        for (auto& conf: transient_tests){
            pthread_create(&conf.thd, NULL, run_transient_test, static_cast<void*>(&conf));
            pthread_setschedparam(conf.thd, SCHED_FIFO, &param);
        }
        
    }


};

#endif