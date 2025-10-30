#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include "CustomSpan.hpp"
#include "Prototypes.hpp"

namespace {
// --- Local test payloads (replace legacy testType*, RxBufferTest, GetTestPack)
// ---
constexpr proto::test::dataType kTestData1{1, 2, 3, 4.f, 2.718281828459045};
constexpr proto::test::dataType2 kTestData2{};
constexpr proto::test::dataType3 kTestData3{};
}  // namespace

// This file exercises ONLY the RxContainer behavior: matchers, Fill/Reset,
// offsets progression, debug branches and basic flag-related checks. The
// concrete field sets, buffers and helpers (GetTestPack, RxBufferTest,
// testType, rxContainer, rxContainer2) are provided by Prototypes.hpp/test
// scaffolding.
//
// Notes:
//  * We keep tests deterministic and avoid relying on transport I/O.
//  * Where a flag (e.g., REVERSE) is not used in the provided field set, the
//    test is skipped with a clear message.

using namespace proto;        // FieldName, FieldFlags
using namespace proto::test;  // helpers: GetTestPack, GetTestPack2,
                              // RxBufferTest, testType, testType2

namespace {

/**
 * Common fixture: no per-test state is required but we use a fixture to make
 * the suite consistent and to allow future set-up/tear-down if needed.
 */
class RxContainerSuite : public testing::Test {
 protected:
  // Backing buffers for protocol instances
  static inline uint8_t rx_simple_[256]{};
  static inline uint8_t tx_simple_[256]{};
  static inline uint8_t rx_complex_[256]{};
  static inline uint8_t tx_complex_[256]{};

  using SimpleProto = SympleProtocol<rx_simple_, tx_simple_>;
  using ComplexProto = ComplexProtocol<rx_complex_, tx_complex_>;

  SimpleProto simple_;    // provides simple_.rx() / simple_.tx()
  ComplexProto complex_;  // provides complex_.rx() / complex_.tx()

  bool received_ = false;  // track if callback invoked

  void SetUp() override {
    received_ = false;
    //        last_variant_ = std::monostate{};
  }
};

// Simple RAII capturer for stdout/stderr to validate protocol debug output.
class StdCapture {
 public:
  StdCapture() {
    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
  }
  ~StdCapture() {
    // if (armed_) Get();
  }
  std::string Get() {
    armed_ = false;
    out_ = testing::internal::GetCapturedStdout();
    err_ = testing::internal::GetCapturedStderr();
    return out_ + err_;
  }
  std::string out_;
  std::string err_;

 private:
  bool armed_{true};
};

}  // namespace

// -----------------------------------------------------------------------------
// Matcher wiring
// -----------------------------------------------------------------------------

/**
 * Verify that the field matchers are bound to the expected RxContainer methods.
 */
TEST_F(RxContainerSuite, Matchers_AreBound) {
  auto& rx = simple_.m_rx;
  EXPECT_EQ(rx.get<FieldName::LEN_FIELD>().test_matcher(),
            std::remove_reference_t<decltype(rx)>::set_data_len);
  EXPECT_EQ(rx.get<FieldName::ALEN_FIELD>().test_matcher(),
            std::remove_reference_t<decltype(rx)>::check_alen);
  EXPECT_EQ(rx.get<FieldName::CRC_FIELD>().test_matcher(),
            std::remove_reference_t<decltype(rx)>::check_crc);
}

// -----------------------------------------------------------------------------
// Reset and offsets
// -----------------------------------------------------------------------------

/**
 * Reset returns all offsets to zero; feeding a real packet re-computes offsets
 * to the expected positions (ID at 0, LEN at 3, ALEN at 4, DATA at 5, ...).
 */
