/**
 * @file RxContainer.hpp
 * @brief Stateful frame parsing container that incrementally matches and fills
 * protocol fields from a byte stream.
 *
 * This header defines ::proto::RxContainer — a typed, compile-time configured
 * receive (RX) container that consumes a stream of bytes (possibly chunked) and
 * assembles protocol frames in-place using a set of field prototypes (see
 * FieldContainer and field prototypes). The container:
 *   - Tracks per-field offsets and incremental read progress.
 *   - Applies per-field matchers (length/anti-length/CRC/type) as soon as a
 * field completes.
 *   - Emits user callbacks when a whole frame is received and validated.
 *   - Supports debug output that prints detailed mismatch diagnostics and a
 * snapshot of the partially received fields when a frame breaks.
 *
 * @note The container is deliberately non-owning regarding I/O. Feed it with
 * bytes via Fill(). Use AddReceiveCallback() to subscribe to fully parsed
 * frames. Callbacks are weakly held to avoid reference cycles and to allow
 * automatic cleanup when user code drops its shared_ptr.
 *
 * @tparam Fields  A std::tuple of field prototypes describing the frame layout
 * in RX direction.
 * @tparam TCrc    CRC policy type. Must provide Reset() and Append(uint32_t,
 * Span<uint8_t>) -> uint32_t.
 */
#pragma once

#include <algorithm>
#include <cstring>
#include <functional>
#include <memory>
#include <tuple>

#include "CustomSpan.hpp"
#include "prototypes/container/FieldContainer.hpp"

namespace proto {
/**
 * @class RxContainer
 * @ingroup containers
 * @brief Incremental parser for protocol frames based on field prototypes.
 *
 * RxContainer inherits FieldContainer and augments it with a small state
 * machine that walks the field tuple in order and fills each field from the
 * input stream. If a field completes, its matcher (if any) is invoked to
 * validate/update container state. When all fields are matched, registered
 * receive callbacks are invoked with a reference to the container.
 *
 * ### Lifecycle & usage
 * 1. Construct an RxContainer. Default matchers for LEN/ALEN/CRC/TYPE are bound
 * if present.
 * 2. Optionally enable debug with `SetDebug(true)` on the base container.
 * 3. Feed incoming data via `Fill(span, read)`. You may pass arbitrarily sized
 * chunks.
 * 4. Subscribe to completed frames via `AddReceiveCallback(callback)`.
 *
 * @warning This class is not thread-safe. If used across threads/ISRs,
 * synchronize external calls to Fill() and callbacks, or provide a
 * single-threaded handoff buffer.
 */
template <typename Fields, typename TCrc = CrcSoft>
class RxContainer : public FieldContainer<Fields, TCrc> {
 public:
  /**
   * @brief Type alias for the callback invoked when a full frame is received.
   *
   * The callback receives a reference to this container so the user can inspect
   * field values. Prefer capturing only what you need to avoid prolonging
   * lifetimes; callbacks are stored as weak_ptr.
   */
  using CallbackType = std::function<void(RxContainer<Fields, TCrc>&)>;
  using Delegate = std::shared_ptr<CallbackType>;

  /**
   * @brief Constructs an RxContainer and auto-binds default matchers for
   * standard fields.
   *
   * If present in @p Fields and the matcher is not already set, the following
   * associations are applied:
   *   - LEN_FIELD  -> SetDataLen()
   *   - ALEN_FIELD -> CheckAlen()
   *   - CRC_FIELD  -> CheckCrc()
   *   - TYPE_FIELD -> CheckType()
   * Finally, the container state is Reset().
   */
  RxContainer() {
    if ((*this).template has_field<FieldName::LEN_FIELD>() &&
        (*this).template get<FieldName::LEN_FIELD>().m_matcher == nullptr) {
      (*this).template get<FieldName::LEN_FIELD>().m_matcher =
          &RxContainer::set_data_len;
    }

    if ((*this).template has_field<FieldName::ALEN_FIELD>() &&
        (*this).template get<FieldName::ALEN_FIELD>().m_matcher == nullptr) {
      (*this).template get<FieldName::ALEN_FIELD>().m_matcher =
          &RxContainer::check_alen;
    }

    if ((*this).template has_field<FieldName::CRC_FIELD>() &&
        (*this).template get<FieldName::CRC_FIELD>().m_matcher == nullptr) {
      (*this).template get<FieldName::CRC_FIELD>().m_matcher =
          &RxContainer::check_crc;
    }

    if ((*this).template has_field<FieldName::TYPE_FIELD>() &&
        (*this).template get<FieldName::TYPE_FIELD>().m_matcher == nullptr) {
      (*this).template get<FieldName::TYPE_FIELD>().m_matcher =
          &RxContainer::check_type;
    }

    this->reset();
  }

