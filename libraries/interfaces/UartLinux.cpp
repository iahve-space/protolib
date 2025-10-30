#include "UartLinux.hpp"

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <thread>

#include "SysFSHelper.hpp"

namespace proto::interface {

auto UartLinuxInterface::write(const CustomSpan<uint8_t> DATA,
                               const std::chrono::milliseconds TIMEOUT)
    -> bool {
  if (fd_ < 0) {
    return false;
  }
  std::lock_guard lock(write_mtx_);
  CustomSpan<uint8_t> ptr = DATA;
  size_t total = 0;
  const auto START = std::chrono::steady_clock::now();

  while (!ptr.empty()) {
    const ssize_t WRITTEN = ::write(fd_, ptr.data(), ptr.size());
    if (WRITTEN < 0) {
      if (errno == EINTR || errno == EAGAIN) {
        continue;
      }
      return false;  // write error
    }
    if (WRITTEN == 0) {
      break;
    }
    total += WRITTEN;
    ptr = DATA.subspan(total);

    if (auto now = std::chrono::steady_clock::now(); now - START >= TIMEOUT) {
      break;
    }
  }

  if (total > 0) {
    tcdrain(fd_);
  }
  return static_cast<int>(total) != 0;
}

auto UartLinuxInterface::uart_reader_thread() -> int {
  while (is_open_) {
    int count = read(receive_buffer_.data(), sizeof(receive_buffer_));
    if (count < 0) {
      if (!is_open_) {
        break;  // выходим, если нас закрывают
      }
      // логика авто-переподключения (по желанию)
      if (is_open_ && open_uart(m_name, baudrate_) < 0 &&
          not vid_pid_.empty()) {
        open_uart(vid_pid_, baudrate_);
      }
      continue;
    }
    if (count == 0) {
      continue;  // просто нет данных сейчас
    }

    size_t read{};
    for (int i = static_cast<int>(m_callbacks.size()) - 1; i >= 0; --i) {
      if (auto callback = m_callbacks[i].lock(); !callback) {
        m_callbacks.erase(m_callbacks.begin() + i);
      } else {
        (*callback)({receive_buffer_.data(), static_cast<size_t>(count)}, read);
      }
    }
  }

  return 0;
}

auto UartLinuxInterface::is_open() -> bool { return is_open_; }

auto UartLinuxInterface::open() -> bool {
  is_open_ = true;
  if (not receive_thread_.joinable()) {
    receive_thread_ = std::thread([this] { uart_reader_thread(); });
  }
  return true;
}

auto UartLinuxInterface::close() -> bool {
  if (is_open()) {
    is_open_ = false;  // 1) просигналили
    const int DESCR = fd_;
    fd_ = -1;
    if (DESCR >= 0) {
      ::close(DESCR);  // 2) закрыли (poll вернёт HUP/ERR или timeout)
    }
    if (receive_thread_.joinable()) {
      receive_thread_.join();  // 3) корректно дождались
    }
  }
  return true;
}

auto UartLinuxInterface::open_uart(const std::string& vid,
                                   const std::string& pid, const int BAUDRATE)
    -> int {
  auto list = fs_tools::SysFSHelper::find_by_id(vid, pid);
  if (list.empty()) {
    return -1;
  }
  // Если нужен именно TTY (например, для UART), выберем первый tty:
  const auto ITER =
      std::find_if(list.begin(), list.end(),
                   [](const auto& func) { return func.m_class_name == "tty"; });
  if (ITER != list.end()) {
    vid_pid_ = vid + ":" + pid;
    return open_uart(ITER->m_dev_path, BAUDRATE);
  }
  return -1;
}

auto UartLinuxInterface::open_uart(const std::string& device,
                                   const int BAUDRATE) -> int {
  if (const auto ITER = std::find(device.begin(), device.end(), ':');
      ITER != device.end()) {
    const auto POS = std::distance(device.begin(), ITER);  // индекс двоеточия
    const auto VID = device.substr(0, POS);
    const auto PID = device.substr(POS + 1);  // всё после ':'
    return open_uart(VID, PID, BAUDRATE);
  }
  const int DESCR = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
  if (DESCR < 0) {
    return -1;
  }

  termios tty{};
  if (tcgetattr(DESCR, &tty) != 0) {
    ::close(DESCR);
    return -1;
  }

  // скорость
  speed_t speed;
  switch (BAUDRATE) {
    case 9600:
      speed = B9600;
      break;
    case 19200:
      speed = B19200;
      break;
    case 38400:
      speed = B38400;
      break;
    case 57600:
      speed = B57600;
      break;
    case 115200:
      speed = B115200;
      break;
    default:
      std::cerr << "Unsupported baud rate\n";
      ::close(DESCR);
      return -1;
  }
  cfsetospeed(&tty, speed);
  cfsetispeed(&tty, speed);

  // 8N1, без управления потоком
  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  // 8 бит
  tty.c_cflag |=
      CLOCAL | CREAD;  // чтение, не становиться "владельцем" терминала
  tty.c_cflag &= ~(PARENB | PARODD);  // без четности
  tty.c_cflag &= ~CSTOPB;             // 1 стоп-бит
  tty.c_cflag &= ~CRTSCTS;            // без аппаратного flow control

  // --- добавлено: выключить все текстовые маппинги и постобработку (raw) ---
  tty.c_iflag &= ~(ICRNL | INLCR | IGNCR);  // не маппить CR/LF
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);   // без XON/XOFF
  tty.c_iflag &=
      ~(BRKINT | ISTRIP | INPCK);  // убрать лишние преобразования/проверки

  tty.c_oflag &= ~OPOST;  // без постобработки вывода

  // локальные флаги: без каноников/эхо/сигналов
  tty.c_lflag = 0;  // уже было, оставляем
  tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN);

  // тайминги чтения (как у тебя)
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 1;

  // на всякий случай очистим очереди и применим атрибуты «с флэшом»
  tcflush(DESCR, TCIOFLUSH);
  if (tcsetattr(DESCR, TCSAFLUSH, &tty) != 0) {
    std::cerr << "Error setting termios attrs: " << strerror(errno) << "\n";
    ::close(DESCR);
    return -1;
  }

  fd_ = DESCR;
  m_name = device;
  baudrate_ = BAUDRATE;
  open();
  return DESCR;
}

auto UartLinuxInterface::read(uint8_t* buffer, const size_t COUNT) -> int {
  std::lock_guard lock(read_mtx_);

  pollfd pfd{fd_, POLLIN | POLLERR | POLLHUP, 0};
  const int PTR = poll(&pfd, 1, 200);  // таймаут 200 мс
  if (PTR == 0) {
    return 0;  // нет данных сейчас
  }
  if (PTR < 0) {
    if (errno == EINTR) {
      return 0;
    }
    return -1;  // фатальная ошибка poll
  }
  if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
    return -1;  // устройство умерло/отключено
  }

  int flags = fcntl(fd_, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl(F_GETFL)");
    return -1;
  }

  if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl(F_SETFL)");
    return -1;
  }

  ssize_t READ = ::read(fd_, buffer, COUNT);

  if (READ < 0) {
    if (errno == EAGAIN || errno == EINTR) {
      return 0;  // временная ошибка — просто нет данных
    }
    perror("read");
    return -1;  // другая ошибка
  }

  if (READ == 0) {
    return -1;  // EOF — клиент закрыл соединение
  }

  return static_cast<int>(READ);
}
}  // namespace proto::interface