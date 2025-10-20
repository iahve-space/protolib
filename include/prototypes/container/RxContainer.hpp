/**
 * @file RxContainer.hpp
 * @brief Stateful frame parsing container that incrementally matches and fills protocol fields from a byte stream.
 *
 * This header defines ::proto::RxContainer — a typed, compile-time configured receive (RX) container
 * that consumes a stream of bytes (possibly chunked) and assembles protocol frames in-place
 * using a set of field prototypes (see FieldContainer and field prototypes). The container:
 *   - Tracks per-field offsets and incremental read progress.
 *   - Applies per-field matchers (length/anti-length/CRC/type) as soon as a field completes.
 *   - Emits user callbacks when a whole frame is received and validated.
 *   - Supports debug output that prints detailed mismatch diagnostics and a snapshot of the partially
 *     received fields when a frame breaks.
 *
 * @note The container is deliberately non-owning regarding I/O. Feed it with bytes via Fill().
 *       Use AddReceiveCallback() to subscribe to fully parsed frames. Callbacks are weakly held
 *       to avoid reference cycles and to allow automatic cleanup when user code drops its shared_ptr.
 *
 * @tparam Fields  A std::tuple of field prototypes describing the frame layout in RX direction.
 * @tparam TCrc    CRC policy type. Must provide Reset() and Append(uint32_t, Span<uint8_t>) -> uint32_t.
 */
#pragma once

#include "FieldContainer.hpp"
#include <tuple>
#include <algorithm>
#include <unordered_map>
#include <cstring>
#include <memory>

#include "CustomSpan.hpp"
#include <functional>

namespace proto
{
    /**
     * @class RxContainer
     * @ingroup containers
     * @brief Incremental parser for protocol frames based on field prototypes.
     *
     * RxContainer inherits FieldContainer and augments it with a small state machine that walks
     * the field tuple in order and fills each field from the input stream. If a field completes,
     * its matcher (if any) is invoked to validate/update container state. When all fields are matched,
     * registered receive callbacks are invoked with a reference to the container.
     *
     * ### Lifecycle & usage
     * 1. Construct an RxContainer. Default matchers for LEN/ALEN/CRC/TYPE are bound if present.
     * 2. Optionally enable debug with `SetDebug(true)` on the base container.
     * 3. Feed incoming data via `Fill(span, read)`. You may pass arbitrarily sized chunks.
     * 4. Subscribe to completed frames via `AddReceiveCallback(callback)`.
     *
     * @warning This class is not thread-safe. If used across threads/ISRs, synchronize external calls
     *          to Fill() and callbacks, or provide a single-threaded handoff buffer.
     */
    template<typename Fields, typename TCrc = CrcSoft>
    class RxContainer : public FieldContainer<Fields, TCrc>
    {
    public:
        /**
         * @brief Type alias for the callback invoked when a full frame is received.
         *
         * The callback receives a reference to this container so the user can inspect field values.
         * Prefer capturing only what you need to avoid prolonging lifetimes; callbacks are stored as weak_ptr.
         */
        using CallbackType = std::function<void(RxContainer<Fields, TCrc>&)>;
        using Delegate = std::shared_ptr<CallbackType>;