  /**
   * @brief Compile-time unrolled dispatcher that calls a functor for a runtime
   * field index.
   * @tparam I    Current compile-time index (do not pass explicitly).
   * @tparam Func Callable of the form `MatchStatus
   * operator()(std::integral_constant<size_t, I>, Args...)`.
   * @param runtime_index Index in [0, FieldContainer::size).
   * @param FUNC Functor to invoke for the matching index.
   * @param args Additional arguments forwarded to @p f.
   * @return MatchStatus returned by the invoked functor, or NOT_MATCH if index
   * is out of range.
   */
  template <typename Func, std::size_t I = 0, typename... Args>
  static auto static_for_index(std::size_t runtime_index, Func&& FUNC,
                               Args&&... args) -> MatchStatus {
    if constexpr (I < FieldContainer<Fields, TCrc>::SIZE) {
      if (runtime_index == I) {
        return FUNC.operator()(std::integral_constant<std::size_t, I>{},
                               args...);
      }
      return static_for_index<Func, I + 1>(
          runtime_index, std::forward<Func>(FUNC), std::forward<Args>(args)...);
    }
    return MatchStatus::NOT_MATCH;
  }

  /**
   * @brief Feeds a chunk of bytes to the parser and advances internal state.
   * @param src  Input chunk.
   * @param[out] read Number of bytes consumed from @p src in the last field
   * step.
   *
   * The method may iterate over multiple fields in a single call. If the
   * current field mismatches, the container optionally prints a diagnostic
   * snapshot (when debug is enabled) and resets state to start searching for
   * the next valid frame boundary. On full frame completion, all valid
   * callbacks are invoked (old expired ones are removed).
   */
  void fill(const CustomSpan<uint8_t>& src, size_t& read) {
    CustomSpan<uint8_t> ptr = src;
    while (!ptr.empty()) {
      static_for_index(
          this->m_field_index,
          [&](auto index_c, CustomSpan<uint8_t>& PTR,
              size_t& READ) -> MatchStatus {
            READ = 0;
            constexpr std::size_t ITER = decltype(index_c)::value;
            auto& field = std::get<ITER>(this->m_fields);
            field.m_offset = this->m_offsets[field.BASE];
            auto result = this->fill_fields<ITER>(PTR, READ);

            if (result == MatchStatus::NOT_MATCH) {
              if (field.m_read_count != 0 &&
                  field.get_size() != field.m_read_count) {
                READ = 0;
              }
              if constexpr (ITER != 0) {
                READ = 0;
                if (this->is_debug()) {
                  auto FUNC =
                      [this]([[maybe_unused]] auto INDEX) -> MatchStatus {
                    constexpr std::size_t VAL = decltype(INDEX)::value;
                    auto& field_ = std::get<VAL>(this->m_fields);
                    std::cout << "Field "
                              << to_string(FieldTraits<decltype(field_)>::NAME)
                              << " received: ";
                    {
                      // сохранить и восстановить формат потока
                      const std::ios_base::fmtflags FLAGS(std::cout.flags());
                      auto old_fill = std::cout.fill();
                      std::cout << std::hex << std::uppercase
                                << std::setfill('0');
                      for (size_t i = 0;
                           i < static_cast<size_t>(field_.m_read_count |
                                                   field_.get_size());
                           ++i) {
                        uint8_t byte = *(field_.begin() + i);
                        // продвигаем к целочисленному типу, чтобы не печаталось
                        // как char
                        const auto TO_PRINT = static_cast<unsigned>(byte);
                        // печатаем как 0xNN
                        std::cout << " 0x" << std::setw(2) << TO_PRINT;
                        if (i + 1 < static_cast<size_t>(field_.m_read_count)) {
                          std::cout << ' ';
                        }
                      }
                      // вернуть формат
                      std::cout.flags(FLAGS);
                      std::cout.fill(old_fill);
                    }
                    std::cout << '\n';
                    return MatchStatus::NOT_MATCH;
                  };

                  std::cout << "-------------BROKEN PACKET START-------------"
                            << '\n';
                  for (int i = 0; i <= (int)this->m_field_index; i++) {
                    static_for_index(i, FUNC);
                  }
                  std::cout << "-------------BROKEN PACKET STOP-------------"
                            << '\n';
                }
              }
              this->reset();
            } else if (result == MatchStatus::MATCH) {
              this->m_offsets[field.BASE] = field.m_size + field.get_offset();
              ++this->m_field_index;
              if (this->m_field_index >= this->SIZE) {
                for (int i = static_cast<int>(receive_callbacks_.size()) - 1;
                     i >= 0; --i) {
                  auto callback =
                      receive_callbacks_[i].lock();  // shared_ptr или nullptr
                  if (!callback) {
                    receive_callbacks_.erase(receive_callbacks_.begin() + i);
                  } else {
                    (*callback)(*this);  // если callback — функция/функтор
                  }
                }
                this->reset();
              }
            }
            PTR = PTR.subspan(READ);
            return result;
          },
          ptr, read);
    }
    //            return result;
  }

