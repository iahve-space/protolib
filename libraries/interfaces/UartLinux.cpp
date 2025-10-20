#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <thread>

#include "Echo.hpp"
#include "UartLinux.hpp"

namespace proto::interface{

    bool UartLinuxInterface::Write(CustomSpan<uint8_t> data, std::chrono::milliseconds timeout) {
        if (fd_ < 0) return false;
        std::lock_guard<std::mutex> lock(write_mtx);
      CustomSpan<uint8_t> ptr = data;
        size_t total = 0;
        auto start = std::chrono::steady_clock::now();


        while (!ptr.empty()) {
            ssize_t written = write(fd_, ptr.data(), ptr.size());
            if (written < 0) {
                if (errno == EINTR || errno == EAGAIN) continue;
                return -1; // write error
            }
            if (written == 0) break;
            total+=written;
            ptr = data.subspan(total);

            auto now = std::chrono::steady_clock::now();
            if (now - start >= timeout) {
                break;
            }
        }

        if (total > 0) tcdrain(fd_);
        return static_cast<int>(total);
    }

    int UartLinuxInterface::UartReaderThread() {

        while (true) {
            ssize_t n = Read(receive_buffer_, sizeof(receive_buffer_));// read(fd_, receive_buffer_, sizeof(receive_buffer_));
            if(n<0){
                break;
            }
            size_t read{};
            for(int i = static_cast<int>( callbacks_.size()-1); i >=0; i--) {
                auto callback = callbacks_[i].lock(); // shared_ptr или nullptr
                if (!callback) {
                    callbacks_.erase(callbacks_.begin() + i);
                } else {
                    (*callback)({receive_buffer_, (size_t)n}, read);
                }
            }
        }
        return 0;
    }

    bool UartLinuxInterface::IsOpen() {
        return is_open_;
    }

    bool UartLinuxInterface::Open()  {
        is_open_ = true;
        return true;
    }

    bool UartLinuxInterface::Close() {
        is_open_ = false;
        return true;
    }

//    bool UartLinuxInterface::AddReceiveCallback(Delegate callback) {
//        callbacks_.push_back(callback);
//        return true;
//    }

    int UartLinuxInterface::OpenUart(const char* device, int baudrate) {
        int fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
        if (fd < 0) {
            std::cerr << "Error opening " << device << ": " << strerror(errno) << "\n";
            return -1;
        }

        struct termios tty {};
        if (tcgetattr(fd, &tty) != 0) {
            std::cerr << "Error getting termios attrs: " << strerror(errno) << "\n";
            close(fd);
            return -1;
        }

        // скорость
        speed_t speed;
        switch (baudrate) {
            case 9600:   speed = B9600;   break;
            case 19200:  speed = B19200;  break;
            case 38400:  speed = B38400;  break;
            case 57600:  speed = B57600;  break;
            case 115200: speed = B115200; break;
            default:
                std::cerr << "Unsupported baud rate\n";
                close(fd);
                return -1;
        }
        cfsetospeed(&tty, speed);
        cfsetispeed(&tty, speed);

        // 8N1, без управления потоком
        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  // 8 бит
        tty.c_cflag |= (CLOCAL | CREAD);             // чтение, не становиться "владельцем" терминала
        tty.c_cflag &= ~(PARENB | PARODD);           // без четности
        tty.c_cflag &= ~CSTOPB;                      // 1 стоп-бит
        tty.c_cflag &= ~CRTSCTS;                     // без аппаратного flow control

        // --- добавлено: выключить все текстовые маппинги и постобработку (raw) ---
        tty.c_iflag &= ~(ICRNL | INLCR | IGNCR);     // не маппить CR/LF
        tty.c_iflag &= ~(IXON | IXOFF | IXANY);      // без XON/XOFF
        tty.c_iflag &= ~(BRKINT | ISTRIP | INPCK);   // убрать лишние преобразования/проверки

        tty.c_oflag &= ~OPOST;                       // без постобработки вывода

        // локальные флаги: без каноники/эхо/сигналов
        tty.c_lflag = 0;                             // уже было, оставляем
        tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN);

        // тайминги чтения (как у тебя)
        tty.c_cc[VMIN]  = 1;
        tty.c_cc[VTIME] = 1;

        // на всякий случай очистим очереди и применим атрибуты «с флэшом»
        tcflush(fd, TCIOFLUSH);
        if (tcsetattr(fd, TCSAFLUSH, &tty) != 0) {
            std::cerr << "Error setting termios attrs: " << strerror(errno) << "\n";
            close(fd);
            return -1;
        }

        fd_ = fd;
        receive_thread_ = std::thread([this]() {
            UartReaderThread();
        });

        return fd;
    }

    int UartLinuxInterface::Read(uint8_t * buffer, size_t count) {
        std::lock_guard<std::mutex> lock(read_mtx);
        ssize_t n = read(fd_, buffer, count);

        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) return 0;
            return -1;  // фатальная ошибка
        }

        if (n == 0) {
            // Обычно не случается с UART, но возможно при закрытии
            return 0;
        }
        return (int)n;
    }
}