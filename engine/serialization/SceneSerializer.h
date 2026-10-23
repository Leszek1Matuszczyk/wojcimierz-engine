// ============================================================
//  Wojcimierz Engine: serialization/SceneSerializer.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once

#include "core/UUID.h"

#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace wz {

class Scene;
class GameObject;

// Aktualna wersja formatu pliku sceny (pole "version").
inline constexpr int kSceneFormatVersion = 1;

// Błąd strukturalny pliku: zła wersja, brak tablicy "objects", obiekt bez poprawnego "id",
// powtórzone "id" itp. Rzucany ZANIM scena zostanie zmieniona, więc wczytanie jest atomowe.
class SerializationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Remapowanie UUID przy wczytywaniu kopii (prefaby, duplikowanie w edytorze).
// Każde "id" obiektu z pliku dostaje nowy UUID, a pola typu UUID wskazujące na obiekty
// z tego samego pliku są przepisywane na nowe wartości. UUID spoza pliku (np. GUID assetu)
// zostają bez zmian. Ten sam IdRemap można przekazać do kilku wczytań, żeby zachować spójność.
class IdRemap {
public:
    // Nowy UUID dla starego; tworzony przy pierwszym zapytaniu.
    UUID Remap(const UUID& oldId);
    // Zmapowany UUID albo oryginał, gdy nie ma go w mapie.
    UUID Lookup(const UUID& id) const;
    const std::unordered_map<UUID, UUID>& Map() const { return map_; }

private:
    std::unordered_map<UUID, UUID> map_;
};

struct LoadResult {
    std::string sceneName;               // pole "name" dokumentu (może być puste)
    std::vector<GameObject*> objects;    // wszystkie utworzone obiekty, w kolejności z pliku
    std::vector<GameObject*> roots;      // utworzone obiekty bez rodzica (np. korzeń prefabu)
    std::vector<std::string> warnings;   // problemy niekrytyczne: nieznany typ, złe pole, zły rodzic...
};

// Zapis i odczyt scen oraz poddrzew (prefabów) do JSON. Format, wersja 1:
//
//   { "version": 1, "name": "Main",
//     "objects": [
//       { "id": "<uuid>", "name": "Player", "parent": null | "<uuid>", "active": true,
//         "components": [
//           { "type": "Transform", "position": [x,y,z], "rotation": [x,y,z,w], "scale": [x,y,z] },
//           { "type": "Rigidbody", "enabled": true, "mass": 1.0, ... } ] } ] }
//
// Obiekty są zapisane płasko w kolejności preorder (rodzic przed dziećmi, dzieci w kolejności
// z hierarchii). Pola komponentów pochodzą z refleksji (REFLECT), a komponenty bez refleksji
// są pomijane przy zapisie. Wczytywanie działa w trzech krokach: tworzy obiekty, ustawia
// hierarchię, a na końcu dołącza komponenty z już ustawionymi polami, więc Awake widzi wczytane
// wartości i kompletną hierarchię.
class SceneSerializer {
public:
    static nlohmann::json SerializeScene(const Scene& scene);
    // Poddrzewo z `root` włącznie; korzeń zapisany jest z "parent": null.
    static nlohmann::json SerializeSubtree(const GameObject& root);

    // Dodaje obiekty z dokumentu do `target` (nie czyści sceny).
    // Bez `remap` obiekty zachowują UUID z pliku, a obiekt o UUID już obecnym w scenie
    // daje SerializationError. Z `remap` wszystkie dostają nowe UUID.
    static LoadResult Deserialize(Scene& target, const nlohmann::json& document, IdRemap* remap = nullptr);

    // Zapis atomowy (plik tymczasowy + rename). Rzuca SerializationError przy błędzie I/O.
    static void SaveToFile(const Scene& scene, const std::filesystem::path& path);
    static LoadResult LoadFromFile(Scene& target, const std::filesystem::path& path, IdRemap* remap = nullptr);
};

} // namespace wz
