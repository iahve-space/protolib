#ifndef fdsa
#include <string>
#endif

#include <chrono>
#include <climits>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>

#include "Ymodem.hpp"
using namespace std::chrono_literals;

YmodemPrerelease::YmodemPrerelease(proto::interface::IInterface& interface)
    : interface_(interface) {
  receive_callback_ = interface_.add_receive_callback(
      [this](const CustomSpan<uint8_t> BUFFER, size_t& read) {
        memcpy(receive_buffer_.begin(), BUFFER.data(), BUFFER.size());
        received_count_ = BUFFER.size();
        read += BUFFER.size();
        received_ = true;
        cv_.notify_all();
      });
}

auto YmodemPrerelease::crc16(const uint8_t* data, const size_t LENGTH)
    -> uint16_t {
  uint16_t crc = 0x0000;
  for (size_t i = 0; i < LENGTH; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << CHAR_BIT;
    for (int j = 0; j < CHAR_BIT; ++j) {
      crc = (crc & 0x8000) != 0 ? crc << 1 ^ 0x1021 : crc << 1;
    }
  }
  return crc;
}

void YmodemPrerelease::send_block(const uint8_t BLOCK_NUMBER,
                                  const uint8_t* data, const size_t LENGTH) {
  std::array<uint8_t, 3 + BLOCK_SIZE + 2> buf{};
  buf[0] = STX;
  buf[1] = BLOCK_NUMBER;
  buf[2] = ~BLOCK_NUMBER;

  std::memcpy(&buf[3], data, LENGTH);
  if (LENGTH < BLOCK_SIZE) {
    std::memset(&buf[3 + LENGTH], 0x1A, BLOCK_SIZE - LENGTH);  // fill with SUB
  }

  const uint16_t CRC = crc16(&buf[3], BLOCK_SIZE);
  buf[3 + BLOCK_SIZE] = CRC >> CHAR_BIT & UINT8_MAX;
  buf[4 + BLOCK_SIZE] = CRC & UINT8_MAX;
  received_ = false;
  interface_.write({buf.begin(), sizeof(buf)});
}

void YmodemPrerelease::send_header_block(const std::string& filename,
                                         const size_t FILESIZE) {
  std::array<char, HEADER_SIZE> header = {};
  std::snprintf(header.data(), HEADER_SIZE, "%s%c%zu", filename.c_str(), 0,
                FILESIZE);

  std::array<uint8_t, 3 + HEADER_SIZE + 2> buf{};
  buf[0] = SOH;
  buf[1] = 0x00;
  buf[2] = UINT8_MAX;

  std::memcpy(&buf[3], header.begin(), 128);

  const uint16_t CRC = crc16(&buf[3], 128);
  buf[131] = CRC >> CHAR_BIT & UINT8_MAX;
  buf[132] = CRC & UINT8_MAX;
  received_ = false;
  interface_.write({buf.begin(), sizeof(buf)});
}

auto YmodemPrerelease::wait(const uint8_t VAL, const size_t TRYS = 400)
    -> bool {
  using namespace std::chrono_literals;
  constexpr auto TIMEOUT = 50ms;

  uint32_t cnt = 0;

  while (cnt++ < TRYS) {
    std::unique_lock lock(mtx_);

    // Ждём либо notify, либо таймаут

    if (const bool OKAY =
            cv_.wait_for(lock, TIMEOUT, [&] { return received_; });
        !OKAY) {
      // таймаут — ничего не получили
      continue;
    }
    received_ = false;
    if (receive_buffer_[0] == VAL) {
      return true;
    }
  }

  return false;  // превышено количество попыток
}

auto YmodemPrerelease::send(const std::string& filename) -> int {
  std::ifstream file(filename, std::ios::binary);
  if (!file) {
    std::cerr << "Can't open file\n";
    return -1;
  }

  file.seekg(0, std::ios::end);
  const size_t FILESIZE = file.tellg();
  file.seekg(0);

  std::cout << "Waiting for 'C'...\n";
  if (!wait(ONLINE_COMMAND)) {
    std::cout << "Bootloader is offline!!" << '\n';
    return -1;
  }

  std::cout << "Bootloader is online!!" << '\n';
  std::cout << "Sending header...\n";
  send_header_block(filename, FILESIZE);
  if (!wait(ACK)) {
    std::cout << "Bootloader doesn't answer for header" << '\n';
    return -1;
  }
  std::cout << "ACK ok ...\n";
  std::cout << "Sending data...\n";
  uint32_t block_num = 1;
  uint8_t last_percentage = 0;
  while (true) {
    std::array<uint8_t, BLOCK_SIZE> buffer{};
    file.read(reinterpret_cast<char*>(buffer.begin()), BLOCK_SIZE);
    const size_t COUNT = file.gcount();
    if (COUNT == 0) {
      break;
    }
    if (COUNT < BLOCK_SIZE) {
      std::cout << "last \n";
    }
    send_block(block_num, buffer.begin(), COUNT);
    if (!wait(ACK, 10)) {
      std::cout << "Can't send file, send_block error" << '\n';
      interface_.write({&ABORT1, 1});
      interface_.write({&ABORT2, 1});
      return -1;
    }
    if (const auto PERCENTAGE = block_num * BLOCK_SIZE * 100 / FILESIZE;
        PERCENTAGE / 5 > last_percentage / 5) {
      std::cout << "Uploaded: " << std::dec
                << block_num * BLOCK_SIZE * 100 / FILESIZE << "%" << '\n';
      last_percentage = PERCENTAGE;
    }

    block_num++;
  }
  std::cout << "Finishing EOT...\n";
  interface_.write({&EOT, 1});
  if (!wait(ACK)) {
    std::cout << "Bootloader doesn't answer for EOT, might be some problems "
                 "with bootloader"
              << '\n';
    return 0;
  }
  std::cout << "ACK ok ...\n";
  send_header_block("", 0);  // завершающий пустой блок
  if (!wait(ACK)) {
    std::cout << "Bootloader doesn't answer for last header block" << '\n';
    return 0;
  }
  std::cout << "File is sent" << '\n';
  std::cout << "Done\n";
  return 0;
}