  /**
   * @brief Reads bytes for a specific field, validates const values and copies
   * into the frame buffer.
   * @tparam Index Index of the field within the tuple.
   * @param ptr  [in,out] Remaining input span; consumed bytes are accounted via
   * @p read.
   * @param read [out]    Number of bytes consumed for this field step.
   * @return PROCESSING if the field is incomplete; MATCH if fully read (and
   * matcher passed or absent); NOT_MATCH on mismatch (const value check or
   * matcher failure).
   *
   * @details
   *  - Honors FieldFlags::REVERSE for endianness when reading constants and
   * storing bytes.
   *  - Invokes `matcher_` of the field when it completes, enabling
   * length/type/CRC checks to run early.
   */
  template <size_t Index>
  auto fill_fields(CustomSpan<uint8_t>& ptr, size_t& read) -> MatchStatus {
    auto& field = std::get<Index>(this->m_fields);
    if (field.get_size() == 0) {
      return MatchStatus::MATCH;
    }
    size_t byte_to_read =
        std::min(ptr.size(), field.get_size() - field.m_read_count);
    if (FieldTraits<decltype(field)>::CONST_VALUE != nullptr) {
      if constexpr (has_flag(std::remove_reference_t<decltype(field)>::FLAGS,
                             FieldFlags::REVERSE)) {
        for (int i = 0; i < static_cast<int>(byte_to_read); i++) {
          if (ptr[i] != field.CONST_VALUE[field.get_size() - 1 -
                                          field.m_read_count - i]) {
            ++read;
            if (this->is_debug()) {
            }
            return MatchStatus::NOT_MATCH;
          }
        }
      } else {
        if (std::memcmp(ptr.data(),
                        (uint8_t*)field.CONST_VALUE + field.m_read_count,
                        byte_to_read) != 0) {
          ++read;
          if (this->is_debug()) {
          }
          return MatchStatus::NOT_MATCH;
        }
      }
    }
    read += byte_to_read;
    if constexpr (has_flag(std::remove_reference_t<decltype(field)>::FLAGS,
                           FieldFlags::REVERSE)) {
      for (int i = 0; i < static_cast<int>(byte_to_read); i++) {
        uint8_t* data = (field.BASE + field.m_offset + field.get_size() - 1) -
                        field.m_read_count - i;
        *data = ptr.data()[i];
      }
    } else {
      std::memcpy(field.BASE + field.m_offset + field.m_read_count, ptr.data(),
                  byte_to_read);
    }
    field.m_read_count += byte_to_read;

    if (field.m_read_count < field.m_size) {
      return MatchStatus::PROCESSING;
    }
    if (field.m_matcher) {
      return field.m_matcher(static_cast<void*>(this));
    }
    field.m_read_count = 0;
    return MatchStatus::MATCH;
  }

