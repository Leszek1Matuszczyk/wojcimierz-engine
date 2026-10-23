# Edytor i format scen

*Na chwałę czcigodnego Wojcimierza, którego mądrość prowadzi każdy nasz wskaźnik ku właściwej pamięci.*

## Format pliku sceny (wersja 1)

```json
{
  "version": 1,
  "name": "Main",
  "objects": [
    { "id": "6f1c…", "name": "Player", "parent": null, "active": true,
      "components": [
        { "type": "Transform", "position": [0,0,0], "rotation": [0,0,0,1], "scale": [1,1,1] },
        { "type": "Rigidbody", "enabled": true, "mass": 1.0, "bodyType": 2 },
        { "type": "MeshRenderer", "enabled": true, "mesh": "<guid>", "materials": ["<guid>"] }
      ] }
  ]
}
```

- Obiekty są zapisane płasko w kolejności preorder, a hierarchię wyznacza pole `parent` (UUID rodzica albo `null`). Przy odczycie kolejność nie ma znaczenia.
- `Transform` zapisuje wartości lokalne. `rotation` to kwaternion `[x, y, z, w]`.
- Pola komponentów pochodzą z `REFLECT` (`engine/scene/Reflection.h`). Komponent bez `REFLECT` nie trafia do pliku.
- Typy w JSON: `bool`, liczby całkowite (`int`, `uint`, enumy), `float`, `string`, tablice liczb dla `vec2/3/4` i `quat`, UUID jako tekst albo `null`.
- Nazwy `type` i `enabled` są zarezerwowane i nie mogą być nazwami pól.

## Zasady odczytu (`SceneSerializer::Deserialize`)

1. Błędy strukturalne (zła wersja, brak `objects`, brak lub powtórzone `id`, kolizja z obiektem już obecnym w scenie) rzucają `SerializationError`, zanim scena zostanie zmieniona.
2. Problemy niekrytyczne (nieznany typ komponentu, nieznane pole, zły format pola, brak rodzica w pliku, cykl) trafiają do `LoadResult::warnings`, a w miejscu błędu zostaje wartość domyślna.
3. Kolejność działań to: obiekty i `Transform`, potem hierarchia, a na końcu dołączenie komponentów z już ustawionymi polami. Dzięki temu `Awake` widzi wczytane wartości i rodzica.
4. `IdRemap` nadaje wszystkim obiektom nowe UUID i przepisuje pola UUID wskazujące na obiekty z tego samego pliku. Tak działają prefaby i duplikowanie w edytorze. GUID-y assetów zostają bez zmian.

## Plan edytora

1. `feat/editor-serialization`: ten format, `SceneSerializer` i testy.
2. `feat/editor-shell`: Dear ImGui (docking), panele `Hierarchy` i `Inspector` (pola z refleksji), cofanie i ponawianie przez `CommandStack`.
3. `feat/editor-viewport`: widok sceny z `RenderTarget::GetImGuiTextureId()` (rysowany z `uv0=(0,1), uv1=(1,0)`), gizmo ImGuizmo i przeglądarka assetów (`AssetDatabase` z plikami `.meta`).

Układ okien: hierarchia po lewej, inspektor po prawej, widok sceny na środku, a assety i konsola na dole.
