
#include "main.hpp"
#include "Ethernet.hpp"
#include "Goose.hpp"
#include "SampledValue.hpp"
#include "raw_socket.hpp"
#include "Virtual_LAN.hpp"

class SampledValue_Config{
public:

    std::string srcMac = "01:0c:cd:04:00:01";
    std::string dstMac;

    uint16_t vlanId = 0x8100;
    uint8_t vlanPcp = 0;
    uint8_t vlanDei = 0;

    uint16_t appID;
    uint8_t noAsdu;
    std::string svID;
    uint16_t smpCnt;
    uint32_t confRev;
    uint8_t smpSynch;
    uint16_t smpMod;

    RawSocket raw_socket;

public:
    SampledValue_Config(){
        this->dstMac = GetMACAddress(IF_NAME);
    }
};


int main(){

    std::cout << "Hello World!" << std::endl;

    std::vector<uint8_t> base_pkt;
    
    SampledValue_Config* sv_conf = new SampledValue_Config();

    sv_conf->appID = 0x4000;
    sv_conf->noAsdu = 2;
    sv_conf->svID = "Conprove_MU01";
    sv_conf->smpCnt = 566;
    sv_conf->confRev = 1;
    sv_conf->smpSynch = 0x01;

    // Ethernet
    Ethernet eth(sv_conf->srcMac, sv_conf->dstMac);
    auto encoded_eth = eth.getEncoded();
    base_pkt.insert(base_pkt.end(), encoded_eth.begin(), encoded_eth.end());

    // Virtual LAN
    Virtual_LAN vlan(sv_conf->vlanId, sv_conf->vlanPcp, sv_conf->vlanDei);
    auto encoded_vlan = vlan.getEncoded();
    base_pkt.insert(base_pkt.end(), encoded_vlan.begin(), encoded_vlan.end());

    // SampledValue
    SampledValue sv(
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

    sv_conf->raw_socket.iov.iov_base = (void*)base_pkt.data();
    sv_conf->raw_socket.iov.iov_len = base_pkt.size();

    int smpCount = sv.getParamPos(1, "smpCnt") + idx_SV_Start;
    base_pkt[smpCount] = 0x00;
    base_pkt[smpCount + 1] = 0x4;

    for (int i = 0; i < 100; i++){
        sendmsg(sv_conf->raw_socket.socket_id, &sv_conf->raw_socket.msg_hdr, 0);
    }

    return 0;
}