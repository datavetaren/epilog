#pragma once

#ifndef _common_random_hpp
#define _common_random_hpp

#include <string>

namespace epilog { namespace common {

class random {
public:
    static std::string next(size_t entropy_bits = 128);
    static int next_int(int max);
    static void next_bytes(uint8_t *bytes, size_t bytes_sz);
    static uint64_t next_int(uint64_t max);

    static void set_for_testing(bool for_testing);

private:
    static bool for_testing_;
};

}}

#endif
