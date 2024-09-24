#ifndef PROTOCOLS_HPP
#define PROTOCOLS_HPP

#include "IEC61850_Types.hpp"

#include <vector>
#include <string>
#include <optional>
#include <unordered_map>
#include <inttypes.h>
#include <cstring>  




class Protocols{
public:

    class Ethernet {
public:
    std::string macSrc;
    std::string macDst;

    Ethernet(const std::string& dst, const std::string& src) : macSrc(src), macDst(dst) {}

    std::vector<uint8_t> macStrToBytes(const std::string& mac) const{
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i < mac.length(); i += 3) {
            if (i + 2 > mac.length()) {
                throw std::invalid_argument("Invalid MAC address length");
            }
            bytes.push_back(static_cast<uint8_t>(std::stoi(mac.substr(i, 2), nullptr, 16)));
        }
        return bytes;
    }

    std::vector<uint8_t> getEncoded() const {
        std::vector<uint8_t> encoded;

        std::vector<uint8_t> srcBytes = macStrToBytes(macSrc);
        std::vector<uint8_t> dstBytes = macStrToBytes(macDst);

        encoded.insert(encoded.end(), dstBytes.begin(), dstBytes.end());
        encoded.insert(encoded.end(), srcBytes.begin(), srcBytes.end());

        return encoded;
    }
};

class Goose {
public:
    uint16_t appID;
    uint16_t reserved1 = 0;
    uint16_t reserved2 = 0;

    std::string gocbRef;
    int32_t timeAllowedtoLive;
    std::string datSet;
    std::optional<std::string> goID;
    UtcTime t;
    int32_t stNum;
    int32_t sqNum;
    bool simulation = false;
    int32_t confRev;
    bool ndsCom = false;
    int32_t numDatSetEntries;
    std::vector<Data> allData;

    mutable std::vector<uint8_t> encoded;
    mutable std::unordered_map<std::string, size_t> indices;

    int offSet;

    Goose(uint16_t appID, const std::string& gocbRef, int32_t timeAllowedtoLive,
             const std::string& datSet, const UtcTime& t,
             int32_t stNum, int32_t sqNum, int32_t confRev,
             int32_t numDatSetEntries, const std::vector<Data>& allData)
        : appID(appID), gocbRef(gocbRef), timeAllowedtoLive(timeAllowedtoLive), datSet(datSet),
          t(t), stNum(stNum), sqNum(sqNum), confRev(confRev), allData(allData) {}

    int getParamPos(const std::string& param) const {
        auto it = indices.find(param);
        return (it != indices.end()) ? (it->second + this->offSet) : -1;
    }