  /**
   * @brief Adjusts DATA_FIELD size based on LEN_FIELD and sizes of fields
   * marked IS_IN_LEN.
   * @param obj Opaque pointer to this container (provided by FillFields).
   * @return MATCH on successful adjustment/validation, NOT_MATCH if declared
   * size disagrees.
   *
   * @note This matcher allows DATA_FIELD to be `kAnySize` (dynamic). When
   * DATA_FIELD has a fixed size, the method validates that LEN_FIELD matches
   * the expected size.
   */
  static auto set_data_len(void* obj) -> MatchStatus {
    auto& container = *static_cast<RxContainer<Fields>*>(obj);
    auto& data_field = container.template get<FieldName::DATA_FIELD>();
    auto& len_field = container.template get<FieldName::LEN_FIELD>();
    auto len = *len_field.get_ptr();

    container.for_each_type([&](auto& field) {
      if (field.NAME != FieldName::DATA_FIELD) {
        if (has_flag(field.FLAGS, FieldFlags::IS_IN_LEN)) {
          len -= field.m_size;
        }
      }
    });

    if constexpr (RxContainer<Fields>::template has_field<
                      FieldName::DATA_FIELD>()) {
      if (data_field.m_size != 0 && data_field.m_size != K_ANY_SIZE) {
        if (len != data_field.get_size()) {
          if (container.is_debug()) {
            auto expected =
                static_cast<unsigned>(*len_field.get_ptr()) +
                (data_field.get_size() - static_cast<unsigned>(len));

            const std::ios_base::fmtflags FLAGS(
                std::cout.flags());  // сохранить формат

            std::cout << "\nMismatch in length field (method SetDataLen):\n"
                      << "  Expected: " << std::dec
                      << static_cast<unsigned>(expected) << " (0x" << std::hex
                      << std::uppercase << static_cast<unsigned>(expected)
                      << ")\n"
                      << "  Received: " << std::dec
                      << static_cast<unsigned>(*len_field.get_ptr()) << " (0x"
                      << std::hex << std::uppercase
                      << static_cast<unsigned>(*len_field.get_ptr()) << ")\n";

            std::cout.flags(FLAGS);  // восстановить
          }
          return MatchStatus::NOT_MATCH;
        }
        data_field.m_size = len;
        return MatchStatus::MATCH;
      }
    }

    data_field.m_size = len;
    return MatchStatus::MATCH;
  }

  /**
   * @brief Validates anti-length (ALEN) against LEN (bitwise NOT).
   * @param obj Opaque pointer to this container.
   * @return MATCH if `~ALEN == LEN`, otherwise NOT_MATCH.
   */
  static auto check_alen(void* obj) -> MatchStatus {
    auto& container = *static_cast<RxContainer<Fields>*>(obj);
    auto len = *container.template get<FieldName::LEN_FIELD>().get_ptr();
    auto alen = *container.template get<FieldName::ALEN_FIELD>().get_ptr();

    alen = ~alen;

    bool result = len == alen;
    if (container.is_debug() && not result) {
      auto to_uint = [](const auto VAL) { return static_cast<unsigned>(VAL); };

      const auto COUT_FLAGS = std::cout.flags();
      const auto FILL = std::cout.fill();

      std::cout << "\nMismatch in ALEN field:\n"
                << "  Expected: " << std::dec << to_uint(~len) << " (0x"
                << std::hex << std::uppercase << std::setw(2)
                << std::setfill('0') << to_uint(~len) << ")\n"
                << "  Received: " << std::dec << to_uint(~alen) << " (0x"
                << std::hex << std::uppercase << std::setw(2)
                << std::setfill('0') << to_uint(~alen) << ")\n";

      std::cout.flags(COUT_FLAGS);
      std::cout.fill(FILL);
    }
    return result ? MatchStatus::MATCH : MatchStatus::NOT_MATCH;
  }

