// ============================================================
//  Wojcimierz Engine: scene/ComponentRegistry.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once

#include "core/UUID.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <variant>
#include <vector>

namespace wz {

class Component;
class GameObject;

// Typy pól, które rozumieją serializacja scen i inspektor edytora.
enum class FieldType {
    Bool,
    Int,   // std::int32_t
    UInt,  // std::uint32_t
    Float,
    String,
    Vec2,
    Vec3,
    Vec4,
    Quat,  // zapis jako [x, y, z, w]
    UUID,  // referencja do GameObjectu albo assetu (GUID)
    Enum,  // dowolny enum, przechowywany jako std::int32_t
};

// Jedna wartość pola. Kolejność alternatyw nie odpowiada FieldType 1:1 (Enum korzysta z int32).
using FieldValue = std::variant<bool, std::int32_t, std::uint32_t, float, std::string, glm::vec2, glm::vec3,
                                glm::vec4, glm::quat, UUID>;

const char* ToString(FieldType type);

// Opis jednego pola komponentu: nazwa w pliku sceny, typ oraz akcesory działające
// na dowolnym Component& (rzutowanie do właściwego typu robi kod wygenerowany przez REFLECT).
struct FieldInfo {
    std::string name;
    FieldType type = FieldType::Float;
    std::function<FieldValue(const Component&)> get;
    // Zwraca false, gdy wartość ma zły typ (wtedy komponent pozostaje bez zmian).
    std::function<bool(Component&, const FieldValue&)> set;
};

// Opis zarejestrowanego typu komponentu.
struct TypeInfo {
    TypeInfo(std::string typeName, std::type_index typeIndex) : name(std::move(typeName)), type(typeIndex) {}

    std::string name;      // np. "Transform"; ta nazwa trafia do pola "type" w JSON
    std::type_index type;  // typeid(T)
    std::vector<FieldInfo> fields;
    // Dodaje komponent do obiektu i go zwraca. Dla Transform zwraca istniejący Transform.
    // Puste, gdy typu nie da się utworzyć domyślnym konstruktorem.
    std::function<Component*(GameObject&)> addTo;

    const FieldInfo* FindField(std::string_view fieldName) const;
};

// Globalny rejestr typów komponentów. Wypełniany statycznie przez makro REFLECT
// (scene/Reflection.h) jeszcze przed main(), a później tylko odczytywany.
// Serializacja tworzy komponenty po nazwie, a inspektor wylicza ich pola.
class ComponentRegistry {
public:
    static ComponentRegistry& Get();

    // Rzuca std::logic_error, gdy nazwa albo typ są już zarejestrowane.
    const TypeInfo& Register(TypeInfo info);

    const TypeInfo* Find(std::string_view name) const;
    const TypeInfo* Find(std::type_index type) const;
    const TypeInfo* FindFor(const Component& component) const; // po dynamicznym typie
    template <typename T>
    const TypeInfo* Find() const { return Find(std::type_index(typeid(T))); }

    // Wszystkie typy posortowane po nazwie (stabilna kolejność np. w menu "Add Component").
    std::vector<const TypeInfo*> All() const;
    std::size_t Size() const { return types_.size(); }

private:
    ComponentRegistry() = default;

    std::deque<TypeInfo> types_; // deque: adresy TypeInfo nie zmieniają się przy dodawaniu
    std::unordered_map<std::string, const TypeInfo*> byName_;
    std::unordered_map<std::type_index, const TypeInfo*> byType_;
};

} // namespace wz
