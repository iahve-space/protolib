#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Prototypes.hpp"
#include "Echo.hpp"
#include <cstring>
#include <variant>

namespace {
using namespace proto;
using namespace proto::test;

// RX/TX backing buffers for simple and complex layouts
static uint8_t rx_simple_[256]{};
static uint8_t tx_simple_[256]{};
static uint8_t rx_complex_[256]{};
static uint8_t tx_complex_[256]{};


// Test payloads
static proto::test::dataType  testType{1,2,3,4.f,2.718281828459045};
static proto::test::dataType2 testType2{}; // default-inited

// Helper: detect std::variant at compile time (C++17)
template<class T> struct is_std_variant : std::false_type {};
template<class... Ts> struct is_std_variant<std::variant<Ts...>> : std::true_type {};
template<class T>
inline constexpr bool is_std_variant_v = is_std_variant<std::decay_t<T>>::value;

using namespace proto::test;
using namespace std::chrono_literals;

TEST(PingPongContainerTest, SanyCaseType1){
    using namespace proto;
    using namespace proto::test;

    SympleProtocol<rx_simple_, tx_simple_> protocol;
    proto::interface::echoInterface interface{};
    interface.Open();
    protocol.SetInterfaces(interface, interface);

    // Helper to assert equality for either pointer-return or variant-return
    auto assertEqual = [](auto&& result, const dataType& expected) {
        using R = std::decay_t<decltype(result)>;
        if constexpr (is_std_variant_v<R>) {
            bool matched = false;
            std::visit([&](auto&& alt){
                using A = std::decay_t<decltype(alt)>;
                if constexpr (std::is_same_v<A, dataType>) {
                    EXPECT_EQ(expected, alt);
                    matched = true;
                } else if constexpr (std::is_same_v<A, std::monostate>) {
                    // no payload
                } else {
                    // Different alternative — not expected for this test
                }
            }, result);
            EXPECT_TRUE(matched) << "Variant did not contain dataType";
        } else {
//            ASSERT_NE(result, nullptr);
            EXPECT_EQ(expected, result);
        }
    };

    // 1) First request/response
    auto r1 = protocol.Request(MakeFieldInfo<FieldName::DATA_FIELD>(&testType));
    auto r1_data_field = meta::get_named<proto::FieldName::DATA_FIELD>(r1);
    assertEqual(r1_data_field, testType);

    // 2) Change payload and request again
    testType.d = 0.234542;
    auto r2 = protocol.Request(MakeFieldInfo<FieldName::DATA_FIELD>(&testType));
    auto r2_data_field = meta::get_named<proto::FieldName::DATA_FIELD>(r2);
    assertEqual(r2_data_field, testType);
}
TEST(PingPongContainerTest, NoiseType1){
    using namespace proto;
    using namespace proto::test;

    SympleProtocol<rx_simple_, tx_simple_> protocol;
    proto::interface::echoInterface interface{};
    interface.Open();
    protocol.SetInterfaces(interface, interface);

    // helper from above: assertEqual(result, expected)
    auto assertEqual = [](auto&& result, const auto& expected) {
        using R = std::decay_t<decltype(result)>;
        if constexpr (is_std_variant_v<R>) {
            bool matched = false;
            std::visit([&](auto&& alt){
                using A = std::decay_t<decltype(alt)>;
                if constexpr (std::is_same_v<A, std::decay_t<decltype(expected)>>) {
                    EXPECT_EQ(expected, alt);
                    matched = true;
                }
            }, result);
            EXPECT_TRUE(matched) << "Variant did not contain expected type";
        } else {
//            ASSERT_NE(result, nullptr);
            EXPECT_EQ(expected, result);
        }
    };

    auto task = [&](auto &noiseData){
        interface.Write(CustomSpan<uint8_t>{noiseData, sizeof(noiseData)}, 1s);
        auto r = protocol.Request(MakeFieldInfo<FieldName::DATA_FIELD>(&testType));
        auto r_data_field = meta::get_named<proto::FieldName::DATA_FIELD>(r);
        assertEqual(r_data_field, testType);
    };

    // 1) random noise before a valid request
    uint8_t noise[] = {4,2,6,7,34,67,44,255,255,255, 0xAA, 0xBB};
    task(noise);

    // 2) wrong length header noise, then valid request
    testType.f = 322;
    uint8_t wrong_len_noise[] = {0xAA, 0xBB, 0xCC, 200, 200};
    task(wrong_len_noise);
}


TEST(PingPongContainerTest, SanyCaseType2){
    using namespace proto;
    using namespace proto::test;

    ComplexProtocol<rx_complex_, tx_complex_> protocol;
    proto::interface::echoInterface interface{};
    interface.Open();
    protocol.SetInterfaces(interface, interface);

    auto assertEqual = [](auto&& result, const auto& expected) {
        using R = std::decay_t<decltype(result)>;
        if constexpr (is_std_variant_v<R>) {
            bool matched = false;
            std::visit([&](auto&& alt){
                using A = std::decay_t<decltype(alt)>;
                if constexpr (std::is_same_v<A, std::decay_t<decltype(expected)>>) {
                    EXPECT_EQ(expected, alt);
                    matched = true;
                }
            }, result);
            EXPECT_TRUE(matched) << "Variant did not contain expected type";
        } else {
//            ASSERT_NE(result, nullptr);
            EXPECT_EQ(expected, result);
        }
    };

    // First request with default-initialized testType2
    auto r1 = protocol.Request(MakeFieldInfo<FieldName::DATA_FIELD>(&testType2));
        auto r1_data_field = meta::get_named<proto::FieldName::DATA_FIELD>(r1);
    assertEqual(r1_data_field, testType2);

    // Second request after modifying a field (if any)
    testType.d = 0.234542; // keep some deterministic change in other payload type
    auto r2 = protocol.Request(MakeFieldInfo<FieldName::DATA_FIELD>(&testType2));
        auto r2_data_field = meta::get_named<proto::FieldName::DATA_FIELD>(r2);
    assertEqual(r2_data_field, testType2);
}

TEST(PingPongContainerTest, NoiseType2){
    using namespace proto;
    using namespace proto::test;

    ComplexProtocol<rx_complex_, tx_complex_> protocol;
    proto::interface::echoInterface interface{};
    interface.Open();
    protocol.SetInterfaces(interface, interface);

    auto assertEqual = [](auto&& result, const auto& expected) {
        using R = std::decay_t<decltype(result)>;
        if constexpr (is_std_variant_v<R>) {
            bool matched = false;
            std::visit([&](auto&& alt){
                using A = std::decay_t<decltype(alt)>;
                if constexpr (std::is_same_v<A, std::decay_t<decltype(expected)>>) {
                    EXPECT_EQ(expected, alt);
                    matched = true;
                }
            }, result);
            EXPECT_TRUE(matched) << "Variant did not contain expected type";
        } else {
//            ASSERT_NE(result, nullptr);
            EXPECT_EQ(expected, result);
        }
    };

    auto task = [&](auto &noiseData){
        interface.Write(CustomSpan<uint8_t>{noiseData, sizeof(noiseData)}, 1s);
        auto r = protocol.Request(MakeFieldInfo<FieldName::DATA_FIELD>(&testType2));
        auto r_data_field = meta::get_named<proto::FieldName::DATA_FIELD>(r);

        assertEqual(r_data_field, testType2);
    };

    // Random garbage before frame
    uint8_t noise[] = {4,2,6,7,34,67,44,255,255,255, 0xAA, 0xBB};
    task(noise);

    // Wrong LEN then valid request
    testType.f = 322;
    uint8_t wrong_len_noise[] = {0xAA, 0xBB, 0xCC, 200, 200};
    task(wrong_len_noise);
}

/**
 * @test Verify that RX emits debug output on broken frames (CRC mismatch).
 * We enable RX debug, then intercept TX->RX path via echoInterface and corrupt
 * the last byte (part of CRC) in each transmitted chunk. We expect RX to print
 * the standard diagnostics that contain "Mismatch in CRC field" and the
 * BROKEN PACKET dump markers.
 */
    TEST(PingPongContainerTest, DebugOutput_Type2_CrcMismatch) {
        using namespace ::testing;
        using namespace proto;
        using namespace proto::test;

        // Local Complex RX/TX containers (old-style) just for this CRC debug test
        using ComplexFieldsRxT  = ComplexFields<rx_complex_>;
        using ComplexFieldsTxT  = ComplexFields<tx_complex_>;
        using proto_fields2Rx   = typename ComplexFieldsRxT::proto_fields;
        using proto_fields2Tx   = typename ComplexFieldsTxT::proto_fields;

        static RxContainer<proto_fields2Rx> rxContainer2{};
        static TxContainer<proto_fields2Tx> txContainer2{};

        auto transmitHandler = [](CustomSpan<uint8_t> span, size_t &read){
            // Aggregate all parts of one frame and send as a single buffer.
            // Complex layout emits exactly 6 parts: ID, LEN, ALEN, TYPE, DATA, CRC.
            static std::array<uint8_t, 512> frame_buf{};
            static size_t acc = 0;  // total bytes accumulated for the current frame
            static int parts = 0;   // number of chunks seen for the current frame

            // Append current chunk
            const size_t n = span.size();
            ASSERT_LT(acc + n, frame_buf.size());
            std::memcpy(frame_buf.data() + acc, span.begin(), n);
            acc   += n;
            parts += 1;

            // When the last part (CRC) arrives, corrupt the last byte and deliver the whole frame
            if (parts == 6) {
                if (acc >= 1) {
                    frame_buf[acc - 1] ^= 0x5A; // Flip the last byte to break CRC
                }

                // Feed the entire corrupted frame to RX in a single Fill call
              CustomSpan<uint8_t> full{frame_buf.data(), acc};
                rxContainer2.Fill(full, read);

                // Reset aggregation state for the next frame
                acc = 0;
                parts = 0;
            }
            // Note: for parts 1..5 we intentionally do NOT forward anything to RX yet.
        };

        bool got_callback = false;
        auto receiveHandler = [&](decltype(rxContainer2)& fields){
            got_callback = true; // should remain false for deliberately corrupted frame
            (void)fields;
        };

        proto::interface::echoInterface interface{};
        interface.Open();

        // Route TX writes through our transmit handler; we will forward to RX ourselves
        auto d  = interface.AddReceiveCallback(transmitHandler);

        // Wire TX/RX to the same interface
        txContainer2.SetInterface(interface);
        auto cd = rxContainer2.AddReceiveCallback(receiveHandler);

        // Turn on RX debug to make it print diagnostics to stdout
        rxContainer2.SetDebug(true);

        // Capture stdout during a single send of a valid payload that we corrupt in-flight
        ::testing::internal::CaptureStdout();
        (void)txContainer2.SendPacket(
                proto::MakeFieldInfo< proto::FieldName::DATA_FIELD>(&testType2)
        );
        std::string out = ::testing::internal::GetCapturedStdout();

        // The RX should not report a successful callback for a corrupted frame
        EXPECT_FALSE(got_callback) << "RX callback must not fire on CRC-mismatched frame";

        // And it should log the expected debug markers
        EXPECT_THAT(out, HasSubstr("Mismatch in CRC field"));
        EXPECT_THAT(out, HasSubstr("BROKEN PACKET START"));
        EXPECT_THAT(out, HasSubstr("BROKEN PACKET STOP"));

        // Clean up: disable debug so other tests are quiet (callbacks will be destroyed with interface)
        rxContainer2.SetDebug(false);
        (void)d; (void)cd;
    }
}
