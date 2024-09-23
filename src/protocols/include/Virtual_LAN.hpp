#ifndef VIRTUAL_LAN_HPP
#define VIRTUAL_LAN_HPP

#include <vector>
#include <inttypes.h>


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

#endif // VIRTUAL_LAN_HPP