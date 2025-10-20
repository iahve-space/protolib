/**
 * @file TxContainer.hpp
 * @brief TX-side container that builds protocol frames from a set of fields.
 *
 * The TxContainer class template assembles a frame from fields, applies
 * matchers (length, ALEN, and CRC), and then writes field-sized byte ranges
 * to an external output interface.
 *
 * Important:
 *  - The class does not own the transmission interface (it only stores a raw
 *    pointer) and it does not buffer the whole frame at once — data is written
 *    field-by-field in declaration order. This keeps RAM usage low on MCUs and
 *    avoids unnecessary copies.
 *  - Public method signatures are frozen for release. Only comments and
 *    documentation are provided here.
 */

#pragma once

#include <tuple>
#include <algorithm>
#include <unordered_map>
#include <cstring>
#include <type_traits>

#include "prototypes/field/FieldInfo.hpp"
#include "prototypes/container/FieldContainer.hpp"
#include "CustomSpan.hpp"
#include "Interface.hpp"

namespace proto
{
    /**
     * @tparam Fields  Tuple of field types (std::tuple<...>) compatible with FieldContainer.
     * @tparam TCrc    CRC implementation type (CrcSoft by default). Must provide
     *                 `Reset()` and `Append(uint32_t, Span<uint8_t>)`.
     *
     * The class forms a frame from fields. Some fields may contribute to the
     * logical length (IS_IN_LEN) and to the checksum (IS_IN_CRC).
     *
     * Usage example:
     * @code{.cpp}
     * TxContainer<MyFields, CrcSoft> tx;
     * tx.SetInterface(uart);
     * tx.SendPacket(
     *     proto::MakeFieldInfo<proto::FieldName::TYPE_FIELD>(&type),
     *     proto::MakeFieldInfo<proto::FieldName::DATA_FIELD>(&payload, payload_size)
     * );
     * @endcode
     *
     * @par Thread safety
     * Not thread-safe. Synchronization is the caller's responsibility.
     *
     * @note Frames are emitted in field-order, as multiple small writes. If you
     *       need a contiguous buffer for testing/diagnostics, consider adding a
     *       separate buffering path at a higher level (not part of this class).
     *
     * @warning The interface pointer must remain valid for the lifetime of the
     *          container; ownership is external.
     */
    template<typename Fields, typename TCrc = CrcSoft>
    class TxContainer : public FieldContainer<Fields, TCrc>
    {
    public:
        /**
         * @brief Binds default matchers to LEN/ALEN/CRC (if those fields exist) and
         *        performs an initial Reset().
         */
        TxContainer(){
            if((*this).template HasField<FieldName::LEN_FIELD>() && (*this).template Get<FieldName::LEN_FIELD>().matcher_ == nullptr){
                (*this).template Get<FieldName::LEN_FIELD>().matcher_ = &TxContainer::CalcLen;
            }
            if((*this).template HasField<FieldName::ALEN_FIELD>() && (*this).template Get<FieldName::ALEN_FIELD>().matcher_ == nullptr){
                (*this).template Get<FieldName::ALEN_FIELD>().matcher_ = &TxContainer::SetALen;
            }
            if((*this).template HasField<FieldName::CRC_FIELD>() && (*this).template Get<FieldName::CRC_FIELD>().matcher_ == nullptr){
                (*this).template Get<FieldName::CRC_FIELD>().matcher_ = &TxContainer::SetCrc;
            }
            this->Reset();
        }
        /**
         * @brief Build and send a packet, setting field values from a pack of FieldInfo.
         *
         * The method resets internal state and then:
         *  - applies sizes to fields provided via FieldInfo;
         *  - if TYPE_FIELD/DATA_FIELD are present and TYPE is not provided explicitly,
         *    infers the packet ID from the concrete data type of DATA_FIELD and sets
         *    both TYPE and DATA size accordingly;
         *  - computes offsets, applies constants and matchers (LEN/ALEN/CRC);
         *  - writes each field's byte span to the configured IInterface.
         *
         * @tparam Infos  Variadic pack of FieldInfo<...> for different fields.
         * @param  infos  Values and sizes for individual fields.
         * @return Total number of bytes written (sum of all field sizes).
         *
         * @warning An output interface must be set via SetInterface() before calling.
         */
        template<typename... Infos>
        size_t SendPacket(Infos&&... infos) {
            this->Reset();
            auto info_tuple = std::make_tuple(std::forward<Infos>(infos)...);
            using InfoTuple = decltype(info_tuple);


            if constexpr (TxContainer<Fields>::template HasField<FieldName::DATA_FIELD>() &&
                          TxContainer<Fields>::template HasField<FieldName::TYPE_FIELD>()){
                auto& data_field = this->template Get<FieldName::DATA_FIELD>();

                if constexpr ( FieldInfoHasName<FieldName::TYPE_FIELD, InfoTuple>()){
                    auto &type_info = GetFieldInfoByName< FieldName::TYPE_FIELD>(info_tuple);
                    data_field.SetId(*type_info.data);
                }

                if constexpr ( FieldInfoHasName<FieldName::DATA_FIELD, InfoTuple>() && is_data_field_prototype<decltype(data_field)>::value) {
                    auto &type_field = this->template Get<FieldName::TYPE_FIELD>();
                    auto &data_info = GetFieldInfoByName< FieldName::DATA_FIELD>(info_tuple);
                    using DataType = typename std::remove_reference_t<decltype(data_info)>::Type;
                    if constexpr (not FieldInfoHasName<FieldName::TYPE_FIELD, InfoTuple>()){
                        int packet_id = data_field.template GetNumber<DataType>();
                        data_field.SetId(packet_id);
                        data_field.template SetSize<DataType>();
                        auto type_info = MakeFieldInfo<proto::FieldName::TYPE_FIELD>(&packet_id);
                        auto expanded_tuple = std::tuple_cat(info_tuple, std::make_tuple(type_info));
                        return ConstructPacketFromTuple(expanded_tuple);
                    }
                }
            }
            return ConstructPacket(std::forward<Infos>(infos)...);
        }
        /**
         * @brief Reset the internal state of the container and its fields.
         *        Delegates to FieldContainer.
         */
        void Reset() override {
            FieldContainer<Fields, TCrc>::Reset();
        }
        /**
         * @brief Assign an output interface (non-owning).
         * @param interface  Object implementing Write(Span<uint8_t>).
         *
         * @note The class stores only a raw pointer. The caller must ensure the
         *       interface outlives the container.
         */
        void SetInterface(interface::IInterface &interface){
            interface_ = &interface;
        }
    private:
        /**
         * @brief Internal frame builder from a tuple of FieldInfo.
         *
         * Steps:
         *  1) For fields present in the tuple, fixes their size via SetSize<>()
         *  2) For each field in order: sets offset, applies data/const/matcher
         *  3) Accumulates total size and writes each field span to the interface
         *
         * @tparam InfoTuple  std::tuple<FieldInfo<...>, ...> with any subset of fields.
         * @param  info_tuple  Tuple of FieldInfo arguments.
         * @return Total frame size in bytes.
         *
         * @note Emission is field-by-field. This intentionally avoids allocating a
         *       contiguous buffer inside the container.
         */
        template<typename InfoTuple>
        size_t ConstructPacketImpl(InfoTuple const& info_tuple) {


            ForEachInfo(info_tuple, [&](auto const& info) {
                using InfoT = std::decay_t<decltype(info)>;   // снимаем ссылки/const
                auto& field = this->template Get<InfoT::name>();
                field.template SetSize<typename InfoT::Type>(info.size);
            });

            this->for_each_type( [&](auto& field) {
                using FieldType = std::remove_reference_t<decltype(field)>;
                using field_type = typename std::remove_reference<decltype(field)>::type;
                field.SetOffset(this->offsets[field.base_]);
                if constexpr (FieldInfoHasName<FieldType::name_, InfoTuple>()){
                    auto &data_info = GetFieldInfoByName<FieldType::name_>(info_tuple);
                    field.Set((void*)data_info.data);
                }
                if (field.matcher_ != nullptr) {
                    field.matcher_(this);
                } else if (field_type::const_value_ != nullptr) {
                    field.ApplyConst();
                }
                this->offsets[field.base_] += field.size_;
            });
            size_t total_size = 0;
            this-> for_each_type([&](auto& field){
                total_size+= field.GetSize();
                if(this->IsDebug()){
                    field.Print();
                }
                if(interface_){
                    interface_->Write({field.begin(), field.GetSize()});
                }
            });
            return total_size;
        }
        /**
         * @brief Convenience wrapper: packs var-args FieldInfo into a tuple and builds.
         */
        template<typename... Infos>
        size_t ConstructPacket(Infos&&... infos) {
            auto info_tuple = std::make_tuple(std::forward<Infos>(infos)...);
            return ConstructPacketImpl<decltype(info_tuple)>(info_tuple);
        }
        /**
         * @brief Build a frame when FieldInfo are already in a tuple.
         */
        template<typename... Ts>
        size_t ConstructPacketFromTuple(const std::tuple<Ts...>& info_tuple) {
            return ConstructPacketImpl(info_tuple);
        }
        /**
         * @brief Legacy overload kept for compatibility (void result).
         * @note Prefer the size-returning overload.
         */
        // Overload to handle when a tuple is passed directly (to avoid nested tuple)
        template <typename... Ts>
        void ConstructPacket(const std::tuple<Ts...>& info_tuple) {
            ConstructPacketImpl(info_tuple);
        }
        /**
         * @brief Output interface (non-owning).
         * @warning Must be set before sending packets.
         */
        interface::IInterface *interface_{};

