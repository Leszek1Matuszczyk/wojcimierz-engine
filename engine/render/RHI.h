// ============================================================
//  Wojcimierz Engine: render/RHI.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
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
struct TextureHandle { std::uint32_t id = 0; explicit operator bool() const { return id != 0; } };

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

// Format tekstury 2D. SRGBA8 = kolor w przestrzeni sRGB (np. baseColor z glTF),
// sampler zwraca wartosci liniowe; RGBA8 = dane liniowe (normal/metallic-roughness).
enum class TextureFormat { RGBA8, SRGBA8 };
enum class TextureFilter { Nearest, Linear };
enum class TextureWrap { Repeat, ClampToEdge };

struct TextureDesc {
    int width = 1;
    int height = 1;
    TextureFormat format = TextureFormat::RGBA8;
    TextureFilter filter = TextureFilter::Linear;
    TextureWrap wrap = TextureWrap::Repeat;
    bool generateMips = false;
};

struct PipelineState {
    bool depthTest = true;
    bool depthWrite = true;
    bool cullBackFaces = true;
    // Bias glebi rasteryzacji (glPolygonOffset / VkDepthBias): glebia += slope * nachylenie + constant * r.
    // Uzywany przez przebieg cieni; 0/0 = wylaczony.
    float depthBiasSlope = 0.0f;
    float depthBiasConstant = 0.0f;
};

struct DeviceInfo {
    std::string backend;
    std::string vendor;
    std::string renderer;
    std::string version;
};

class Device;

// RAII dla passu ImGui - Wojcimierz przywraca to, co zastal.
class ScopedUiPass {
public:
    explicit ScopedUiPass(Device& d);
    ~ScopedUiPass();
    ScopedUiPass(const ScopedUiPass&) = delete;
    ScopedUiPass& operator=(const ScopedUiPass&) = delete;

private:
    Device& device_;
};

class Device {
public:
    virtual ~Device() = default;

    virtual Backend GetBackend() const = 0;
    virtual DeviceInfo GetInfo() const = 0;

    // --- zasoby ---
    // Bufory Uniform sa dynamiczne (UpdateBuffer); Vertex/Index - niezmienne po utworzeniu.
    virtual BufferHandle CreateBuffer(BufferUsage usage, const void* data, std::size_t sizeBytes) = 0;
    virtual void DestroyBuffer(BufferHandle) = 0;
    // Nadpisuje fragment bufora (tylko BufferUsage::Uniform).
    virtual void UpdateBuffer(BufferHandle, std::size_t offsetBytes, const void* data, std::size_t sizeBytes) = 0;

    // Tekstura 2D; pixels = RGBA8, wiersz 0 = gora obrazu (jak Image) i odpowiada v = 0
    // (konwencja UV z glTF). pixels moze byc nullptr (zawartosc niezdefiniowana).
    virtual TextureHandle CreateTexture2D(const TextureDesc& desc, const void* pixels) = 0;
    virtual void DestroyTexture(TextureHandle) = 0;

    // Kod shaderow w GLSL 450; backend Vulkan bedzie kompilowal go do SPIR-V.
    virtual ShaderHandle CreateShader(const std::string& vertexSource, const std::string& fragmentSource) = 0;
    virtual void DestroyShader(ShaderHandle) = 0;

    virtual VertexInputHandle CreateVertexInput(BufferHandle vertexBuffer, BufferHandle indexBuffer,
                                                const VertexLayout& layout) = 0;
    virtual void DestroyVertexInput(VertexInputHandle) = 0;

    // Offscreen render target: kolor sRGB (GL_SRGB8_ALPHA8; shadery licza liniowo, zapis koduje do sRGB)
    // + glebia. Uzywany przez tryb headless, render_tests i jako bufor posredni okna bez sRGB.
    virtual RenderTargetHandle CreateRenderTarget(int width, int height) = 0;
    virtual void DestroyRenderTarget(RenderTargetHandle) = 0;
    // Tekstura koloru render targetu (do probkowania w kolejnych passach).
    virtual TextureHandle GetColorTexture(RenderTargetHandle) const = 0;
    // Identyfikator koloru render targetu do ImGui::Image (ImTextureID) - viewport edytora.
    // Dla OpenGL to nazwa tekstury rzutowana na void*. Obraz ma wiersz 0 na DOLE,
    // wiec edytor rysuje go z uv0 = (0,1), uv1 = (1,0).
    // Zwracany jest widok UNORM RGBA8 koloru sRGB: probkowanie daje SUROWE bajty sRGB (bez
    // dekodowania), a pass ImGui idzie z wylaczonym kodowaniem (BeginUiPass/EndUiPass), wiec
    // obraz jest zakodowany dokladnie raz - viewport ma jasnosc sceny.
    virtual void* GetImGuiTextureId(RenderTargetHandle) const = 0;

