#include "Echo.hpp"

namespace proto::interface{

    bool echoInterface::Write(CustomSpan<uint8_t> buffer, std::chrono::milliseconds timeout) {
        (void)timeout;
        if (!is_open_) {
            std::cerr << "Interface is not Open!" << std::endl;
            return false;
        }
        std::lock_guard<std::mutex> lock(write_mtx);
        size_t read = 0;
        for(int i = static_cast<int>(callbacks_.size() - 1); i >= 0; i--) {
            auto callback = callbacks_[i].lock(); // shared_ptr или nullptr
            if (!callback) {
                callbacks_.erase(callbacks_.begin() + i);
            } else {
                (*callback)(buffer.subspan(read), read);
            }
        }
        return true;
    }


    bool echoInterface::IsOpen() {
        return is_open_;
    }

    bool echoInterface::Open()  {
        is_open_ = true;
        return true;
    }

    bool echoInterface::Close() {
        is_open_ = false;
        return true;
    }

    int echoInterface::Read(uint8_t *buffer, size_t count) {
        (void)buffer;
        (void)count;
        return 0;
    }
}