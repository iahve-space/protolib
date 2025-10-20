#include "CrcArm.hpp"


void Crc32Arm::Reset()
{
  __HAL_CRC_DR_RESET(&hcrc_);
}

uint32_t Crc32Arm::Calc(const uint8_t *buffer, size_t count) {
  __HAL_CRC_DR_RESET(&hcrc_);
  uint32_t crc = HAL_CRC_Calculate(&hcrc_, (uint32_t*)buffer, count);
  return ~(crc);
}

uint32_t Crc32Arm::Append(uint32_t crc, const uint8_t *buffer, size_t count) {
  uint32_t updated_crc = HAL_CRC_Accumulate(&hcrc_, (uint32_t*)buffer, count);
  return ~(updated_crc);
}


