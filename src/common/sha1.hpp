#pragma once

#include <stdint.h>
#include <string>
#include <string.h>
#include <memory>
#include "hex.hpp"

#ifndef _common_sha1_hpp
#define _common_sha1_hpp

namespace epilog { namespace common {

#define epilog_rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))
#define epilog_blk0(i) (block->l[i] = (epilog_rol(block->l[i],24)&0xff00ff00) |(epilog_rol(block->l[i],8)&0x00ff00ff))
#define epilog_blk(i) (block->l[i&15] = epilog_rol(block->l[(i+13)&15]^block->l[(i+8)&15]^block->l[(i+2)&15]^block->l[i&15],1))

#define epilog_R0(v, w, x, y, z, i) \
    z+=((w&(x^y))^y)+epilog_blk0(i)+0x5a827999+epilog_rol(v,5);w=epilog_rol(w,30);
#define epilog_R1(v, w, x, y, z, i) \
    z+=((w&(x^y))^y)+epilog_blk(i)+0x5a827999+epilog_rol(v,5);w=epilog_rol(w,30);
#define epilog_R2(v, w, x, y, z, i) \
    z+=(w^x^y)+epilog_blk(i)+0x6ed9eba1+epilog_rol(v,5);w=epilog_rol(w,30);
#define epilog_R3(v, w, x, y, z, i) \
    z+=(((w|x)&y)|(w&x))+epilog_blk(i)+0x8f1bbcdc+epilog_rol(v,5);w=epilog_rol(w,30);
#define epilog_R4(v, w, x, y, z, i) \
    z+=(w^x^y)+epilog_blk(i)+0xca62c1d6+epilog_rol(v,5);w=epilog_rol(w,30);

class sha1 {
public:
  static const size_t HASH_SIZE = 20;
  static const size_t BLOCK_SIZE = 64;
    
  sha1() {
    init();
  }

  void init() {
    state[0] = 0x67452301;
    state[1] = 0xefcdab89;
    state[2] = 0x98badcfe;
    state[3] = 0x10325476;
    state[4] = 0xc3d2e1f0;
    count[0] = count[1] = 0;
  }

  void update(const void *p, size_t len) {
    const uint8_t *data = reinterpret_cast<const uint8_t *>(p);
    size_t i, j;

    j = (count[0] >> 3) & 63;
    if ((count[0] += (uint32_t) (len << 3)) < (len << 3)) {
        count[1]++;
    }
    count[1] += (uint32_t) (len >> 29);
    if ((j + len) > 63) {
        memcpy(&buffer[j], data, (i = 64 - j));
        transform(buffer);
        for (; i + 63 < len; i += 64) {
	    memcpy(buffer, data + i, 64);
            transform(buffer);
        }
        j = 0;
    }
    else i = 0;
    memcpy(&buffer[j], &data[i], len - i);
  }

  const std::string finalize() {
    uint8_t dig[HASH_SIZE];
    finalize(dig);
    return hex::to_string(dig, HASH_SIZE);
  }
  
