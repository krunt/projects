#include "Utils.h"

const std::string quote_string(const std::string &s) {
    std::string res;
    for (int i = 0; i < s.size(); ++i) {
        char c = s.at(i);
        switch (c) {
          case '\t':
            res.push_back('\\');
            res.push_back('t');
            break;
          case '\f':
            res.push_back('\\');
            res.push_back('f');
            break;
          case '\r':
            res.push_back('\\');
            res.push_back('r');
            break;
          case '\a':
            res.push_back('\\');
            res.push_back('a');
            break;
          case '\b':
            res.push_back('\\');
            res.push_back('b');
            break;
          case '\n':
            res.push_back('\\');
            res.push_back('n');
            break;
          default:
            res.push_back(c); 
            break;
        };
    }
    return res;
}

uint32_t epoch_from_zxid(uint64_t zxid) {
    return zxid >> 32;
}

uint32_t counter_from_zxid(uint64_t zxid) {
    return zxid & 0xFFFFFFFFU;
}

uint64_t make_zxid(uint32_t epoch, uint32_t counter) {
    return epoch << 32 | counter;
}