  /**
   * @brief Computes CRC over fields flagged IS_IN_CRC and compares it with
   * CRC_FIELD.
   * @param obj Opaque pointer to this container.
   * @return MATCH if the computed CRC equals the value in CRC_FIELD; NOT_MATCH
   * otherwise.
   *
   * @requirements TCrc must expose:
   *   - void Reset();
   *   - uint32_t Append(uint32_t, Span<uint8_t>);
   *
   * @remarks The CRC width is inferred from the CRC_FIELD storage type when
   * pretty-printing debug values.
   */
  static auto check_crc(void* obj) -> MatchStatus {
    auto& container = *static_cast<RxContainer<Fields, TCrc>*>(obj);
    auto crc_in_field =
        *container.template get<FieldName::CRC_FIELD>().get_ptr();
    using crc_type = decltype(crc_in_field);
    uint32_t crc = 0;
    container.m_crc.reset();

    container.for_each_type([&](auto& field) {
      using field_type = std::remove_reference_t<decltype(field)>;
      if constexpr (has_flag(field_type::FLAGS, FieldFlags::IS_IN_CRC)) {
        auto* data = field.get_ptr();
        size_t size = field.get_size();
        crc = container.m_crc.append(crc, {(uint8_t*)data, size});
      }
    });

    bool result = crc_in_field == static_cast<decltype(crc_in_field)>(crc);
    if (container.is_debug() && not result) {
      const auto FLAGS = std::cout.flags();  // сохранить текущие флаги
      const auto FILL = std::cout.fill();    // и текущий fill-символ

      auto to_u = [](const auto VAL) -> uint64_t {
        return static_cast<uint64_t>(VAL);
      };
      const crc_type CRC = -1;
      const auto CRC_EXP = to_u(crc & CRC);
      const auto CRC_GOT = to_u(crc_in_field & CRC);

      // ширина HEX по размеру исходного типа (если crc — uint16_t, будет 4;
      // если uint32_t — 8)
      constexpr int HEXW = sizeof(crc) * 2;

      std::cout << "\nMismatch in CRC field:\n"
                << "  Expected: " << std::dec << CRC_EXP << " (0x" << std::hex
                << std::uppercase << std::setw(HEXW) << std::setfill('0')
                << CRC_EXP << ")\n"
                << "  Received: " << std::dec << CRC_GOT << " (0x" << std::hex
                << std::uppercase << std::setw(HEXW) << std::setfill('0')
                << CRC_GOT << ")\n";

      std::cout.flags(FLAGS);  // восстановить
      std::cout.fill(FILL);
    }
    return result ? MatchStatus::MATCH : MatchStatus::NOT_MATCH;
  }

