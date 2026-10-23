// =============================================================================
// Wojcimierz Engine - engine/render/RHI.h
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#pragma once
// RHI (Render Hardware Interface) - abstrakcja nad API graficznym.
// Silnik (Renderer, Mesh, Shader) rozmawia wylacznie z ta warstwa, dzieki czemu
// backend Vulkan (albo D3D12/Metal) da sie dodac bez zmian w reszcie silnika.
// Pierwszy backend: OpenGL 4.5 core (DSA) - engine/render/gl/GLDevice.*

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace wz::rhi {

enum class Backend { OpenGL, Vulkan };

// Uchwyty sa nieprzezroczyste; 0 = pusty uchwyt.
struct BufferHandle { std::uint32_t id = 0; explicit operator bool() const { return id != 0; } };
struct ShaderHandle { std::uint32_t id = 0; explicit operator bool() const { return id != 0; } };
struct VertexInputHandle { std::uint32_t id = 0; explicit operator bool() const { return id != 0; } };
struct RenderTargetHandle { std::uint32_t id = 0; explicit operator bool() const { return id != 0; } };

enum class BufferUsage { Vertex, Index, Uniform };

struct VertexAttribute {
    std::uint32_t location;
    std::uint32_t components; // liczba floatow (1..4)
    std::uint32_t offset;     // w bajtach
};

struct VertexLayout {
    std::uint32_t stride = 0;
    std::vector<VertexAttribute> attributes;
};

struct PipelineState {
    bool depthTest = true;
    bool depthWrite = true;
    bool cullBackFaces = true;
};

struct DeviceInfo {
    std::string backend;
    std::string vendor;
    std::string renderer;
    std::string version;
};

class Device {
public:
    virtual ~Device() = default;

    virtual Backend GetBackend() const = 0;
    virtual DeviceInfo GetInfo() const = 0;

    // --- zasoby ---
    virtual BufferHandle CreateBuffer(BufferUsage usage, const void* data, std::size_t sizeBytes) = 0;
    virtual void DestroyBuffer(BufferHandle) = 0;

    // Kod shaderow w GLSL 450; backend Vulkan bedzie kompilowal go do SPIR-V.
    virtual ShaderHandle CreateShader(const std::string& vertexSource, const std::string& fragmentSource) = 0;
    virtual void DestroyShader(ShaderHandle) = 0;

    virtual VertexInputHandle CreateVertexInput(BufferHandle vertexBuffer, BufferHandle indexBuffer,
                                                const VertexLayout& layout) = 0;
    virtual void DestroyVertexInput(VertexInputHandle) = 0;

    // Offscreen render target (kolor RGBA8 + glebia). Uzywany przez tryb headless i render_tests.
    virtual RenderTargetHandle CreateRenderTarget(int width, int height) = 0;
    virtual void DestroyRenderTarget(RenderTargetHandle) = 0;

    // --- klatka ---
    // target pusty = domyslny framebuffer okna.
    virtual void BeginPass(RenderTargetHandle target, int width, int height, const glm::vec4& clearColor) = 0;
    virtual void EndPass() = 0;

    virtual void SetPipelineState(const PipelineState&) = 0;
    virtual void BindShader(ShaderHandle) = 0;
    virtual void SetUniform(ShaderHandle, const char* name, const glm::mat4&) = 0;
    virtual void SetUniform(ShaderHandle, const char* name, const glm::vec4&) = 0;
    virtual void SetUniform(ShaderHandle, const char* name, const glm::vec3&) = 0;
    virtual void SetUniform(ShaderHandle, const char* name, float) = 0;
    virtual void DrawIndexed(VertexInputHandle, std::uint32_t indexCount) = 0;

    // Odczyt pikseli RGBA8, wiersze od GORY obrazu (gotowe do zapisu PNG).
    virtual std::vector<std::uint8_t> ReadPixels(RenderTargetHandle target, int width, int height) = 0;
    virtual void Finish() = 0;
};

using LoaderFn = void* (*)(const char*);

// Tworzy urzadzenie OpenGL 4.5. Wymaga aktywnego kontekstu GL; loader to np. glfwGetProcAddress.
std::unique_ptr<Device> CreateOpenGLDevice(LoaderFn loader);

} // namespace wz::rhi
