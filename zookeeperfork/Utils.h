#ifndef _UTILS_
#define _UTILS_

#include <string>
#include <stdint.h>

const std::string quote_string(const std::string &s);
uint32_t epoch_from_zxid(uint64_t zxid);
uint32_t counter_from_zxid(uint64_t zxid);
uint64_t make_zxid(uint32_t epoch, uint32_t counter);

#endif
