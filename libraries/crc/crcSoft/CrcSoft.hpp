#pragma once
#include <array>
#include <climits>
#include <cstdint>

#include "Crc.hpp"

#define CRC32_POLY 0x04C11DB7
#define CRC32_POLY_R 0xEDB88320

class CrcSoft : public ICrc {
 public:
  CrcSoft() : ICrc("crc32 arm module") {
    for (int i = 0; i < 256; ++i) {
      uint32_t CRR = i;
      uint32_t CHAR = i << 24;
      for (int j = CHAR_BIT; j > 0; --j) {
        CHAR =
            (CHAR & 0x80000000) != 0U ? (CHAR << 1) ^ CRC32_POLY : (CHAR << 1);
        CRR = (CRR & 0x00000001) != 0U ? (CRR >> 1) ^ CRC32_POLY_R : (CRR >> 1);
      }
      crc32_table_[i] = CHAR;
      crc32r_table_[i] = CRR;
    }
  }
  void reset() override;

  auto calc(CustomSpan<uint8_t> /*unused*/) -> uint32_t override;
  auto append(uint32_t crc, CustomSpan<uint8_t> /*unused*/)
      -> uint32_t override;

 private:
  std::array<uint32_t, UINT8_MAX + 1> crc32_table_{};
  std::array<uint32_t, UINT8_MAX + 1> crc32r_table_{};
};
