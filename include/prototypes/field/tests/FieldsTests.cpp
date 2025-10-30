/**
 * @file FieldsTests.cpp
 * @brief Unit tests for protocol field prototypes.
 *
 * These tests validate the low-level logic of field handling, including:
 *  - Offsets and pointer mapping to the shared buffer;
 *  - Size reporting for simple and complex field sets;
 *  - Constant value presence and application to the buffer;
 *  - Setting values and verifying memory consistency;
 *  - Iterator support for traversing field bytes;
 *  - DataFieldPrototype: variant access and packet ID handling.
 *
 * NOTE: Container-related tests (RxContainer, TxContainer, etc.) are not
 * included here. This suite focuses strictly on the field-level mechanics.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <variant>

#include "TestFields.hpp"

using testing::Eq;

namespace testhelpers {

/**
 * @brief Fill a buffer with a constant byte value.
 */
inline void fill(uint8_t* buf, size_t n, uint8_t v) { std::memset(buf, v, n); }

/**
 * @brief Compare two memory regions for equality.
 * @return Success if identical, failure otherwise with diagnostic output.
 */
inline testing::AssertionResult MemEqual(const void* a, const void* b,
                                           size_t n) {
  if (std::memcmp(a, b, n) == 0) return testing::AssertionSuccess();
  return testing::AssertionFailure()
         << "Memory differs on first " << n << " bytes";
}

/**
 * @brief Verify that a field points to the expected position in the shared
 * buffer.
 */
template <typename Field>
testing::AssertionResult FieldPointsTo(const Field& f, const void* base,
                                         size_t offset) {
  auto* expect = static_cast<const uint8_t*>(base) + offset;
  if (f.get_ptr() == reinterpret_cast<const decltype(f.get_ptr())>(expect))
    return testing::AssertionSuccess();
  return testing::AssertionFailure()
         << "Field points to " << static_cast<const void*>(f.get_ptr())
         << " but expected " << static_cast<const void*>(expect);
}

/**
 * @brief Verify that the iterator of a field yields exactly the raw bytes
 * stored in GetData().
 */
template <typename Field>
void ExpectIterEqualsRaw(const Field& fld) {
  size_t i = 0;
  for (auto b : fld) {
    ASSERT_EQ(b, reinterpret_cast<const uint8_t*>(fld.get_ptr())[i++]);
  }
}

}  // namespace testhelpers

/**
 * @class FieldsTestSuite
 * @brief Common fixture for testing field prototypes.
 *
 * Provides:
 *  - Shared buffer;
 *  - Aliases for simple and complex protocol field layouts;
 *  - Pre-computed offsets for each field in @ref SetUp.
 */
class FieldsTestSuite : public testing::Test {
 protected:
  /// Shared buffer used by all fields.
  static inline uint8_t buffer_[100] = {'a', 'b', 'c', 'd', 'e', 'f',
                                        'g', 'h', 'i', 'j', 'k'};

  using SimpleFields = proto::test::SympleFields<buffer_>;
  using ComplexFieldsT = proto::test::ComplexFields<buffer_>;
  using SimpleTuple = SimpleFields::proto_fields;
  using ComplexTuple = ComplexFieldsT::proto_fields;

  /// Multiple instances of simple field tuples for testing.
  std::array<SimpleTuple, 3> simple_{};

  void SetUp() override {
    // Assign offsets to each field based on actual instance sizes.
    for (auto& f : simple_) {
      auto& id = std::get<0>(f);
      auto& len = std::get<1>(f);
      auto& alen = std::get<2>(f);
      auto& data = std::get<3>(f);
      auto& crc = std::get<4>(f);

      id.TestSetOffset(0);
      len.TestSetOffset(id.get_size());
      alen.TestSetOffset(id.get_size() + len.get_size());
      data.TestSetOffset(id.get_size() + len.get_size() + alen.get_size());
      crc.TestSetOffset(id.get_size() + len.get_size() + alen.get_size() +
                        data.get_size());
    }
  }

  static uint8_t* buf() { return buffer_; }
};

// -------------------- TEST CASES --------------------

/**
 * @test Verify that fields map to the correct offsets in the shared buffer.
 */
