#pragma once

#include <cstdint>
#include <tuple>

#include "NamedTuple.hpp"
#include "ProtocolNoSysEndpoint.hpp"

namespace proto::exoAtlant {

inline uint32_t CRC32_get(const uint8_t* buf, const uint size) {
  uint32_t crc = ~0;
  for (uint32_t i = 0; i < size; ++i) {
    crc ^= buf[i] << (i % 24);
    // crc =~crc;
  }
  return ~crc;
}

class custom_crc final : ICrc {
 public:
  explicit custom_crc() : ICrc("custom crc") {}
  void reset() {};
  uint32_t calc(CustomSpan<uint8_t> buffer) {
    return CRC32_get(buffer.data(), buffer.size());
  }

  uint32_t append(uint32_t last_crc, const CustomSpan<uint8_t> data) {
    return last_crc ^ calc(data);
  }
};
/*
 * Использую свою библиотеку бинарных протоколов для описания протокольной
 * части.
 */
enum type_t : uint8_t {
  PACKET1 = 0x00,
  PACKET2 = 0x01,
  PACKET3 = 0x02,
};

struct Packet1 {
  uint8_t number;
  uint8_t data[10];
  bool operator==(const Packet1& other) const {
    return number == other.number && memcmp(data, other.data, 10) == 0;
  }
  bool operator!=(const Packet1& other) const { return !(*this == other); }
};
struct Packet2 {
  uint8_t number;
  uint8_t data[14];
  bool operator==(const Packet2& other) const {
    return number == other.number && memcmp(data, other.data, 14) == 0;
  }
  bool operator!=(const Packet2& other) const { return !(*this == other); }
};
struct Packet3 {
  uint8_t number;
  uint8_t data[15];
  bool operator==(const Packet3& other) const {
    return number == other.number && memcmp(data, other.data, 15) == 0;
  }
  bool operator!=(const Packet3& other) const { return !(*this == other); }
};

Packet1 trank1;
Packet2 trank2;
Packet3 trank3;

template <uint8_t* BASE>
struct exoAtlantPacket {
  constexpr static uint8_t prefix[4] = {'P', 'R', 'T', 'S'};

  using Packets = std::tuple<proto::PacketInfo<PACKET1, Packet1>,
                             proto::PacketInfo<PACKET2, Packet2>,
                             proto::PacketInfo<PACKET3, Packet3> >;
  using IdFieldType =
      proto::FieldPrototype<proto::FieldName::ID_FIELD, const uint8_t*, BASE,
                            proto::FieldFlags::NOTHING, 4, 4, prefix>;
  using LenFieldType =
      proto::FieldPrototype<proto::FieldName::LEN_FIELD, uint32_t, BASE,
                            proto::FieldFlags::NOTHING>;
  using VersFieldType =
      proto::FieldPrototype<proto::FieldName::VERSION_FIELD, uint8_t, BASE,
                            proto::FieldFlags::IS_IN_CRC |
                                proto::FieldFlags::IS_IN_LEN>;
  using TypeFieldType =
      proto::FieldPrototype<proto::FieldName::TYPE_FIELD, type_t, BASE,
                            proto::FieldFlags::IS_IN_CRC |
                                proto::FieldFlags::IS_IN_LEN>;
  using DestFieldType =
      proto::FieldPrototype<proto::FieldName::DEST_FIELD, uint8_t, BASE,
                            proto::FieldFlags::IS_IN_CRC |
                                proto::FieldFlags::IS_IN_LEN>;
  using SourceFieldType =
      proto::FieldPrototype<proto::FieldName::SOURCE_FIELD, uint8_t, BASE,
                            proto::FieldFlags::IS_IN_CRC |
                                proto::FieldFlags::IS_IN_LEN>;
  using AnswerFieldType =
      proto::FieldPrototype<proto::FieldName::ANS_TYPE_FIELD, uint8_t, BASE,
                            proto::FieldFlags::IS_IN_CRC |
                                proto::FieldFlags::IS_IN_LEN>;
  using SpFieldType =
      proto::FieldPrototype<proto::FieldName::STATUS_FIELD, uint8_t, BASE,
                            proto::FieldFlags::IS_IN_CRC |
                                proto::FieldFlags::IS_IN_LEN>;
  using DataFieldType =
      proto::DataFieldPrototype<Packets, BASE,
                                proto::FieldFlags::IS_IN_CRC |
                                    proto::FieldFlags::IS_IN_LEN>;
  using CrcFieldType =
      proto::FieldPrototype<proto::FieldName::CRC_FIELD, uint32_t, BASE,
                            proto::FieldFlags::IS_IN_LEN>;

