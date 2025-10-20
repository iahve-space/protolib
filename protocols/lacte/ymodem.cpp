#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <cstring>
#include <iostream>
#include <fstream>

#include "ymodem.hpp"



uint16_t YmodemPrerelease::crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int j = 0; j < 8; ++j)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}

void YmodemPrerelease::send_block(uint8_t block_num, const uint8_t* data, size_t len) {

    uint8_t buf[3 + BLOCK_SIZE + 2] = {};
    buf[0] = STX;
    buf[1] = block_num;
    buf[2] = ~block_num;

    std::memcpy(&buf[3], data, len);
    if (len < BLOCK_SIZE)
        std::memset(&buf[3 + len], 0x1A, BLOCK_SIZE - len); // fill with SUB

    uint16_t crc = crc16(&buf[3], BLOCK_SIZE);
    buf[3 + BLOCK_SIZE] = (crc >> 8) & 0xFF;
    buf[4 + BLOCK_SIZE] = crc & 0xFF;
    received = false;
    interface_.Write({buf, sizeof(buf)});
}

void YmodemPrerelease::send_header_block(const std::string& filename, size_t filesize) {
    uint8_t header[128] = {};
    std::snprintf(reinterpret_cast<char*>(header), 128, "%s%c%zu", filename.c_str(), 0, filesize);

    uint8_t buf[3 + 128 + 2] = {};
    buf[0] = SOH;
    buf[1] = 0x00;
    buf[2] = 0xFF;

    std::memcpy(&buf[3], header, 128);

    uint16_t crc = crc16(&buf[3], 128);
    buf[131] = (crc >> 8) & 0xFF;
    buf[132] = crc & 0xFF;
    received = false;
    interface_.Write({buf, sizeof(buf)});
}

bool YmodemPrerelease::Wait(char c, size_t trys = 300) {
    using namespace std::chrono_literals;
    const auto timeout = 10ms;

    uint32_t cnt = 0;

    while (cnt++ < trys) {
        std::unique_lock<std::mutex> lock(mtx);

        // Ждём либо notify, либо таймаут
        bool ok = cv.wait_for(lock, timeout, [&]{ return received; });

        if (!ok) {
            // таймаут — ничего не получили
            continue;
        }
        received = false;
        if (receive_buffer[0] == c) {
            return true;
        }
    }

    return false; // превышено количество попыток
}

int YmodemPrerelease::send(const std::string& filename) {

    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Can't open file\n";
        return -1;
    }

    file.seekg(0, std::ios::end);
    size_t filesize = file.tellg();
    file.seekg(0);

    std::cout << "Waiting for 'C'...\n";
    if(!Wait(C)){
        std::cout << "Bootloader is offline!!" << std::endl;
        return -1;
    }

    std::cout << "Bootloader is online!!" << std::endl;
    std::cout << "Sending header...\n";
    send_header_block(filename, filesize);
    if(!Wait(ACK)){
        std::cout << "Bootloader doesn't answer for header" << std::endl;
        return -1;
    }
    std::cout << "ACK ok ...\n";
    std::cout << "Sending data...\n";
    uint8_t buffer[BLOCK_SIZE];
    uint32_t block_num = 1;
    uint8_t last_percentage = 0;
    while (1) {
        file.read(reinterpret_cast<char*>(buffer), BLOCK_SIZE);
        size_t count = file.gcount();
        if (count == 0) {
            break;
        }
        if (count < 1024) {
            std::cout << "last \n";
        }
        send_block( block_num++, buffer, count);
        if(!Wait(ACK)){
            std::cout << "Can't send file, send_block error" << std::endl;
            interface_.Write( {&ABORT1, 1});
            interface_.Write({&ABORT2, 1});
        }
        auto percenage = (block_num* BLOCK_SIZE * 100)/filesize;
        if(percenage/5 > (last_percentage/5  )){
            std::cout << "Uploaded: " << (block_num* BLOCK_SIZE * 100)/filesize << "%" << std::endl;
            last_percentage = percenage;
        }

        if (!file && !file.eof()) {
            std::cerr << "Error reading file\n";
            return -1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::cout << "Finishing EOT...\n";
    interface_.Write({&EOT, 1});
    if(!Wait(ACK)){
        std::cout << "Bootloader doesn't answer for EOT" << std::endl;
        return -1;
    }
    std::cout << "ACK ok ...\n";
    send_header_block("", 0); // завершающий пустой блок
    if(!Wait(ACK)){
        std::cout << "Bootloader doesn't answer for last header block" << std::endl;
        return -1;
    }
    std::cout << "File is sent" << std::endl;
    std::cout << "Done\n";
    return 0 ;
}