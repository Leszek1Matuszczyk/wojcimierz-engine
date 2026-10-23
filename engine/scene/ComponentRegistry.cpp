// ============================================================
//  Wojcimierz Engine: scene/ComponentRegistry.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "scene/ComponentRegistry.h"

#include "scene/Component.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#if defined(__GNUG__)
#include <cxxabi.h>
#endif

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

namespace {

std::string Demangle(const char* mangled) {
#if defined(__GNUG__)
    int status = 0;
    std::unique_ptr<char, void (*)(void*)> out(abi::__cxa_demangle(mangled, nullptr, nullptr, &status), std::free);
    if (status == 0 && out) return out.get();
#endif
    return mangled;
}

std::string CppNameOf(const TypeInfo& t) { return t.cppName.empty() ? Demangle(t.type.name()) : t.cppName; }

} // namespace

const TypeInfo* ComponentRegistry::Register(TypeInfo info) {
    // Rejestracja może działać przed main(), więc żadnych wyjątków: komunikat na stderr
    // i deterministycznie odrzucamy drugą rejestrację (pierwsza wygrywa).
    if (auto it = byType_.find(info.type); it != byType_.end()) {
        if (it->second->name == info.name) return it->second; // jawna + statyczna rejestracja tego samego typu
        std::fprintf(stderr, "[wz] ComponentRegistry: typ %s jest juz zarejestrowany jako '%s'; odrzucam nazwe '%s'\n",
                     CppNameOf(info).c_str(), it->second->name.c_str(), info.name.c_str());
        return nullptr;
    }
    if (auto it = byName_.find(info.name); it != byName_.end()) {
        std::fprintf(stderr, "[wz] ComponentRegistry: komponent '%s' zarejestrowany dwa razy (typy %s i %s); "
                             "zostaje pierwszy\n",
                     info.name.c_str(), CppNameOf(*it->second).c_str(), CppNameOf(info).c_str());
        return nullptr;
    }
    for (std::size_t i = 0; i < info.fields.size(); ++i)
        for (std::size_t j = i + 1; j < info.fields.size(); ++j)
            if (info.fields[i].name == info.fields[j].name) {
                std::fprintf(stderr, "[wz] ComponentRegistry: pole '%s' w '%s' wystepuje dwa razy; odrzucam typ\n",
                             info.fields[i].name.c_str(), info.name.c_str());
                return nullptr;
            }

    types_.push_back(std::move(info));
    const TypeInfo* stored = &types_.back();
    byName_.emplace(stored->name, stored);
    byType_.emplace(stored->type, stored);
    return stored;
}

std::unique_ptr<Component> ComponentRegistry::Create(std::string_view name) const {
    const TypeInfo* info = Find(name);
    return info && info->create ? info->create() : nullptr;
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