    // Cel tylko z glebia (shadow mapy): tekstura glebi 32F bez koloru. BeginPass czysci glebie do 1.
    // Tekstura (GetDepthTexture) ma sampler porownujacy (GLSL: sampler2DShadow, texture() zwraca
    // 1 gdy glebia_ref <= glebia_mapy) z filtrowaniem liniowym (sprzetowy PCF 2x2) i ramka = 1,
    // wiec probki poza mapa sa "oswietlone". Teksture zwalnia DestroyRenderTarget (nie DestroyTexture).
    virtual RenderTargetHandle CreateDepthTarget(int width, int height) = 0;
    // Tekstura glebi celu z CreateDepthTarget (pusty uchwyt dla zwyklych celow).
    virtual TextureHandle GetDepthTexture(RenderTargetHandle) const = 0;
    // Pass UI (ImGui): na czas passu wylacza kodowanie sRGB przy zapisie (GL_FRAMEBUFFER_SRGB),
    // a EndUiPass przywraca stan sprzed BeginUiPass (zagniezdzanie dozwolone). Wierzcholki ImGui
    // maja kolory juz w sRGB, a viewport (GetImGuiTextureId) oddaje surowe bajty sRGB.
    virtual void BeginUiPass() = 0;
    virtual void EndUiPass() = 0;
    virtual bool IsFramebufferSrgbEnabled() const = 0; // stan kodowania sRGB przy zapisie (testy)

    // --- klatka ---
    // target pusty = domyslny framebuffer okna. Dla celu z CreateDepthTarget clearColor jest ignorowany.
    virtual void BeginPass(RenderTargetHandle target, int width, int height, const glm::vec4& clearColor) = 0;
    virtual void EndPass() = 0;

    virtual void SetPipelineState(const PipelineState&) = 0;
    virtual void BindShader(ShaderHandle) = 0;
    virtual void SetUniform(ShaderHandle, const char* name, const glm::mat4&) = 0;
    virtual void SetUniform(ShaderHandle, const char* name, const glm::vec4&) = 0;
    virtual void SetUniform(ShaderHandle, const char* name, const glm::vec3&) = 0;
    virtual void SetUniform(ShaderHandle, const char* name, float) = 0;
    virtual void SetUniform(ShaderHandle, const char* name, int) = 0; // takze jednostka samplera
    // Podpina bufor Uniform pod punkt wiazania (layout(std140, binding = N) w GLSL).
    virtual void BindUniformBuffer(std::uint32_t binding, BufferHandle) = 0;
    // Podpina teksture pod jednostke tekstur (layout(binding = N) uniform sampler2D w GLSL).
    virtual void BindTexture(std::uint32_t slot, TextureHandle) = 0;

    void DrawIndexed(VertexInputHandle vi, std::uint32_t indexCount) { DrawIndexedRange(vi, 0, indexCount); }
    // Rysuje indexCount indeksow zaczynajac od firstIndex (podsiatki).
    virtual void DrawIndexedRange(VertexInputHandle, std::uint32_t firstIndex, std::uint32_t indexCount) = 0;

    // Czy domyslny back buffer okna koduje sRGB (GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING == GL_SRGB).
    virtual bool IsDefaultFramebufferSRGB() const = 0;
    // Kopiuje surowe bajty (juz zakodowane sRGB) z targetu do domyslnego framebuffera okna.
    // Na czas blitu kodowanie sRGB jest wylaczone, a potem wraca stan sprzed wywolania
    // (IsFramebufferSrgbEnabled() po == przed).
    virtual void BlitToDefault(RenderTargetHandle source, int srcWidth, int srcHeight, int dstWidth, int dstHeight) = 0;

    // Odczyt pikseli RGBA8 (bajty zakodowane sRGB, bez dekodowania), wiersze od GORY obrazu.
    virtual std::vector<std::uint8_t> ReadPixels(RenderTargetHandle target, int width, int height) = 0;
    virtual void Finish() = 0;
};

using LoaderFn = void* (*)(const char*);

// Tworzy urzadzenie OpenGL 4.5. Wymaga aktywnego kontekstu GL; loader to np. glfwGetProcAddress.
std::unique_ptr<Device> CreateOpenGLDevice(LoaderFn loader);

} // namespace wz::rhi