TEST_F(FieldsTestSuite, OffsetsAndPointers) {
  for (size_t i = 0; i < simple_.size(); ++i) {
    SCOPED_TRACE(::testing::Message() << "tuple idx=" << i);
    auto& f = simple_[i];

    auto& id = std::get<0>(f);
    auto& len = std::get<1>(f);
    auto& alen = std::get<2>(f);
    auto& data = std::get<3>(f);
    auto& crc = std::get<4>(f);

    size_t off_id = 0;
    size_t off_len = off_id + id.get_size();
    size_t off_alen = off_len + len.get_size();
    size_t off_data = off_alen + alen.get_size();
    size_t off_crc = off_data + data.get_size();

    EXPECT_TRUE(testhelpers::FieldPointsTo(id, buf(), off_id));
    EXPECT_TRUE(testhelpers::FieldPointsTo(len, buf(), off_len));
    EXPECT_TRUE(testhelpers::FieldPointsTo(alen, buf(), off_alen));
    EXPECT_TRUE(testhelpers::FieldPointsTo(data, buf(), off_data));
    EXPECT_TRUE(testhelpers::FieldPointsTo(crc, buf(), off_crc));
  }
}

/**
 * @test Verify reported sizes for both simple and complex field layouts.
 *
 * - Simple fields must match their natural types.
 * - Complex layout must report K_ANY_SIZE for the dynamic data field.
 */
TEST_F(FieldsTestSuite, SizesSimpleAndComplex) {
  // Simple layout
  {
    SimpleTuple field{};
    auto& id = std::get<0>(field);
    auto& len = std::get<1>(field);
    auto& alen = std::get<2>(field);
    auto& data = std::get<3>(field);
    auto& crc = std::get<4>(field);

    EXPECT_EQ(len.get_size(), sizeof(uint8_t));
    EXPECT_EQ(alen.get_size(), sizeof(uint8_t));
    EXPECT_EQ(data.get_size(), sizeof(proto::test::dataType));
    EXPECT_EQ(crc.get_size(), sizeof(uint16_t));
    EXPECT_EQ(id.get_size(), 3u);  // fixed in SympleFields
  }
  // Complex layout
  {
    ComplexTuple f2{};
    EXPECT_EQ(std::get<0>(f2).get_size(), 3u);
    EXPECT_EQ(std::get<1>(f2).get_size(), sizeof(uint8_t));
    EXPECT_EQ(std::get<2>(f2).get_size(), sizeof(uint8_t));
    EXPECT_EQ(std::get<3>(f2).get_size(), sizeof(uint8_t));
    EXPECT_EQ(std::get<4>(f2).get_size(), proto::K_ANY_SIZE);  // dynamic field
    EXPECT_EQ(std::get<5>(f2).get_size(), sizeof(uint16_t));
  }
}

/**
 * @test Verify that constant values are present and can be applied to the
 * buffer.
 */
TEST_F(FieldsTestSuite, ConstValuePresentAndApply) {
  for (auto& f : simple_) {
    auto& id = std::get<0>(f);
    ASSERT_NE(id.CONST_VALUE, nullptr);

    testhelpers::fill(buf(), sizeof(buffer_), 0x00);
    id.TestApplyConst();
    EXPECT_TRUE(
        testhelpers::MemEqual(id.get_ptr(), id.CONST_VALUE, id.get_size()));
  }
}

/**
 * @test Verify Set() correctly updates buffer contents, and
 *       iterators yield the same raw bytes as GetData().
 */
TEST_F(FieldsTestSuite, SetValuesAndIterators) {
  for (auto& f : simple_) {
    testhelpers::fill(buf(), sizeof(buffer_), 0xFF);
    auto& id = std::get<0>(f);
    auto& len = std::get<1>(f);
    auto& alen = std::get<2>(f);
    auto& data = std::get<3>(f);
    auto& crc = std::get<4>(f);

    // Const field (ID)
    id.TestApplyConst();
    EXPECT_TRUE(testhelpers::MemEqual(buf(), id.CONST_VALUE, id.get_size()));

    // LEN field
    len.TestSet(uint8_t{125});
    EXPECT_EQ(*len.get_ptr(), 125);

    // ALEN field (~125 with explicit cast to avoid narrowing warning)
    alen.TestSet(static_cast<uint8_t>(~uint8_t{125}));
    EXPECT_EQ(*alen.get_ptr(), static_cast<uint8_t>(~uint8_t{125}));

    // DATA field
    proto::test::dataType dv{};
    data.TestSet(dv);
    EXPECT_TRUE(testhelpers::MemEqual(buf() + data.test_offset(),
                                      data.get_ptr(), data.get_size()));

    // CRC field
    crc.TestSet(uint16_t{0x1234});
    EXPECT_EQ(*crc.get_ptr(), 0x1234);

    // Iterators
    testhelpers::ExpectIterEqualsRaw(id);
    testhelpers::ExpectIterEqualsRaw(len);
    testhelpers::ExpectIterEqualsRaw(alen);
    testhelpers::ExpectIterEqualsRaw(data);
    testhelpers::ExpectIterEqualsRaw(crc);
  }
}

