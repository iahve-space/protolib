#pragma once
#include <cstdint>

#include "cstddef"
#include "Crc.hpp"


#define CRC32_POLY   0x04C11DB7
#define CRC32_POLY_R 0xEDB88320

class CrcSoft : public ICrc {
public:
    CrcSoft(): ICrc("crc32 arm module")
    {
      int i, j;
      uint32_t c, cr;
      for (i = 0; i < 256; ++i)
      {
        cr = i;
        c = i << 24;
        for (j = 8; j > 0; --j)
        {
          c = c & 0x80000000 ? (c << 1) ^ CRC32_POLY : (c << 1);
          cr = cr & 0x00000001 ? (cr >> 1) ^ CRC32_POLY_R : (cr >> 1);
        }
        crc32_table[i] = c;
        crc32r_table[i] = cr;
        //dprintf("crc32r_table[%u] = %X ", i, crc32r_table[i]);
      }
    }
    void Reset() override;

  uint32_t Calc(CustomSpan<uint8_t>) override;
  uint32_t Append(uint32_t crc, CustomSpan<uint8_t>) override;
private:
  uint32_t crc32_table[256]{};
  uint32_t crc32r_table[256]{};
};

