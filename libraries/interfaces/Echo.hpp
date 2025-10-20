#pragma once

#include <vector>
#include <iostream>
#include <mutex>

#include "CustomSpan.hpp"
#include "Interface.hpp"

namespace proto::interface {
    class echoInterface : public IInterface {
    public:
        explicit echoInterface() : IInterface("echo interface"), is_open_(false) {}

        bool Write(CustomSpan<uint8_t> buffer, std::chrono::milliseconds timeout) override;

        bool IsOpen() override;

        bool Open() override;

        bool Close() override;

//        bool AddReceiveCallback(Delegate) override ;

    private:
        std::mutex write_mtx;
        bool is_open_;
        std::vector<uint8_t> receive_buffer_;
//        std::vector<std::weak_ptr<CallbackType>> callbacks_;
        int Read(uint8_t * buffer, size_t count) override;
    };
}