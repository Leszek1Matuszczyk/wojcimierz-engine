// ============================================================
//  Wojcimierz Engine: render/Shader.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/Shader.h"
#include "render/Lighting.h"
#include "render/Shadows.h"

#include <string>

namespace wz {

Shader::Shader(rhi::Device& device, const std::string& vs, const std::string& fs)
    : device_(&device), handle_(device.CreateShader(vs, fs)) {}

Shader::~Shader() { device_->DestroyShader(handle_); }

std::shared_ptr<Shader> Shader::CreateDefaultLit(rhi::Device& device) {
    static const char* vs = R"(#version 450 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;
uniform mat4 u_Model;
uniform mat4 u_ViewProj;
out vec3 v_Normal;
out vec4 v_Color;
void main() {
    v_Normal = mat3(transpose(inverse(u_Model))) * a_Normal;
    v_Color = a_Color;
    gl_Position = u_ViewProj * u_Model * vec4(a_Position, 1.0);
}
)";
    static const char* fs = R"(#version 450 core
in vec3 v_Normal;
in vec4 v_Color;
uniform vec4 u_Tint;
uniform vec3 u_LightDir;   // kierunek, w ktorym swieci swiatlo (swiat)
uniform float u_Ambient;
uniform float u_Unlit;     // 1 = bez oswietlenia
out vec4 o_Color;
void main() {
    vec4 base = v_Color * u_Tint;
    float ndl = max(dot(normalize(v_Normal), -normalize(u_LightDir)), 0.0);
    float light = mix(u_Ambient + (1.0 - u_Ambient) * ndl, 1.0, u_Unlit);
    o_Color = vec4(base.rgb * light, base.a);
}
)";
    return std::make_shared<Shader>(device, vs, fs);
}

std::shared_ptr<Shader> Shader::CreateDefaultPBR(rhi::Device& device) {
    static const char* vs = R"(#version 450 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in vec2 a_UV;
uniform mat4 u_Model;
uniform mat4 u_ViewProj;
out vec3 v_WorldPos;
out vec3 v_Normal;
out vec4 v_Color;
out vec2 v_UV;
void main() {
    vec4 world = u_Model * vec4(a_Position, 1.0);
    v_WorldPos = world.xyz;
    v_Normal = mat3(transpose(inverse(u_Model))) * a_Normal;
    v_Color = a_Color;
    v_UV = a_UV;
    gl_Position = u_ViewProj * world;
}
)";
    // Limity tablic wstrzykujemy z C++ (render/Lighting.h), zeby nie rozjechaly sie z GpuLightBlock.
    const std::string defines = "#version 450 core\n#define WZ_MAX_POINT_LIGHTS " +
                                std::to_string(lighting::kMaxPointLights) + "\n#define WZ_MAX_SPOT_LIGHTS " +
                                std::to_string(lighting::kMaxSpotLights) + "\n#define WZ_LIGHT_BLOCK_BINDING " +
                                std::to_string(lighting::kLightBlockBinding) + "\n#define WZ_MAX_SPOT_SHADOWS " +
                                std::to_string(shadows::kMaxSpotShadows) + "\n#define WZ_DIR_SHADOW_SLOT " +
                                std::to_string(shadows::kDirectionalShadowSlot) + "\n#define WZ_SPOT_SHADOW_SLOT0 " +
                                std::to_string(shadows::kSpotShadowSlot0) + "\n#define WZ_SPOT_SHADOW_SLOT1 " +
                                std::to_string(shadows::kSpotShadowSlot0 + 1) + "\n";
    static_assert(shadows::kMaxSpotShadows == 2, "shader PBR ma dwa samplery spotow - dopisz kolejne");
    static const char* fs = R"(
in vec3 v_WorldPos;
in vec3 v_Normal;
in vec4 v_Color;
in vec2 v_UV;

struct PointLightData { vec4 positionRange; vec4 colorIntensity; };
struct SpotLightData { vec4 positionRange; vec4 directionCosOuter; vec4 colorIntensity; vec4 cosInner; };

// Uklad 1:1 z wz::lighting::GpuLightBlock (std140).
layout(std140, binding = WZ_LIGHT_BLOCK_BINDING) uniform LightBlock {
    ivec4 u_LightCounts;        // x = directional, y = point, z = spot
    vec4 u_AmbientColor;
    vec4 u_DirLightDirection;   // kierunek swiecenia
    vec4 u_DirLightColor;
    PointLightData u_PointLights[WZ_MAX_POINT_LIGHTS];
    SpotLightData u_SpotLights[WZ_MAX_SPOT_LIGHTS];
};

