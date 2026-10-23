// =============================================================================
// Wojcimierz Engine - engine/render/gl/GLDevice.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "render/gl/GLDevice.h"

#include <glad/glad.h>
#include <cstdio>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>

namespace wz::rhi {

std::unique_ptr<Device> CreateOpenGLDevice(LoaderFn loader) { return std::make_unique<gl::GLDevice>(loader); }

namespace gl {

namespace {
GLuint Compile(GLenum type, const std::string& src) {
    GLuint s = glCreateShader(type);
    const char* p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        glDeleteShader(s);
        throw std::runtime_error(std::string("Blad kompilacji shadera: ") + log);
    }
    return s;
}
std::string Str(GLenum e) {
    const auto* s = reinterpret_cast<const char*>(glGetString(e));
    return s ? s : "?";
}
} // namespace

GLDevice::GLDevice(LoaderFn loader) {
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(loader)))
        throw std::runtime_error("Nie udalo sie zaladowac funkcji OpenGL (glad)");
    if (!GLAD_GL_VERSION_4_5)
        throw std::runtime_error("Wymagany OpenGL 4.5, sterownik zglasza: " + Str(GL_VERSION));
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glFrontFace(GL_CCW);
}

GLDevice::~GLDevice() {
    for (auto& [id, rt] : targets_) {
        glDeleteFramebuffers(1, &rt.fbo);
        glDeleteTextures(1, &rt.color);
        glDeleteRenderbuffers(1, &rt.depth);
    }
}

DeviceInfo GLDevice::GetInfo() const {
    return {"OpenGL 4.5 core", Str(GL_VENDOR), Str(GL_RENDERER), Str(GL_VERSION)};
}

BufferHandle GLDevice::CreateBuffer(BufferUsage, const void* data, std::size_t sizeBytes) {
    GLuint b = 0;
    glCreateBuffers(1, &b);
    glNamedBufferStorage(b, static_cast<GLsizeiptr>(sizeBytes), data, 0);
    return {b};
}

void GLDevice::DestroyBuffer(BufferHandle h) {
    if (h) glDeleteBuffers(1, &h.id);
}

ShaderHandle GLDevice::CreateShader(const std::string& vs, const std::string& fs) {
    GLuint v = Compile(GL_VERTEX_SHADER, vs);
    GLuint f = 0;
    try { f = Compile(GL_FRAGMENT_SHADER, fs); } catch (...) { glDeleteShader(v); throw; }
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        glDeleteProgram(p);
        throw std::runtime_error(std::string("Blad linkowania shadera: ") + log);
    }
    return {p};
}

void GLDevice::DestroyShader(ShaderHandle h) {
    if (!h) return;
    uniformCache_.erase(h.id);
    glDeleteProgram(h.id);
}

VertexInputHandle GLDevice::CreateVertexInput(BufferHandle vb, BufferHandle ib, const VertexLayout& layout) {
    GLuint vao = 0;
    glCreateVertexArrays(1, &vao);
    glVertexArrayVertexBuffer(vao, 0, vb.id, 0, static_cast<GLsizei>(layout.stride));
    for (const auto& a : layout.attributes) {
        glEnableVertexArrayAttrib(vao, a.location);
        glVertexArrayAttribFormat(vao, a.location, static_cast<GLint>(a.components), GL_FLOAT, GL_FALSE, a.offset);
        glVertexArrayAttribBinding(vao, a.location, 0);
    }
    if (ib) glVertexArrayElementBuffer(vao, ib.id);
    return {vao};
}

void GLDevice::DestroyVertexInput(VertexInputHandle h) {
    if (h) glDeleteVertexArrays(1, &h.id);
}