TEST_F(RxContainerSuite, Reset_OffsetsGoToZero_ThenRebuiltOnFill) {
  auto& rx = simple_.m_rx;
  auto& tx = simple_.m_tx;

  using RxT = std::remove_reference_t<decltype(rx)>;
  auto recv = rx.add_receive_callback([&](RxT&) { received_ = true; });

  received_ = false;
  rx.reset();

  // Build a valid packet using the official API and place it into TX buffer
  const size_t PACKET_SIZE =
      tx.send_packet(proto::make_field_info<FieldName::DATA_FIELD>(
          &kTestData1, sizeof(kTestData1)));

  // Feed all but the last byte – callback must NOT fire yet
  size_t read = 0;
  rx.fill(CustomSpan(tx_simple_, PACKET_SIZE - 1), read);
  EXPECT_FALSE(received_) << "Callback fired before full packet arrived";

  // Feed the last byte – callback MUST fire
  rx.fill(CustomSpan(tx_simple_ + (PACKET_SIZE - 1), 1),
          read);
  EXPECT_TRUE(received_) << "Callback didn't fire on full packet";

  // After Reset() every field offset must be zero again
  rx.reset();
  rx.for_each_type([&](auto& fld) { EXPECT_EQ(fld.get_offset(), 0u); });
}

// -----------------------------------------------------------------------------
// Incremental Fill and offsets progression on the same buffer
// -----------------------------------------------------------------------------

/**
 * Feed each field sequentially from the same underlying buffer and ensure
 * offsets progress exactly by the number of bytes consumed.
 */
TEST_F(RxContainerSuite, Fill_OffsetsProgressOnSameBuffer) {
  auto& rx = simple_.m_rx;
  auto& tx = simple_.m_tx;

  using RxT = std::remove_reference_t<decltype(rx)>;
  size_t callback_count = 0;

  auto recv = rx.add_receive_callback([&](RxT& c) {
    ++callback_count;
    EXPECT_EQ(std::memcmp(c.get<FieldName::DATA_FIELD>().get_ptr(), &kTestData1,
                          sizeof(kTestData1)),
              0);
  });

  rx.reset();
  const size_t packetSize =
      tx.send_packet(proto::make_field_info<FieldName::DATA_FIELD>(
          &kTestData1, sizeof(kTestData1)));

  // Feed in five parts corresponding to field sizes (ID, LEN, ALEN, DATA, CRC)
  const size_t s_id = rx.get<FieldName::ID_FIELD>().get_size();
  const size_t s_len = rx.get<FieldName::LEN_FIELD>().get_size();
  const size_t s_alen = rx.get<FieldName::ALEN_FIELD>().get_size();
  const size_t s_data = rx.get<FieldName::DATA_FIELD>().get_size();
  const size_t s_crc = rx.get<FieldName::CRC_FIELD>().get_size();

  ASSERT_EQ(s_id + s_len + s_alen + s_data + s_crc, packetSize);

  size_t off = 0, read = 0;
  auto feed = [&](size_t n) {
    rx.fill(CustomSpan(tx_simple_ + off, n), read);
    off += read;
  };

  feed(s_id);
  feed(s_len);
  feed(s_alen);
  feed(s_data);
  feed(s_crc);

  EXPECT_EQ(off, packetSize);
  EXPECT_EQ(callback_count, 1u);
}

// -----------------------------------------------------------------------------
// Debug branch exercising LEN, ALEN, CRC mismatches
// -----------------------------------------------------------------------------

/**
 * With debug enabled, craft LEN/ALEN/CRC mismatches across multiple Fill calls
 * and ensure the code paths are executed without throwing and handler remains
 * callable for a correct packet.
 */