        /**
         * @brief Constructs an RxContainer and auto-binds default matchers for standard fields.
         *
         * If present in @p Fields and the matcher is not already set, the following associations are applied:
         *   - LEN_FIELD  -> SetDataLen()
         *   - ALEN_FIELD -> CheckAlen()
         *   - CRC_FIELD  -> CheckCrc()
         *   - TYPE_FIELD -> CheckType()
         * Finally, the container state is Reset().
         */
        RxContainer(){

            if((*this).template HasField<FieldName::LEN_FIELD>() && (*this).template Get<FieldName::LEN_FIELD>().matcher_ == nullptr){
                (*this).template Get<FieldName::LEN_FIELD>().matcher_ = &RxContainer::SetDataLen;
            }

            if((*this).template HasField<FieldName::ALEN_FIELD>() && (*this).template Get<FieldName::ALEN_FIELD>().matcher_ == nullptr){
                (*this).template Get<FieldName::ALEN_FIELD>().matcher_ = &RxContainer::CheckAlen;
            }

            if((*this).template HasField<FieldName::CRC_FIELD>() && (*this).template Get<FieldName::CRC_FIELD>().matcher_ == nullptr){
                (*this).template Get<FieldName::CRC_FIELD>().matcher_ = &RxContainer::CheckCrc;
            }

            if((*this).template HasField<FieldName::TYPE_FIELD>() && (*this).template Get<FieldName::TYPE_FIELD>().matcher_ == nullptr){
                (*this).template Get<FieldName::TYPE_FIELD>().matcher_ = &RxContainer::CheckType;
            }

            this->Reset();
        }

        /**
         * @brief Compile-time unrolled dispatcher that calls a functor for a runtime field index.
         * @tparam I    Current compile-time index (do not pass explicitly).
         * @tparam Func Callable of the form `MatchStatus operator()(std::integral_constant<size_t, I>, Args...)`.
         * @param runtime_index Index in [0, FieldContainer::size).
         * @param f Functor to invoke for the matching index.
         * @param args Additional arguments forwarded to @p f.
         * @return MatchStatus returned by the invoked functor, or NOT_MATCH if index is out of range.
         */
        template<typename Func, std::size_t I = 0, typename... Args>
        static MatchStatus static_for_index(std::size_t runtime_index, Func&& f, Args&&... args) {
            if constexpr (I < FieldContainer<Fields, TCrc>::size) {
                if (runtime_index == I) {
                    return f.template operator()(std::integral_constant<std::size_t, I>{} ,args...);
                } else {
                    return static_for_index<Func, I + 1>(runtime_index, std::forward<Func>(f), std::forward<Args>(args)...);
                }
            }
            return MatchStatus::NOT_MATCH;
        }

        /**
         * @brief Feeds a chunk of bytes to the parser and advances internal state.
         * @param src  Input chunk.
         * @param[out] read Number of bytes consumed from @p src in the last field step.
         *
         * The method may iterate over multiple fields in a single call. If the current field mismatches,
         * the container optionally prints a diagnostic snapshot (when debug is enabled) and resets state
         * to start searching for the next valid frame boundary. On full frame completion, all valid
         * callbacks are invoked (old expired ones are removed).
         */
        void Fill(const CustomSpan<uint8_t> &src, size_t &read){
          CustomSpan<uint8_t> ptr = src;
            MatchStatus result = MatchStatus::NOT_MATCH;
            while(!ptr.empty() )
            {
                static_for_index(
                        this->field_index_,
                        [&](auto index_c, CustomSpan<uint8_t>& ptr, size_t& read) -> MatchStatus {
                            read = 0;
                            constexpr std::size_t I = decltype(index_c)::value;
                            auto & field = std::get<I>(this->fields_);
                            field.offset_ = this->offsets[field.base_];
                            auto result = this->FillFields<I>( ptr, read);

                            if( result == MatchStatus::NOT_MATCH){
                                if(field.read_count_!=0 && field.GetSize()!=field.read_count_){
                                    read = 0;
                                }
                                if constexpr (I != 0 ){
                                    read = 0;
                                    if(this->IsDebug())
                                    {
                                        auto f = [this](auto index_c) -> MatchStatus {
                                            constexpr std::size_t J = decltype(index_c)::value;
                                            auto &field = std::get<J>(this->fields_);
                                            std::cout << "Field " << ToString(FieldTraits<decltype(field)>::name)
                                                      << " received: ";
                                            {
                                                // сохранить и восстановить формат потока
                                                std::ios_base::fmtflags f(std::cout.flags());
                                                auto old_fill = std::cout.fill();
                                                std::cout << std::hex << std::uppercase << std::setfill('0');
                                                for (size_t i = 0; i < static_cast<size_t>(field.read_count_ |
                                                                                           field.GetSize() ); ++i) {
                                                    uint8_t byte = *(field.begin() + i);
                                                    // продвигаем к целочисленному типу, чтобы не печаталось как char
                                                    unsigned v = static_cast<unsigned>(byte);
                                                    // печатаем как 0xNN
                                                    std::cout << " 0x" << std::setw(2) << v;
                                                    if (i + 1 < static_cast<size_t>(field.read_count_))
                                                        std::cout << ' ';
                                                }
                                                // вернуть формат
                                                std::cout.flags(f);
                                                std::cout.fill(old_fill);
                                            }
                                            std::cout << std::endl;
                                            return MatchStatus::NOT_MATCH;
                                        };

                                        std::cout << "-------------BROKEN PACKET START-------------" << std::endl;
                                        for (int i = 0; i <= (int)this->field_index_; i++) {
                                            static_for_index(i, f);
                                        }
                                        std::cout << "-------------BROKEN PACKET STOP-------------" << std::endl;
                                    }
                                }
                                this->Reset();
                            }
                            else if(result == MatchStatus::MATCH){
                                this->offsets[field.base_] = field.size_ + field.GetOffset();
                                this->field_index_++;
                                if(this->field_index_ >= this->size)
                                {
                                    for (int i = static_cast<int>(receive_callbacks_.size()) - 1; i >= 0; --i) {
                                        auto callback = receive_callbacks_[i].lock(); // shared_ptr или nullptr
                                        if (!callback) {
                                            receive_callbacks_.erase(receive_callbacks_.begin() + i);
                                        } else {
                                            (*callback)(*this); // если callback — функция/функтор
                                        }
                                    }
                                    this->Reset();
                                }
                            }
                            ptr = ptr.subspan(read);
                            return result;
                        },
                        ptr, read
                );
            }
//            return result;
        }

