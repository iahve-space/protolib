#pragma once
/**
 * @file FieldPrototype.hpp
 * @brief FieldPrototype template for describing and handling protocol fields.
 *
 * Provides:
 *  - Definition of field metadata (name, flags, size, offset, const values);
 *  - Access to raw field data in a shared buffer;
 *  - Helpers for printing, setting, and resetting field content;
 *  - Traits for compile-time inspection of field properties;
 *  - Support for dynamic field sizes with an upper bound (@ref MAX_SIZE).
 */

#include <cstdint>
#include <cstring>
#include <sstream>
#include <typeindex>
#include <type_traits>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string_view>
#include <limits>

#include "prototypes/field/FieldFlags.hpp"

namespace proto {

    /**
     * @enum MatchStatus
     * @brief Status returned by matcher functions when comparing field values.
     */
    enum class MatchStatus {
        NOT_MATCH,   //!< Field does not match expected value.
        PROCESSING,  //!< Matching is in progress.
        MATCH        //!< Field matches expected value.
    };

    /**
     * @struct EmptyDataType
     * @brief Marker type representing "no data".
     *
     * Used in templates where an optional data type is required.
     */
    struct EmptyDataType {
        bool operator==(const EmptyDataType& other) const{
            (void)other;
            return true;
        }

    };

    /// Special constant meaning "size can be any".
    static constexpr size_t kAnySize = std::numeric_limits<size_t>::max();

    /// Maximum allowed field size in bytes.
//    static constexpr size_t kMaxFieldSize = 2048;

    /// Matcher callback type.
    ///
    /// A matcher inspects the current field buffer and returns @ref MatchStatus.
    /// Signature uses a raw pointer to the field storage; implementers should
    /// cast to the appropriate type for read-only checks. (The pointer is not
    /// owned and must not be stored.)
    using MatcherType = MatchStatus (*)(void*);

    /**
     * @class FieldPrototype
     * @brief Template for protocol field definition and runtime handling.
     *
     * Each field has:
     *  - Name (enum @ref proto::FieldName);
     *  - Type (`T`), which can be a plain type or pointer;
     *  - Shared buffer base pointer (`BASE`);
     *  - Flags (@ref FieldFlags);
     *  - Compile-time or dynamic size;
     *  - Optional constant value (for fixed fields).
     *
     * @tparam NAME         FieldName identifier.
     * @tparam T            Field type (plain type or pointer).
     * @tparam BASE         Base pointer to the shared buffer.
     * @tparam FLAGS        FieldFlags mask.
     * @tparam MAX_SIZE     Hard cap for runtime size (safety limit for dynamic fields).
     * @tparam SIZE         Compile-time size. Use @ref kAnySize for dynamic-size fields.
     * @tparam CONST_VALUE  Optional pointer to a constant value copied on reset/apply.
     * @tparam MATCHER      Optional matcher callback for validation/dispatch.
     */
    template<
            FieldName NAME,
            typename T,
            uint8_t* BASE,
            FieldFlags FLAGS,
            size_t MAX_SIZE =4096,
            size_t SIZE = (std::is_pointer_v<T> ? kAnySize : sizeof(T)),
            typename std::conditional_t<std::is_pointer_v<T>, std::remove_pointer_t<T>, T>* CONST_VALUE = nullptr,
            MatcherType MATCHER = nullptr>
    class FieldPrototype {
        template<typename Fields, typename TCrc>
        friend class FieldContainer;
        template<typename Fields, typename TCrc>
        friend class RxContainer;
        template<typename Fields, typename TCrc>
        friend class TxContainer;

    public:
        /// Resolved type (dereferenced if T is a pointer).
        using FieldType = typename std::conditional_t<
                std::is_pointer_v<T>,
                std::remove_pointer_t<T>,
                T>;