  using packet_fields =
      std::tuple<IdFieldType, LenFieldType, VersFieldType, TypeFieldType,
                 DestFieldType, SourceFieldType, AnswerFieldType, SpFieldType,
                 DataFieldType, CrcFieldType>;
};

/*
 Это часть относится к адаптации задания к используемой библиотеке.
 1) Есть необычный момент в задании, что структура пакета содержит поля
 протокола. В моей реализации, протокол возвращает tuple с полями, что гораздо
 удобнее, и можно точно отделить протокол от пакета. Но адаптируем строго под
 API из задания. 2) лучше использовать cpp стиль в определении структур. Так что
 немного поменяю определение с typedef struct на struct pkt_desc_t 3) не указано
 что именно входит в len так что установил на свое усмотрение.
 */

using ver_t = uint8_t;
using sp_t = uint8_t;
using addr_t = uint8_t;
using exo_res_t = uint8_t;
struct pkt_desc_t {
  ver_t ver;        ///< Версия протокола
  type_t type;      ///< Тип пакета
  sp_t sp;          ///< Тип субпакета
  addr_t addr_dst;  ///< Адрес получателя
  addr_t addr_src;  ///< Адрес отправителя
  exo_res_t
      res;  ///< Код результата (используется в ответе. В запросах равен 0)
  uint8_t* data;  ///< Данные
  uint maxsize;   ///< Размер буфера данных
  uint size;      ///< Актуальный размер данных в буфере
  // дополнил для тестов
  bool operator==(const pkt_desc_t& rhs) const {
    if (this->ver != rhs.ver) return false;
    if (this->type != rhs.type) return false;
    if (this->sp != rhs.sp) return false;
    if (this->addr_dst != rhs.addr_dst) return false;
    if (this->addr_src != rhs.addr_src) return false;
    if (this->res != rhs.res) return false;
    if (this->size != rhs.size) return false;
    if (type == PACKET1) {
      if (*(Packet1*)(data) != *(Packet1*)(rhs.data)) return false;
    }
    if (type == PACKET2) {
      if (*(Packet2*)(data) != *(Packet2*)(rhs.data)) return false;
    }
    if (type == PACKET3) {
      if (*(Packet3*)(data) != *(Packet3*)(rhs.data)) return false;
    }

    return true;
  }
};

template <uint8_t* RX_BASE, uint8_t* TX_BASE>
class ExoAtlantProtocol_
    : public ProtocolNoSysEndpoint<
          typename exoAtlantPacket<RX_BASE>::packet_fields,
          typename exoAtlantPacket<TX_BASE>::packet_fields, custom_crc> {
 public:
  ExoAtlantProtocol_(bool debug) { this->set_debug(debug); }
  /**
   * @brief Сериализация исходящего блока данных
   *
   * @param[in]  packet   Пакет
   * @param[out] buf      Буфер
   * @param[in]  bufsize  Размер буфера
   *
   * @return Фактическое количество данных в буфере
   */
  uint serialize(pkt_desc_t& packet, uint8_t* buf, uint bufSize) {
    auto packet_size = this->send(
        make_field_info<FieldName::VERSION_FIELD>(&packet.ver),
        make_field_info<FieldName::TYPE_FIELD>(&packet.type),
        make_field_info<FieldName::STATUS_FIELD>(&packet.sp),
        make_field_info<FieldName::DEST_FIELD>(&packet.addr_dst),
        make_field_info<FieldName::SOURCE_FIELD>(&packet.addr_src),
        make_field_info<FieldName::ANS_TYPE_FIELD>(&packet.res),
        make_field_info<FieldName::DATA_FIELD>(packet.data, packet.size));
    if (bufSize >= packet_size) {
      memcpy(buf, TX_BASE, packet_size);
      return packet_size;
    }
    return 0;
  }

  /**
   * @brief Разбор входящего блока данных
   *
   * @param[in] buf       Блок данных
   * @param[in] size      Размер блока данных
   *
   * @return Пакет
   */
  pkt_desc_t parse(const uint8_t* buf, uint size) {
    pkt_desc_t result{};
    CustomSpan<uint8_t> span(buf, size);
    size_t read = 0;
    this->m_rx.fill(span, read);
    if (not this->m_rx_queue.empty()) {
      auto snap = this->m_rx_queue.front();
      result.ver = meta::get_named<FieldName::VERSION_FIELD>(snap);
      result.type = meta::get_named<FieldName::TYPE_FIELD>(snap);
      result.sp = meta::get_named<FieldName::STATUS_FIELD>(snap);
      result.addr_dst = meta::get_named<FieldName::DEST_FIELD>(snap);
      result.addr_src = meta::get_named<FieldName::SOURCE_FIELD>(snap);
      result.res = meta::get_named<FieldName::ANS_TYPE_FIELD>(snap);
      auto data_field = meta::get_named<FieldName::DATA_FIELD>(snap);
      if (std::holds_alternative<Packet1>(data_field)) {
        auto tmp = std::get<Packet1>(data_field);
        result.data = (uint8_t*)&trank1;
        memcpy(result.data, &tmp, sizeof(Packet1));
        result.size = sizeof(Packet1);
      }
      if (std::holds_alternative<Packet2>(data_field)) {
        auto tmp = std::get<Packet2>(data_field);
        result.data = (uint8_t*)&trank2;
        memcpy(result.data, &tmp, sizeof(Packet2));
        result.size = sizeof(Packet2);
      }
      if (std::holds_alternative<Packet3>(data_field)) {
        auto tmp = std::get<Packet3>(data_field);
        result.data = (uint8_t*)&trank3;
        memcpy(result.data, &tmp, sizeof(Packet3));
        result.size = sizeof(Packet3);
      }

      this->m_rx_queue.pop_front();
    }
    return result;
  }
  /// @brief Сброс состояния парсера
  void reset() { this->m_rx.reset(); }

 private:
};

}  // namespace proto::exoAtlant