/**
 * @test Verify DataFieldPrototype behavior:
 *       - Variant returns std::monostate before SetId;
 *       - Correct variant type after SetId;
 *       - Correct size calculation;
 *       - Unknown ID is rejected.
 */
TEST_F(FieldsTestSuite, DataField_VariantAndLookups) {
  using Map =
      std::tuple<proto::PacketInfo<1, proto::test::dataType>,  // fixed-size
                 proto::PacketInfo<2, uint8_t*>,             // pointer payload
                 proto::PacketInfo<3, proto::EmptyDataType>  // empty => size 0
                 >;

  using DF =
      proto::DataFieldPrototype<Map, buffer_, proto::FieldFlags::NOTHING>;
  DF df;

  // Not set → monostate
  {
    auto v0 = df.get_copy();
    EXPECT_TRUE(std::holds_alternative<std::monostate>(v0));
  }

  // Fixed-size type
  ASSERT_TRUE(df.set_id(1));
  {
    auto v1 = df.get_copy();
    EXPECT_TRUE(std::holds_alternative<proto::test::dataType>(v1));
    EXPECT_EQ(df.get_size(), sizeof(proto::test::dataType));
  }

  // Pointer type → just verify the variant holds uint8_t*
  ASSERT_TRUE(df.set_id(2));
  {
    auto v2 = df.get_copy();
    EXPECT_TRUE(std::holds_alternative<std::vector<uint8_t>>(v2));
    // Size for pointer payload is managed externally (container); no size check
    // here.
  }

  // Empty data → monostate and size 0
  ASSERT_TRUE(df.set_id(3));
  {
    auto v3 = df.get_copy();
    EXPECT_TRUE(std::holds_alternative<proto::EmptyDataType>(v3));
    EXPECT_EQ(df.get_size(), 0u);
  }

  // Unknown ID → SetId returns false and keeps previous state intact
  EXPECT_FALSE(df.set_id(9999));
}

/**
 * @test Verify DataFieldPrototype with strongly-typed enum IDs using variant
 * API.
 *
 * Expectations:
 *  - Before SetId() → std::monostate.
 *  - Pk::A (fixed-size) → variant holds proto::test::dataType.
 *  - Pk::B (pointer payload) → variant holds uint8_t*.
 *  - Pk::C (EmptyDataType) → variant is std::monostate and size is 0.
 */
TEST_F(FieldsTestSuite, DataField_EnumIdsWork) {
  enum class Pk : uint8_t { A = 1, B = 2, C = 3 };

  using MapE = std::tuple<
      proto::PacketInfo<static_cast<size_t>(Pk::A), proto::test::dataType>,
      proto::PacketInfo<static_cast<size_t>(Pk::B), uint8_t*>,
      proto::PacketInfo<static_cast<size_t>(Pk::C), proto::EmptyDataType>>;

  using DFE =
      proto::DataFieldPrototype<MapE, buffer_, proto::FieldFlags::NOTHING>;
  DFE df;

  // Not set → monostate
  {
    auto v0 = df.get_copy();
    EXPECT_TRUE(std::holds_alternative<std::monostate>(v0));
  }

  // A → fixed-size
  ASSERT_TRUE(df.set_id(static_cast<int>(Pk::A)));
  {
    auto v = df.get_copy();
    ASSERT_TRUE(std::holds_alternative<proto::test::dataType>(v));
    EXPECT_EQ(df.get_size(), sizeof(proto::test::dataType));
  }

  // B → pointer payload
  ASSERT_TRUE(df.set_id(static_cast<int>(Pk::B)));
  {
    auto v = df.get_copy();
    EXPECT_TRUE(std::holds_alternative<std::vector<uint8_t>>(v));
  }

  // C → empty
  ASSERT_TRUE(df.set_id(static_cast<int>(Pk::C)));
  {
    auto v = df.get_copy();
    EXPECT_TRUE(std::holds_alternative<proto::EmptyDataType>(v));
    EXPECT_EQ(df.get_size(), 0u);
  }
}

TEST_F(FieldsTestSuite, SizesComplex_TemplateSizes) {
  ComplexTuple f2{};
  EXPECT_EQ(std::get<0>(f2).TemplateSize(), 3u);
  EXPECT_EQ(std::get<1>(f2).TemplateSize(), sizeof(uint8_t));
  EXPECT_EQ(std::get<2>(f2).TemplateSize(), sizeof(uint8_t));
  EXPECT_EQ(std::get<3>(f2).TemplateSize(), sizeof(uint8_t));
  EXPECT_EQ(std::get<4>(f2).TemplateSize(), proto::K_ANY_SIZE);
  EXPECT_EQ(std::get<5>(f2).TemplateSize(), sizeof(uint16_t));
}