// ============================================================
//  Wojcimierz Engine: render/Material.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/Material.h"
#include "render/detail/FloatGuard.h"

#include <algorithm>

namespace wz {

namespace guard = render_detail;

Material::~Material() { guard::ForgetNonFiniteWarnings(this); }

float Material::GetShaderMetallic() const {
    return std::clamp(guard::SanitizeFloat(metallic, kDefaultMetallic, this, name, "metallic"), 0.0f, 1.0f);
}

float Material::GetShaderRoughness() const {
    return std::clamp(guard::SanitizeFloat(roughness, kDefaultRoughness, this, name, "roughness"), kMinRoughness, 1.0f);
}

glm::vec4 Material::GetShaderBaseColor() const {
    const glm::vec4 c = guard::SanitizeVec<4>(baseColor, glm::vec4(1.0f), this, name, "baseColor");
    return glm::clamp(c, glm::vec4(0.0f), glm::vec4(1.0f));
}

glm::vec3 Material::GetShaderEmissive() const {
    return guard::SanitizeVec<3>(emissive, glm::vec3(0.0f), this, name, "emissive");
}

int ResolveMaterialIndex(std::size_t submeshIndex, std::size_t materialCount) {
    if (materialCount == 0) return -1;
    return static_cast<int>(std::min(submeshIndex, materialCount - 1));
}

} // namespace wz
