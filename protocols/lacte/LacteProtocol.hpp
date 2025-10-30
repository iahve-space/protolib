#pragma once

#include <NamedTuple.hpp>

#include "LacteProtocolPrototype.hpp"
#include "ProtocolEndpoint.hpp"
#include "Ymodem.hpp"
#include "libraries/crc/crc16Modbus/Crc16Modbus.hpp"

namespace proto::lacte {
template <uint8_t *RX_BASE, uint8_t *TX_BASE>
class LacteHostProtocol
    : public ProtocolEndpoint<typename BoardPacket<RX_BASE>::packet_fields,
                              typename HostPacket<TX_BASE>::packet_fields,
                              Crc16Modbus> {
 public:
  using ReceiveType =
      typename ProtocolEndpoint<typename BoardPacket<RX_BASE>::packet_fields,
                                typename HostPacket<TX_BASE>::packet_fields,
                                Crc16Modbus>::ReceiveType;

  template <typename DATA_TYPE, PacketNumbers NUM>
  auto get() -> std::optional<DATA_TYPE> {
    static std::mutex mtx;
    std::lock_guard lock(mtx);
    uint32_t time = static_cast<uint32_t>(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    PacketNumbers num = NUM;
    std::optional<DATA_TYPE> result;
    auto snap = this->request(make_field_info<FieldName::TYPE_FIELD>(&num),
                              make_field_info<FieldName::TIME_FIELD>(&time));
    auto data_field = meta::get_named<FieldName::DATA_FIELD>(snap);
    if (std::holds_alternative<DATA_TYPE>(data_field)) {
      result = std::get<DATA_TYPE>(data_field);
    }
    return result;
  }
  template <typename DATA_TYPE, Params NUM>
  auto get_param() -> std::optional<DATA_TYPE> {
    Params num = NUM;
    std::optional<DATA_TYPE> result;
    uint32_t time = static_cast<uint32_t>(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

    auto param = this->ReguestParam(
        make_field_info<proto::FieldName::DATA_FIELD>(&num),
        make_field_info<proto::FieldName::TIME_FIELD>(&time));
    if (std::holds_alternative<DATA_TYPE>(param)) {
      return std::get<DATA_TYPE>(param);
    }
    return std::nullopt;
  }

  auto get_info() -> std::optional<InfoPacketType> {
    return get<InfoPacketType, INFO>();
  }
  auto get_version() -> std::optional<VersionPacketType> {
    return get<VersionPacketType, VERSION>();
  }
  auto get_uid() -> std::optional<UIDPacketType> {
    return get<UIDPacketType, UID>();
  }
  auto get_rfid() -> std::optional<RFIDNumberType> {
    return get<RFIDNumberType, RFID_ID>();
  }
  auto get_rfid_data() -> std::optional<RFIDDataPacketType> {
    return get<RFIDDataPacketType, RFID_DATA>();
  }
  auto restart() -> std::optional<BootAnswerType> {
    return get<BootAnswerType, RESTART>();
  }
  static auto flash(const char *path, interface::IInterface &interface)
      -> bool {
    YmodemPrerelease ymodem(interface);
    return ymodem.send(path) == 0;
  }
};

template <uint8_t *RX_BASE, uint8_t *TX_BASE>
class LacteBoardProtocol
    : public ProtocolEndpoint<typename HostPacket<RX_BASE>::packet_fields,
                              typename BoardPacket<TX_BASE>::packet_fields,
                              Crc16Modbus> {
 public:
  template <class PARAM_TYPE>
  auto answer(Params param_number, PARAM_TYPE param) -> size_t {
    std::vector<uint8_t> transaction(sizeof(param) + 1);
    transaction[0] = static_cast<uint8_t>(param_number);
    std::memcpy(transaction.data() + 1, static_cast<void *>(&param),
                sizeof(param));

    uint8_t num = GET_PARAMS;
    return this->Send(proto::make_field_info<FieldName::TYPE_FIELD>(&num),
                      proto::make_field_info<FieldName::DATA_FIELD>(
                          transaction.data(), transaction.size()));
  }

  template <class PARAM_TYPE>
  auto answer(PacketNumbers param_number, PARAM_TYPE param) -> size_t {
    return this->send(
        proto::make_field_info<FieldName::TYPE_FIELD>(&param_number),
        proto::make_field_info<FieldName::DATA_FIELD>(&param));
  }
};
}  // namespace proto::lacte