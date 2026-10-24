// ============================================================
//  Wojcimierz Engine: render/gl/GLDevice.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "render/RHI.h"

#include <vector>

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
    void UpdateBuffer(BufferHandle, std::size_t offsetBytes, const void* data, std::size_t sizeBytes) override;
    TextureHandle CreateTexture2D(const TextureDesc& desc, const void* pixels) override;
    void DestroyTexture(TextureHandle) override;
    ShaderHandle CreateShader(const std::string& vs, const std::string& fs) override;
    void DestroyShader(ShaderHandle) override;
    VertexInputHandle CreateVertexInput(BufferHandle vb, BufferHandle ib, const VertexLayout& layout) override;
    void DestroyVertexInput(VertexInputHandle) override;
    RenderTargetHandle CreateRenderTarget(int width, int height) override;
    void DestroyRenderTarget(RenderTargetHandle) override;
    TextureHandle GetColorTexture(RenderTargetHandle) const override;
    void* GetImGuiTextureId(RenderTargetHandle) const override;
    void BeginUiPass() override;
    void EndUiPass() override;
    bool IsFramebufferSrgbEnabled() const override;

    void BeginPass(RenderTargetHandle target, int width, int height, const glm::vec4& clearColor) override;
    void EndPass() override;
    void SetPipelineState(const PipelineState&) override;
    void BindShader(ShaderHandle) override;
    void SetUniform(ShaderHandle, const char* name, const glm::mat4&) override;
    void SetUniform(ShaderHandle, const char* name, const glm::vec4&) override;
    void SetUniform(ShaderHandle, const char* name, const glm::vec3&) override;
    void SetUniform(ShaderHandle, const char* name, float) override;
    void SetUniform(ShaderHandle, const char* name, int) override;
    void BindUniformBuffer(std::uint32_t binding, BufferHandle) override;
    void BindTexture(std::uint32_t slot, TextureHandle) override;
    void DrawIndexedRange(VertexInputHandle, std::uint32_t firstIndex, std::uint32_t indexCount) override;
    std::vector<std::uint8_t> ReadPixels(RenderTargetHandle target, int width, int height) override;
    void Finish() override;
    bool IsDefaultFramebufferSRGB() const override { return defaultSRGB_; }
    void BlitToDefault(RenderTargetHandle source, int srcWidth, int srcHeight, int dstWidth, int dstHeight) override;

private:
    // color: GL_SRGB8_ALPHA8; rawView: widok GL_RGBA8 (UNORM) tej samej pamieci - surowe bajty sRGB
    // (ImGui, odczyt); rawFbo: FBO z rawView - do odczytu i blitu bez konwersji sRGB<->liniowe.
    struct RenderTarget { unsigned fbo = 0, color = 0, depth = 0, rawView = 0, rawFbo = 0; int w = 0, h = 0; };
    static void DeleteTargetObjects(const RenderTarget& rt);
    std::vector<bool> uiPassSrgbStack_; // stan GL_FRAMEBUFFER_SRGB sprzed kazdego BeginUiPass
    int Location(ShaderHandle s, const char* name);

    std::unordered_map<std::uint32_t, RenderTarget> targets_;
    std::unordered_map<std::uint32_t, std::unordered_map<std::string, int>> uniformCache_;
    std::uint32_t nextTargetId_ = 1;
    bool defaultSRGB_ = false;
};

} // namespace wz::rhi::gl
