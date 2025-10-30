#pragma once
#include <Interface.hpp>
#include <condition_variable>
#include <cstddef>
#include <mutex>

class YmodemPrerelease {
 public:
  explicit YmodemPrerelease(proto::interface::IInterface& interface);
  /**
   * Отправка файла по протоколу YMODEM.
   */
  auto send(const std::string&) -> int;

 private:
  proto::interface::Delegate receive_callback_;
  proto::interface::IInterface& interface_;
  std::array<uint8_t, UINT8_MAX> receive_buffer_{};
  size_t received_count_{};
  bool received_{false};
  std::mutex mtx_;
  std::condition_variable cv_;

  static constexpr uint8_t SOH = 0x01;
  static constexpr uint8_t STX = 0x02;
  static constexpr uint8_t EOT = 0x04;
  static constexpr uint8_t ACK = 0x06;
  static constexpr uint8_t NAK = 0x15;
  static constexpr uint8_t CAN = 0x18;
  static constexpr uint8_t ONLINE_COMMAND = 0x43;
  static constexpr uint8_t ABORT1 = 0x41; /* 'A' == 0x41, abort by user */
  static constexpr uint8_t ABORT2 = 0x61; /* 'a' == 0x61, abort by user */
  static constexpr std::size_t BLOCK_SIZE = 1024;
  static constexpr std::size_t HEADER_SIZE = 128;

  auto wait(uint8_t VAL, size_t) -> bool;

  /**
   * Вычисление CRC-16 (Modbus/CCITT) для блока данных.
   */
  static auto crc16(const uint8_t* data, std::size_t LENGTH) -> uint16_t;

  /**
   * Отправка одного блока данных (STX).
   */
  void send_block(uint8_t BLOCK_NUMBER, const uint8_t* data,
                  std::size_t LENGTH);

  /**
   * Отправка заголовочного блока с именем и размером файла.
   */
  void send_header_block(const std::string& filename, std::size_t filesize);
};