        /// Compile-time field metadata.
        static constexpr FieldName name_{NAME};
        static constexpr FieldFlags flags_{FLAGS};
        static constexpr uint8_t* const base_{BASE};
        static constexpr FieldType* const const_value_{(FieldType*)CONST_VALUE};
//        static constexpr size_t max_size_{kMaxFieldSize};

        // Disable copying to ensure uniqueness.
        FieldPrototype(FieldPrototype&&) = delete;
        FieldPrototype(const FieldPrototype&) = delete;
        FieldPrototype& operator=(FieldPrototype&&) = delete;
        FieldPrototype& operator=(const FieldPrototype&) = delete;

        /**
         * @brief Construct a field with compile-time or dynamic size.
         *
         * Ensures that fixed-size fields match the provided SIZE.
         */
        FieldPrototype() {
            if constexpr (!std::is_pointer_v<T>) {
                if constexpr (sizeof(T) != SIZE) {
                    static_assert(sizeof(T) == SIZE, "Size of T must match SIZE");
                }
            }
        }

        virtual ~FieldPrototype() = default;

        /// @return Pointer to the start of field data in the shared buffer (const).
        [[nodiscard]] const uint8_t* begin() const { return BASE + offset_; }

        /// @return Pointer one past the end of field data in the shared buffer (const).
        [[nodiscard]] const uint8_t* end() const { return BASE + offset_ + size_; }

        /// @return Current offset (relative to buffer start).
        [[nodiscard]] size_t GetOffset([[maybe_unused]] void* opt = nullptr) const { return offset_; }

        /// @return Current size in bytes (clamped to @ref MAX_SIZE when larger).
        [[nodiscard]] virtual size_t GetSize() const { return size_ < MAX_SIZE? size_:MAX_SIZE; }


        /// @note Internal trait helpers to detect std::vector and derive a copy type.
      // ---- trait: это std::vector ? ----
      template<class> struct is_std_vector : std::false_type {};
      template<class U, class A>
      struct is_std_vector<std::vector<U, A>> : std::true_type {};

      template<class TT>
      static constexpr bool is_std_vector_v = is_std_vector<TT>::value;


        using Elem    = std::remove_pointer_t<T>;        // например, const unsigned char
        using ElemVal = std::remove_const_t<Elem>;          // unsigned char
// ---- ваш тип возвращаемого значения ----
// если поле-указатель (T = X*), вернуть std::vector<X>, иначе — T
      using CopyType = std::conditional_t<
          std::is_pointer_v<T>,
          std::vector<ElemVal>,
          T
      >;

        /**
         * @brief Return a value copy of the field content.
         *
         * If `T` is a pointer type, returns a contiguous `std::vector<ElemVal>`
         * sized from the current byte-length. If `T` is not a pointer, returns
         * a plain value of type `T` (using memcpy for trivially copyable types).
         */
        [[nodiscard]] CopyType GetCopy() const {
          if constexpr (is_std_vector_v<CopyType>) {
            // T — указатель ⇒ возвращаем вектор элементов
            const std::size_t bytes = GetSize();                  // сколько байт в поле
            const std::size_t n     = bytes / sizeof(Elem);       // сколько элементов
            CopyType v;                                            // это std::vector<Elem>
            v.resize(n);
            if (n) {
              // предполагаем, что данные плоские и совместимы по представлению
              std::memcpy(v.data(), GetPtr(), n * sizeof(Elem));
            }
            return v;
          } else {
            // T — не указатель ⇒ вернуть T по значению
            CopyType value{};
            if constexpr (std::is_trivially_copyable_v<CopyType>) {
              // безопасно побайтно
              std::memcpy(&value, GetPtr(), sizeof(CopyType));
            } else {
              // лучше явно сконструировать из байтов, если есть такой конструктор,
              // или написать парсер; memcpy для нетривиальных типов — UB.
              // Пример (если у вас есть подходящий конструктор):
              // value = CopyType(reinterpret_cast<const std::byte*>(GetPtr()), GetSize());
              static_assert(std::is_trivially_copyable_v<CopyType>,
                            "Provide a parser/constructor for non-trivial CopyType");
            }
            return value;
          }
        }
        /// @return Typed pointer to the field storage (read-only view).
        [[nodiscard]] constexpr const FieldType* GetPtr() const {
            return reinterpret_cast<const FieldType*>(BASE + offset_);
        }


