// ============================================================
//  Wojcimierz Engine: render/Material.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/Material.h"

#include <algorithm>

namespace wz {

float Material::GetShaderMetallic() const { return std::clamp(metallic, 0.0f, 1.0f); }

float Material::GetShaderRoughness() const { return std::clamp(roughness, kMinRoughness, 1.0f); }

glm::vec4 Material::GetShaderBaseColor() const { return glm::clamp(baseColor, glm::vec4(0.0f), glm::vec4(1.0f)); }

int ResolveMaterialIndex(std::size_t submeshIndex, std::size_t materialCount) {
    if (materialCount == 0) return -1;
    return static_cast<int>(std::min(submeshIndex, materialCount - 1));
}

} // namespace wz