        /**
         * @brief Reads bytes for a specific field, validates const values and copies into the frame buffer.
         * @tparam Index Index of the field within the tuple.
         * @param ptr  [in,out] Remaining input span; consumed bytes are accounted via @p read.
         * @param read [out]    Number of bytes consumed for this field step.
         * @return PROCESSING if the field is incomplete; MATCH if fully read (and matcher passed or absent);
         *         NOT_MATCH on mismatch (const value check or matcher failure).
         *
         * @details
         *  - Honors FieldFlags::REVERSE for endianness when reading constants and storing bytes.
         *  - Invokes `matcher_` of the field when it completes, enabling length/type/CRC checks to run early.
         */
        template<size_t Index>
        MatchStatus FillFields( CustomSpan<uint8_t>& ptr, size_t& read) {
            auto & field = std::get<Index>(this->fields_);
            if(field.GetSize() == 0){
                return MatchStatus::MATCH;
            }
            size_t byte_to_read = std::min(ptr.size(), field.GetSize() - field.read_count_);
            if (FieldTraits<decltype(field)>::const_value != nullptr){
                if constexpr (HasFlag(std::remove_reference_t<decltype(field)>::flags_, FieldFlags::REVERSE)){
                    for(int i = 0; i < (int)byte_to_read; i ++){
                        if(ptr[i] != field.const_value_[field.GetSize() - 1 - field.read_count_ - i]){
                            ++read;
                            if(this->IsDebug()){}
                            return MatchStatus::NOT_MATCH;
                        }
                    }
                }
                else{
                    if(std::memcmp(ptr.data(), (uint8_t*)field.const_value_ + field.read_count_, byte_to_read) != 0){
                        ++read;
                        if(this->IsDebug()){}
                        return MatchStatus::NOT_MATCH;
                    }
                }
            }
            read += byte_to_read;
            if constexpr (HasFlag(std::remove_reference_t<decltype(field)>::flags_, FieldFlags::REVERSE)){
                for(int i = 0; i < (int)byte_to_read; i ++){
                    uint8_t *data = (field.base_ + field.offset_ + field.GetSize()-1) - field.read_count_ -i;
                    *data = ptr.data()[i];
                }
            }
            else{
                std::memcpy(field.base_ + field.offset_ + field.read_count_, ptr.data(), byte_to_read);
            }
            field.read_count_ += byte_to_read;

            if(field.read_count_ < field.size_){
                return MatchStatus::PROCESSING;
            }
            else if(field.matcher_){
                return field.matcher_((void*)this);
            }
            else {
                field.read_count_ = 0;
                return MatchStatus::MATCH;
            }
        }

