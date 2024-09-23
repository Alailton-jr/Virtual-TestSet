#ifndef TRANSIENT_HPP
#define TRANSIENT_HPP

#include <pthread.h>
#include <string>
#include <inttypes.h>
#include <vector>

#include "SampledValue.hpp"

struct transient_config{

    std::string fileName;
    uint8_t loop;
    uint8_t noChannels;
    std::vector<uint8_t[2]> channelConfig;
    SampledValue Sv;
    

    int stop, running, error;
    pthread_t thd;
};











#endif // TRANSIENT_HPP

