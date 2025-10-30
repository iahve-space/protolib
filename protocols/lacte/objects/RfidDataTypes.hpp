#pragma once

#include <cctype>
#include <cstdint>
#include <cstring>
#include <ostream>
#include <string>
#include <string_view>

#include "protocols/lacte/objects/ActivationTimeType.hpp"
#include "protocols/lacte/objects/DrinkCounterType.hpp"
#include "protocols/lacte/objects/LacteIdType.hpp"
#include "protocols/lacte/objects/LacteSnType.hpp"
#include "protocols/lacte/objects/MachineSnType.hpp"
#include "protocols/lacte/objects/MagicWordType.hpp"
#include "protocols/lacte/objects/McuUidType.hpp"
#include "protocols/lacte/objects/ProductDateType.hpp"
#include "protocols/lacte/objects/ProductShelfLifeType.hpp"
#include "protocols/lacte/objects/ProductVolumeType.hpp"
#include "protocols/lacte/objects/TimeCounterType.hpp"
#include "protocols/lacte/objects/UsageTimeType.hpp"

namespace proto::lacte {

using namespace std::string_view_literals;

#pragma pack(push, 1)

// ── целый пакет
// ───────────────────────────────────────────────────────────────
struct RFIDDataPacketType {
  MagicWord m_magicWord;  // 0xCCAA
  LacteId m_lacteId;
  LacteSn m_lacteSn;  // SN пачки молока
  uint16_t m_reserve{};
  ProductVolume m_productVolume;
  ProdDate m_prodDate;           // Unix timestampÏ
  ProductShelfLife m_shelfLife;  // резерв
  UsageTime m_usageTime;
  McuUid m_mcuUid;        // серийник платы
  MachineSn m_machineSn;  // серийник устройства
  ActivationTime m_activationTime;
  DrinkCounter m_drinkCounter;
  TimeCounter m_timeCounter;

  auto operator==(const RFIDDataPacketType& other) const -> bool {
    return std::memcmp(this, &other, sizeof(RFIDDataPacketType)) == 0;
  }
  auto operator!=(const RFIDDataPacketType& other) const -> bool {
    return !(*this == other);
  }

  friend auto operator<<(std::ostream& out, const RFIDDataPacketType& val)
      -> std::ostream& {
    out << "RFIDDataPacket:\n  " << val.m_magicWord << "\n  " << val.m_lacteId
        << "\n  " << val.m_lacteSn << "\n  " << val.m_productVolume << "\n  "
        << val.m_prodDate << "\n  " << val.m_shelfLife << "\n  "
        << val.m_usageTime << "\n  " << val.m_mcuUid << "\n  "
        << val.m_machineSn << "\n  " << val.m_activationTime << "\n  "
        << val.m_drinkCounter << "\n  " << val.m_timeCounter;
    return out;
  }
};

#pragma pack(pop)

// (Необязательно) гарантии размеров базовых полей

static_assert(sizeof(RFIDDataPacketType) == 52,
              "RFIDDataPacketType must be 52 bytes");

}  // namespace proto::lacte