TEST_F(RxContainerSuite, Debug_MismatchPathsAreCovered) {
  auto& rx = simple_.m_rx;
  auto& tx = simple_.m_tx;

  using RxT = std::remove_reference_t<decltype(rx)>;
  auto recv = rx.add_receive_callback([&](RxT&) { received_ = true; });

  rx.set_debug(true);

  // Helper to (re)build a valid TX frame and then feed a possibly-corrupted
  // copy
  auto feed_corrupted = [&](auto corrupter) {
    received_ = false;
    rx.reset();
    const size_t packetSize =
        tx.send_packet(proto::make_field_info<FieldName::DATA_FIELD>(
            &kTestData1, sizeof(kTestData1)));
    std::vector tmp(tx_simple_,
                    tx_simple_ + packetSize);
    corrupter(tmp);
    size_t read = 0;
    rx.fill(CustomSpan(tmp.data(), tmp.size()), read);
    EXPECT_FALSE(received_) << "Corrupted packet unexpectedly accepted";
  };

  // 1) Break LEN (index 3 for simple layout: ID is 3 bytes)
  {
    StdCapture cap;
    feed_corrupted([](std::vector<uint8_t>& v) { v[3] ^= 0x01; });
    const std::string log = cap.Get();
    EXPECT_NE(log.find("Mismatch in length field"), std::string::npos);
    EXPECT_NE(log.find("BROKEN PACKET START"), std::string::npos);
  }

  // 2) Break ALEN (index 4)
  {
    StdCapture cap;
    feed_corrupted([](std::vector<uint8_t>& v) { v[4] ^= 0x01; });
    const std::string log = cap.Get();
    EXPECT_NE(log.find("Mismatch in ALEN field"), std::string::npos);
    EXPECT_NE(log.find("BROKEN PACKET START"), std::string::npos);
  }

  // 3) Break CRC (last byte)
  {
    StdCapture cap;
    feed_corrupted([](std::vector<uint8_t>& v) { v.back() ^= 0xFF; });
    const std::string log = cap.Get();
    EXPECT_NE(log.find("Mismatch in CRC field"), std::string::npos);
    EXPECT_NE(log.find("BROKEN PACKET START"), std::string::npos);
  }

  // Finally: good packet must be accepted
  received_ = false;
  rx.reset();
  const size_t ok =
      tx.send_packet(proto::make_field_info<FieldName::DATA_FIELD>(
          &kTestData1, sizeof(kTestData1)));
  size_t read = 0;
  rx.fill(CustomSpan(tx_simple_, ok), read);
  EXPECT_TRUE(received_) << "Valid packet not accepted";
}

// -----------------------------------------------------------------------------
// Complex layout: debug paths
// -----------------------------------------------------------------------------

/**
 * Repeat basic debug-path coverage on the more complex layout (with TYPE field
 * and DATA size derived from it).
 */
TEST_F(RxContainerSuite, Debug_MismatchPathsCovered_ComplexLayout) {
  auto& rx2 = complex_.m_rx;
  auto& tx2 = complex_.m_tx;

  using Rx2T = std::remove_reference_t<decltype(rx2)>;
  auto last_variant_ = rx2.get<FieldName::DATA_FIELD>().get_copy();
  auto recv2 = rx2.add_receive_callback([&](Rx2T& c) {
    received_ = true;
    // Save the variant from DATA field inside the callback because
    // the container state is reset after the handler returns.
    last_variant_ = c.get<FieldName::DATA_FIELD>().get_copy();
  });
  rx2.set_debug(true);

  auto send_ok = [&](uint8_t type_val) {
    if (type_val == 1) {
      return tx2.send_packet(
          proto::make_field_info<FieldName::TYPE_FIELD>(&type_val),
          proto::make_field_info<FieldName::DATA_FIELD>(&kTestData1,
                                                        sizeof(kTestData1)));
    }
    if (type_val == 2) {
      return tx2.send_packet(
          proto::make_field_info<FieldName::TYPE_FIELD>(&type_val),
          proto::make_field_info<FieldName::DATA_FIELD>(&kTestData2,
                                                        sizeof(kTestData2)));
    }
    return tx2.send_packet(
        proto::make_field_info<FieldName::TYPE_FIELD>(&type_val),
        proto::make_field_info<FieldName::DATA_FIELD>(&kTestData3,
                                                      sizeof(kTestData3)));
  };

  // 1) Break ALEN (index 4: ID(3), LEN(1), ALEN(1))
  {
    received_ = false;
    rx2.reset();
    const size_t n = send_ok(1);
    std::vector tmp(tx_complex_,
                    tx_complex_ + n);
    tmp[4] ^= 0x01;
    StdCapture cap;
    size_t read = 0;
    rx2.fill(CustomSpan(tmp.data(), tmp.size()), read);
    const std::string log = cap.Get();
    EXPECT_FALSE(received_);
    EXPECT_NE(log.find("Mismatch in ALEN field"), std::string::npos);
    EXPECT_NE(log.find("BROKEN PACKET START"), std::string::npos);
  }

  // 2) Break TYPE (index 5)
  {
    received_ = false;
    rx2.reset();
    const size_t n = send_ok(1);
    std::vector tmp(tx_complex_,
                    tx_complex_ + n);
    tmp[5] = 0;  // invalid type
    StdCapture cap;
    size_t read = 0;
    rx2.fill(CustomSpan(tmp.data(), tmp.size()), read);
    const std::string log = cap.Get();
    EXPECT_FALSE(received_);
    EXPECT_NE(log.find("Incorrect type received"), std::string::npos);
    EXPECT_NE(log.find("BROKEN PACKET START"), std::string::npos);
  }

  // 3) Break CRC (last byte)
  {
    received_ = false;
    rx2.reset();
    const size_t n = send_ok(1);
    std::vector tmp(tx_complex_,
                    tx_complex_ + n);
    tmp.back() ^= 0xFF;
    StdCapture cap;
    size_t read = 0;
    rx2.fill(CustomSpan(tmp.data(), tmp.size()), read);
    const std::string log = cap.Get();
    EXPECT_FALSE(received_);
    EXPECT_NE(log.find("Mismatch in CRC field"), std::string::npos);
    EXPECT_NE(log.find("BROKEN PACKET START"), std::string::npos);
  }

  // 4) Valid packet is accepted and DATA type is resolved by TYPE
  {
    received_ = false;
    rx2.reset();
    const size_t n = send_ok(2);
    size_t read = 0;
    rx2.fill(CustomSpan(tx_complex_, n), read);
    EXPECT_TRUE(received_);
    // Verify variant selection captured in the receive handler
    EXPECT_TRUE(std::holds_alternative<proto::test::dataType2>(last_variant_));
  }
}