  /**
   * @brief Updates DATA_FIELD alternative by received TYPE_FIELD and validates
   * its expected size.
   * @param obj Opaque pointer to this container.
   * @return MATCH if type is known and sizes match (or are dynamic), NOT_MATCH
   * otherwise.
   *
   * @details For data-field variants, SetId(type) selects the concrete
   * alternative. When DATA_FIELD exposes a fixed size for the selected
   * alternative, a mismatch is treated as a protocol error and the frame is
   * rejected.
   */
  static auto check_type(void* obj) -> MatchStatus {
    auto& container = *static_cast<RxContainer<Fields>*>(obj);

    int type = *container.template get<FieldName::TYPE_FIELD>().get_ptr();
    auto& data_field = container.template get<FieldName::DATA_FIELD>();

    if constexpr (IsDataFieldPrototype<decltype(data_field)>::value) {
      if (not data_field.set_id(type)) {
        if (container.is_debug()) {
          const auto FLAGS = std::cout.flags();  // сохранить флаги
          const auto FILL = std::cout.fill();    // и символ заполнения
          auto to_u = [](auto VAL) -> uint64_t {
            return static_cast<uint64_t>(VAL);
          };
          const auto TYPE_U = to_u(type);
          std::cout << "\n---------------------------\n"
                    << "Incorrect type received (method CheckType):\n"
                    << "  Received type id: " << std::dec << TYPE_U
                    << "\n---------------------------\n";

          std::cout.flags(FLAGS);  // восстановить формат флагов
          std::cout.fill(FILL);    // восстановить fill-символ
        }
        return MatchStatus::NOT_MATCH;
      }
    }
    size_t packet_size = data_field.get_size();
    if (packet_size != K_ANY_SIZE) {
      if (data_field.m_size != 0 && data_field.m_size != packet_size) {
        if (container.is_debug()) {
          const auto FLAGS = std::cout.flags();  // сохранить флаги
          const auto FILL = std::cout.fill();    // и символ заполнения

          auto to_u = [](const auto VAL) -> uint64_t {
            return static_cast<uint64_t>(VAL);
          };

          const auto TYPE_U = to_u(type);
          const auto EXPECT_U = to_u(packet_size);
          const auto GOT_U = to_u(data_field.m_size);

          // ширина HEX по размеру size_t (подходит для размеров буферов)
          constexpr int HEXW = sizeof(size_t) * 2;

          std::cout << "\n---------------------------\n"
                    << "Mismatch in data field size (method CheckType):\n"
                    << "  Received type id: " << std::dec << TYPE_U << " (0x"
                    << std::hex << std::uppercase << std::setw(HEXW)
                    << std::setfill('0') << TYPE_U << ")\n"
                    << "  Expected size:    " << std::dec << EXPECT_U << " (0x"
                    << std::hex << std::uppercase << std::setw(HEXW)
                    << std::setfill('0') << EXPECT_U << ")\n"
                    << "  Calculated size:  " << std::dec << GOT_U << " (0x"
                    << std::hex << std::uppercase << std::setw(HEXW)
                    << std::setfill('0') << GOT_U << ")\n"
                    << "---------------------------\n";

          std::cout.flags(FLAGS);  // восстановить формат флагов
          std::cout.fill(FILL);    // восстановить fill-символ
        }
        return MatchStatus::NOT_MATCH;
      }
      data_field.m_size = packet_size;

      return MatchStatus::MATCH;
    }
    return MatchStatus::NOT_MATCH;
  }

  /**
   * @brief Returns the current DATA_FIELD size (may be dynamic).
   */
  [[nodiscard]] auto get_size() const -> size_t {
    return this->template Get<FieldName::DATA_FIELD>().GetSize();
  }
  /**
   * @brief Subscribes to notifications when a full frame is parsed.
   * @param callback Callable of signature `void(RxContainer&)`.
   * @return A shared_ptr token to keep the subscription alive. Drop it to
   * auto-unsubscribe.
   * @thread_safety Callbacks execute synchronously from Fill(). Avoid
   * long-running work inside.
   */
  [[nodiscard]] auto add_receive_callback(CallbackType callback) -> Delegate {
    auto result = std::make_shared<CallbackType>(callback);
    receive_callbacks_.push_back(result);
    return result;
  }

 private:
  /**
   * @brief Weak list of subscribers. Expired entries are cleaned up on each
   * frame completion.
   */
  std::vector<std::weak_ptr<CallbackType>> receive_callbacks_;
};
}  // namespace proto

/**
 * @section RxContainer_Improvements Potential improvements
 * @par Backpressure / flow control
 *   Consider returning a struct from Fill() that reports total bytes consumed
 * and whether a frame callback was fired, which can help upstream code pace
 * input.
 * @par Error policy hooks
 *   Provide a user hook to observe NOT_MATCH reasons with an enum, instead of
 * relying solely on stdout.
 * @par Partial-frame timeouts
 *   Optionally track timestamps and reset if a field remains incomplete for too
 * long.
 * @par Debug printer abstraction
 *   Replace direct std::cout usage with an injected logger to allow unit tests
 * to intercept logs and to cut I/O in production builds without ifdefs.
 * @par Iterator-friendly Fill
 *   Overload Fill(begin,end) to consume from arbitrary containers without
 * building a Span first.
 * @par Small-buffer optimization
 *   For tiny frames, consider an internal scratch buffer to reduce memcpy for
 * constant prefix checks.
 */