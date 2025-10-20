#include "CrcSoft.hpp"


void CrcSoft::Reset()
{

}

uint32_t CrcSoft::Calc(const CustomSpan<uint8_t> data) {
    uint32_t v;
    uint32_t crc = 0;
    crc = ~crc;
    size_t count = data.size();
    uint8_t *ptr = data.data();
    while(count > 0)
    {
        v = *ptr++;
        crc = ( crc >> 8 ) ^ crc32r_table[( crc ^ (v ) ) & 0xff];
        count--;
    }
    return ~crc;
}

uint32_t CrcSoft::Append(uint32_t crc, const CustomSpan<uint8_t> data) {
    uint32_t v;
    crc = ~crc;
    size_t count = data.size();
    uint8_t *ptr = data.data();
    while(count > 0)
    {
        v = *ptr++;
        crc = ( crc >> 8 ) ^ crc32r_table[( crc ^ (v ) ) & 0xff];
        count--;
    }
    return ~crc;
}


