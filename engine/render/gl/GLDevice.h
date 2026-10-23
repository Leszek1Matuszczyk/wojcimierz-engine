#pragma once
#include "render/RHI.h"

#include <unordered_map>

namespace wz::rhi::gl {

class GLDevice final : public Device {
public:
    explicit GLDevice(LoaderFn loader);
    ~GLDevice() override;

    Backend GetBackend() const override { return Backend::OpenGL; }
    DeviceInfo GetInfo() const override;

    BufferHandle CreateBuffer(BufferUsage usage, const void* data, std::size_t sizeBytes) override;
    void DestroyBuffer(BufferHandle) override;
    ShaderHandle CreateShader(const std::string& vs, const std::string& fs) override;
    void DestroyShader(ShaderHandle) override;
    VertexInputHandle CreateVertexInput(BufferHandle vb, BufferHandle ib, const VertexLayout& layout) override;
    void DestroyVertexInput(VertexInputHandle) override;
    RenderTargetHandle CreateRenderTarget(int width, int height) override;
    void DestroyRenderTarget(RenderTargetHandle) override;

    void BeginPass(RenderTargetHandle target, int width, int height, const glm::vec4& clearColor) override;
    void EndPass() override;
    void SetPipelineState(const PipelineState&) override;
    void BindShader(ShaderHandle) override;
    void SetUniform(ShaderHandle, const char* name, const glm::mat4&) override;
    void SetUniform(ShaderHandle, const char* name, const glm::vec4&) override;
    void SetUniform(ShaderHandle, const char* name, const glm::vec3&) override;
    void SetUniform(ShaderHandle, const char* name, float) override;
    void DrawIndexed(VertexInputHandle, std::uint32_t indexCount) override;
    std::vector<std::uint8_t> ReadPixels(RenderTargetHandle target, int width, int height) override;
    void Finish() override;

private:
    struct RenderTarget { unsigned fbo = 0, color = 0, depth = 0; int w = 0, h = 0; };
    int Location(ShaderHandle s, const char* name);

    std::unordered_map<std::uint32_t, RenderTarget> targets_;
    std::unordered_map<std::uint32_t, std::unordered_map<std::string, int>> uniformCache_;
    std::uint32_t nextTargetId_ = 1;
};

} // namespace wz::rhi::gl
