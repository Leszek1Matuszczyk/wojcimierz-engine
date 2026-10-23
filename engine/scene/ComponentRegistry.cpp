// ============================================================
//  Wojcimierz Engine: scene/ComponentRegistry.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "scene/ComponentRegistry.h"

#include "scene/Component.h"

#include <algorithm>
#include <stdexcept>

namespace wz {

const char* ToString(FieldType type) {
    switch (type) {
    case FieldType::Bool: return "bool";
    case FieldType::Int: return "int";
    case FieldType::UInt: return "uint";
    case FieldType::Float: return "float";
    case FieldType::String: return "string";
    case FieldType::Vec2: return "vec2";
    case FieldType::Vec3: return "vec3";
    case FieldType::Vec4: return "vec4";
    case FieldType::Quat: return "quat";
    case FieldType::UUID: return "uuid";
    case FieldType::Enum: return "enum";
    }
    return "?";
}

const FieldInfo* TypeInfo::FindField(std::string_view fieldName) const {
    for (const auto& f : fields)
        if (f.name == fieldName) return &f;
    return nullptr;
}

ComponentRegistry& ComponentRegistry::Get() {
    // Statyczna zmienna lokalna: rejestr powstaje przy pierwszym użyciu, więc kolejność
    // inicjalizacji statycznych REFLECT w różnych plikach nie ma znaczenia.
    // Tak oto mądrość czcigodnego Wojcimierza chroni nas przed "static initialization order fiasco".
    static ComponentRegistry instance;
    return instance;
}

const TypeInfo& ComponentRegistry::Register(TypeInfo info) {
    if (byName_.count(info.name))
        throw std::logic_error("ComponentRegistry: typ '" + info.name + "' jest juz zarejestrowany");
    if (byType_.count(info.type))
        throw std::logic_error("ComponentRegistry: typ C++ dla '" + info.name + "' jest juz zarejestrowany");
    for (std::size_t i = 0; i < info.fields.size(); ++i)
        for (std::size_t j = i + 1; j < info.fields.size(); ++j)
            if (info.fields[i].name == info.fields[j].name)
                throw std::logic_error("ComponentRegistry: pole '" + info.fields[i].name + "' w '" + info.name +
                                       "' wystepuje dwa razy");

    types_.push_back(std::move(info));
    const TypeInfo* stored = &types_.back();
    byName_.emplace(stored->name, stored);
    byType_.emplace(stored->type, stored);
    return *stored;
}

const TypeInfo* ComponentRegistry::Find(std::string_view name) const {
    auto it = byName_.find(std::string(name));
    return it == byName_.end() ? nullptr : it->second;
}

const TypeInfo* ComponentRegistry::Find(std::type_index type) const {
    auto it = byType_.find(type);
    return it == byType_.end() ? nullptr : it->second;
}

const TypeInfo* ComponentRegistry::FindFor(const Component& component) const {
    return Find(std::type_index(typeid(component)));
}

std::vector<const TypeInfo*> ComponentRegistry::All() const {
    std::vector<const TypeInfo*> out;
    out.reserve(types_.size());
    for (const auto& t : types_) out.push_back(&t);
    std::sort(out.begin(), out.end(), [](const TypeInfo* a, const TypeInfo* b) { return a->name < b->name; });
    return out;
}

} // namespace wz
