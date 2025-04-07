

#include "tests.hpp"

#include <nlohmann/json.hpp>
#include <utility>
using json = nlohmann::json;

void from_json(const json& j, SampledValue_Config& sv) {
    j.at("appID").get_to(sv.appID);
    j.at("confRev").get_to(sv.confRev);
    j.at("dstMac").get_to(sv.dstMac);
    j.at("noAsdu").get_to(sv.noAsdu);
    j.at("smpCnt").get_to(sv.smpCnt);
    j.at("smpMod").get_to(sv.smpMod);
    j.at("smpRate").get_to(sv.smpRate);
    j.at("smpSynch").get_to(sv.smpSynch);
    j.at("svID").get_to(sv.svID);
    j.at("vlanDei").get_to(sv.vlanDei);
    j.at("vlanId").get_to(sv.vlanId);
    j.at("vlanPcp").get_to(sv.vlanPcp);
    j.at("noChannels").get_to(sv.noChannels);
}

void from_json(const json& j, transient_config& cfg) {
    j.at("channelConfig").get_to(cfg.channelConfig);
    j.at("file_data_fs").get_to(cfg.file_data_fs);
    j.at("fileName").get_to(cfg.fileName);
    j.at("interval").get_to(cfg.interval);
    j.at("interval_flag").get_to(cfg.interval_flag);
    j.at("loop_flag").get_to(cfg.loop_flag);
    j.at("scale").get_to(cfg.scale);
    j.at("timed_start").get_to(cfg.timed_start);
    j.at("start_time").get_to(cfg.start_time);
    j.at("sv_config").get_to(cfg.sv_config);
}

void from_json(const json& j, Goose_info& cfg) {
    j.at("goCbRef").get_to(cfg.goCbRef);
    j.at("mac_dst").get_to(cfg.mac_dst);
    j.at("input").get_to(cfg.input);
}

std::vector<Goose_info> get_goose_input_config(const std::string& config_path){
    std::vector<Goose_info> goInputs;
    std::ifstream f(config_path);
    if (!f.is_open()) {
        return {};
    }
    try{
        json data = json::parse(f);
        const auto& test_configs = data.at("Goose_info");
        for (const auto& test_entry : test_configs) {
            Goose_info cfg = test_entry.get<Goose_info>();
            goInputs.push_back(cfg);
        }
    } catch (const json::exception& e) {
        return {};
    }
    return goInputs;
}


std::vector<transient_config> get_transient_test_config(const std::string& config_path) {
    std::vector<transient_config> transient_configs;
    std::ifstream f(config_path);
    if (!f.is_open()) {
        transient_config test_config;
        test_config.error_msg = "Failed to open config file: " + config_path;
        test_config.fileloaded = 0;
        return {test_config};
    }
    try {
        json data = json::parse(f);
        const auto& test_configs = data.at("Test Config");
        for (const auto& test_entry : test_configs) {
            if (test_entry.at("test_type").get<std::string>() == "transient") {
                transient_config cfg = test_entry.get<transient_config>();
                cfg.fileName = "files/" + cfg.fileName;
                cfg.fileloaded = 1;
                transient_configs.push_back(cfg);
            }
        }
    } catch (const json::exception& e) {
        transient_config error_cfg;
        error_cfg.error_msg = "JSON error: " + std::string(e.what());
        error_cfg.fileloaded = 0;
        transient_configs.push_back(error_cfg);
    }
    return transient_configs;
}

Sv_packet get_sampledValue_pkt_info(SampledValue_Config& svConf){

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

