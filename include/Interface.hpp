#pragma once

#include <cstdint>
#include <cstddef>
#include <utility>
#include <functional>
#include <chrono>
#include <memory>

#include "CustomSpan.hpp"

namespace proto::interface{
    using CallbackType = std::function<void(CustomSpan<uint8_t> buffer, size_t &read)>;
    using Delegate = std::shared_ptr<CallbackType>;

    using namespace std::chrono_literals;

    class IInterface {
    public:
        explicit IInterface(const char *name):name_(name){};
        virtual bool Write(CustomSpan<uint8_t> buffer, std::chrono::milliseconds timeout = 1s) = 0;

        virtual bool IsOpen() = 0;
        virtual bool Open() = 0;
        virtual bool Close() = 0;
        [[nodiscard]] virtual Delegate AddReceiveCallback( const CallbackType& callback ){
            auto d = std::make_shared<CallbackType>(callback);
            callbacks_.push_back(d);
            return d;
        }

    protected:
        std::string name_;
        std::vector<std::weak_ptr<CallbackType>> callbacks_;
    private:
        virtual int Read(uint8_t *buffer, size_t count) = 0;


    };
}
