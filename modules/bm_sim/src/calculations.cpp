/* Copyright 2013-present Barefoot Networks, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Antonin Bas (antonin@barefootnetworks.com)
 *
 */

#include "bm_sim/calculations.h"

#include "xxhash.h"
#include "crc_tables.h"

namespace {

/* This code was adapted from:
   http://www.barrgroup.com/Embedded-Systems/How-To/CRC-Calculation-C-Code */

uint32_t reflect(uint32_t data, int nBits) {
  uint32_t reflection = 0x00000000;
  int bit;

  // Reflect the data about the center bit.
  for (bit = 0; bit < nBits; ++bit) {
    // If the LSB bit is set, set the reflection of it.
    if (data & 0x01) {
      reflection |= (1 << ((nBits - 1) - bit));
    }
    data = (data >> 1);
  }

  return reflection;
}

}

namespace hash {

template <typename T,
	  typename std::enable_if<std::is_unsigned<T>::value, int>::type = 0>
T xxh64(const char *buf, size_t len) {
  return static_cast<T>(XXH64(buf, len, 0));
}

template <typename T,
	  typename std::enable_if<std::is_unsigned<T>::value, int>::type = 0>
T crc16(const char *buf, size_t len) {
  uint16_t remainder = 0x0000;
  uint16_t final_xor_value = 0x0000;
  for(unsigned int byte = 0; byte < len; byte++) {
    int data = reflect(buf[byte], 8) ^ (remainder >> 8);
    remainder = table_crc16[data] ^ (remainder << 8);
  }
  return static_cast<T>(reflect(remainder, 16) ^ final_xor_value);
}

template unsigned int xxh64<unsigned int>(const char *, size_t);
template uint64_t xxh64<uint64_t>(const char *, size_t);

template unsigned int crc16<unsigned int>(const char *, size_t);
template uint64_t crc16<uint64_t>(const char *, size_t);

}