        /**
         * @brief Print field contents in a table format.
         *
         * Shows:
         *  - Field name
         *  - Value (hex)
         *  - Size
         *  - Offset
         *  - Inclusion in length / CRC as defined by @ref FieldFlags::IS_IN_LEN and @ref FieldFlags::IS_IN_CRC
         *  - Const value (if defined)
         */
        void Print() {
            std::cout << '\n' << std::string(90, '-') << '\n';
            std::cout << "| " << std::setw(15) << std::left << "FieldName"
                      << " | " << std::setw(24) << "Value (Hex)"
                      << " | " << std::setw(6)  << "Size"
                      << " | " << std::setw(6)  << "Offset"
                      << " | " << std::setw(10) << "Is in len"
                      << " | " << std::setw(10) << "Is in crc" << " |\n";

            using Type = std::decay_t<decltype(*this)>;

            std::ostringstream hex_stream;
            const auto* data = reinterpret_cast<const uint8_t*>(begin());
            for (size_t i = 0; i < GetSize(); ++i) {
                hex_stream << std::hex << std::uppercase
                           << std::setw(2) << std::setfill('0')
                           << static_cast<int>(data[i]) << " ";
            }

            std::cout << "| " << std::setw(15) << ToString(Type::name_)
                      << " | " << std::setw(24) << hex_stream.str()
                      << " | " << std::setw(6) << GetSize()
                      << " | " << std::setw(6) << GetOffset()
                      << " | " << std::setw(10)
                      << (((flags_ & FieldFlags::IS_IN_LEN) != FieldFlags::NOTHING) ? "TRUE" : "FALSE")
                      << " | " << std::setw(10)
                      << (((flags_ & FieldFlags::IS_IN_CRC) != FieldFlags::NOTHING) ? "TRUE" : "FALSE")
                      << " |";

            if (const_value_ != nullptr) {
                std::ostringstream const_stream;
                for (size_t i = 0; i < GetSize(); ++i) {
                    const_stream << std::hex << std::uppercase
                                 << std::setw(2) << std::setfill('0')
                                 << static_cast<int>(reinterpret_cast<const uint8_t*>(const_value_)[i]) << " ";
                }
                std::cout << "\n| " << std::setw(15) << "ConstValue"
                          << " | " << std::setw(24) << const_stream.str()
                          << " |";
            }
        }

    protected:

        /// Runtime state: how many bytes were read into this field.
        size_t read_count_{};
        /// Optional matcher callback for this field.
        MatcherType matcher_ = MATCHER;
        /// Current size (may differ from @ref SIZE if dynamic).
        size_t size_{SIZE};

        /// Offset in the shared buffer.
        size_t offset_{0};

        /**
         * @brief Configure runtime size for dynamic fields.
         *
         * Effective only when `SIZE == kAnySize`. The new size is taken from
         * `size_to_set` when provided, otherwise deduced from `sizeof(DATA)`
         * when `DATA` is not a pointer and not @ref proto::EmptyDataType.
         * The value is clamped by @ref MAX_SIZE. For fields with CONST_VALUE
         * the size cannot be changed.
         *
         * @tparam DATA      Element type used for deduction when size is not provided.
         * @param  size_to_set Byte length to set, or @ref kAnySize to auto-deduce.
         * @return true if size accepted (or unchanged), false if it exceeds @ref MAX_SIZE.
         */
        template<class DATA>
        constexpr bool SetSize(size_t size_to_set = kAnySize) {
            if constexpr (const_value_ != nullptr) {
                static_assert(CONST_VALUE == nullptr, "You can't set size for fields with const_value");
            }
            if constexpr (SIZE == kAnySize) {
                if constexpr (!std::is_same_v<std::remove_pointer_t<DATA>, proto::EmptyDataType>) {
                    constexpr size_t kStaticSize = std::is_pointer_v<DATA> ? 0 : sizeof(DATA);
                    if (size_to_set != kAnySize) {
                        if(size_to_set <= MAX_SIZE){
                            this->size_ = size_to_set;
                        }
                        else{
                            return false;
                        }
                    } else if constexpr (kStaticSize > 0) {
                        this->size_ = kStaticSize;
                    }
                } else {
                    this->size_ = 0;
                }
            }
            return true;
        }

