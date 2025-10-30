#pragma once

#include <array>

#include "Crc.hpp"
#include "CustomSpan.hpp"

class Crc16Modbus : ICrc {
 public:
  auto calc(CustomSpan<uint8_t> data) -> uint32_t override;
  auto append(uint32_t CRC, CustomSpan<uint8_t> DATA) -> uint32_t override;
  void reset() override;
  Crc16Modbus() : ICrc("crc32 arm module") {}

 private:
  uint16_t crc_{0};
  constexpr static std::array<uint16_t, 16> CRC_TABLES = {
      0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
      0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400};
};
