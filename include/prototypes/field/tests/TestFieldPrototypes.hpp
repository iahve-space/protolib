#pragma once

#include "prototypes/field/DataField.hpp"
#include "prototypes/field/FieldPrototype.hpp"

namespace proto {

namespace test {

template <FieldName NAME, typename T, uint8_t* BASE, FieldFlags FLAGS,
          size_t MAX_SIZE = 4096,
          size_t SIZE_ = (std::is_pointer_v<T> ? K_ANY_SIZE : sizeof(T)),
          std::conditional_t<std::is_pointer_v<T>, std::remove_pointer_t<T>, T>*
              CONST_VALUE = nullptr,
          MatcherType MATCHER = nullptr>
class TestFieldPrototype : public FieldPrototype<NAME, T, BASE, FLAGS, MAX_SIZE,
                                                 SIZE_, CONST_VALUE, MATCHER> {
 public:
  using delegate = MatchStatus (*)(void*);
  auto test_matcher() -> delegate { return this->m_matcher; }
  static const size_t SIZE{SIZE_};
  auto test_offset() -> size_t {
    ;
    return this->m_offset;
  }
  uint8_t* TestBase() {
    ;
    return this->BASE;
  }
  uint8_t* TestGetRaw() {
    ;
    return this->GetRaw();
  }
  size_t TemplateSize() { return this->m_size; };
  void TestSetOffset(size_t offset) { this->set_offset(offset); }
  void TestSet(const T& value) { this->set(value); }
  void TestApplyConst() { this->apply_const(); }
  [[nodiscard]] auto* TestGet() { return this->Value(); }
  [[nodiscard]] const T* TestGet() const { return this->Get(); }
};

template <typename PACKETS, uint8_t* BASE, FieldFlags FLAGS,
          size_t MAX_SIZE = 4096>
class TestDataFieldPrototype
    : public proto::DataFieldPrototype<PACKETS, BASE, FLAGS, MAX_SIZE> {
 public:
  using delegate = proto::MatchStatus (*)(void*);
  delegate TestMatcher() { return this->matcher_; }
  size_t TemplateSize() { return this->m_size; };
  size_t TemplateOffset() {
    ;
    return this->offset_;
  }
  void TestSetOffset(size_t offset) { this->SetOffset(offset); }
  void TestSet(uint8_t* value) { this->Set(value); }
  [[nodiscard]] uint8_t* TestGet() { return this->Get(); }
  [[nodiscard]] const uint8_t* TestGet() const { return this->Get(); }
};
}  // namespace test

template <FieldName NAME, typename T, uint8_t* BASE, FieldFlags FLAGS,
          size_t MAX_SIZE, size_t SIZE, auto* CONST_VALUE, MatcherType MATCHER>
struct fieldsTuple<test::TestFieldPrototype<NAME, T, BASE, FLAGS, MAX_SIZE,
                                            SIZE, CONST_VALUE, MATCHER>> {
  using Type = test::TestFieldPrototype<NAME, T, BASE, FLAGS, MAX_SIZE, SIZE,
                                        CONST_VALUE, MATCHER>;
};

template <typename T, uint8_t* BASE, FieldFlags FLAGS, size_t MAX_SIZE>
struct fieldsTuple<test::TestDataFieldPrototype<T, BASE, FLAGS, MAX_SIZE>> {
  using Type = test::TestDataFieldPrototype<T, BASE, FLAGS, MAX_SIZE>;
};
}  // namespace proto