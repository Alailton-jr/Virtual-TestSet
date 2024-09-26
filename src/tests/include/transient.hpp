#ifndef TRANSIENT_HPP
#define TRANSIENT_HPP

#include <vector>
#include <inttypes.h>
#include "sv_sender.hpp"
#include <pthread.h>
#include "raw_socket.hpp"


struct transient_config{

    std::string fileName;
    uint8_t loop_flag;
    uint8_t interval_flag;
    double interval;

    std::vector<std::vector<uint8_t>> channelConfig;
    std::vector<double> scale;
    double file_data_fs;
    SampledValue_Config sv_config;

    RawSocket* socket;
    

    int stop, running, error;
    pthread_t thd;
};


void* run_transient_test(void* arg);











#endif // TRANSIENT_HPP

