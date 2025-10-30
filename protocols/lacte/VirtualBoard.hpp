#pragma once

#include <cstdint>
#include <libraries/interfaces/Echo.hpp>

#include "protocols/lacte/LacteProtocol.hpp"
#include "protocols/lacte/objects/RfidNumberType.hpp"

namespace proto::lacte {

template <uint8_t* RX_BASE, uint8_t* TX_BASE>
class VirtualBoard {
 public:
  using RxFieldsSnapshot =
      typename LacteBoardProtocol<RX_BASE, TX_BASE>::RxFieldsSnapshot;
  static constexpr uint32_t DEFAULT_LACTE_SN = 1105824325;
  static constexpr std::array<uint8_t, 2> DEFAULT_VERSION = {1, 0};
  static constexpr std::array<uint8_t, 4> DEFAULT_PROD_DATE = {0x20, 0x23, 0x10,
                                                               0x01};
  static constexpr std::array<uint8_t, 12> DEFAULT_MCU_UID = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B};
  static constexpr std::array<uint8_t, 12> DEFAULT_UID_DATA = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B};
  static constexpr std::array<uint8_t, 4> DEFAULT_MACHINE_SN = {0x11, 0x22,
                                                                0x33, 0x44};
  static constexpr std::array<uint8_t, 4> DEFAULT_ACTIVATION_TIME = {
      0x01, 0x05, 0x01, 0x02};
  static constexpr std::array<uint8_t, 4> DEFAULT_DRINK_COUNTER = {0x06, 0x05,
                                                                   0x04, 0x30};
  static constexpr std::array<uint8_t, 4> DEFAULT_TIME_COUNTER = {0x44, 0x33,
                                                                  0x22, 0x11};
  static constexpr std::array<uint8_t, 4> DEFAULT_RESERVE = {0x00, 0x00, 0x00,
                                                             0x00};
  static constexpr uint64_t DEFAULT_RFID_ID =
      3925928352713434350 & 0xffffffffffffff;

  VersionPacketType m_version_data{{1, 0}};
  InfoPacketType m_info_data{BoardStatus::IDLE, ErrorFlags{0x0},
                             RFIDNumberType{DEFAULT_RFID_ID}};
  UIDPacketType m_uid_data{1, 0};
  RFIDNumberType m_rfid{DEFAULT_RFID_ID};
  RFIDDataPacketType m_rfid_data{};

  // Setter methods for board state fields
  void set_version(const VersionPacketType& VAL) { m_version_data = VAL; }
  void set_info(const InfoPacketType& VAL) { m_info_data = VAL; }
  void set_uid(const UIDPacketType& VAL) { m_uid_data = VAL; }
  void set_rfid(const RFIDNumberType& VAL) {
    m_rfid = VAL;
    m_info_data.m_rfid = VAL;
  }
  void set_rfid_data(const RFIDDataPacketType& VAL) { m_rfid_data = VAL; }
  void set_magic_word(const MagicWord& VAL) { m_rfid_data.m_magicWord = VAL; }

  void set_lacte_id(const LacteId& VAL) {
    std::memcpy(&m_rfid_data.m_lacteId, &VAL, sizeof(m_rfid_data.m_lacteId));
  }

  void set_lacte_sn(const LacteSn& VAL) {
    std::memcpy(&m_rfid_data.m_lacteSn, &VAL, sizeof(m_rfid_data.m_lacteSn));
  }

  void set_prod_volume(const ProductVolume& VAL) {
    m_rfid_data.m_productVolume = VAL;
  }

  void set_prod_date(const ProdDate VAL) { m_rfid_data.m_prodDate = VAL; }

  void set_prod_shelf_life(const ProductShelfLife VAL) {
    m_rfid_data.m_shelfLife = VAL;
  }

  void set_usage_time(const UsageTime VAL) { m_rfid_data.m_usageTime = VAL; }

  void set_mcu_uid(const McuUid& VAL) { m_rfid_data.m_mcuUid = VAL; }

  void set_machine_sn(const MachineSn& VAL) {
    std::memcpy(&m_rfid_data.m_machineSn.m_data, &VAL.m_data,
                sizeof(m_rfid_data.m_machineSn.m_data));
  }

  void set_activation_time(const ActivationTime& VAL) {
    std::memcpy(&m_rfid_data.m_activationTime, &VAL,
                sizeof(m_rfid_data.m_activationTime));
  }

  void set_drink_counter(const DrinkCounter& VAL) {
    m_rfid_data.m_drinkCounter = VAL;
  }

  void set_time_counter(const TimeCounter VAL) {
    m_rfid_data.m_timeCounter = VAL;
  }

  LacteBoardProtocol<RX_BASE, TX_BASE> m_board_proto;
  interface::EchoInterface m_from_host_interface;
  interface::EchoInterface m_from_board_interface;
  interface::Delegate m_host_interface_send_delegate;

  interface::Delegate m_board_interface_send_delegate;
  typename decltype(m_board_proto.m_rx)::Delegate m_board_receive_delegate;

  void set_debug(bool debug) {
    m_board_proto.m_rx.set_debug(debug);
    m_board_proto.m_tx.set_debug(debug);
  }

  template <class HOST_CONTAINER>
  void set_host(HOST_CONTAINER& host_container) {
    m_board_interface_send_delegate =
        m_from_board_interface.add_receive_callback(
            [&](CustomSpan<uint8_t> span, size_t& read) {
              host_container.m_rx.fill(span, read);
            });
    host_container.set_interfaces(m_from_board_interface,
                                  m_from_host_interface);
  }

  explicit VirtualBoard() {
    m_from_host_interface.open();
    m_from_board_interface.open();

    m_board_proto.m_tx.set_interface(m_from_board_interface);

    m_host_interface_send_delegate = m_from_host_interface.add_receive_callback(
        [this](CustomSpan<uint8_t> span, size_t& read) {
          std::vector<uint8_t> data(span.size());
          memcpy(data.data(), span.data(), span.size());
          m_board_proto.m_rx.fill(span, read);
        });
    m_board_proto.set_receive_callback([&](RxFieldsSnapshot&& snap) {
      auto& field = meta::get_named<FieldName::TYPE_FIELD>(snap);

      if (field == INFO) {
        m_board_proto.answer(INFO, m_info_data);
      } else if (field == VERSION) {
        m_board_proto.answer(VERSION, m_version_data);
      }

      else if (field == UID) {
        m_board_proto.answer(UID, m_uid_data);
      } else if (field == RFID_ID) {
        m_board_proto.answer(RFID_ID, m_rfid);
      } else if (field == RFID_DATA) {
        m_board_proto.answer(RFID_DATA, m_rfid_data);
      } else if (field == RESTART) {
        m_board_proto.answer(RESTART, BootAnswerType());
      } else if (field == GET_PARAMS) {
        if (auto& field_data = meta::get_named<FieldName::DATA_FIELD>(snap);
            std::holds_alternative<Params>(field_data)) {
        }
      }
    });
    m_rfid_data.m_magicWord.m_data = MagicWord::DEFAULT_VAL;
    m_rfid_data.m_lacteSn.m_data = DEFAULT_LACTE_SN;
    memcpy(&m_version_data, DEFAULT_VERSION.data(), sizeof(DEFAULT_VERSION));
    memcpy(&m_rfid_data.m_prodDate, DEFAULT_PROD_DATE.data(),
           sizeof(DEFAULT_PROD_DATE));
    memcpy(&m_rfid_data.m_mcuUid, DEFAULT_MCU_UID.data(),
           sizeof(DEFAULT_MCU_UID));
    memcpy(&m_uid_data, DEFAULT_UID_DATA.data(), sizeof(DEFAULT_UID_DATA));
    memcpy(&m_rfid_data.m_machineSn.m_data, DEFAULT_MACHINE_SN.data(),
           sizeof(DEFAULT_MACHINE_SN));
    memcpy(&m_rfid_data.m_activationTime, DEFAULT_ACTIVATION_TIME.data(),
           sizeof(DEFAULT_ACTIVATION_TIME));
    memcpy(&m_rfid_data.m_drinkCounter, DEFAULT_DRINK_COUNTER.data(),
           sizeof(DEFAULT_DRINK_COUNTER));
    memcpy(&m_rfid_data.m_timeCounter, DEFAULT_TIME_COUNTER.data(),
           sizeof(DEFAULT_TIME_COUNTER));
  }
};
}  // namespace proto::lacte
