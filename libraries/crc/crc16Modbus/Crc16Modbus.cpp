#include "Crc16Modbus.hpp"

auto Crc16Modbus::calc(const CustomSpan<uint8_t> DATA) -> uint32_t {
  reset();
  return append(crc_, DATA);
}

auto Crc16Modbus::append(const uint32_t CRC, const CustomSpan<uint8_t> DATA)
    -> uint32_t {
  (void)CRC;
  const uint8_t *dataPtr = DATA.data();
  for (size_t i = 0; i < DATA.size(); i++) {
    const uint8_t CHAR = *dataPtr++;
    crc_ = CRC_TABLES[(CHAR ^ crc_) & 15] ^ crc_ >> 4;
    crc_ = CRC_TABLES[(CHAR >> 4 ^ crc_) & 15] ^ crc_ >> 4;
  }
  return crc_;
}

void Crc16Modbus::reset() { crc_ = UINT16_MAX; }