#include "Crc16Modbus.hpp"

uint32_t Crc16Modbus::Calc(const CustomSpan<uint8_t> data) {
    Reset();
    return Append(crc_, data);
}

uint32_t Crc16Modbus::Append(uint32_t dump, CustomSpan<uint8_t> data) {
    (void)dump;
    uint8_t *dataPtr = data.data();
    uint8_t ch;
    for (size_t i = 0; i < data.size(); i++) {
        ch = *dataPtr++;
        crc_ = crctalbeabs[(ch ^ crc_) & 15] ^ (crc_ >> 4);
        crc_ = crctalbeabs[((ch >> 4) ^ crc_) & 15] ^ (crc_ >> 4);
    }
    return crc_;
}

void Crc16Modbus::Reset() {
    crc_ = 0xFFFF;
}