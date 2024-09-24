#ifndef ETHERNET_HPP
#define ETHERNET_HPP

#include <vector>
#include <string>
#include <inttypes.h>

#include <stdexcept>


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


#endif // ETHERNET_HPP