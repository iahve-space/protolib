#pragma once
/**
 * @file DataField.hpp
 * @brief Variant DATA field that maps protocol IDs to payload types at compile time.
 *
 * Provides:
 *  - @ref proto::PacketInfo : compile-time (ID, Type) binding;
 *  - @ref proto::DataFieldPrototype : field that selects one payload type by runtime ID;
 *  - @ref proto::MakePacketVariant : helper to generate `std::variant` of all possible payload types;
 *  - @ref proto::is_data_field_prototype : trait to detect DataFieldPrototype types.
 *
 * ## Example
 * @code{.cpp}
 * using Map = std::tuple<
 *     proto::PacketInfo<1, MyHeader>,
 *     proto::PacketInfo<2, std::vector<uint8_t>>,
 *     proto::PacketInfo<3, proto::EmptyDataType>
 * >;
 *
 * uint8_t buffer[1024]{};
 * using DataField = proto::DataFieldPrototype<Map, buffer, proto::FieldFlags::NOTHING>;
 *
 * DataField field;
 * field.SetId(1);
 * auto variant = field.GetVariant();
 * if (auto* h = std::get_if<MyHeader>(&variant)) {
 *     // process header
 * }
 * @endcode
 */

#include <tuple>
#include <type_traits>
#include <cstring>
#include <vector>

#include "prototypes/field/FieldPrototype.hpp"
#include "UniqueVariant.hpp"

namespace proto {

    /**
     * @brief Compile-time information about a packet type bound to a numeric ID.
     *
     * @tparam Id         Protocol ID (unique in PACKETS tuple).
     * @tparam PacketType Associated payload type.
     */
    template<size_t Id, typename PacketType>
    struct PacketInfo {
        static constexpr size_t id = Id;   //!< Protocol ID.
        using type = PacketType;           //!< Associated payload type.
    };