    std::vector<uint8_t> getEncoded() {
        encoded.clear();  // Clear previous encoding
        encoded.reserve(2048);

        std::vector<uint8_t> pduEncoded = this->getPduEncoded();

        uint16_t pduSize = pduEncoded.size();
        uint16_t length = 8 + 2 + pduSize;
        offSet = 14;

        numDatSetEntries = allData.size();

        if (pduSize > 0xff) {
            length += 2;
            offSet += 2;
        } else if (pduSize > 0x80) {
            length += 1;
            offSet += 1;
        }

        // EtherType
        encoded.push_back(0x88);
        encoded.push_back(0xb8);

        // APPID
        encoded.push_back((appID >> 8) & 0xFF);
        encoded.push_back(appID & 0xFF);

        // Length 
        encoded.push_back((length >> 8) & 0xFF);
        encoded.push_back(length & 0xFF);

        // Reserved 1
        encoded.push_back((reserved1 >> 8) & 0xFF);
        encoded.push_back(reserved1 & 0xFF);

        // Reserved 2
        encoded.push_back((reserved2 >> 8) & 0xFF);
        encoded.push_back(reserved2 & 0xFF);

        // PDU
        encoded.push_back(0x61);
        if (pduSize > 0xff) {
            encoded.push_back(0x82);
            encoded.push_back((pduSize >> 8) & 0xFF);
            encoded.push_back(pduSize & 0xFF);
        } else if (pduSize > 0x80) {
            encoded.push_back(0x81);
            encoded.push_back(pduSize & 0xFF);
        } else {
            encoded.push_back(pduSize & 0xFF);
        }
        encoded.insert(encoded.end(), pduEncoded.begin(), pduEncoded.end());

        return encoded;
    }

private:
    std::vector<uint8_t> getPduEncoded() {
        std::vector<uint8_t> _encoded;
        indices.clear(); 

        this->numDatSetEntries = allData.size();

        // gocbRef
        indices["gocbRef"] = _encoded.size();
        _encoded.push_back(0x80); // Tag [0] VisibleString
        _encoded.push_back(gocbRef.size());
        _encoded.insert(_encoded.end(), gocbRef.begin(), gocbRef.end());

        // timeAllowedtoLive
        indices["timeAllowedtoLive"] = _encoded.size();
        _encoded.push_back(0x81); // Tag [1] INTEGER
        _encoded.push_back(4);
        for (int i = 3; i >= 0; --i) {
            _encoded.push_back((timeAllowedtoLive >> (i * 8)) & 0xFF);
        }

        // datSet
        indices["datSet"] = _encoded.size();
        _encoded.push_back(0x82); // Tag [2] VisibleString
        _encoded.push_back(datSet.size());
        _encoded.insert(_encoded.end(), datSet.begin(), datSet.end());

        // goID
        if (goID) {
            indices["goID"] = _encoded.size();
            _encoded.push_back(0x83); // Tag [3] VisibleString OPTIONAL
            _encoded.push_back(goID->size());
            _encoded.insert(_encoded.end(), goID->begin(), goID->end());
        }

        // t
        indices["t"] = _encoded.size();
        _encoded.push_back(0x84); // Tag [4] UtcTime
        auto tEncoded = t.getEncoded();
        _encoded.push_back(tEncoded.size());
        _encoded.insert(_encoded.end(), tEncoded.begin(), tEncoded.end());

        // stNum
        indices["stNum"] = _encoded.size();
        _encoded.push_back(0x85); // Tag [5] INTEGER (stNum)
        _encoded.push_back(4);
        for (int i = 3; i >= 0; --i) {
            _encoded.push_back((stNum >> (i * 8)) & 0xFF);
        }

        // sqNum
        indices["sqNum"] = _encoded.size();
        _encoded.push_back(0x86); // Tag [6] INTEGER (sqNum)
        _encoded.push_back(4);
        for (int i = 3; i >= 0; --i) {
            _encoded.push_back((sqNum >> (i * 8)) & 0xFF);
        }

        // Simulation
        indices["simulation"] = _encoded.size();
        _encoded.push_back(0x87); // Tag [7] BOOLEAN (simulation)
        _encoded.push_back(1);
        _encoded.push_back(simulation ? 0xFF : 0x00);

        // confRev
        indices["confRev"] = _encoded.size();
        _encoded.push_back(0x88); // Tag [8] INTEGER (confRev)
        _encoded.push_back(4);
        for (int i = 3; i >= 0; --i) {
            _encoded.push_back((confRev >> (i * 8)) & 0xFF);
        }


        // ndscom
        indices["ndsCom"] = _encoded.size();
        _encoded.push_back(0x89); // Tag [9] BOOLEAN (ndsCom)
        _encoded.push_back(1);
        _encoded.push_back(ndsCom ? 0xFF : 0x00);

        // numDatSetEntries
        indices["numDatSetEntries"] = _encoded.size();
        _encoded.push_back(0x8A); // Tag [10] INTEGER (numDatSetEntries)
        _encoded.push_back(4);
        for (int i = 3; i >= 0; --i) {
            _encoded.push_back((numDatSetEntries >> (i * 8)) & 0xFF);
        }

        // Encode allData
        indices["allData"] = _encoded.size();
        _encoded.push_back(0xab); // Tag [11] SEQUENCE OF Data
        std::vector<uint8_t> allDataEncoded;
        for (const auto& data : allData) {
            auto dataEncoded = data.getEncoded();
            allDataEncoded.insert(allDataEncoded.end(), dataEncoded.begin(), dataEncoded.end());
        }
        _encoded.push_back(allDataEncoded.size());
        _encoded.insert(_encoded.end(), allDataEncoded.begin(), allDataEncoded.end());

        return _encoded;
    }
};

class SampledValue {
public:
    // Header
    uint16_t appID;
    uint16_t reserved1 = 0;
    uint16_t reserved2 = 0;

    // SAVPDU 0x60
    uint8_t noAsdu;             //0x80
    uint8_t security;           //0x81 - OPTIONAL

    //ASDU 0x30
    std::string svID;   //0x80
    std::string datSet; //0x81 - OPTIONAL
    uint16_t smpCnt;    //0x82
    uint32_t confRev;   //0x83
    UtcTime refrTm;     //0x84 - OPTIONAL
    uint8_t smpSynch;   //0x85
    uint16_t smpRate;   //0x86 - OPTIONAL
    // std::vector<std::vector<uint32_t>> seqData; //0x87
    uint16_t smpMod;     //0x88 - OPTIONAL

