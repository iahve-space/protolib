#ifndef CUSTOM_SPAN_HPP
#define CUSTOM_SPAN_HPP

#include <cstddef>
#include <cstdint>
#include <cassert>

template<typename T>
class CustomSpan {
public:
    CustomSpan() : ptr_(nullptr), size_(0) {}
    CustomSpan(const T* ptr, size_t size) : ptr_(const_cast<T*>(ptr)), size_(size) {}

    [[nodiscard]] T* data() const { return ptr_; }
    [[nodiscard]] size_t size() const { return size_; }
    [[nodiscard]] bool empty() const { return size_ == 0; }

    T& operator[](size_t index) const {
        assert(index < size_);
        return ptr_[index];
    }

    [[nodiscard]] T* begin() const { return ptr_; }
    [[nodiscard]] T* end() const { return ptr_ + size_; }

    [[nodiscard]] CustomSpan<T> subspan(size_t offset, size_t count = static_cast<size_t>(-1)) const {
        assert(offset <= size_);
        size_t new_size = count == static_cast<size_t>(-1) ? size_ - offset : count;
        assert(offset + new_size <= size_);
        return CustomSpan<T>(ptr_ + offset, new_size);
    }

private:
    T* ptr_;
    size_t size_;
};

#endif // CUSTOM_SPAN_HPP