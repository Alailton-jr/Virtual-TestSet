#ifndef TRANSIENT_HPP
#define TRANSIENT_HPP

#include <pthread.h>
#include <string>
#include <inttypes.h>
#include <vector>

#include "sv_sender.hpp"

struct transient_config{

    std::string fileName;
    uint8_t loop_flag;
    uint8_t interval_flag;
    double interval;


    uint8_t noChannels;
    std::vector<uint8_t[2]> channelConfig;
    std::vector<double> scale;
    double file_data_fs;
    SampledValue_Config sv_config;
    

    int stop, running, error;
    pthread_t thd;
};


void* run_transient_test(void* arg);











#endif // TRANSIENT_HPP