    mutable std::vector<uint8_t> encoded;
    mutable std::vector<std::unordered_map<std::string, size_t>> indices;
    int offSet;
    int asduSize;

    SampledValue(uint16_t appID, uint8_t noAsdu, const std::string &svID, uint16_t smpCnt, uint32_t confRev, uint8_t smpSynch, uint16_t smpMod)
        : appID(appID), noAsdu(noAsdu), svID(svID), smpCnt(smpCnt), confRev(confRev), smpSynch(smpSynch), smpMod(smpMod) {
        
        this->indices.resize(noAsdu);
    }

    int getParamPos(int noAsdu, const std::string& param) const {
        auto it = indices[noAsdu].find(param);
        return (it != indices[noAsdu].end()) ? (it->second + this->offSet + noAsdu*this->asduSize) : -1;
    }

    std::vector<uint8_t> getEncoded(uint8_t noChannel) {
        encoded.clear();  // Clear previous encoding
        encoded.reserve(2048);
        this->offSet = 0;

        std::vector<uint8_t> savPduEncoded = this->getSavPduEncoded(noChannel);

        uint16_t savPduSize = savPduEncoded.size();
        uint16_t length = 8 + 2 + savPduSize;
        this->offSet = 9;

        if (savPduSize > 0xff) {
            length += 2;
            offSet += 2;
        } else if (savPduSize > 0x80) {
            length += 1;
            offSet += 1;
        }

        // EtherType
        encoded.push_back(0x88);
        encoded.push_back(0xba);

        // APPID
        encoded.push_back((appID >> 8) & 0xFF);
        encoded.push_back(appID & 0xFF);

        // Length 
        encoded.push_back((length >> 8) & 0xFF);
        encoded.push_back(length & 0xFF);

        // Reserved 1
        encoded.push_back((reserved1 >> 8) & 0xFF);
        encoded.push_back(reserved1 & 0xFF);

        // Reserved 2
        encoded.push_back((reserved2 >> 8) & 0xFF);
        encoded.push_back(reserved2 & 0xFF);

        // PDU
        encoded.push_back(0x60);
        if (savPduSize > 0xff) {
            encoded.push_back(0x82);
            encoded.push_back((savPduSize >> 8) & 0xFF);
            encoded.push_back(savPduSize & 0xFF);
        } else if (savPduSize > 0x80) {
            encoded.push_back(0x81);
            encoded.push_back(savPduSize & 0xFF);
        } else {
            encoded.push_back(savPduSize & 0xFF);
        }
        offSet += encoded.size();
        encoded.insert(encoded.end(), savPduEncoded.begin(), savPduEncoded.end());

        return encoded;
    }

private:

    std::vector<uint8_t> getAsduEncoded(uint8_t noChannel, int asdu) {

        std::vector<uint8_t> _encoded;

        // svID
        indices[asdu]["svID"] = _encoded.size();
        _encoded.push_back(0x80); // Tag [0] VisibleString
        _encoded.push_back(svID.size());
        _encoded.insert(_encoded.end(), svID.begin(), svID.end());

        // datSet
        if (!datSet.empty()) {
            indices[asdu]["datSet"] = _encoded.size();
            _encoded.push_back(0x81); // Tag [1] VisibleString
            _encoded.push_back(datSet.size());
            _encoded.insert(_encoded.end(), datSet.begin(), datSet.end());
        }

        // smpCnt
        indices[asdu]["smpCnt"] = _encoded.size();
        _encoded.push_back(0x82); // Tag [2] INTEGER
        _encoded.push_back(2);
        _encoded.push_back((smpCnt >> 8) & 0xFF);
        _encoded.push_back(smpCnt & 0xFF);

        // confRev
        indices[asdu]["confRev"] = _encoded.size();
        _encoded.push_back(0x83); // Tag [3] INTEGER
        _encoded.push_back(4);
        _encoded.push_back((confRev >> 24) & 0xFF);
        _encoded.push_back((confRev >> 16) & 0xFF);
        _encoded.push_back((confRev >> 8) & 0xFF);
        _encoded.push_back(confRev & 0xFF);

        // refrTm
        if (refrTm.defined) {
            indices[asdu]["refrTm"] = _encoded.size();
            _encoded.push_back(0x84); // Tag [4] UtcTime
            auto refrTmEncoded = refrTm.getEncoded();
            _encoded.push_back(refrTmEncoded.size());
            _encoded.insert(_encoded.end(), refrTmEncoded.begin(), refrTmEncoded.end());
        }

        // smpSynch
        indices[asdu]["smpSynch"] = _encoded.size();
        _encoded.push_back(0x85); // Tag [5] BOOLEAN
        _encoded.push_back(1);
        _encoded.push_back(smpSynch);

        // smpRate
        if (smpRate) {
            indices[asdu]["smpRate"] = _encoded.size();
            _encoded.push_back(0x86); // Tag [6] INTEGER
            _encoded.push_back(2);
            _encoded.push_back((smpRate >> 8) & 0xFF);
            _encoded.push_back(smpRate & 0xFF);
        }

        // seqData
        indices[asdu]["seqData"] = _encoded.size();
        _encoded.push_back(0x87); // Tag [7] SEQUENCE OF Data
        _encoded.push_back(noChannel*8);
        for (int channel =0;channel<noChannel;channel++){
            // For each channel, add 8 bytes of data
            for (int i = 3; i >= 0; --i) {
                _encoded.push_back(0);
                _encoded.push_back(0);
            }
        }

        // smpMod
        if (smpMod) {
            indices[asdu]["smpMod"] = _encoded.size();
            _encoded.push_back(0x88); // Tag [8] INTEGER
            _encoded.push_back(2);
            _encoded.push_back((smpMod >> 8) & 0xFF);
            _encoded.push_back(smpMod & 0xFF);
        }

        return _encoded;
    }

