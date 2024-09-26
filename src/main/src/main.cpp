
#include "main.hpp"
#include "raw_socket.hpp"

#include "Protocols.hpp"
#include "sv_sender.hpp"

#include "tests.hpp"

// Testing getDataFromCsv
#include <fstream>
#include <sstream>
#include <vector>

#include <math.h>

void plotIt(std::vector<double> x, std::vector<double> y){

    std::ofstream dataFile("data.txt");

    for (size_t i = 0; i < x.size(); ++i) {
        dataFile << x[i] << " " << y[i] << std::endl;
    }


    dataFile.close();
    system("gnuplot -e 'set terminal pdf; set output \"plot.pdf\"; plot \"data.txt\" with lines'");
    remove("data.txt");
}

void test_sampledValue_Pkt(){

    std::vector<uint8_t> base_pkt;
    
    SampledValue_Config* sv_conf = new SampledValue_Config();

    sv_conf->appID = 0x4000;
    sv_conf->noAsdu = 2;
    sv_conf->svID = "Conprove_MU01";
    sv_conf->smpCnt = 566;
    sv_conf->confRev = 1;
    sv_conf->smpSynch = 0x01;

    // Ethernet
    Protocols::Ethernet eth(sv_conf->srcMac, sv_conf->dstMac);
    auto encoded_eth = eth.getEncoded();
    base_pkt.insert(base_pkt.end(), encoded_eth.begin(), encoded_eth.end());

    // Virtual LAN
    Protocols::Virtual_LAN vlan(sv_conf->vlanId, sv_conf->vlanPcp, sv_conf->vlanDei);
    auto encoded_vlan = vlan.getEncoded();
    base_pkt.insert(base_pkt.end(), encoded_vlan.begin(), encoded_vlan.end());

    // SampledValue
    Protocols::SampledValue sv(
        sv_conf->appID,
        sv_conf->noAsdu,
        sv_conf->svID,
        sv_conf->smpCnt,
        sv_conf->confRev,
        sv_conf->smpSynch,
        sv_conf->smpMod
    );

    auto encoded_sv = sv.getEncoded(8);
    int idx_SV_Start = base_pkt.size();
    base_pkt.insert(base_pkt.end(), encoded_sv.begin(), encoded_sv.end());

    // Send the packet
    RawSocket raw_socket;

    raw_socket.iov.iov_base = (void*)base_pkt.data();
    raw_socket.iov.iov_len = base_pkt.size();

    int smpCount = sv.getParamPos(1, "smpCnt") + idx_SV_Start;
    base_pkt[smpCount] = 0x00;
    base_pkt[smpCount + 1] = 0x4;

    for (int i = 0; i < 100; i++){
        sendmsg(raw_socket.socket_id, &raw_socket.msg_hdr, 0);
    }

}

void testTransient(){
    
    transient_config tran_conf;

    tran_conf.channelConfig = {
        // {0,1}, {1,2}, {2,3}, {4,4}, {5,5}, {6,6}
        {0,6}, {1,5}, {2,4}, {4,3}, {5,2}, {6,1}
    };
    tran_conf.file_data_fs = 9600;
    tran_conf.fileName = "files/noFault.csv";
    tran_conf.interval = 0;
    tran_conf.interval_flag = 0;
    tran_conf.loop_flag = 1;
    tran_conf.scale = {1,1,1,1,1,1,1,1};


    //SV Config 
    tran_conf.sv_config.appID = 0x4000;
    tran_conf.sv_config.confRev = 1;
    tran_conf.sv_config.dstMac = "01-0C-CD-04-00-01";
    tran_conf.sv_config.noAsdu = 1;
    tran_conf.sv_config.smpCnt = 0;
    tran_conf.sv_config.smpMod = 0;
    tran_conf.sv_config.smpRate = 4800;
    tran_conf.sv_config.smpSynch = 1;
    tran_conf.sv_config.svID = "SV_01";
    tran_conf.sv_config.vlanDei = 0;
    tran_conf.sv_config.vlanId = 100;
    tran_conf.sv_config.vlanPcp = 4;
    tran_conf.sv_config.noChannels = 8;

    Tests_Class test;

    test.start_transient_test({tran_conf});


    sleep(120);
    test.stop_transient_test();
}

void test_Sniffer(){

    TestSet_Class testSet;

    Goose_info goInfo = {
        .goCbRef = "",
        .mac_dst = {0x01, 0x0c, 0xcd, 0x01, 0x00, 0x01},
        .input = {{0,0},{1,1}}
    };

    testSet.sniffer.startThread({goInfo});
    sleep(15);
    testSet.sniffer.stopThread();
}

int main(){

    std::cout << "Hello World!" << std::endl;


    test_Sniffer();
    // testTransient();

    return 0;
}