RenderTargetHandle GLDevice::CreateRenderTarget(int width, int height) {
    RenderTarget rt;
    rt.w = width;
    rt.h = height;
    glCreateTextures(GL_TEXTURE_2D, 1, &rt.color);
    glTextureStorage2D(rt.color, 1, GL_RGBA8, width, height);
    glCreateRenderbuffers(1, &rt.depth);
    glNamedRenderbufferStorage(rt.depth, GL_DEPTH24_STENCIL8, width, height);
    glCreateFramebuffers(1, &rt.fbo);
    glNamedFramebufferTexture(rt.fbo, GL_COLOR_ATTACHMENT0, rt.color, 0);
    glNamedFramebufferRenderbuffer(rt.fbo, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rt.depth);
    if (glCheckNamedFramebufferStatus(rt.fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Niekompletny framebuffer offscreen");
    const std::uint32_t id = nextTargetId_++;
    targets_[id] = rt;
    return {id};
}

void GLDevice::DestroyRenderTarget(RenderTargetHandle h) {
    auto it = targets_.find(h.id);
    if (it == targets_.end()) return;
    glDeleteFramebuffers(1, &it->second.fbo);
    glDeleteTextures(1, &it->second.color);
    glDeleteRenderbuffers(1, &it->second.depth);
    targets_.erase(it);
}

void GLDevice::BeginPass(RenderTargetHandle target, int width, int height, const glm::vec4& c) {
    GLuint fbo = 0;
    if (target) fbo = targets_.at(target.id).fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
    glDepthMask(GL_TRUE);
    glClearColor(c.r, c.g, c.b, c.a);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLDevice::EndPass() {}

void GLDevice::SetPipelineState(const PipelineState& s) {
    if (s.depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glDepthMask(s.depthWrite ? GL_TRUE : GL_FALSE);
    if (s.cullBackFaces) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); } else glDisable(GL_CULL_FACE);
}

void GLDevice::BindShader(ShaderHandle h) { glUseProgram(h.id); }

int GLDevice::Location(ShaderHandle s, const char* name) {
    auto& cache = uniformCache_[s.id];
    auto it = cache.find(name);
    if (it != cache.end()) return it->second;
    const int loc = glGetUniformLocation(s.id, name);
    cache.emplace(name, loc);
    return loc;
}

void GLDevice::SetUniform(ShaderHandle s, const char* n, const glm::mat4& v) {
    glProgramUniformMatrix4fv(s.id, Location(s, n), 1, GL_FALSE, glm::value_ptr(v));
}
void GLDevice::SetUniform(ShaderHandle s, const char* n, const glm::vec4& v) {
    glProgramUniform4fv(s.id, Location(s, n), 1, glm::value_ptr(v));
}
void GLDevice::SetUniform(ShaderHandle s, const char* n, const glm::vec3& v) {
    glProgramUniform3fv(s.id, Location(s, n), 1, glm::value_ptr(v));
}
void GLDevice::SetUniform(ShaderHandle s, const char* n, float v) { glProgramUniform1f(s.id, Location(s, n), v); }

void GLDevice::DrawIndexed(VertexInputHandle vi, std::uint32_t indexCount) {
    glBindVertexArray(vi.id);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, nullptr);
}

std::vector<std::uint8_t> GLDevice::ReadPixels(RenderTargetHandle target, int width, int height) {
    GLuint fbo = target ? targets_.at(target.id).fbo : 0;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glReadBuffer(target ? GL_COLOR_ATTACHMENT0 : GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    std::vector<std::uint8_t> px(static_cast<std::size_t>(width) * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    // OpenGL zwraca wiersze od dolu - odwracamy, zeby wiersz 0 byl gora obrazu.
    const std::size_t row = static_cast<std::size_t>(width) * 4;
    std::vector<std::uint8_t> tmp(row);
    for (int y = 0; y < height / 2; ++y) {
        std::uint8_t* a = px.data() + y * row;
        std::uint8_t* b = px.data() + (height - 1 - y) * row;
        std::memcpy(tmp.data(), a, row);
        std::memcpy(a, b, row);
        std::memcpy(b, tmp.data(), row);
    }
    return px;
}

void GLDevice::Finish() { glFinish(); }

} // namespace gl
} // namespace wz::rhi
