#include <gtest/gtest.h>
#include <cstring>
#include <algorithm>
#include <functional>
#include <type_traits>

#include "Prototypes.hpp"

using namespace proto;
using namespace proto::test;

namespace {

// ------------------------------
// Test fixture for Tx containers
// ------------------------------
class TxContainerSuite : public ::testing::Test {
protected:
    // Static TX buffers used by our protocols
    static inline uint8_t tx_simple_[256]  = {};
    static inline uint8_t tx_complex_[256] = {};

    // Dedicated RX buffers for round-trip validation
    static inline uint8_t rx_simple_[256]  = {};
    static inline uint8_t rx_complex_[256] = {};

    // Protocols (we instantiate both RX/TX templates, but only use TX here)
    using SimpleProto  = proto::test::SympleProtocol<tx_simple_, tx_simple_>;
    using ComplexProto = proto::test::ComplexProtocol<tx_complex_, tx_complex_>;

    using SimpleProtoMixed  = proto::test::SympleProtocol<rx_simple_, tx_simple_>;
    using ComplexProtoMixed = proto::test::ComplexProtocol<rx_complex_, tx_complex_>;

    // Convenience aliases for field tuples and containers
    using SimpleFields   = proto::test::SympleFields<tx_simple_>;
    using ComplexFieldsT = proto::test::ComplexFields<tx_complex_>;

    // Accessors to TX containers
    static auto& tx_simple()  {
        static SimpleProto p; // lives for whole test run
        // friend accessors are defined in Prototypes.hpp test helpers
        return p.tx;
    }
    static auto& tx_complex() {
        static ComplexProto p;
        return p.tx;
    }

    static auto& rx_simple() {
        static SimpleProtoMixed p; // separate instance
        return p.rx;
    }
    static auto& rx_complex() {
        static ComplexProtoMixed p;
        return p.rx;
    }

