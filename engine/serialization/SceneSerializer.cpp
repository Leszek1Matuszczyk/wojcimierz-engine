// ============================================================
//  Wojcimierz Engine: serialization/SceneSerializer.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "serialization/SceneSerializer.h"

#include "core/Math.h"
#include "scene/ComponentRegistry.h"
#include "scene/Scene.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>
#include <system_error>
#include <unordered_set>

namespace wz {

using nlohmann::json;

UUID IdRemap::Remap(const UUID& oldId) {
    auto [it, inserted] = map_.try_emplace(oldId);
    if (inserted) it->second = UUID::Generate();
    return it->second;
}

UUID IdRemap::Lookup(const UUID& id) const {
    auto it = map_.find(id);
    return it == map_.end() ? id : it->second;
}

namespace {

constexpr std::string_view kTypeKey = "type";
constexpr std::string_view kEnabledKey = "enabled";

// float -> double przez najkrótszą reprezentację dziesiętną, żeby w pliku było 0.1, a nie
// 0.10000000149011612. Odczyt (double -> float) daje dokładnie tę samą wartość float.
double PrettyFloat(float v) {
    char buf[32];
    auto [end, ec] = std::to_chars(buf, buf + sizeof(buf), v);
    if (ec != std::errc()) return static_cast<double>(v);
    return std::strtod(std::string(buf, end).c_str(), nullptr);
}

// Ścisła walidacja UTF-8 (bez overlongów, surogatów i kodów > U+10FFFF), zgodna z tym,
// co przyjmuje parser nlohmann. Bez niej dump() rzuciłby type_error 316 w środku zapisu.
bool IsValidUtf8(std::string_view s) {
    std::size_t i = 0;
    while (i < s.size()) {
        const auto c = static_cast<unsigned char>(s[i]);
        std::size_t len = 0;
        std::uint32_t cp = 0;
        if (c < 0x80) { ++i; continue; }
        if ((c & 0xE0) == 0xC0) { len = 2; cp = c & 0x1F; }
        else if ((c & 0xF0) == 0xE0) { len = 3; cp = c & 0x0F; }
        else if ((c & 0xF8) == 0xF0) { len = 4; cp = c & 0x07; }
        else return false;
        if (i + len > s.size()) return false;
        for (std::size_t k = 1; k < len; ++k) {
            const auto cc = static_cast<unsigned char>(s[i + k]);
            if ((cc & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (cc & 0x3F);
        }
        if ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000)) return false;
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
        i += len;
    }
    return true;
}

void RequireUtf8(std::string_view s, const std::string& where) {
    if (!IsValidUtf8(s)) throw SerializationError(where + ": tekst nie jest poprawnym UTF-8");
}

// NaN i ±Inf nie mają zapisu w JSON (nlohmann zapisałby po cichu null), więc zapis ich nie przyjmuje.
// Mądrość Wojcimierza: lepiej głośno odmówić niż po cichu zgubić dane.
float RequireFinite(float v, const std::string& where) {
    if (!IsFinite(v))
        throw SerializationError(where + ": wartosc " + (std::isnan(v) ? "NaN" : "Inf") +
                                 " nie moze trafic do pliku sceny");
    return v;
}

template <std::size_t N>
json FloatArray(const float* data, const std::string& where) {
    json a = json::array();
    for (std::size_t i = 0; i < N; ++i) a.push_back(PrettyFloat(RequireFinite(data[i], where + "[" + std::to_string(i) + "]")));
    return a;
}

json EncodeValue(const FieldValue& value, FieldType type, const std::string& where) {
    switch (type) {
    case FieldType::Bool: return std::get<bool>(value);
    case FieldType::Int:
    case FieldType::Enum: return std::get<std::int32_t>(value);
    case FieldType::UInt: return std::get<std::uint32_t>(value);
    case FieldType::Float: return PrettyFloat(RequireFinite(std::get<float>(value), where));
    case FieldType::String: {
        const auto& str = std::get<std::string>(value);
        RequireUtf8(str, where);
        return str;
    }
    case FieldType::Vec2: return FloatArray<2>(&std::get<glm::vec2>(value).x, where);
    case FieldType::Vec3: return FloatArray<3>(&std::get<glm::vec3>(value).x, where);
    case FieldType::Vec4: return FloatArray<4>(&std::get<glm::vec4>(value).x, where);
    case FieldType::Quat: {
        const auto& q = std::get<glm::quat>(value);
        const float xyzw[4] = {q.x, q.y, q.z, q.w}; // zawsze [x, y, z, w], niezależnie od układu glm
        return FloatArray<4>(xyzw, where);
    }
    case FieldType::UUID: {
        const auto& id = std::get<UUID>(value);
        return id.IsNil() ? json(nullptr) : json(id.ToString());
    }
    }
    return nullptr;
}

template <std::size_t N>
bool ReadFloats(const json& j, float* out) {
    if (!j.is_array() || j.size() != N) return false;
    for (std::size_t i = 0; i < N; ++i) {
        if (!j[i].is_number()) return false;
        out[i] = j[i].get<float>();
        if (!IsFinite(out[i])) return false; // np. 1e39 poza zakresem float
    }
    return true;
}

std::optional<FieldValue> DecodeValue(const json& j, FieldType type, const IdRemap* remap) {
    switch (type) {
    case FieldType::Bool:
        if (j.is_boolean()) return FieldValue(j.get<bool>());
        return std::nullopt;
    case FieldType::Int:
    case FieldType::Enum: {
        if (!j.is_number_integer()) return std::nullopt;
        const auto v = j.get<std::int64_t>();
        if (v < std::numeric_limits<std::int32_t>::min() || v > std::numeric_limits<std::int32_t>::max())
            return std::nullopt;
        return FieldValue(static_cast<std::int32_t>(v));
    }
    case FieldType::UInt: {
        if (!j.is_number_integer()) return std::nullopt;
        if (j.is_number_unsigned()) {
            const auto v = j.get<std::uint64_t>();
            if (v > std::numeric_limits<std::uint32_t>::max()) return std::nullopt;
            return FieldValue(static_cast<std::uint32_t>(v));
        }
        const auto v = j.get<std::int64_t>();
        if (v < 0 || v > std::numeric_limits<std::uint32_t>::max()) return std::nullopt;
        return FieldValue(static_cast<std::uint32_t>(v));
    }
    case FieldType::Float:
        if (j.is_number() && IsFinite(j.get<float>())) return FieldValue(j.get<float>());
        return std::nullopt;
    case FieldType::String:
        if (j.is_string() && IsValidUtf8(j.get_ref<const std::string&>())) return FieldValue(j.get<std::string>());
        return std::nullopt;
    case FieldType::Vec2: {
        glm::vec2 v;
        return ReadFloats<2>(j, &v.x) ? std::optional<FieldValue>(v) : std::nullopt;
    }
    case FieldType::Vec3: {
        glm::vec3 v;
        return ReadFloats<3>(j, &v.x) ? std::optional<FieldValue>(v) : std::nullopt;
    }
    case FieldType::Vec4: {
        glm::vec4 v;
        return ReadFloats<4>(j, &v.x) ? std::optional<FieldValue>(v) : std::nullopt;
    }
    case FieldType::Quat: {
        float xyzw[4];
        if (!ReadFloats<4>(j, xyzw)) return std::nullopt;
        return FieldValue(glm::quat(xyzw[3], xyzw[0], xyzw[1], xyzw[2])); // konstruktor glm: (w, x, y, z)
    }
    case FieldType::UUID: {
        if (j.is_null()) return FieldValue(UUID());
        if (!j.is_string()) return std::nullopt;
        auto id = UUID::FromString(j.get<std::string>());
        if (!id) return std::nullopt;
        // Referencje do obiektów z tego samego pliku idą za remapem; GUID-y assetów zostają.
        return FieldValue(remap ? remap->Lookup(*id) : *id);
    }
    }
    return std::nullopt;
}

json SerializeComponent(const Component& c, const TypeInfo& info, bool isTransform, const std::string& where) {
    json out = json::object();
    out[std::string(kTypeKey)] = info.name;
    if (!isTransform) out[std::string(kEnabledKey)] = c.enabled;
    for (const auto& f : info.fields) {
        if (f.name == kTypeKey || f.name == kEnabledKey)
            throw SerializationError("Pole '" + f.name + "' w '" + info.name + "' ma zarezerwowana nazwe");
        out[f.name] = EncodeValue(f.get(c), f.type, where + ", " + info.name + "." + f.name);
    }
    return out;
}

json SerializeObject(const GameObject& go, bool writeParent) {
    json o = json::object();
    // Ścieżka do komunikatów: obiekt (nazwa jako bajty mogą być złe, więc w miejscu błędu id wystarczy).
    const std::string where = "obiekt " + go.GetId().ToString() +
                              (IsValidUtf8(go.GetName()) ? " ('" + go.GetName() + "')" : "");
    RequireUtf8(go.GetName(), where + ", nazwa obiektu");
    o["id"] = go.GetId().ToString();
    o["name"] = go.GetName();
    const Transform* parent = go.transform().GetParent();
    o["parent"] = (writeParent && parent) ? json(parent->gameObject().GetId().ToString()) : json(nullptr);
    o["active"] = go.IsActiveSelf();

    json comps = json::array();
    const auto& registry = ComponentRegistry::Get();
    for (std::size_t i = 0; i < go.GetComponentCount(); ++i) {
        const Component& c = go.GetComponentAt(i);
        const TypeInfo* info = registry.FindFor(c);
        if (!info) continue; // komponent bez REFLECT nie jest zapisywany
        comps.push_back(SerializeComponent(c, *info, &c == &go.transform(), where));
    }
    o["components"] = std::move(comps);
    return o;
}

// Preorder: rodzic przed dziećmi, dzieci w kolejności z hierarchii.
void AppendSubtree(json& objects, const GameObject& go, bool isSubtreeRoot) {
    if (go.IsPendingDestroy()) return;
    objects.push_back(SerializeObject(go, !isSubtreeRoot));
    for (const Transform* child : go.transform().GetChildren()) AppendSubtree(objects, child->gameObject(), false);
}

json MakeDocument(std::string_view name) {
    RequireUtf8(name, "nazwa sceny");
    json doc = json::object();
    doc["version"] = kSceneFormatVersion;
    doc["name"] = name;
    doc["objects"] = json::array();
    return doc;
}

// Wczytany, jeszcze niedołączony komponent.
struct PendingComponent {
    std::unique_ptr<Component> component;
    bool enabled = true;
};

struct PendingObject {
    UUID fileId;
    UUID id; // po remapie
    std::string name;
    std::optional<UUID> parentFileId;
    bool active = true;
    std::vector<std::pair<const FieldInfo*, FieldValue>> transformFields;
    std::vector<PendingComponent> components;
};

std::string Where(std::size_t index, const std::string& name) {
    return "obiekt #" + std::to_string(index) + (name.empty() ? "" : " ('" + name + "')");
}

std::optional<UUID> ParseIdField(const json& o, const char* key) {
    auto it = o.find(key);
    if (it == o.end() || !it->is_string()) return std::nullopt;
    return UUID::FromString(it->get<std::string>());
}

void ApplyFields(const json& jc, const TypeInfo& info, const IdRemap* remap, const std::string& where,
                 LoadResult& result, const std::function<void(const FieldInfo&, FieldValue&&)>& apply) {
    for (auto it = jc.begin(); it != jc.end(); ++it) {
        if (it.key() == kTypeKey || it.key() == kEnabledKey) continue;
        const FieldInfo* f = info.FindField(it.key());
        if (!f) {
            result.warnings.push_back(where + ": " + info.name + " nie ma pola '" + it.key() + "', pomijam");
            continue;
        }
        auto value = DecodeValue(*it, f->type, remap);
        if (!value) {
            result.warnings.push_back(where + ": pole " + info.name + "." + f->name + " ma zly format (oczekiwano " +
                                      ToString(f->type) + "), zostaje wartosc domyslna");
            continue;
        }
        apply(*f, std::move(*value));
    }
}

} // namespace

json SceneSerializer::SerializeScene(const Scene& scene) {
    json doc = MakeDocument(scene.GetName());
    for (std::size_t i = 0; i < scene.GetGameObjectCount(); ++i) {
        const GameObject& go = scene.GetGameObjectAt(i);
        if (!go.transform().GetParent()) AppendSubtree(doc["objects"], go, false);
    }
    return doc;
}

json SceneSerializer::SerializeSubtree(const GameObject& root) {
    json doc = MakeDocument(root.GetName());
    AppendSubtree(doc["objects"], root, true);
    return doc;
}

LoadResult SceneSerializer::Deserialize(Scene& target, const json& document, IdRemap* remap) {
    LoadResult result;

    // --- Krok 0: walidacja struktury. Do końca kroku 1 scena nie jest dotykana. ---
    if (!document.is_object()) throw SerializationError("Dokument sceny musi byc obiektem JSON");
    auto ver = document.find("version");
    if (ver == document.end() || !ver->is_number_integer())
        throw SerializationError("Brak pola 'version' (liczba calkowita)");
    const auto version = ver->get<std::int64_t>();
    if (version < 1 || version > kSceneFormatVersion)
        throw SerializationError("Nieobslugiwana wersja formatu sceny: " + std::to_string(version) +
                                 " (obslugiwane: 1.." + std::to_string(kSceneFormatVersion) + ")");
    if (auto n = document.find("name"); n != document.end() && n->is_string()) {
        RequireUtf8(n->get_ref<const std::string&>(), "nazwa sceny");
        result.sceneName = n->get<std::string>();
    }
    auto objs = document.find("objects");
    if (objs == document.end() || !objs->is_array()) throw SerializationError("Brak tablicy 'objects'");

    // Najpierw wszystkie identyfikatory, bo remap pól UUID musi znać cały plik.
    std::vector<PendingObject> pending(objs->size());
    std::unordered_set<UUID> fileIds;
    for (std::size_t i = 0; i < objs->size(); ++i) {
        const json& o = (*objs)[i];
        if (!o.is_object()) throw SerializationError(Where(i, "") + ": wpis musi byc obiektem JSON");
        auto& p = pending[i];
        if (auto n = o.find("name"); n != o.end() && n->is_string()) {
            RequireUtf8(n->get_ref<const std::string&>(), Where(i, "") + ", nazwa obiektu");
            p.name = n->get<std::string>();
        }
        auto id = ParseIdField(o, "id");
        if (!id || id->IsNil()) throw SerializationError(Where(i, p.name) + ": brak poprawnego pola 'id'");
        if (!fileIds.insert(*id).second)
            throw SerializationError(Where(i, p.name) + ": powtorzone id " + id->ToString());
        p.fileId = *id;
        p.id = remap ? remap->Remap(*id) : *id;
        if (target.FindById(p.id))
            throw SerializationError(Where(i, p.name) + ": obiekt o id " + p.id.ToString() +
                                     " juz istnieje w scenie (uzyj IdRemap)");
    }

    // --- Krok 1: parsowanie obiektów i komponentów (komponenty jeszcze niedołączone). ---
    // Czcigodny Wojcimierz uczy: najpierw zrozum cały plik, dopiero potem zmieniaj świat.
    const auto& registry = ComponentRegistry::Get();
    for (std::size_t i = 0; i < objs->size(); ++i) {
        const json& o = (*objs)[i];
        auto& p = pending[i];
        const std::string where = Where(i, p.name);

        if (auto a = o.find("active"); a != o.end() && a->is_boolean()) p.active = a->get<bool>();
        if (auto par = o.find("parent"); par != o.end() && !par->is_null()) {
            auto parentId = par->is_string() ? UUID::FromString(par->get<std::string>()) : std::nullopt;
            if (!parentId)
                result.warnings.push_back(where + ": niepoprawne pole 'parent', obiekt bedzie korzeniem");
            else if (!fileIds.count(*parentId))
                result.warnings.push_back(where + ": rodzic " + parentId->ToString() +
                                          " nie wystepuje w pliku, obiekt bedzie korzeniem");
            else
                p.parentFileId = *parentId;
        }

        auto comps = o.find("components");
        if (comps == o.end()) continue;
        if (!comps->is_array()) throw SerializationError(where + ": 'components' musi byc tablica");
        bool transformSeen = false;
        for (const json& jc : *comps) {
            auto typeIt = jc.is_object() ? jc.find(kTypeKey) : jc.end();
            if (!jc.is_object() || typeIt == jc.end() || !typeIt->is_string()) {
                result.warnings.push_back(where + ": komponent bez pola 'type', pomijam");
                continue;
            }
            const std::string typeName = typeIt->get<std::string>();
            const TypeInfo* info = registry.Find(typeName);
            if (!info) {
                result.warnings.push_back(where + ": nieznany typ komponentu '" + typeName + "', pomijam");
                continue;
            }
            if (info->type == std::type_index(typeid(Transform))) {
                if (transformSeen) {
                    result.warnings.push_back(where + ": drugi Transform, pomijam");
                    continue;
                }
                transformSeen = true;
                ApplyFields(jc, *info, remap, where, result, [&](const FieldInfo& f, FieldValue&& v) {
                    p.transformFields.emplace_back(&f, std::move(v));
                });
                continue;
            }
            if (!info->create) {
                result.warnings.push_back(where + ": typu '" + typeName + "' nie da sie utworzyc, pomijam");
                continue;
            }
            PendingComponent pc;
            pc.component = info->create();
            if (auto en = jc.find(kEnabledKey); en != jc.end() && en->is_boolean()) pc.enabled = en->get<bool>();
            Component& comp = *pc.component;
            ApplyFields(jc, *info, remap, where, result, [&](const FieldInfo& f, FieldValue&& v) {
                if (!f.set(comp, v))
                    result.warnings.push_back(where + ": nie udalo sie ustawic " + info->name + "." + f.name);
            });
            p.components.push_back(std::move(pc));
        }
    }

    // --- Krok 2: tworzenie obiektów i Transformów. Od tego miejsca nic już nie rzuca. ---
    std::unordered_map<UUID, GameObject*> byFileId;
    result.objects.reserve(pending.size());
    for (auto& p : pending) {
        GameObject& go = target.CreateGameObject(p.name);
        go.SetId(p.id);
        go.SetActive(p.active);
        for (auto& [field, value] : p.transformFields)
            if (!field->set(go.transform(), value))
                result.warnings.push_back("Transform '" + p.name + "': nie udalo sie ustawic " + field->name);
        byFileId.emplace(p.fileId, &go);
        result.objects.push_back(&go);
    }

    // --- Krok 3: hierarchia (wartości w pliku są lokalne, więc worldPositionStays = false). ---
    for (std::size_t i = 0; i < pending.size(); ++i) {
        GameObject* go = result.objects[i];
        if (pending[i].parentFileId) {
            GameObject* parent = byFileId.at(*pending[i].parentFileId);
            if (!go->transform().SetParent(&parent->transform(), false)) {
                result.warnings.push_back(Where(i, pending[i].name) + ": cykl w hierarchii, obiekt bedzie korzeniem");
            }
        }
    }
    for (GameObject* go : result.objects)
        if (!go->transform().GetParent()) result.roots.push_back(go);

    // --- Krok 4: dołączenie komponentów; Awake widzi już pola i hierarchię. ---
    for (std::size_t i = 0; i < pending.size(); ++i) {
        for (auto& pc : pending[i].components) {
            Component& c = result.objects[i]->AddComponent(std::move(pc.component));
            c.enabled = pc.enabled;
        }
    }
    return result;
}

void SceneSerializer::SaveToFile(const Scene& scene, const std::filesystem::path& path) {
    std::string text;
    try {
        text = SerializeScene(scene).dump(2) + "\n";
    } catch (const json::exception& e) { // ostatnia linia obrony: żaden wyjątek nlohmann nie wycieka
        throw SerializationError(std::string("Blad kodowania JSON: ") + e.what());
    }
    auto tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) throw SerializationError("Nie mozna otworzyc do zapisu: " + tmp.string());
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!out) throw SerializationError("Blad zapisu: " + tmp.string());
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        throw SerializationError("Nie mozna zapisac " + path.string());
    }
}

LoadResult SceneSerializer::LoadFromFile(Scene& target, const std::filesystem::path& path, IdRemap* remap) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw SerializationError("Nie mozna otworzyc: " + path.string());
    json doc = json::parse(in, nullptr, /*allow_exceptions=*/false, /*ignore_comments=*/true);
    if (doc.is_discarded()) throw SerializationError("Niepoprawny JSON: " + path.string());
    return Deserialize(target, doc, remap);
}

} // namespace wz