    /**
     * @brief Field prototype representing a DATA field that can contain
     *        one of multiple packet types selected by ID.
     *
     * @tparam PACKETS  std::tuple of PacketInfo entries describing available payloads.
     * @tparam BASE     Base pointer to shared buffer.
     * @tparam FLAGS    Field flags.
     * @tparam MAX_SIZE Maximum size in bytes (applies to pointer payloads).
     */
    template<typename PACKETS, uint8_t *BASE, FieldFlags FLAGS, size_t MAX_SIZE = 4096>
    class DataFieldPrototype
            : public FieldPrototype<FieldName::DATA_FIELD, uint8_t*, BASE, FLAGS, MAX_SIZE>
    {
        template<typename Fields, typename TCrc> friend class FieldContainer;
        template<typename Fields, typename TCrc> friend class RxContainer;
        template<typename Fields, typename TCrc> friend class TxContainer;

    public:
        static constexpr bool IS_DATA_FIELD{true}; //!< Trait tag.
        using Packets = PACKETS;                   //!< Tuple of PacketInfo.
        using FieldType = typename meta::UniqueVariant<PACKETS>::type; //!< Variant with all payload alternatives.

        /**
         * @brief Set active packet ID.
         * @param id Protocol ID.
         * @return true if id exists in PACKETS, false otherwise.
         */
        bool SetId(int id) {
            bool valid = false;
            std::apply([&](auto&&... packet) {
                ((packet.id == id ? (void)(valid = true, this->template SetSize<typename std::decay_t<decltype(packet)>::type>()) : (void)0), ...);
            }, Packets{});
            if (valid) current_id_ = id;
            return valid;
        }

        /**
         * @brief Get current packet size in bytes.
         * @return Size or proto::kAnySize if unresolved.
         */
        [[nodiscard]] size_t GetSize() const override {
            return PacketSize(current_id_) ;
        }

        /**
         * @brief Get current value as std::variant (copy).
         *
         * - Returns std::monostate if no ID is set or ID not found.
         * - Returns value of type T (copy for fixed-size types, pointer for pointer payloads).
         * - EmptyDataType → std::monostate.
         *
         * @return Variant holding current value.
         */
        FieldType GetCopy() const {
            return GetVariantImpl<0>();
        }

        /**
         * @brief Returns true if an ID is set.
         */
        bool HasId() const noexcept { return current_id_ >= 0; }

        /**
         * @brief Get the currently selected ID (or -1 if unset).
         */
        int Id() const noexcept { return current_id_; }

        /**
         * @brief Set active packet by its payload type.
         * @tparam T Payload type present in Packets mapping.
         * @return true if type is found and selected.
         */
        template<typename T>
        bool SetType() {
            constexpr int id = GetNumber<T>();
            static_assert(id != -1, "SetType<T>(): T is not present in PACKETS mapping");
            return SetId(id);
        }

    protected:
        /// Compile-time lookup: tuple index by ID.
        template<int NAME, std::size_t I = 0>
        static constexpr int GetIndex() {
            if constexpr (I >= std::tuple_size_v<Packets>) {
                return -1;
            } else {
                using Current = std::tuple_element_t<I, Packets>;
                if constexpr (NAME == Current::id) {
                    return static_cast<int>(I);
                } else {
                    return GetIndex<NAME, I + 1>();
                }
            }
        }

        /// Compile-time lookup: ID by type.
        template<typename T, std::size_t I = 0>
        static constexpr int GetNumber() {
            if constexpr (I >= std::tuple_size_v<Packets>) {
                return -1;
            } else {
                using Current = std::tuple_element_t<I, Packets>;
                if constexpr (std::is_same_v<T, typename Current::type>) {
                    return static_cast<int>(Current::id);
                } else {
                    return GetNumber<T, I + 1>();
                }
            }
        }

        /// Reset field: clears id and size.
        void Reset() override {
            current_id_ = -1;
            this->size_ = 0;
        }

    private:
        int current_id_{-1}; //!< Currently active protocol ID.

        /// Compute natural size of packet by id.
        [[nodiscard]] size_t PacketSize(int id) const {
            auto result = proto::kAnySize;
            auto handle_one = [&](const auto& pkt) {
                using PktType = typename std::decay_t<decltype(pkt)>::type;
                if (pkt.id == id) {
                    if constexpr (std::is_pointer_v<PktType>) {
                        result = this->size_;
                    } else if constexpr (std::is_same_v<PktType, EmptyDataType>) {
                        result = 0;
                    } else {
                        result = sizeof(PktType);
                    }
                }
            };
            std::apply([&](const auto&... pkts) {
                (void)std::initializer_list<int>{ (handle_one(pkts), 0)... };
            }, Packets{});
            return result;
        }

        /// Recursive helper for GetVariant.
        template<std::size_t I>
        FieldType GetVariantImpl() const {
            if constexpr (I >= std::tuple_size_v<Packets>) {
                return FieldType{}; // monostate alternative (index 0)
            } else {
                using Info = std::tuple_element_t<I, Packets>;
                using T = typename Info::type;
                using N = std::remove_cv_t<std::remove_reference_t<T>>;
                // Alternative index inside FieldType: 1 + position of N in FieldList
                static constexpr std::size_t Alt =
                        meta::UniqueVariant<PACKETS>::template alt_index_transformed<N>;

                if (current_id_ == static_cast<int>(Info::id)) {
                    if constexpr (std::is_pointer_v<T>) {
                        using Elem = std::remove_const_t<std::remove_pointer_t<T>>;
                        const auto* raw = reinterpret_cast<const Elem*>(this->base_ + this->offset_);
                        const std::size_t count = this->GetSize() / sizeof(Elem);
                        std::vector<Elem> vec(count);
                        if (count) {
                            std::memcpy(vec.data(), raw, count * sizeof(Elem));
                        }
                        return FieldType{std::in_place_index<Alt>, std::move(vec)};
                    } else {
                        T value{};
                        std::memcpy(&value, this->base_ + this->offset_, sizeof(T));
                        return FieldType{std::in_place_index<Alt>, value};
                    }
                }
                return GetVariantImpl<I + 1>();
            }
        }
    };

    /// Specialization for DataFieldPrototype in meta code.
    template<typename PACKETS, uint8_t* BASE, FieldFlags FLAGS, size_t MAX_SIZE>
    struct fieldsTuple<proto::DataFieldPrototype<PACKETS, BASE, FLAGS, MAX_SIZE>> {
        using Type = proto::DataFieldPrototype<PACKETS, BASE, FLAGS, MAX_SIZE>;
    };

    /// Trait to detect DataFieldPrototype types.
    template<typename T, typename = void>
    struct is_data_field_prototype : std::false_type {};

    template<typename T>
    struct is_data_field_prototype<
            T,
            std::void_t<decltype(std::remove_cv_t<std::remove_reference_t<T>>::IS_DATA_FIELD)>
    > : std::true_type {};

} // namespace proto