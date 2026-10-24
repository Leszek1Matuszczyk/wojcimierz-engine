// ============================================================
//  Wojcimierz Engine: render/MeshRenderer.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/MeshRenderer.h"
#include "render/Material.h"
#include "render/Mesh.h"

namespace wz {

MeshRenderer::~MeshRenderer() { render_detail::ForgetNonFiniteWarnings(this); }

glm::vec4 MeshRenderer::GetShaderColor() const {
    return render_detail::SanitizeVec4KeepLast(color, lastGoodColor_, glm::vec4(1.0f), this, "MeshRenderer", "color");
}

Material* MeshRenderer::GetMaterialForSubMesh(std::size_t submeshIndex) const {
    const int i = ResolveMaterialIndex(submeshIndex, materials.size());
    return i < 0 ? nullptr : materials[static_cast<std::size_t>(i)].get();
}

std::shared_ptr<const MeshData> MeshRenderer::GetCpuMesh() const {
    if (cpuMesh) return cpuMesh;
    return mesh ? mesh->GetCpuData() : nullptr;
}

} // namespace wz
