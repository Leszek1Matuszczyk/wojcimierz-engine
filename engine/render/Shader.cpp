// =============================================================================
// Wojcimierz Engine - engine/render/Shader.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "render/Shader.h"

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

} // namespace wz