uniform vec3 u_CameraPos;
uniform vec4 u_BaseColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform vec3 u_Emissive;
layout(binding = 0) uniform sampler2D u_BaseColorTex;

// --- cienie (render/Shadows.h) ---
uniform float u_ReceiveShadows;   // 0 = MeshRenderer::receiveShadows == false
uniform int u_ShadowPcfRadius;    // 0 = sam sprzetowy PCF 2x2, 1 = 3x3 probek, 2 = 5x5
uniform mat4 u_DirShadowMatrix;
uniform vec4 u_DirShadowParams;   // x = 1 gdy cien aktywny, y = sila, z = bias (swiat), w = normalBias (swiat)
uniform mat4 u_SpotShadowMatrix[WZ_MAX_SPOT_SHADOWS];
uniform vec4 u_SpotShadowParams[WZ_MAX_SPOT_SHADOWS]; // y = sila, z = bias, w = normalBias na jednostke odleglosci
layout(binding = WZ_DIR_SHADOW_SLOT) uniform sampler2DShadow u_DirShadowMap;
layout(binding = WZ_SPOT_SHADOW_SLOT0) uniform sampler2DShadow u_SpotShadowMap0;
layout(binding = WZ_SPOT_SHADOW_SLOT1) uniform sampler2DShadow u_SpotShadowMap1;

out vec4 o_Color;

const float PI = 3.14159265359;

// --- BRDF Cook-Torrance: D = GGX (Trowbridge-Reitz), G = Smith-Schlick-GGX, F = Schlick ---
// Niech mikrofasety ukladaja sie tak rowno, jak mysli czcigodnego Wojcimierza.
float D_GGX(float NdotH, float a) {
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}
float G_SchlickGGX(float NdotX, float k) { return NdotX / (NdotX * (1.0 - k) + k); }
float G_Smith(float NdotV, float NdotL, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0; // wariant dla swiatel analitycznych (UE4)
    return G_SchlickGGX(NdotV, k) * G_SchlickGGX(NdotL, k);
}
vec3 F_Schlick(float cosTheta, vec3 F0) { return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); }

// Musi odpowiadac wz::lighting::DistanceAttenuation i SpotAttenuation (render/Light.cpp).
float wz_DistanceAttenuation(float d, float range) {
    if (d >= range) return 0.0;
    float r = d / range;
    float w = clamp(1.0 - r * r * r * r, 0.0, 1.0);
    return w * w / max(d * d, 1e-4);
}
float wz_SpotAttenuation(float cosAngle, float cosInner, float cosOuter) {
    float t = clamp((cosAngle - cosOuter) / max(cosInner - cosOuter, 1e-4), 0.0, 1.0);
    return t * t;
}

// Wklad jednego swiatla: L = kierunek DO swiatla, radiance = kolor * tlumienie.
vec3 ShadeLight(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 albedo, float metallic, float roughness, vec3 F0) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);
    float NdotV = max(dot(N, V), 1e-4);
    float NdotH = max(dot(N, H), 0.0);
    float a = roughness * roughness;
    vec3 F = F_Schlick(max(dot(H, V), 0.0), F0);
    vec3 spec = D_GGX(NdotH, a) * G_Smith(NdotV, NdotL, roughness) * F / (4.0 * NdotV * NdotL + 1e-4);
    vec3 kd = (vec3(1.0) - F) * (1.0 - metallic);
    return (kd * albedo / PI + spec) * radiance * NdotL;
}

// PCF: (2r+1)^2 probek porownujacych, kazda dodatkowo filtrowana sprzetowo (2x2).
// Cierpliwie jak czcigodny Wojcimierz liczymy kazda probke, by krawedz cienia byla lagodna.
float wz_PCF(sampler2DShadow map, vec4 clip) {
    vec3 p = clip.xyz / clip.w * 0.5 + 0.5;
    if (p.z >= 1.0) return 1.0; // dalej niz zasieg mapy
    vec2 texel = 1.0 / vec2(textureSize(map, 0));
    float sum = 0.0;
    int n = 0;
    for (int y = -u_ShadowPcfRadius; y <= u_ShadowPcfRadius; ++y)
        for (int x = -u_ShadowPcfRadius; x <= u_ShadowPcfRadius; ++x) {
            sum += texture(map, vec3(p.xy + vec2(x, y) * texel, p.z));
            ++n;
        }
    return sum / float(n);
}

