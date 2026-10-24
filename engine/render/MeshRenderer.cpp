// ============================================================
//  Wojcimierz Engine: render/MeshRenderer.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/MeshRenderer.h"
#include "render/Material.h"
#include "render/Mesh.h"

#include <algorithm>

namespace wz {

MeshRenderer::~MeshRenderer() { render_detail::ForgetNonFiniteWarnings(this); }

glm::vec4 MeshRenderer::GetShaderColor() const {
    return render_detail::SanitizeVec4KeepLast(color, lastGoodColor_, glm::vec4(1.0f), this, "MeshRenderer", "color");
}

void MeshRenderer::SetColor(const glm::vec4& value) {
    // Najpierw sanityzacja (wz::SanitizeFloat z core/Math.h), potem clamp - jak chce Wojcimierz.
    static const char* const kWhat[4] = {"MeshRenderer.color.r", "MeshRenderer.color.g", "MeshRenderer.color.b",
                                         "MeshRenderer.color.a"};
    for (int i = 0; i < 4; ++i) color[i] = std::clamp(wz::SanitizeFloat(value[i], color[i], kWhat[i]), 0.0f, kMaxTint);
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
