#ifndef TESTS_HPP
#define TESTS_HPP

#include "sv_sender.hpp"
#include "Protocols.hpp"

#include "transient.hpp"

struct Sv_packet{
    std::vector<uint8_t> base_pkt;
    std::vector<uint32_t> data_pos;
    std::vector<uint32_t> smpCnt_pos;
    uint8_t noAsdu;
    uint8_t noChannels;
    uint16_t smpRate;
};

inline  Sv_packet get_sampledValue_pkt_info(SampledValue_Config& svConf){

    Sv_packet packetInfo;

    // 
    packetInfo.noAsdu = svConf.noAsdu;
    packetInfo.smpRate = svConf.smpRate;
    packetInfo.noChannels = svConf.noChannels;

    // Ethernet
    Protocols::Ethernet eth(svConf.srcMac, svConf.dstMac);
    auto encoded_eth = eth.getEncoded();
    packetInfo.base_pkt.insert(packetInfo.base_pkt.end(), encoded_eth.begin(), encoded_eth.end());

    // Virtual LAN
    Protocols::Virtual_LAN vlan(svConf.vlanId, svConf.vlanPcp, svConf.vlanDei);
    auto encoded_vlan = vlan.getEncoded();
    packetInfo.base_pkt.insert(packetInfo.base_pkt.end(), encoded_vlan.begin(), encoded_vlan.end());

    // SampledValue
    Protocols::SampledValue sv(
        svConf.appID,
        svConf.noAsdu,
        svConf.svID,
        svConf.smpCnt,
        svConf.confRev,
        svConf.smpSynch,
        svConf.smpMod
    );

    // Initial position of SampledValue block
    int idx_SV_Start = packetInfo.base_pkt.size();

    auto encoded_sv = sv.getEncoded(8);
    packetInfo.base_pkt.insert(packetInfo.base_pkt.end(), encoded_sv.begin(), encoded_sv.end());

    for (int num=0; num<svConf.noAsdu; num++){
        int data_pos = sv.getParamPos(num, "seqData") + idx_SV_Start;
        int smpCont_pos = sv.getParamPos(num, "smpCnt") + idx_SV_Start;

        packetInfo.data_pos.push_back(data_pos);
        packetInfo.smpCnt_pos.push_back(smpCont_pos);
    }

    return packetInfo;
}

class Tests_Class{
public:
    std::vector<uint8_t>* digital_input;
    std::vector<transient_config> transient_tests;
private:
    
    uint8_t priority = 80;

public:

    RawSocket raw_socket;
    Tests_Class(){
        
    }

    void start_transient_test(std::vector<transient_config> configs){

        struct sched_param param;
        param.sched_priority = this->priority;
        transient_tests.clear();

        for (int i=0; i<configs.size(); i++){
            transient_tests.push_back(configs[i]);
        }

        for (auto& conf: transient_tests){
            conf.socket = &this->raw_socket;
            conf.digital_input = this->digital_input;
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

#endif