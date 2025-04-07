#ifndef TESTS_HPP
#define TESTS_HPP

#include "sv_sender.hpp"
#include "Protocols.hpp"
#include "transient.hpp"
#include "sniffer.hpp"

std::vector<Goose_info> get_goose_input_config(const std::string& config_path);
std::vector<transient_config> get_transient_test_config(const std::string& config_path);

struct Sv_packet{
    std::vector<uint8_t> base_pkt;
    std::vector<uint32_t> data_pos;
    std::vector<uint32_t> smpCnt_pos;
    uint8_t noAsdu;
    uint8_t noChannels;
    uint16_t smpRate;
};

class Tests_Class{
public:
    std::vector<uint8_t> digital_input;
    std::vector<transient_config> transient_tests;
    RawSocket raw_socket;
    SnifferClass sniffer;

private:
    
    uint8_t priority = 80;

public:

    Tests_Class(){
        digital_input.resize(16);
        sniffer.digitalInput = &digital_input;
    }

    int32_t is_running(){
        for (auto& conf: transient_tests){
            if (conf.running == 1){
                return 1;
            }
        }
        return 0;
    }

    void start_transient_test(std::vector<transient_config> configs){

        if(sniffer.running){
            sniffer.stopThread();
        }
        std::vector<Goose_info> goInput = get_goose_input_config("files/goose_input_config.json");
        sniffer.startThread(goInput);

        struct sched_param param;
        param.sched_priority = this->priority;
        transient_tests.clear();

        for (int i=0; i<configs.size(); i++){
            transient_tests.push_back(configs[i]);
        }

        for (auto& conf: transient_tests){
            conf.socket = &this->raw_socket;
            conf.digital_input = &this->digital_input;
            pthread_create(&conf.thd, NULL, run_transient_test, static_cast<void*>(&conf));
            pthread_setschedparam(conf.thd, SCHED_FIFO, &param);
        }
    }

    void stop_transient_test(){
        for (auto& conf: transient_tests){
            conf.stop = 1;
        }
    }

};

Sv_packet get_sampledValue_pkt_info(SampledValue_Config& svConf);

#endif