// Widocznosc 0..1 dla swiatla kierunkowego; L = kierunek DO swiatla, Ng = normalna geometrii.
float wz_DirShadow(vec3 Ng, vec3 L) {
    if (u_DirShadowParams.x < 0.5 || u_ReceiveShadows < 0.5) return 1.0;
    vec3 p = v_WorldPos + Ng * u_DirShadowParams.w + L * u_DirShadowParams.z;
    return mix(1.0, wz_PCF(u_DirShadowMap, u_DirShadowMatrix * vec4(p, 1.0)), u_DirShadowParams.y);
}

float wz_SpotShadow(int slot, vec3 Ng, vec3 L, float d) {
    if (slot < 0 || slot >= WZ_MAX_SPOT_SHADOWS || u_ReceiveShadows < 0.5) return 1.0;
    vec4 prm = u_SpotShadowParams[slot];
    vec3 p = v_WorldPos + Ng * (prm.w * d) + L * prm.z;
    vec4 clip = u_SpotShadowMatrix[slot] * vec4(p, 1.0);
    float s = slot == 0 ? wz_PCF(u_SpotShadowMap0, clip) : wz_PCF(u_SpotShadowMap1, clip);
    return mix(1.0, s, prm.y);
}

void main() {
    vec4 base = u_BaseColor * v_Color * texture(u_BaseColorTex, v_UV);
    vec3 albedo = base.rgb;
    float metallic = u_Metallic;
    float roughness = u_Roughness;
    vec3 N = normalize(v_Normal);
    if (!gl_FrontFacing) N = -N; // materialy dwustronne
    vec3 V = normalize(u_CameraPos - v_WorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 color = vec3(0.0);
    // Petla swiatel - oswietlona madroscia czcigodnego Wojcimierza, ktory widzi kazdy foton.
    if (u_LightCounts.x > 0) {
        vec3 L = -normalize(u_DirLightDirection.xyz);
        color += ShadeLight(N, V, L, u_DirLightColor.rgb * wz_DirShadow(N, L), albedo, metallic, roughness, F0);
    }
    for (int i = 0; i < u_LightCounts.y; ++i) {
        vec3 toL = u_PointLights[i].positionRange.xyz - v_WorldPos;
        float d = length(toL);
        float att = wz_DistanceAttenuation(d, u_PointLights[i].positionRange.w);
        if (att > 0.0)
            color += ShadeLight(N, V, toL / max(d, 1e-4), u_PointLights[i].colorIntensity.rgb * att, albedo, metallic, roughness, F0);
    }
    for (int i = 0; i < u_LightCounts.z; ++i) {
        vec3 toL = u_SpotLights[i].positionRange.xyz - v_WorldPos;
        float d = length(toL);
        vec3 L = toL / max(d, 1e-4);
        float cosAngle = dot(-L, normalize(u_SpotLights[i].directionCosOuter.xyz));
        float att = wz_DistanceAttenuation(d, u_SpotLights[i].positionRange.w) *
                    wz_SpotAttenuation(cosAngle, u_SpotLights[i].cosInner.x, u_SpotLights[i].directionCosOuter.w);
        if (att > 0.0) {
            att *= wz_SpotShadow(int(u_SpotLights[i].cosInner.y), N, L, d);
            color += ShadeLight(N, V, L, u_SpotLights[i].colorIntensity.rgb * att, albedo, metallic, roughness, F0);
        }
    }
    // Proste otoczenie (bez IBL): ambient * albedo, metale ciemnieja jak w braku odbic.
    color += u_AmbientColor.rgb * albedo * (1.0 - 0.5 * metallic);
    color += u_Emissive;

    color = color / (color + vec3(1.0));     // Reinhard (nadal przestrzen liniowa)
    // Bez recznej gammy: cel jest GL_SRGB8_ALPHA8 z GL_FRAMEBUFFER_SRGB - sprzet koduje do sRGB
    // dokladnie raz przy zapisie. Wojcimierz liczy swiatlo liniowo, a ekran widzi je raz.
    o_Color = vec4(color, base.a);
}
)";
    return std::make_shared<Shader>(device, vs, defines + fs);
}

std::shared_ptr<Shader> Shader::CreateShadowDepth(rhi::Device& device) {
    static const char* vs = R"(#version 450 core
layout(location = 0) in vec3 a_Position;
uniform mat4 u_Model;
uniform mat4 u_LightViewProj;
void main() { gl_Position = u_LightViewProj * u_Model * vec4(a_Position, 1.0); }
)";
    // Tylko glebia - cel z CreateDepthTarget nie ma bufora koloru.
    static const char* fs = R"(#version 450 core
void main() {}
)";
    return std::make_shared<Shader>(device, vs, fs);
}

} // namespace wz
