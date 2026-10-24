// ============================================================
//  Wojcimierz Engine: assets/ModelImporter.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "assets/ModelAsset.h"

#include <memory>
#include <string>

namespace wz {

struct ModelImportOptions {
    bool generateMissingNormals = true; // brak NORMAL -> normalne wygladzone (wazone polem trojkatow)
    bool loadTextures = true;           // false = materialy bez obrazow (szybciej, np. narzedzia)
};

// Import glTF 2.0 (.gltf z buforami zewnetrznymi lub osadzonymi data: URI oraz .glb) przez cgltf.
//
// Mapowanie:
//  * mesh -> MeshData, primitive (tylko TRIANGLES) -> SubMesh; atrybuty POSITION, NORMAL,
//    TEXCOORD_0, COLOR_0; brak indeksow -> 0..n-1, brak UV -> (0,0), brak koloru -> bialy,
//  * material.pbrMetallicRoughness -> Material: baseColorFactor, baseColorTexture (PNG/JPEG),
//    metallicFactor, roughnessFactor, emissiveFactor (* KHR_materials_emissive_strength), doubleSided,
//    primitive bez materialu -> domyslny material glTF (metallic = roughness = 1),
//  * node -> ModelNode (TRS albo macierz rozlozona na TRS), aktywna scena -> rootNodes.
// Nieobslugiwane (ostrzezenie w ModelAsset::warnings): skinning, morph targets, kamery, swiatla,
// tekstury inne niz baseColor, KHR_texture_transform, kompresja Draco/meshopt.
class ModelImporter {
public:
    // Zwraca nullptr przy bledzie; opis bledu w *error (jesli podano).
    static std::shared_ptr<ModelAsset> Load(const std::string& path, std::string* error = nullptr,
                                            const ModelImportOptions& options = {});
};

// Generuje normalne wierzcholkow z trojkatow (suma nieznormalizowanych normalnych scian,
// czyli wazona polem). Zakres [firstVertex, firstVertex + vertexCount) i indeksy absolutne.
void GenerateNormals(MeshData& mesh, std::uint32_t firstVertex, std::uint32_t vertexCount,
                     std::uint32_t firstIndex, std::uint32_t indexCount);

} // namespace wz
