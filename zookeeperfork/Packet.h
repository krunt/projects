#ifndef _PACKET_
#define _PACKET_

#include <string>
#include <arpa/inet.h>

class Packet {
public:
    Packet() 
    {}

    void reset() {
        data_.clear();
    }

    const char *data() const {
        return data_.c_str();
    }

    int size() const {
        return data_.size();
    }

    Packet &append(uint16_t val) {
        uint16_t nval = htons(val);
        data_.append(reinterpret_cast<const char *>(&nval), sizeof(nval));
        return *this;
    }

    Packet &get(uint16_t &val) {
        assert(data_.size() >= sizeof(val));
        val = ntohs(((uint16_t)data_[0]) << 8 + (uint16_t)data_[1]);
        return *this;
    }

    Packet &append(uint32_t val) {
        uint32_t nval = htonl(val);
        data_.append(reinterpret_cast<const char *>(&nval), sizeof(nval));
        return *this;
    }

    Packet &get(uint32_t &val) {
        assert(data_.size() >= sizeof(val));
        val = ntohl(((uint32_t)data_[0]) << 24 
                            + ((uint32_t)data_[1]) << 16
                            + ((uint32_t)data_[2]) << 8
                            + (uint32_t)data_[3]);
        return *this;
    }

    Packet &append(uint64_t val) {
        uint64_t nval = val;
        // if big endian
        if (htons(0x00FF) == 0xFF00) {
            nval = (uint64_t)htonl(val & 0xFFFFFFFFU) << 32 | htonl(val >> 32);
        } 
        data_.append(reinterpret_cast<const char *>(&nval), sizeof(nval));
        return *this;
    }

    Packet &append(const char *data, int size) {
        data_.append(data, size);
        return *this;
    }

    Packet &append(const std::string &data) {
        data_.append(data);
        return *this;
    }

    Packet &get(std::string &data) {
        data = data_;
        return *this;
    }

    template <typename T>
    Packet &operator << (const T &data) {
        append(data);
        return *this;
    }

    template <typename T>
    Packet &operator >> (T &data) {
        get(data);
        return *this;
    }

private:
    std::string data_;
};

#endif