        /**
         * @brief Adjusts DATA_FIELD size based on LEN_FIELD and sizes of fields marked IS_IN_LEN.
         * @param obj Opaque pointer to this container (provided by FillFields).
         * @return MATCH on successful adjustment/validation, NOT_MATCH if declared size disagrees.
         *
         * @note This matcher allows DATA_FIELD to be `kAnySize` (dynamic). When DATA_FIELD has a fixed size,
         *       the method validates that LEN_FIELD matches the expected size.
         */
        static MatchStatus SetDataLen(void* obj){
            auto& container = *static_cast<RxContainer<Fields>*>(obj);
            auto &data_field = container.template Get<FieldName::DATA_FIELD>();
            auto &len_field = container.template Get<FieldName::LEN_FIELD>();
            auto len = *len_field.GetPtr();

            container.for_each_type( [&](auto& field){
                if (field.name_ != FieldName::DATA_FIELD) {
                    if (HasFlag(field.flags_, FieldFlags::IS_IN_LEN)) {
                        len -= field.size_;
                    }
                }
            });

            if constexpr (RxContainer<Fields>::template HasField<FieldName::DATA_FIELD>()){
                if (data_field.size_ != 0 && data_field.size_ != kAnySize) {
                    if (len != data_field.GetSize()) {
                        if(container.IsDebug()){
                            auto expected = static_cast<unsigned>(*len_field.GetPtr())
                                            + (data_field.GetSize() - static_cast<unsigned>(len));

                            std::ios_base::fmtflags f(std::cout.flags()); // сохранить формат

                            std::cout << "\nMismatch in length field (method SetDataLen):\n"
                                      << "  Expected: " << std::dec << static_cast<unsigned>(expected)
                                      << " (0x" << std::hex << std::uppercase << static_cast<unsigned>(expected) << ")\n"
                                      << "  Received: " << std::dec << static_cast<unsigned>(*len_field.GetPtr())
                                      << " (0x" << std::hex << std::uppercase << static_cast<unsigned>(*len_field.GetPtr()) << ")\n";

                            std::cout.flags(f); // восстановить
                        }
                        return MatchStatus::NOT_MATCH;
                    }
                    data_field.size_ = len;
                    return MatchStatus::MATCH;
                }
            }

            data_field.size_ = len;
            return MatchStatus::MATCH;
        }

        /**
         * @brief Validates anti-length (ALEN) against LEN (bitwise NOT).
         * @param obj Opaque pointer to this container.
         * @return MATCH if `~ALEN == LEN`, otherwise NOT_MATCH.
         */
        static MatchStatus CheckAlen(void *obj){
            auto& container = *static_cast<RxContainer<Fields>*>(obj);
            auto len = *container.template Get<FieldName::LEN_FIELD>().GetPtr();
            auto alen = *container.template Get<FieldName::ALEN_FIELD>().GetPtr();

            alen = ~alen;

            bool result = len == alen;
            if(container.IsDebug() && not result){
                auto to_uint = [](auto v) { return static_cast<unsigned>(v); };

                auto f = std::cout.flags();
                auto fill = std::cout.fill();

                std::cout << "\nMismatch in ALEN field:\n"
                          << "  Expected: " << std::dec << to_uint(~len)
                          << " (0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << to_uint(~len) << ")\n"
                          << "  Received: " << std::dec << to_uint(~alen)
                          << " (0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << to_uint(~alen) << ")\n";

