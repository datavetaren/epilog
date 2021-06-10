#pragma once

#ifndef _db_util_hpp
#define _db_util_hpp

#include "../common/sha1.hpp"
#include "../common/bits.hpp"

namespace epilog { namespace db {

using fstream = std::fstream;

    
struct hash_t {
    bool operator == (const hash_t &other) const {
        return memcmp(hash, other.hash, sizeof(hash)) == 0;
    }
  
    uint8_t hash[common::sha1::HASH_SIZE];
};
    
}}

#endif