// -----------------------------------------------------------------------------
// Complex layout: CRC REVERSE behavior
// -----------------------------------------------------------------------------

/**
 * В complex-лейауте CRC помечен флагом REVERSE (байтовый порядок свопнут).
 * Тест проверяет, что перестановка двух CRC-байт ломает пакет (контейнер
 * его отвергает и печатает "Mismatch in CRC field"), а оригинальный пакет
 * проходит. Так мы подтверждаем, что порядок байт CRC значим и TX формирует
 * кадр в ожидаемом «реверснутом» порядке.
 */
TEST_F(RxContainerSuite, Complex_CRCReverse_ByteSwapBreaksPacket) {
  auto& rx2 = complex_.m_rx;
  auto& tx2 = complex_.m_tx;

  using Rx2T = std::remove_reference_t<decltype(rx2)>;
  bool got = false;
  auto recv2 = rx2.add_receive_callback([&](Rx2T&) { got = true; });
  rx2.set_debug(true);

  // 1) Хороший пакет должен пройти
  {
    got = false;
    rx2.reset();
    constexpr uint8_t type_val = 2;  // тип с реальными данными
    const size_t n = tx2.send_packet(
        proto::make_field_info<FieldName::TYPE_FIELD>(&type_val),
        proto::make_field_info<FieldName::DATA_FIELD>(&kTestData2,
                                                      sizeof(kTestData2)));
    size_t read = 0;
    rx2.fill(CustomSpan(tx_complex_, n), read);
    EXPECT_TRUE(got) << "Valid complex packet was not accepted";
  }

  // 2) Меняем местами последние два байта (CRC) — пакет должен быть отклонён
  {
    got = false;
    rx2.reset();
    constexpr uint8_t type_val = 2;
    const size_t n = tx2.send_packet(
        proto::make_field_info<FieldName::TYPE_FIELD>(&type_val),
        proto::make_field_info<FieldName::DATA_FIELD>(&kTestData2,
                                                      sizeof(kTestData2)));
    std::vector tmp(tx_complex_, tx_complex_ + n);
    if (n >= 2) std::swap(tmp[n - 2], tmp[n - 1]);

    StdCapture cap;
    size_t read = 0;
    rx2.fill(CustomSpan(tmp.data(), tmp.size()), read);
    const std::string log = cap.Get();

    EXPECT_FALSE(got)
        << "Packet with swapped CRC bytes was incorrectly accepted";
    EXPECT_NE(log.find("Mismatch in CRC field"), std::string::npos)
        << "Expected CRC mismatch message in debug log";
    EXPECT_NE(log.find("BROKEN PACKET START"), std::string::npos)
        << "Expected broken-packet dump in debug log";
  }
}