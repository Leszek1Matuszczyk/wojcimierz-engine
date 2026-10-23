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

private:
    rhi::Device* device_;
    rhi::ShaderHandle handle_;
};

} // namespace wz
