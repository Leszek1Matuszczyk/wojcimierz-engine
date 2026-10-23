// ============================================================
//  Wojcimierz Engine: render/MeshRenderer.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "scene/Component.h"

#include "render/MeshData.h"

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace wz {

class Material;
class Mesh;
class Shader;

// Odpowiednik MeshFilter + MeshRenderer z Unity.
//
// Dwa tryby:
//  * PBR - lista `materials` niepusta: kazda podsiatka (Mesh::GetSubMeshes) rysowana jest
//    swoim materialem (materials[i]; brakujace = ostatni material), oswietlenie ze
//    swiatel sceny (DirectionalLight / PointLight / SpotLight),
//  * legacy - `materials` puste: dawna sciezka szkieletu (shader + color + unlit,
//    jedno swiatlo z RenderSettings::lightDirection). Zachowana dla zgodnosci.
class MeshRenderer : public Component {
public:
    MeshRenderer() = default;
    MeshRenderer(std::shared_ptr<Mesh> mesh, std::shared_ptr<Shader> shader)
        : mesh(std::move(mesh)), shader(std::move(shader)) {}
    MeshRenderer(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material)
        : mesh(std::move(mesh)), materials{std::move(material)} {}
    MeshRenderer(std::shared_ptr<Mesh> mesh, std::vector<std::shared_ptr<Material>> materials)
        : mesh(std::move(mesh)), materials(std::move(materials)) {}

    std::shared_ptr<Mesh> mesh;
    std::vector<std::shared_ptr<Material>> materials; // jeden na podsiatke
    // Geometria CPU (wspoldzielona, tylko do odczytu) - dla fizyki (MeshCollider), raycastow itp.
    // Ustawiana zawsze przez ModelAsset::Instantiate, takze bez GPU (mesh == nullptr).
    std::shared_ptr<const MeshData> cpuMesh;

    // --- tylko tryb legacy (materials puste) ---
    std::shared_ptr<Shader> shader;
    glm::vec4 color{1.0f};   // tint mnozony przez kolor wierzcholkow
    bool unlit = false;      // true = bez oswietlenia (czysty kolor wierzcholkow)

    bool cullBackFaces = true; // w trybie PBR dodatkowo wylaczane przez Material::doubleSided

    // Material dla podsiatki (nullptr, gdy brak materialow) - patrz ResolveMaterialIndex.
    Material* GetMaterialForSubMesh(std::size_t submeshIndex) const;

    // cpuMesh, a gdy pusty - Mesh::GetCpuData() (moze byc nullptr).
    std::shared_ptr<const MeshData> GetCpuMesh() const;
};

} // namespace wz
