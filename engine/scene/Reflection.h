// ============================================================
//  Wojcimierz Engine: scene/Reflection.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once

// Lekka refleksja komponentów. Jeden opis pól służy serializacji scen i prefabów
// oraz inspektorowi edytora.
//
// Użycie w pliku .cpp komponentu (najlepiej tym, który definiuje jego metody,
// bo linker może pominąć z biblioteki statycznej plik .o, do którego nic się nie odwołuje):
//
//     REFLECT(Rigidbody,
//         FIELD(mass),                                   // publiczne pole
//         FIELD_NAMED("type", bodyType),                 // publiczne pole pod inną nazwą
//         PROPERTY("velocity", GetVelocity, SetVelocity)) // para getter/setter
//
// Obsługiwane typy pól: bool, int32, uint32, float, std::string, glm::vec2/3/4, glm::quat,
// wz::UUID oraz dowolny enum (zapisywany jako int32). Nazwa typu w rejestrze to nazwa
// bez przestrzeni nazw, więc REFLECT(wz::Rigidbody, ...) rejestruje "Rigidbody".
//
// Makra krótkie (REFLECT, FIELD, FIELD_NAMED, PROPERTY) można wyłączyć, definiując
// WZ_NO_SHORT_REFLECT_MACROS; wtedy zostają WZ_REFLECT, WZ_FIELD, WZ_FIELD_NAMED, WZ_PROPERTY.

#include "scene/ComponentRegistry.h"
#include "scene/GameObject.h"

#include <string_view>
#include <type_traits>
#include <utility>

namespace wz::detail {

template <typename T, typename = void>
struct FieldTraits {
    static constexpr bool kSupported = false;
};

#define WZ_DETAIL_FIELD_TRAITS(CppType, Kind)                              \
    template <>                                                            \
    struct FieldTraits<CppType> {                                          \
        static constexpr bool kSupported = true;                           \
        static constexpr FieldType kType = FieldType::Kind;                \
        using Stored = CppType;                                            \
        static Stored ToStored(const CppType& v) { return v; }             \
        static CppType FromStored(const Stored& v) { return v; }           \
    };

WZ_DETAIL_FIELD_TRAITS(bool, Bool)
WZ_DETAIL_FIELD_TRAITS(std::int32_t, Int)
WZ_DETAIL_FIELD_TRAITS(std::uint32_t, UInt)
WZ_DETAIL_FIELD_TRAITS(float, Float)
WZ_DETAIL_FIELD_TRAITS(std::string, String)
WZ_DETAIL_FIELD_TRAITS(glm::vec2, Vec2)
WZ_DETAIL_FIELD_TRAITS(glm::vec3, Vec3)
WZ_DETAIL_FIELD_TRAITS(glm::vec4, Vec4)
WZ_DETAIL_FIELD_TRAITS(glm::quat, Quat)
WZ_DETAIL_FIELD_TRAITS(::wz::UUID, UUID)
#undef WZ_DETAIL_FIELD_TRAITS

template <typename E>
struct FieldTraits<E, std::enable_if_t<std::is_enum_v<E>>> {
    static constexpr bool kSupported = true;
    static constexpr FieldType kType = FieldType::Enum;
    using Stored = std::int32_t;
    static Stored ToStored(E v) { return static_cast<Stored>(v); }
    static E FromStored(Stored v) { return static_cast<E>(v); }
};

// Pole opisane parą funkcji: get(const Self&) -> V oraz set(Self&, const V&).
template <typename Self, typename Getter, typename Setter>
FieldInfo MakeProperty(std::string name, Getter get, Setter set) {
    using V = std::decay_t<decltype(get(std::declval<const Self&>()))>;
    using Traits = FieldTraits<V>;
    static_assert(Traits::kSupported, "REFLECT: nieobslugiwany typ pola (patrz scene/Reflection.h)");
    using Stored = typename Traits::Stored;

    FieldInfo f;
    f.name = std::move(name);
    f.type = Traits::kType;
    f.get = [get](const Component& c) -> FieldValue {
        return FieldValue(Traits::ToStored(get(static_cast<const Self&>(c))));
    };
    f.set = [set](Component& c, const FieldValue& value) -> bool {
        const auto* stored = std::get_if<Stored>(&value);
        if (!stored) return false;
        set(static_cast<Self&>(c), Traits::FromStored(*stored));
        return true;
    };
    return f;
}

// Pole opisane wskaźnikiem na publiczną składową.
template <typename Self, typename M>
FieldInfo MakeField(std::string name, M Self::*member) {
    return MakeProperty<Self>(
        std::move(name), [member](const Self& s) -> const M& { return s.*member; },
        [member](Self& s, const M& v) { s.*member = v; });
}

// "wz::render::MeshRenderer" -> "MeshRenderer"
constexpr std::string_view UnqualifiedName(std::string_view n) {
    const auto pos = n.rfind("::");
    n = pos == std::string_view::npos ? n : n.substr(pos + 2);
    while (!n.empty() && n.front() == ' ') n.remove_prefix(1);
    return n;
}

template <typename T, typename FieldsFn>
const TypeInfo& RegisterType(std::string_view spelledName, FieldsFn makeFields) {
    static_assert(std::is_base_of_v<Component, T>, "REFLECT: typ musi dziedziczyc po wz::Component");
    TypeInfo info(std::string(UnqualifiedName(spelledName)), std::type_index(typeid(T)));
    info.fields = makeFields();
    if constexpr (std::is_same_v<T, Transform>) {
        // Każdy GameObject ma już Transform, więc "dodanie" zwraca istniejący.
        info.addTo = [](GameObject& go) -> Component* { return &go.transform(); };
    } else if constexpr (std::is_default_constructible_v<T>) {
        info.addTo = [](GameObject& go) -> Component* { return &go.AddComponent<T>(); };
    }
    return ComponentRegistry::Get().Register(std::move(info));
}

} // namespace wz::detail

#define WZ_REFLECT_CONCAT_(a, b) a##b
#define WZ_REFLECT_CONCAT(a, b) WZ_REFLECT_CONCAT_(a, b)

// Rejestruje typ w ComponentRegistry podczas inicjalizacji statycznej.
// Wewnątrz listy pól alias `Self` oznacza rejestrowany typ.
#define WZ_REFLECT(Type, ...)                                                                   \
    namespace {                                                                                 \
    [[maybe_unused]] const ::wz::TypeInfo& WZ_REFLECT_CONCAT(wzReflectedType_, __LINE__) =      \
        ::wz::detail::RegisterType<Type>(#Type, [] {                                            \
            using Self = Type;                                                                  \
            return std::vector<::wz::FieldInfo>{__VA_ARGS__};                                   \
        });                                                                                     \
    }

#define WZ_FIELD(member) ::wz::detail::MakeField<Self>(#member, &Self::member)
#define WZ_FIELD_NAMED(name, member) ::wz::detail::MakeField<Self>(name, &Self::member)
#define WZ_PROPERTY(name, getter, setter)                                                       \
    ::wz::detail::MakeProperty<Self>(                                                           \
        name, [](const Self& s) { return s.getter(); },                                         \
        [](Self& s, const auto& v) { s.setter(v); })

#ifndef WZ_NO_SHORT_REFLECT_MACROS
#define REFLECT(Type, ...) WZ_REFLECT(Type, __VA_ARGS__)
#define FIELD(member) WZ_FIELD(member)
#define FIELD_NAMED(name, member) WZ_FIELD_NAMED(name, member)
#define PROPERTY(name, getter, setter) WZ_PROPERTY(name, getter, setter)
#endif
