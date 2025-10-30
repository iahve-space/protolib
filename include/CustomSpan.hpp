#ifndef CUSTOM_SPAN_HPP
#define CUSTOM_SPAN_HPP

#include <cassert>
#include <cstddef>

template <typename T>
class CustomSpan {
 public:
  CustomSpan() : ptr_(nullptr), size_(0) {}
  CustomSpan(const T* ptr, size_t size)
      : ptr_(const_cast<T*>(ptr)), size_(size) {}

  [[nodiscard]] auto data() const -> T* { return ptr_; }
  [[nodiscard]] auto size() const -> size_t { return size_; }
  [[nodiscard]] auto empty() const -> bool { return size_ == 0; }

  auto operator[](size_t index) const -> T& {
    assert(index < size_);
    return ptr_[index];
  }

  [[nodiscard]] auto begin() const -> T* { return ptr_; }
  [[nodiscard]] auto end() const -> T* { return ptr_ + size_; }

  [[nodiscard]] auto subspan(size_t offset,
                             size_t count = static_cast<size_t>(-1)) const
      -> CustomSpan<T> {
    assert(offset <= size_);
    size_t new_size = count == static_cast<size_t>(-1) ? size_ - offset : count;
    assert(offset + new_size <= size_);
    return CustomSpan<T>(ptr_ + offset, new_size);
  }

 private:
  T* ptr_;
  size_t size_;
};

#endif  // CUSTOM_SPAN_HPP