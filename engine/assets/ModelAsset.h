// ============================================================
//  Wojcimierz Engine: assets/ModelAsset.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "render/Image.h"
#include "render/MeshData.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <vector>

namespace wz {

class GameObject;
class Material;
class Mesh;
class Scene;
class Transform;

namespace rhi { class Device; }

// Wezel hierarchii modelu (odpowiednik node z glTF) - lokalne TRS wzgledem rodzica.
struct ModelNode {
    std::string name;
    int parent = -1;            // indeks w ModelAsset::nodes, -1 = korzen
    std::vector<int> children;  // indeksy w ModelAsset::nodes
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    int mesh = -1;              // indeks w ModelAsset::meshes, -1 = pusty wezel
};

// Model zaimportowany z pliku (glTF 2.0) - asset wspoldzielony (std::shared_ptr).
//
// Cykl zycia:
//   auto model = ModelImporter::Load("assets/robot.glb");  // CPU: geometria, materialy, obrazy
//   model->UploadToGpu(device);                             // GPU: Mesh + Texture (raz)
//   GameObject& robot = model->Instantiate(scene);          // dowolnie wiele instancji
//
// Instantiate dziala tez bez UploadToGpu (MeshRenderer::mesh = nullptr) - przydatne
// w testach i narzedziach bez kontekstu GPU.
class ModelAsset {
public:
    std::string name;        // nazwa pliku bez rozszerzenia
    std::string sourcePath;

    // Jedna siatka glTF = jedna MeshData; kazdy primitive = jedna podsiatka (SubMesh).
    std::vector<MeshData> meshes;
    // Materialy kazdej siatki, po jednym na podsiatke (gotowe do MeshRenderer::materials).
    std::vector<std::vector<std::shared_ptr<Material>>> meshMaterials;
    // Wszystkie materialy z pliku (+ ewentualny domyslny na koncu), bez duplikatow.
    std::vector<std::shared_ptr<Material>> materials;
    // Indeks obrazu baseColor dla materials[i] (-1 = brak tekstury).
    std::vector<int> materialBaseColorImage;
    std::vector<Image> images; // zdekodowane RGBA8 (wiersz 0 = gora, v = 0)

    std::vector<ModelNode> nodes;
    std::vector<int> rootNodes; // korzenie aktywnej sceny glTF

    std::vector<std::string> warnings; // np. pominiete primitive'y, nieobslugiwane rozszerzenia

    // --- GPU (wz_engine, render/ModelUpload.cpp) ---
    // Tworzy Mesh dla kazdej siatki i Texture (sRGB) dla obrazow baseColor
    // i podpina je pod Material::baseColorTexture. Kolejne wywolania nic nie robia.
    void UploadToGpu(rhi::Device& device);
    bool IsUploaded() const { return uploaded_; }
    const std::vector<std::shared_ptr<Mesh>>& GetGpuMeshes() const { return gpuMeshes_; }

    // Tworzy GameObject-kontener o nazwie modelu z cala hierarchia wezlow jako dziecmi
    // (Transform + MeshRenderer z materialami per podsiatka). parent = opcjonalny rodzic kontenera.
    GameObject& Instantiate(Scene& scene, Transform* parent = nullptr) const;

    std::size_t GetTotalVertexCount() const;
    std::size_t GetTotalIndexCount() const;

private:
    std::vector<std::shared_ptr<Mesh>> gpuMeshes_;
    bool uploaded_ = false;
};

} // namespace wz
