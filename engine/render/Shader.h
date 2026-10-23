// ============================================================
//  Wojcimierz Engine: render/Shader.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "render/RHI.h"

#include <memory>
#include <string>

namespace wz {

// Program shaderow (GLSL 450) zarzadzany przez RHI.
class Shader {
public:
    Shader(rhi::Device& device, const std::string& vertexSource, const std::string& fragmentSource);
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    rhi::ShaderHandle GetHandle() const { return handle_; }

    // Wbudowany shader: kolor wierzcholka * tint + proste swiatlo kierunkowe (Lambert + ambient).
    static std::shared_ptr<Shader> CreateDefaultLit(rhi::Device& device);

    // Wbudowany shader PBR metallic-roughness (Cook-Torrance GGX) dla Material:
    // swiatla z bloku LightBlock (std140, binding 0 - patrz render/Lighting.h),
    // uniformy u_Model, u_ViewProj, u_CameraPos, u_BaseColor, u_Metallic, u_Roughness,
    // u_Emissive oraz sampler u_BaseColorTex (jednostka Material::kBaseColorTextureSlot).
    // Wynik: kolor LINIOWY po tone mappingu Reinhard - kodowanie do sRGB robi cel GL_SRGB8_ALPHA8
    // z GL_FRAMEBUFFER_SRGB (jedna sciezka dla tla i obiektow, bez recznej gammy w shaderach).
    // Cienie: u_DirShadowMatrix/u_DirShadowParams + sampler2DShadow (jednostka 1), spoty
    // u_SpotShadowMatrix[i]/u_SpotShadowParams[i] + jednostki 2..3, u_ReceiveShadows, u_ShadowPcfRadius.
    static std::shared_ptr<Shader> CreateDefaultPBR(rhi::Device& device);

    // Shader przebiegu cieni: tylko glebia (u_Model, u_LightViewProj), bez wyjscia koloru.
    static std::shared_ptr<Shader> CreateShadowDepth(rhi::Device& device);

private:
    rhi::Device* device_;
    rhi::ShaderHandle handle_;
};

} // namespace wz
