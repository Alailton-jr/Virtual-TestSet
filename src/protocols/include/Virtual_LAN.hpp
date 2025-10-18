#ifndef VIRTUAL_LAN_HPP
#define VIRTUAL_LAN_HPP

#include <vector>
#include <inttypes.h>
#include <stdexcept>


class Virtual_LAN {
public:
    uint8_t priority;
    bool DEI;
    uint16_t ID;

    Virtual_LAN(uint8_t pri, bool dei, uint16_t id) : priority(pri), DEI(dei), ID(id) {
        // Validate priority (3 bits: 0-7)
        if (priority > 7) {
            throw std::invalid_argument("VLAN priority must be 0-7, got " + std::to_string(priority));
        }
        // Validate VLAN ID (12 bits: 0-4095)
        if (ID > 4095) {
            throw std::invalid_argument("VLAN ID must be 0-4095, got " + std::to_string(ID));
        }
    }

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