    std::vector<uint8_t> getSeqAsduEncoded(uint8_t noChannel){
        std::vector<uint8_t> _encoded;

        for (int asdu = 0; asdu < this->noAsdu; asdu++){
            
            std::vector<uint8_t> asduEncoded = this->getAsduEncoded(noChannel, asdu);
            uint32_t asduSize = asduEncoded.size();

            _encoded.push_back(0x30); // Tag [0] SEQUENCE OF ASDU
            if (asduSize > 0xff) {
                _encoded.push_back(0x82);
                _encoded.push_back((asduSize >> 8) & 0xFF);
                _encoded.push_back(asduSize & 0xFF);
            } else if (asduSize > 0x80) {
                _encoded.push_back(0x81);
                _encoded.push_back(asduSize & 0xFF);
            } else {
                _encoded.push_back(asduSize & 0xFF);
            }
            _encoded.insert(_encoded.end(), asduEncoded.begin(), asduEncoded.end());

            if (asdu == 0){
                this->asduSize = _encoded.size();
            }

        }

        return _encoded;
    }

    std::vector<uint8_t> getSavPduEncoded(uint8_t noChannel) {
        std::vector<uint8_t> _encoded;
        indices.clear();

        // SEQ ASDU
        std::vector<uint8_t> seqAsduEncoded = this->getSeqAsduEncoded(noChannel);
        uint32_t seqAsduSize = seqAsduEncoded.size();

        // noAsdu
        _encoded.push_back(0x80); // Tag [0] INTEGER
        _encoded.push_back(1);
        _encoded.push_back(noAsdu);

        // security
        if (security) {
            _encoded.push_back(0x81); // Tag [1] BOOLEAN
            _encoded.push_back(1);
            _encoded.push_back(security ? 0xFF : 0x00);
        }

        this->offSet += _encoded.size();

        //seqAsdu
        _encoded.push_back(0xA2); // Tag [2] SEQUENCE OF ASDU
        if (seqAsduSize > 0xff) {
            _encoded.push_back(0x82);
            _encoded.push_back((seqAsduSize >> 8) & 0xFF);
            _encoded.push_back(seqAsduSize & 0xFF);
        } else if (seqAsduSize > 0x80) {
            _encoded.push_back(0x81);
            _encoded.push_back(seqAsduSize & 0xFF);
        } else {
            _encoded.push_back(seqAsduSize & 0xFF);
        }
        _encoded.insert(_encoded.end(), seqAsduEncoded.begin(), seqAsduEncoded.end());

        return _encoded;
    }
};

class Virtual_LAN {
public:
    uint8_t priority;
    bool DEI;
    uint16_t ID;

    Virtual_LAN(uint8_t pri, bool dei, uint16_t id) : priority(pri), DEI(dei), ID(id) {}

    std::vector<uint8_t> getEncoded() const {
        std::vector<uint8_t> encoded;

        uint16_t tci = (priority << 13) | (DEI << 12) | ID;

        encoded.push_back(0x81);
        encoded.push_back(0x00);

        encoded.push_back((tci >> 8) & 0xFF);
        encoded.push_back(tci & 0xFF);

        return encoded;
    }
};


};



#endif // PROTOCOLS_HPP
