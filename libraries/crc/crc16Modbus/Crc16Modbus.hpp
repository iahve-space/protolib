#pragma once

#include "CustomSpan.hpp"
#include "Crc.hpp"

class Crc16Modbus : ICrc{
public:
    uint32_t Calc(CustomSpan<uint8_t> data) override;
    uint32_t Append(uint32_t crc, CustomSpan<uint8_t> data) override;
    void Reset() override;
    Crc16Modbus() : ICrc("crc32 arm module"){}

private:
    uint16_t crc_{0};
    constexpr static const uint16_t crctalbeabs[] = {
            0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
            0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400
    };
};

