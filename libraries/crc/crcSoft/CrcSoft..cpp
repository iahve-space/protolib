#include "CrcSoft.hpp"

void CrcSoft::reset() {}

auto CrcSoft::calc(const CustomSpan<uint8_t> DATA) -> uint32_t {
  uint32_t crc = 0;
  crc = ~crc;
  size_t count = DATA.size();
  const uint8_t *ptr = DATA.data();
  while (count > 0) {
    const uint32_t VAL = *ptr++;
    crc = crc >> CHAR_BIT ^ crc32r_table_[(crc ^ VAL) & UINT8_MAX];
    count--;
  }
  return ~crc;
}

auto CrcSoft::append(uint32_t crc, const CustomSpan<uint8_t> DATA) -> uint32_t {
  crc = ~crc;
  size_t count = DATA.size();
  const uint8_t *ptr = DATA.data();
  while (count > 0) {
    const uint32_t VAL = *ptr++;
    crc = crc >> CHAR_BIT ^ crc32r_table_[(crc ^ VAL) & UINT8_MAX];
    count--;
  }
  return ~crc;
}
