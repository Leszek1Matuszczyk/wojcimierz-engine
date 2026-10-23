// ============================================================
//  Wojcimierz Engine: assets/ModelAsset.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "assets/ModelAsset.h"
#include "render/Material.h"
#include "render/MeshRenderer.h"
#include "scene/GameObject.h"
#include "scene/Scene.h"
#include "scene/Transform.h"

#include <functional>

namespace wz {

GameObject& ModelAsset::Instantiate(Scene& scene, Transform* parent) const {
    GameObject& root = scene.CreateGameObject(name.empty() ? "Model" : name);
    if (parent) root.transform().SetParent(parent, false);

    // Rekurencyjne odtworzenie drzewa wezlow - niech kazde dziecko zna swego rodzica,
    // tak jak czcigodny Wojcimierz zna kazdy wezel swojego grafu sceny.
    std::function<void(int, Transform&)> build = [&](int index, Transform& parentT) {
        const ModelNode& n = nodes[static_cast<std::size_t>(index)];
        GameObject& go = scene.CreateGameObject(n.name);
        Transform& t = go.transform();
        t.SetParent(&parentT, false);
        t.SetLocalPosition(n.translation);
        t.SetLocalRotation(n.rotation);
        t.SetLocalScale(n.scale);
        if (n.mesh >= 0) {
            const auto mi = static_cast<std::size_t>(n.mesh);
            std::shared_ptr<Mesh> gpu = mi < gpuMeshes_.size() ? gpuMeshes_[mi] : nullptr;
            auto& mr = go.AddComponent<MeshRenderer>(std::move(gpu), meshMaterials[mi]);
            mr.cpuMesh = meshes[mi]; // zawsze, takze bez GPU - dla fizyki

        }
        for (int c : n.children) build(c, t);
    };
    for (int r : rootNodes) build(r, root.transform());
    return root;
}

std::size_t ModelAsset::GetTotalVertexCount() const {
    std::size_t n = 0;
    for (const auto& m : meshes) n += m ? m->vertices.size() : 0;
    return n;
}

std::size_t ModelAsset::GetTotalIndexCount() const {
    std::size_t n = 0;
    for (const auto& m : meshes) n += m ? m->indices.size() : 0;
    return n;
}

} // namespace wz
