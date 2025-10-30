#pragma once

#ifdef HAL_CONFIG
#include <cstdint>
#include "cstddef"
#include "Crc.hpp"


class Crc32Arm : public ICrc {
public:
    Crc32Arm(): ICrc("crc32 arm module")
    {
      __HAL_RCC_CRC_CLK_ENABLE();  // Включите тактирование блока CRC
      HAL_CRC_DeInit(&hcrc_);  // Деинициализация CRC
      hcrc_.Instance = CRC;
      hcrc_.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
      hcrc_.Init.CRCLength = CRC_POLYLENGTH_32B;
      hcrc_.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
      hcrc_.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_BYTE;//CRC_INPUTDATA_INVERSION_NONE;
      hcrc_.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_ENABLE;//CRC_OUTPUTDATA_INVERSION_ENABLE;
      hcrc_.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;

      if( HAL_CRC_Init(&hcrc_) !=HAL_OK)
      {
        throw "can't init crc";
      }
    }
    void Reset() override;

  uint32_t Calc(const uint8_t *buffer, size_t count) override;
  uint32_t Append(uint32_t crc,const uint8_t *buffer, size_t count) override;
private:
    CRC_HandleTypeDef hcrc_{};
};

#endif //HAL_CONFIG