                std::cout.flags(f);
                std::cout.fill(fill);
            }
            return result ? MatchStatus::MATCH : MatchStatus::NOT_MATCH;
        }

        /**
         * @brief Computes CRC over fields flagged IS_IN_CRC and compares it with CRC_FIELD.
         * @param obj Opaque pointer to this container.
         * @return MATCH if the computed CRC equals the value in CRC_FIELD; NOT_MATCH otherwise.
         *
         * @requirements TCrc must expose:
         *   - void Reset();
         *   - uint32_t Append(uint32_t, Span<uint8_t>);
         *
         * @remarks The CRC width is inferred from the CRC_FIELD storage type when pretty-printing debug values.
         */
        static MatchStatus CheckCrc(void *obj){
            auto& container = *static_cast<RxContainer<Fields, TCrc>*>(obj);
            auto crc_in_field = *container.template Get<FieldName::CRC_FIELD>().GetPtr();
            using crc_type = decltype(crc_in_field);
            uint32_t crc = 0;
            container.crc_.Reset();

            container.for_each_type([&](auto& field){
                using field_type = typename std::remove_reference<decltype(field)>::type;
                if constexpr (HasFlag(field_type::flags_, FieldFlags::IS_IN_CRC)) {
                    auto *data = field.GetPtr();
                    size_t size = field.GetSize();
                    crc = container.crc_.Append(crc, {(uint8_t*)data, size});
                }
            });

            bool result = crc_in_field == static_cast<decltype(crc_in_field)>(crc);
            if(container.IsDebug() && not result){
                auto f = std::cout.flags();      // сохранить текущие флаги
                auto fill = std::cout.fill();    // и текущий fill-символ

                auto to_u = [](auto v) -> uint64_t { return static_cast<uint64_t>(v); };
                crc_type c=-1;
                const auto crc_exp = to_u(crc & c );
                const auto crc_got = to_u(crc_in_field & c);

                // ширина HEX по размеру исходного типа (если crc — uint16_t, будет 4; если uint32_t — 8)
                constexpr int hexw = static_cast<int>(sizeof(crc) * 2);

                std::cout << "\nMismatch in CRC field:\n"
                          << "  Expected: " << std::dec << crc_exp
                          << " (0x" << std::hex << std::uppercase << std::setw(hexw) << std::setfill('0') << crc_exp << ")\n"
                          << "  Received: " << std::dec << crc_got
                          << " (0x" << std::hex << std::uppercase << std::setw(hexw) << std::setfill('0') << crc_got << ")\n";

                std::cout.flags(f);              // восстановить
                std::cout.fill(fill);
            }
            return result? MatchStatus::MATCH : MatchStatus::NOT_MATCH;
        }

        /**
         * @brief Updates DATA_FIELD alternative by received TYPE_FIELD and validates its expected size.
         * @param obj Opaque pointer to this container.
         * @return MATCH if type is known and sizes match (or are dynamic), NOT_MATCH otherwise.
         *
         * @details For data-field variants, SetId(type) selects the concrete alternative.
         *          When DATA_FIELD exposes a fixed size for the selected alternative, a mismatch
         *          is treated as a protocol error and the frame is rejected.
         */
        static MatchStatus CheckType(void *obj){
            auto& container = *static_cast<RxContainer<Fields>*>(obj);

            int type = *container.template Get<FieldName::TYPE_FIELD>().GetPtr();
            auto& data_field = container.template Get<FieldName::DATA_FIELD>();

            if constexpr (is_data_field_prototype<decltype(data_field)>::value) {

                if (not data_field.SetId(type)) {
                    if(container.IsDebug()) {
                        auto f = std::cout.flags();     // сохранить флаги
                        auto fill = std::cout.fill();   // и символ заполнения
                        auto to_u = [](auto v) -> uint64_t { return static_cast<uint64_t>(v); };
                        const auto type_u = to_u(type);
                        std::cout
                                << "\n---------------------------\n"
                                << "Incorrect type received (method CheckType):\n"
                                << "  Received type id: " << std::dec << type_u
                                << "\n---------------------------\n";

                        std::cout.flags(f);             // восстановить формат флагов
                        std::cout.fill(fill);           // восстановить fill-символ

                    }
                    return MatchStatus::NOT_MATCH;
                }
            }
            size_t packet_size = data_field.GetSize();
            if(packet_size!= kAnySize ){
                if(data_field.size_!=0 && data_field.size_ !=packet_size){

                    if(container.IsDebug()){
                        auto f = std::cout.flags();     // сохранить флаги
                        auto fill = std::cout.fill();   // и символ заполнения

                        auto to_u = [](auto v) -> uint64_t { return static_cast<uint64_t>(v); };

                        const auto type_u   = to_u(type);
                        const auto expect_u = to_u(packet_size);
                        const auto got_u    = to_u(data_field.size_);

                        // ширина HEX по размеру size_t (подходит для размеров буферов)
                        constexpr int hexw = static_cast<int>(sizeof(size_t) * 2);

                        std::cout
                                << "\n---------------------------\n"
                                << "Mismatch in data field size (method CheckType):\n"
                                << "  Received type id: " << std::dec << type_u
                                << " (0x" << std::hex << std::uppercase << std::setw(hexw) << std::setfill('0') << type_u << ")\n"
                                << "  Expected size:    " << std::dec << expect_u
                                << " (0x" << std::hex << std::uppercase << std::setw(hexw) << std::setfill('0') << expect_u << ")\n"
                                << "  Calculated size:  " << std::dec << got_u
                                << " (0x" << std::hex << std::uppercase << std::setw(hexw) << std::setfill('0') << got_u << ")\n"
                                << "---------------------------\n";

                        std::cout.flags(f);             // восстановить формат флагов
                        std::cout.fill(fill);           // восстановить fill-символ
                    }
                    return MatchStatus::NOT_MATCH;
                }
                else{
                    data_field.size_ = packet_size;
                }
                return MatchStatus::MATCH;
            }
            return MatchStatus::NOT_MATCH;
        }

        /**
         * @brief Returns the current DATA_FIELD size (may be dynamic).
         */
        [[nodiscard]] size_t GetSize() const {
            return this->template Get<FieldName::DATA_FIELD>().GetSize();
        }
        /**
         * @brief Subscribes to notifications when a full frame is parsed.
         * @param callback Callable of signature `void(RxContainer&)`.
         * @return A shared_ptr token to keep the subscription alive. Drop it to auto-unsubscribe.
         * @thread_safety Callbacks execute synchronously from Fill(). Avoid long-running work inside.
         */
        [[nodiscard]] Delegate AddReceiveCallback( CallbackType callback) {
            auto result = std::make_shared<CallbackType>(callback);
            receive_callbacks_.push_back(result);
            return result;
        }
    private:
        /**
         * @brief Weak list of subscribers. Expired entries are cleaned up on each frame completion.
         */
        std::vector<std::weak_ptr<CallbackType>> receive_callbacks_;
    };
}

/**
 * @section RxContainer_Improvements Potential improvements
 * @par Backpressure / flow control
 *   Consider returning a struct from Fill() that reports total bytes consumed and whether a frame
 *   callback was fired, which can help upstream code pace input.
 * @par Error policy hooks
 *   Provide a user hook to observe NOT_MATCH reasons with an enum, instead of relying solely on stdout.
 * @par Partial-frame timeouts
 *   Optionally track timestamps and reset if a field remains incomplete for too long.
 * @par Debug printer abstraction
 *   Replace direct std::cout usage with an injected logger to allow unit tests to intercept logs
 *   and to cut I/O in production builds without ifdefs.
 * @par Iterator-friendly Fill
 *   Overload Fill(begin,end) to consume from arbitrary containers without building a Span first.
 * @par Small-buffer optimization
 *   For tiny frames, consider an internal scratch buffer to reduce memcpy for constant prefix checks.
 */