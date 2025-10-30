#pragma once

#include <cstdint>
#include <tuple>

#include "protocols/lacte/packets/BootAnswerType.hpp"
#include "protocols/lacte/packets/InfoPacketType.hpp"
#include "protocols/lacte/packets/UIDPacketType.hpp"
#include "protocols/lacte/packets/VersionPacketType.hpp"

#include "protocols/lacte/objects/RfidDataTypes.hpp"

#include "ProtocolEndpoint.hpp"

namespace proto::lacte {

enum class Params : uint8_t {
  SOME_PARAM1 = 0,
  SOME_PARAM2 = 1,
  SOME_PARAM3 = 2,
  SOME_PARAM4 = 3
};

enum PacketNumbers : uint8_t {
  INFO = 0x00,
  VERSION = 0x01,
  UID = 0x02,
  RFID_ID = 0x03,
  RFID_DATA = 0x04,
  SET_PARAMS = 0x40,
  GET_PARAMS = 0x41,
  RESTART = 0x7F
};

inline auto operator<<(std::ostream& out, const PacketNumbers PACKET)
    -> std::ostream& {
  switch (PACKET) {
    case INFO:
      return out << "INFO(0x00)";
    case VERSION:
      return out << "VERSION(0x01)";
    case UID:
      return out << "UID(0x02)";
    case RFID_ID:
      return out << "RFID_ID(0x03)";
    case RFID_DATA:
      return out << "RFID_DATA(0x04)";
    case SET_PARAMS:
      return out << "SET_PARAMS(0x40)";
    case GET_PARAMS:
      return out << "GET_PARAMS(0x41)";
    case RESTART:
      return out << "RESTART(0x7F)";
    default:
      return out << "UNKNOWN(0x" << std::hex << static_cast<int>(PACKET)
                 << std::dec << ")";
  }
}

inline constexpr uint8_t HOST_PREFIX[2] = {0xFF, 0x55};
template <uint8_t* BASE>
struct HostPacket {
  using Packets = std::tuple<
      PacketInfo<INFO, EmptyDataType>, PacketInfo<VERSION, EmptyDataType>,
      PacketInfo<UID, EmptyDataType>, PacketInfo<RFID_ID, EmptyDataType>,
      PacketInfo<RFID_DATA, EmptyDataType>, PacketInfo<SET_PARAMS, uint8_t*>,
      PacketInfo<GET_PARAMS, Params>, PacketInfo<RESTART, uint8_t*>>;

  using idFieldType = FieldPrototype<FieldName::ID_FIELD, const uint8_t*, BASE,
                                     FieldFlags::NOTHING, 2, 2, HOST_PREFIX>;
  using lenFieldType = FieldPrototype<FieldName::LEN_FIELD, uint8_t, BASE,
                                      FieldFlags::IS_IN_CRC>;
  using timeFieldType =
      FieldPrototype<FieldName::TIME_FIELD, uint32_t, BASE,
                     FieldFlags::IS_IN_CRC | FieldFlags::IS_IN_LEN>;
  using typeFieldType =
      FieldPrototype<FieldName::TYPE_FIELD, uint8_t, BASE,
                     FieldFlags::IS_IN_CRC | FieldFlags::IS_IN_LEN>;
  using dataFieldType =
      DataFieldPrototype<Packets, BASE,
                         FieldFlags::IS_IN_CRC | FieldFlags::IS_IN_LEN>;
  using crcFieldType =
      FieldPrototype<FieldName::CRC_FIELD, uint16_t, BASE, FieldFlags::REVERSE>;

  using packet_fields = std::tuple<idFieldType, lenFieldType, timeFieldType,
                                   typeFieldType, dataFieldType, crcFieldType>;
};

inline constexpr uint8_t BOARD_PREFIX[2] = {0xFF, 0xAA};
template <uint8_t* BASE>
struct BoardPacket {
  using Packets = std::tuple<
      PacketInfo<INFO, InfoPacketType>, PacketInfo<VERSION, VersionPacketType>,
      PacketInfo<UID, UIDPacketType>, PacketInfo<RFID_ID, RFIDNumberType>,
      PacketInfo<RFID_DATA, RFIDDataPacketType>,
      PacketInfo<SET_PARAMS, uint8_t*>, PacketInfo<GET_PARAMS, uint8_t*>,
      PacketInfo<RESTART, BootAnswerType>>;
  using boardIdFieldType =
      FieldPrototype<FieldName::ID_FIELD, const uint8_t*, BASE,
                     FieldFlags::NOTHING, 2, 2, BOARD_PREFIX>;
  using boardLenFieldType = FieldPrototype<FieldName::LEN_FIELD, uint8_t, BASE,
                                           FieldFlags::IS_IN_CRC>;
  using boardAnsCommFieldType =
      FieldPrototype<FieldName::TYPE_FIELD, uint8_t, BASE,
                     FieldFlags::IS_IN_CRC | FieldFlags::IS_IN_LEN>;
  using boardDataFieldType =
      DataFieldPrototype<Packets, BASE,
                         FieldFlags::IS_IN_CRC | FieldFlags::IS_IN_LEN>;
  using boardCrcFieldType =
      FieldPrototype<FieldName::CRC_FIELD, uint16_t, BASE, FieldFlags::REVERSE>;

  using packet_fields =
      std::tuple<boardIdFieldType, boardLenFieldType, boardAnsCommFieldType,
                 boardDataFieldType, boardCrcFieldType>;
};

inline auto operator<<(std::ostream& out, const EmptyDataType& /*unused*/)
    -> std::ostream& {
  out << "\nEmptyDataType" << '\n';
  return out;
}

}  // namespace proto::lacte