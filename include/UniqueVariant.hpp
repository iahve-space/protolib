#pragma once
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>
#include <cstddef>

namespace proto::meta {

// ---------- typelist ----------
    template<class... Ts>
    struct typelist {};

// contains
    template<class List, class T>
    struct tl_contains;

    template<class T>
    struct tl_contains<typelist<>, T> : std::false_type {};

    template<class T, class... Ts>
    struct tl_contains<typelist<T, Ts...>, T> : std::true_type {};

    template<class U, class... Ts, class T>
    struct tl_contains<typelist<U, Ts...>, T> : tl_contains<typelist<Ts...>, T> {};

// push_back
    template<class List, class T>
    struct tl_push_back;

    template<class... Ts, class T>
    struct tl_push_back<typelist<Ts...>, T> { using type = typelist<Ts..., T>; };

// unique
    template<class List>
    struct tl_unique;

    template<>
    struct tl_unique<typelist<>> { using type = typelist<>; };

    template<class T, class... Ts>
    struct tl_unique<typelist<T, Ts...>> {
    private:
        using tail_unique = typename tl_unique<typelist<Ts...>>::type;
    public:
        using type = std::conditional_t<
                tl_contains<tail_unique, T>::value,
                tail_unique,
                typename tl_push_back<tail_unique, T>::type
        >;
    };

// index_of (жёсткая ошибка, если не нашли)
    template<class List, class T>
    struct tl_index_of;

    template<class T, class... Ts>
    struct tl_index_of<typelist<T, Ts...>, T>
            : std::integral_constant<std::size_t, 0> {};

    template<class U, class... Ts, class T>
    struct tl_index_of<typelist<U, Ts...>, T>
            : std::integral_constant<std::size_t, 1 + tl_index_of<typelist<Ts...>, T>::value> {};

    template<class T>
    struct tl_index_of<typelist<>, T> {
        static_assert(!std::is_same_v<T, T>, "tl_index_of: type not found in list");
    };

// to_variant
    template<class List>
    struct tl_to_variant;

    template<class... Ts>
    struct tl_to_variant<typelist<Ts...>> {
        using type = std::variant<std::monostate, Ts...>;
    };

// ---------- нормализация и преобразования ----------
    template<class T>
    using normalize_t = std::remove_cv_t<std::remove_reference_t<T>>;

// указатель -> std::vector<elem>
    template<class T> struct ptr_to_vector { using type = T; };
    template<class U> struct ptr_to_vector<U*> { using type = std::vector<normalize_t<U>>; };
    template<class T>
    using ptr_to_vector_t = typename ptr_to_vector<T>::type;

// tuple<PacketInfo<id, Type>...> -> typelist< transformed(Type)... >
    template<class PACKETS> struct packets_to_typelist;

    template<class... Pairs>
    struct packets_to_typelist<std::tuple<Pairs...>> {
        using type = typelist< ptr_to_vector_t< normalize_t<typename Pairs::type> >... >;
    };

// ---------- фасад ----------
    template<class PACKETS>
    struct UniqueVariant {
        using all_payload_types    = typename packets_to_typelist<PACKETS>::type;
        using unique_payload_types = typename tl_unique<all_payload_types>::type;

        // список полезных типов (без дублей)
        using list = unique_payload_types;

        // индекс типа в произвольном typelist-е
        template<class List, class T>
        using index_of = tl_index_of<List, T>;

// сам variant (всегда с std::monostate первым)
        using type = typename tl_to_variant<unique_payload_types>::type;

// тип после нормализации и преобразования указателя -> vector
        template<class T>
        using transformed_t = ptr_to_vector_t<normalize_t<T>>;

// индекс по исходному типу пакета (автоматически применяет transformed_t)
        template<class T>
        using index_of_transformed = tl_index_of<unique_payload_types, transformed_t<T>>;

// удобный индекс альтернативы для уже-нормализованного типа T (смещение на 1 из-за monostate)
        template<class T>
        static constexpr std::size_t alt_index =
                1 + tl_index_of<unique_payload_types, normalize_t<T>>::value;

// удобный индекс альтернативы для ИСХОДНОГО типа пакета T (автоматически применяет transformed_t)
        template<class T>
        static constexpr std::size_t alt_index_transformed =
                1 + tl_index_of<unique_payload_types, transformed_t<T>>::value;
    };

} // namespace proto::meta