        /**
         * @brief Compute LEN_FIELD as the sum of sizes of fields marked with IS_IN_LEN.
         * @return MatchStatus::MATCH
         */
        static MatchStatus CalcLen(void* obj) {
            auto& container = *static_cast<TxContainer<Fields>*>(obj);
            auto& len_field = container.template Get<FieldName::LEN_FIELD>();
            using LenFieldType = typename std::remove_reference_t<decltype(len_field)>::FieldType;
            LenFieldType len = 0;
            container.for_each_type([&](auto& field){
                using FieldType = std::remove_reference_t<decltype(field)>;
                if constexpr (HasFlag(FieldType::flags_, FieldFlags::IS_IN_LEN)) {
                    len += field.size_;
                }
            });
            len_field.Set(len);
            return MatchStatus::MATCH;
        }
        /**
         * @brief Set ALEN as bitwise negation of LEN.
         * @return MatchStatus::MATCH
         */
        static MatchStatus SetALen(void* obj) {
            auto& container = *static_cast<TxContainer<Fields>*>(obj);
            auto& len_field = container.template Get<FieldName::LEN_FIELD>();
            if constexpr (TxContainer<Fields>::template HasField<FieldName::ALEN_FIELD>()){
                auto& alen_field = container.template Get<FieldName::ALEN_FIELD>();
                alen_field.Set(~(*len_field.GetPtr()));
            }
            return MatchStatus::MATCH;
        }
        /**
         * @brief Compute CRC over fields marked with IS_IN_CRC and write it into CRC_FIELD.
         *        The accumulator implementation is provided by TCrc.
         * @return MatchStatus::MATCH
         *
         * @remark TCrc must expose `Reset()` and
         *         `Append(uint32_t, Span<uint8_t>) -> uint32_t`.
         */
        static MatchStatus SetCrc(void *obj){
            auto& container = *static_cast<TxContainer<Fields, TCrc>*>(obj);
            auto& crc_field = container.template Get<FieldName::CRC_FIELD>();
            container.crc_.Reset();
            using crc_type =typename std::remove_reference_t<decltype(crc_field)>::FieldType;
            uint32_t crc = 0;
            container.for_each_type([&](auto& field){
                using FieldType = std::remove_reference_t<decltype(field)>;
                if constexpr (HasFlag(FieldType::flags_, FieldFlags::IS_IN_CRC)) {
                    auto *data = field.GetPtr();
                    size_t size = field.GetSize();
                    crc = container.crc_.Append(crc, {(uint8_t*)data, size});
                }
            });
            crc_field.Set(crc);
            return MatchStatus::MATCH;
        }
    };
}