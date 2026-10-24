// ============================================================
//  Wojcimierz Engine: scene/ComponentRegistry.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once

#include "core/UUID.h"
#include "scene/Component.h" // FieldInfo::get/set i Create() używają Component; unique_ptr<Component> wymaga pełnego typu przy niszczeniu

#include <cstdint>
#include <deque>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <variant>
#include <vector>

namespace wz {

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
    std::string cppName;   // pisownia z REFLECT, np. "wz::Transform" (komunikaty o błędach)
    std::type_index type;  // typeid(T)
    std::vector<FieldInfo> fields;
    // Dodaje komponent do obiektu i go zwraca. Dla Transform zwraca istniejący Transform.
    // Puste, gdy typu nie da się utworzyć domyślnym konstruktorem.
    std::function<Component*(GameObject&)> addTo;
    // Tworzy komponent BEZ dołączania do obiektu (Awake jeszcze nie wywołane).
    // Serializacja najpierw ustawia pola, a dopiero potem dołącza komponent przez
    // GameObject::AddComponent(std::unique_ptr<Component>), więc Awake widzi wczytane
    // wartości, tak jak w Unity. Puste dla Transform i typów bez domyślnego konstruktora.
    std::function<std::unique_ptr<Component>()> create;

    const FieldInfo* FindField(std::string_view fieldName) const;
};

// Globalny rejestr typów komponentów. Wypełniają go jawne funkcje modułów
// (RegisterSceneComponents() itd., wołane przez Engine) oraz, dodatkowo, statyczne
// autorejestracje z makra REFLECT. Serializacja tworzy komponenty po nazwie,
// a inspektor wylicza ich pola.
class ComponentRegistry {
public:
    static ComponentRegistry& Get();
    // Osobne instancje tylko dla testów; silnik używa Get(). Tryb strict startuje z DefaultStrict().
    ComponentRegistry();

    // Tryb strict: każde odrzucenie w Register() (duplikat nazwy, typ pod inną nazwą, powtórzone
    // pole) po komunikacie na stderr kończy program przez std::abort(). Przeznaczony dla CI,
    // żeby duplikat nie przeszedł niezauważony. Domyślnie wyłączony.
    // Włączenie: opcja CMake -DWZ_REGISTRY_STRICT=ON (definicja kompilacji) albo zmienna
    // środowiskowa WZ_REGISTRY_STRICT=1. Zmienna pusta albo "0", "false", "off", "no"
    // (bez względu na wielkość liter, białe znaki na brzegach są pomijane) oznacza wyłączenie; każda inna wartość włącza.
    // Uwaga: przy -DWZ_REGISTRY_STRICT=ON tryb strict jest wkompilowany i zmienna środowiskowa
    // (także =0) go NIE wyłącza. Wyłączyć można go wtedy tylko w kodzie: SetStrict(false).
    // Mądrość Wojcimierza: w CI błąd ma zatrzymać budowę, u gracza tylko zostawić ślad.
    static bool DefaultStrict();
    void SetStrict(bool strict) { strict_ = strict; }
    bool IsStrict() const { return strict_; }

    // Rejestruje typ. Nigdy nie rzuca, bo może działać przed main():
    //  - ten sam typ C++ pod tą samą nazwą drugi raz: zwraca istniejący wpis (idempotentne),
    //  - nazwa zajęta przez inny typ, typ już zarejestrowany pod inną nazwą albo powtórzone
    //    pole: wypisuje komunikat na stderr i zwraca nullptr. Pierwsza rejestracja wygrywa.
    // nullptr znaczy "odrzucone, szczegóły na stderr".
    // Wątki: rejestr NIE ma blokady. Rejestracja jest dozwolona tylko w inicjalizacji statycznej
    // i w Register*Components() wołanych z Engine na wątku głównym przed startem innych wątków;
    // potem rejestr jest tylko do odczytu (odczyty z wielu wątków są wtedy bezpieczne).
    // Mądrość Wojcimierza: błąd ma mieć głos i nazwisko, a nie ciche zakończenie programu.
    const TypeInfo* Register(TypeInfo info);

    // Nowa instancja komponentu po nazwie z rejestru (bez dołączania do obiektu).
    // nullptr dla nieznanej nazwy oraz typów bez domyślnego konstruktora (np. Transform).
    std::unique_ptr<Component> Create(std::string_view name) const;

    const TypeInfo* Find(std::string_view name) const;
    const TypeInfo* Find(std::type_index type) const;
    const TypeInfo* FindFor(const Component& component) const; // po dynamicznym typie
    template <typename T>
    const TypeInfo* Find() const { return Find(std::type_index(typeid(T))); }

    // Wszystkie typy posortowane po nazwie (stabilna kolejność np. w menu "Add Component").
    std::vector<const TypeInfo*> All() const;
    std::size_t Size() const { return types_.size(); }

private:
    const TypeInfo* Reject() const; // nullptr albo std::abort() w trybie strict

    bool strict_ = false;
    std::deque<TypeInfo> types_; // deque: adresy TypeInfo nie zmieniają się przy dodawaniu
    std::unordered_map<std::string, const TypeInfo*> byName_;
    std::unordered_map<std::type_index, const TypeInfo*> byType_;
};

// Jawna rejestracja typu z REFLECT. Definicję (specjalizację) generuje makro REFLECT
// w tym samym .cpp, więc wywołanie tej funkcji z kodu modułu wymusza na linkerze
// dołączenie pliku z komponentem, nawet z biblioteki statycznej. Przed wywołaniem
// w innym pliku zadeklaruj specjalizację: WZ_DECLARE_REFLECTED(wz::Rigidbody).
// Szablon główny jest usunięty: wywołanie bez widocznej specjalizacji (brak WZ_DECLARE_REFLECTED)
// to błąd kompilacji, a nie cichy symbol nierozwiązany albo UB.
template <typename T>
const TypeInfo* RegisterReflected() = delete;

} // namespace wz

#define WZ_DECLARE_REFLECTED(Type) \
    template <>                    \
    const ::wz::TypeInfo* wz::RegisterReflected<Type>()
