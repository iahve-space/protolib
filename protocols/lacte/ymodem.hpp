#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <mutex>
#include <condition_variable>

#include "Interface.hpp"

class YmodemPrerelease{
public:

    YmodemPrerelease(proto::interface::IInterface& interface): interface_(interface){
        receive_callback_ = interface_.AddReceiveCallback([this](CustomSpan<uint8_t> buffer, size_t &read){
            memcpy(receive_buffer, buffer.data(), buffer.size());
            received_count = buffer.size();
            read+=buffer.size();
            received = true;
            cv.notify_all();
        });
    };
    /**
 * Отправка файла по протоколу YMODEM.
 */
    int send(const std::string& filepath);

private:
    proto::interface::Delegate receive_callback_;
    proto::interface::IInterface& interface_;
    uint8_t receive_buffer[255]{};
    size_t received_count{};
    bool received{false};
    std::mutex mtx{};
    std::condition_variable cv{};

    static constexpr uint8_t SOH       = 0x01;
    static constexpr uint8_t STX       = 0x02;
    static constexpr uint8_t EOT       = 0x04;
    static constexpr uint8_t ACK       = 0x06;
    static constexpr uint8_t NAK       = 0x15;
    static constexpr uint8_t CAN       = 0x18;
    static constexpr uint8_t C         = 0x43;
    static constexpr uint8_t ABORT1	= 0x41;  /* 'A' == 0x41, abort by user */
    static constexpr uint8_t ABORT2	= 0x61;  /* 'a' == 0x61, abort by user */
    static constexpr std::size_t BLOCK_SIZE = 1024;

    bool Wait(char c, size_t);

    /**
 * Вычисление CRC-16 (Modbus/CCITT) для блока данных.
 */
    static uint16_t crc16(const uint8_t* data, std::size_t length);

/**
 * Отправка одного блока данных (STX).
 */
    void send_block(uint8_t block_number, const uint8_t* data, std::size_t length);

/**
 * Отправка заголовочного блока с именем и размером файла.
 */
    void send_header_block(const std::string& filename, std::size_t filesize);

};