        /// Assign byte offset relative to the shared buffer start.
        void SetOffset(size_t offset) { offset_ = offset; }

        /// Set field value from a typed object; forwards to the void* overload.
        constexpr void Set(const T& value) {
            static_assert(CONST_VALUE == nullptr, "Const value mustn't be provided for Set");
            if constexpr (std::is_pointer_v<T>) {
                Set(static_cast<const void*>(value));
            } else {
                Set(static_cast<const void*>(&value));
            }
        }

        /**
         * @brief Set field value from raw bytes.
         *
         * Copies `GetSize()` bytes into the field storage. When @ref FieldFlags::REVERSE
         * is set, bytes are written in reverse order.
         */
        virtual void Set(const void* value) {
            if constexpr ((FLAGS & FieldFlags::REVERSE) != FieldFlags::NOTHING) {
                const auto* src = static_cast<const uint8_t*>(value);
                for (size_t i = 0; i < GetSize(); ++i) {
                    (BASE + offset_)[i] = src[GetSize() - 1 - i];
                }
            } else {
                std::memcpy(BASE + offset_, value, GetSize());
            }
        }

        /**
         * @brief Copy @ref CONST_VALUE into the storage (if defined), honoring @ref FieldFlags::REVERSE.
         */
        void ApplyConst() {
            if constexpr (CONST_VALUE != nullptr) {
                if constexpr ((FLAGS & FieldFlags::REVERSE) != FieldFlags::NOTHING) {
                    const auto* src = reinterpret_cast<const uint8_t*>(CONST_VALUE);
                    for (size_t i = 0; i < GetSize(); ++i) {
                        (BASE + offset_)[i] = src[GetSize() - 1 - i];
                    }
                } else {
                    std::memcpy(BASE + offset_, CONST_VALUE, GetSize());
                }
            }
        }

        /// Reset runtime state (offset/read_count/size) to defaults.
        virtual void Reset() {
            offset_ = 0;
            read_count_ = 0;
            size_ = SIZE;
        }
    };

    /**
     * @struct FieldTraits
     * @brief Compile-time helper to extract field properties.
     *
     * Provides compile-time mirrors of the field's static metadata so it can be
     * used in type-metaprogramming contexts without an instance.
     */
    template <typename Field>
    struct FieldTraits {
        using Type = typename std::remove_reference<Field>::type;
        static constexpr auto const_value = Type::const_value_;
        static constexpr auto name = Type::name_;
        static constexpr auto flags = Type::flags_;
        static constexpr bool is_const = (Type::const_value_ != nullptr);
    };

    /**
     * @struct fieldsTuple
     * @brief Helper to unwrap a FieldPrototype into a `Type` alias used by container tuples.
     */
    template<typename Field>
    struct fieldsTuple; // base template

    template<FieldName NAME, typename T, uint8_t* BASE, FieldFlags FLAGS, size_t MAX_SIZE, size_t SIZE, auto* CONST_VALUE, MatcherType MATCHER>
    struct fieldsTuple<proto::FieldPrototype<NAME, T, BASE, FLAGS, MAX_SIZE, SIZE, CONST_VALUE, MATCHER>> {
        using Type = FieldPrototype<NAME, T, BASE, FLAGS, MAX_SIZE, SIZE, CONST_VALUE, MATCHER>;
    };

} // namespace proto