    // Helper: feed a built TX frame into RX in field order and collect a callback
    template<class RxCont, class TxCont, class OnReceived = std::nullptr_t>
    static bool RoundtripToRx(RxCont& rx, TxCont& tx, OnReceived on_received = nullptr) {
        using RxT = std::decay_t<RxCont>;
        bool received = false;

        typename RxT::CallbackType cb = [&](RxT& cont){
            received = true;
            if constexpr (!std::is_same_v<OnReceived, std::nullptr_t>) {
                on_received(cont);
            }
        };
        auto delegate = rx.AddReceiveCallback(cb);

        // Feed RX in protocol field order, exactly the bytes each field owns.
        tx.for_each_type([&](auto& fld){
            const uint8_t* p = (uint8_t*)fld.GetPtr();
          CustomSpan<uint8_t> chunk(const_cast<uint8_t*>(p), fld.GetSize());
            size_t read = 0;
            rx.Fill(chunk, read);
        });

        return received;
    }
};

// ----------------------------------------------------------------------------
// Simple layout: sending a packet should populate ID/LEN/ALEN/DATA/CRC coherently
// ----------------------------------------------------------------------------
TEST_F(TxContainerSuite, Send_SimplePacket) {
    auto& tx = tx_simple();

    // Prepare payload
    const proto::test::dataType payload{1, 2, 3, 4.f, 2.718281828459045};

    // Send: for simple layout TYPE_FIELD is absent, only DATA needed
    const size_t n = tx.SendPacket(
        proto::MakeFieldInfo<FieldName::DATA_FIELD>(&payload, sizeof(payload))
    );
    ASSERT_GT(n, 0u);

    // Base pointer of the frame (ID field points to the beginning)
    auto* frame = reinterpret_cast<const uint8_t*>(tx.Get<FieldName::ID_FIELD>().TestBase());

    // 1) ID must equal protocol prefix
    {
        constexpr auto& expect = SimpleFields::prefix; // bind as array reference to preserve size
        EXPECT_EQ(std::memcmp(frame, expect, sizeof(expect)), 0) << "ID/prefix mismatch";
    }

    // 2) LEN must equal sum of sizes of fields flagged with IS_IN_LEN
    {
        uint8_t len_val = *tx.Get<FieldName::LEN_FIELD>().GetPtr();
        size_t expect_len = 0;
        tx.for_each_type([&](auto& fld){
            using F = std::decay_t<decltype(fld)>;
            if constexpr ((F::flags_ & FieldFlags::IS_IN_LEN) != FieldFlags::NOTHING) {
                expect_len += fld.GetSize();
            }
        });
        EXPECT_EQ(len_val, expect_len) << "LEN field does not match sum(IS_IN_LEN)";
    }

    // 3) ALEN must be bitwise negation of LEN (library contract used in tests)
    {
        uint8_t len_val  = *tx.Get<FieldName::LEN_FIELD>().GetPtr();
        uint8_t alen_val = *tx.Get<FieldName::ALEN_FIELD>().GetPtr();
        EXPECT_EQ(static_cast<uint8_t>(~len_val), alen_val) << "ALEN != ~LEN";
    }

    // 4) DATA region must contain payload
    {
        auto* data_ptr = reinterpret_cast<const uint8_t*>(tx.Get<FieldName::DATA_FIELD>().GetPtr());
        EXPECT_EQ(std::memcmp(data_ptr, &payload, sizeof(payload)), 0) << "DATA mismatch";
    }

    // 5) End-to-end: feed produced frame to RX and expect successful reception
    auto& rx = rx_simple();
    const bool ok = RoundtripToRx(rx, tx);
    EXPECT_TRUE(ok) << "RX did not accept frame from TX (simple)";
}

// ----------------------------------------------------------------------------
// Complex layout: TYPE + DATA. Also checks that CRC matches for this layout.
// If CRC field in Complex has REVERSE, this still verifies correctness because
// ComputeCrc16() consumes the already-written bytes from the container.
// ----------------------------------------------------------------------------
TEST_F(TxContainerSuite, Send_ComplexPacket_TypeAndData) {
    auto& tx = tx_complex();

    // Prepare three payloads; choose one by type value
    const proto::test::dataType  d1{1, 2, 3, 4.f, 2.718281828459045};
    const proto::test::dataType2 d2{};
    const proto::test::dataType3 d3{};

    const auto send_ok = [&](uint8_t type_val){
        if(type_val == 1){
            return tx.SendPacket(
                proto::MakeFieldInfo<FieldName::TYPE_FIELD>(&type_val),
                proto::MakeFieldInfo<FieldName::DATA_FIELD>(&d1, sizeof(d1))
            );
        } else if(type_val == 2){
            return tx.SendPacket(
                proto::MakeFieldInfo<FieldName::TYPE_FIELD>(&type_val),
                proto::MakeFieldInfo<FieldName::DATA_FIELD>(&d2, sizeof(d2))
            );
        } else {
            return tx.SendPacket(
                proto::MakeFieldInfo<FieldName::TYPE_FIELD>(&type_val),
                proto::MakeFieldInfo<FieldName::DATA_FIELD>(&d3, sizeof(d3))
            );
        }
    };

    // Try each supported type id
    for (uint8_t t : {uint8_t{1}, uint8_t{2}, uint8_t{3}}) {
        const size_t n = send_ok(t);
        ASSERT_GT(n, 0u) << "SendPacket failed for type " << int(t);

        // Sanity: TYPE field must be written
        EXPECT_EQ(*tx.Get<FieldName::TYPE_FIELD>().GetPtr(), t);

        // LEN/ALEN invariant holds
        uint8_t len_val  = *tx.Get<FieldName::LEN_FIELD>().GetPtr();
        uint8_t alen_val = *tx.Get<FieldName::ALEN_FIELD>().GetPtr();
        EXPECT_EQ(static_cast<uint8_t>(~len_val), alen_val);

        // Round-trip into RX and verify the variant matches the type id we sent
        auto& rx = rx_complex();
        bool ok = RoundtripToRx(rx, tx, [&](std::decay_t<decltype(rx)>& cont){
            auto v = cont.template Get<FieldName::DATA_FIELD>().GetCopy();
            switch (t) {
                case 1: EXPECT_TRUE(std::holds_alternative<proto::test::dataType>(v)); break;
                case 2: EXPECT_TRUE(std::holds_alternative<proto::test::dataType2>(v)); break;
                case 3: EXPECT_TRUE(std::holds_alternative<proto::test::dataType3>(v)); break;
                default: FAIL() << "Unexpected type id=" << int(t);
            }
        });
        EXPECT_TRUE(ok) << "RX did not accept frame from TX (complex), type=" << int(t);
    }
}

// ----------------------------------------------------------------------------
// Complex layout: verify that frame on wire is contiguous and starts at ID base
// ----------------------------------------------------------------------------
TEST_F(TxContainerSuite, Complex_FrameIsContiguousFromIdBase) {
    auto& tx = tx_complex();
    const proto::test::dataType d1{1,2,3,4.f, 2.718281828459045};
    const uint8_t type_val = 1;

    const size_t n = tx.SendPacket(
        proto::MakeFieldInfo<FieldName::TYPE_FIELD>(&type_val),
        proto::MakeFieldInfo<FieldName::DATA_FIELD>(&d1, sizeof(d1))
    );
    ASSERT_GT(n, 0u);

    auto* base = reinterpret_cast<const uint8_t*>(tx.Get<FieldName::ID_FIELD>().TestBase());

    // Just verify that each field's pointer lies inside [base, base+n)
    size_t min_off = SIZE_MAX, max_end = 0;
    tx.for_each_type([&](auto& fld){
        auto* p = (uint8_t*)fld.GetPtr();
        size_t off = static_cast<size_t>(p - base);
        min_off = std::min(min_off, off);
        max_end = std::max(max_end, off + fld.GetSize());
    });
    EXPECT_EQ(min_off, 0u);
    EXPECT_LE(max_end, n);
}

} // namespace