  void finalize(uint8_t digest[HASH_SIZE]) {
    uint32_t i;
    uint8_t finalcount[8];

    for (i = 0; i < 8; i++) {
        finalcount[i] = (uint8_t) ((count[(i >= 4 ? 0 : 1)]
                >> ((3 - (i & 3)) * 8)) & 255);
    }
    update((uint8_t *) "\200", 1);
    while ((count[0] & 504) != 448) {
        update((uint8_t *) "\0", 1);
    }
    update(finalcount, 8);
    for (i = 0; i < HASH_SIZE; i++) {
        digest[i] = (uint8_t)
                ((state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
    }

    i = 0;
    memset(buffer, 0, 64);
    memset(state, 0, 20);
    memset(count, 0, 8);
    memset(finalcount, 0, 8);
  }

private:

   inline void transform(const uint8_t buffer[64]) {
     uint32_t a, b, c, d, e;
     typedef union {
       uint8_t c[64];
       uint32_t l[16];
     } CHAR64LONG16;
     CHAR64LONG16 *block;

     block = (CHAR64LONG16*)buffer;

     /* Copy state to working vars */
     a = state[0];
     b = state[1];
     c = state[2];
     d = state[3];
     e = state[4];
     
     /* 4 rounds of 20 operations each. Loop unrolled. */
     epilog_R0(a, b, c, d, e, 0);
     epilog_R0(e, a, b, c, d, 1);
     epilog_R0(d, e, a, b, c, 2);
     epilog_R0(c, d, e, a, b, 3);
     epilog_R0(b, c, d, e, a, 4);
     epilog_R0(a, b, c, d, e, 5);
     epilog_R0(e, a, b, c, d, 6);
     epilog_R0(d, e, a, b, c, 7);
     epilog_R0(c, d, e, a, b, 8);
     epilog_R0(b, c, d, e, a, 9);
     epilog_R0(a, b, c, d, e, 10);
     epilog_R0(e, a, b, c, d, 11);
     epilog_R0(d, e, a, b, c, 12);
     epilog_R0(c, d, e, a, b, 13);
     epilog_R0(b, c, d, e, a, 14);
     epilog_R0(a, b, c, d, e, 15);
     epilog_R1(e, a, b, c, d, 16);
     epilog_R1(d, e, a, b, c, 17);
     epilog_R1(c, d, e, a, b, 18);
     epilog_R1(b, c, d, e, a, 19);
     epilog_R2(a, b, c, d, e, 20);
     epilog_R2(e, a, b, c, d, 21);
     epilog_R2(d, e, a, b, c, 22);
     epilog_R2(c, d, e, a, b, 23);
     epilog_R2(b, c, d, e, a, 24);
     epilog_R2(a, b, c, d, e, 25);
     epilog_R2(e, a, b, c, d, 26);
     epilog_R2(d, e, a, b, c, 27);
     epilog_R2(c, d, e, a, b, 28);
     epilog_R2(b, c, d, e, a, 29);
     epilog_R2(a, b, c, d, e, 30);
     epilog_R2(e, a, b, c, d, 31);
     epilog_R2(d, e, a, b, c, 32);
     epilog_R2(c, d, e, a, b, 33);
     epilog_R2(b, c, d, e, a, 34);
     epilog_R2(a, b, c, d, e, 35);
     epilog_R2(e, a, b, c, d, 36);
     epilog_R2(d, e, a, b, c, 37);
     epilog_R2(c, d, e, a, b, 38);
     epilog_R2(b, c, d, e, a, 39);
     epilog_R3(a, b, c, d, e, 40);
     epilog_R3(e, a, b, c, d, 41);
     epilog_R3(d, e, a, b, c, 42);
     epilog_R3(c, d, e, a, b, 43);
     epilog_R3(b, c, d, e, a, 44);
     epilog_R3(a, b, c, d, e, 45);
     epilog_R3(e, a, b, c, d, 46);
     epilog_R3(d, e, a, b, c, 47);
     epilog_R3(c, d, e, a, b, 48);
     epilog_R3(b, c, d, e, a, 49);
     epilog_R3(a, b, c, d, e, 50);
     epilog_R3(e, a, b, c, d, 51);
     epilog_R3(d, e, a, b, c, 52);
     epilog_R3(c, d, e, a, b, 53);
     epilog_R3(b, c, d, e, a, 54);
     epilog_R3(a, b, c, d, e, 55);
     epilog_R3(e, a, b, c, d, 56);
     epilog_R3(d, e, a, b, c, 57);
     epilog_R3(c, d, e, a, b, 58);
     epilog_R3(b, c, d, e, a, 59);
     epilog_R4(a, b, c, d, e, 60);
     epilog_R4(e, a, b, c, d, 61);
     epilog_R4(d, e, a, b, c, 62);
     epilog_R4(c, d, e, a, b, 63);
     epilog_R4(b, c, d, e, a, 64);
     epilog_R4(a, b, c, d, e, 65);
     epilog_R4(e, a, b, c, d, 66);
     epilog_R4(d, e, a, b, c, 67);
     epilog_R4(c, d, e, a, b, 68);
     epilog_R4(b, c, d, e, a, 69);
     epilog_R4(a, b, c, d, e, 70);
     epilog_R4(e, a, b, c, d, 71);
     epilog_R4(d, e, a, b, c, 72);
     epilog_R4(c, d, e, a, b, 73);
     epilog_R4(b, c, d, e, a, 74);
     epilog_R4(a, b, c, d, e, 75);
     epilog_R4(e, a, b, c, d, 76);
     epilog_R4(d, e, a, b, c, 77);
     epilog_R4(c, d, e, a, b, 78);
     epilog_R4(b, c, d, e, a, 79);
     
     /* Add the working vars back into context.state[] */
     state[0] += a;
     state[1] += b;
     state[2] += c;
     state[3] += d;
     state[4] += e;
     
     /* Wipe variables */
     a = b = c = d = e = 0;
   }

  uint32_t state[5];
  uint32_t count[2];
  uint8_t buffer[64];
};

}}

#endif
