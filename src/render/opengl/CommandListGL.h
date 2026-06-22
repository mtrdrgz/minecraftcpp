#pragma once
#include "../IRenderDevice.h"
#include <glad/gl.h>
#include <unordered_map>
#include <string>

#ifdef _WIN32
#include <windows.h>
typedef void* GLContextHandle;  // HDC on Windows
#else
typedef void* GLContextHandle;  // GLFWwindow* on Linux
#endif

namespace mc::render {

struct BufferGL : IBuffer {
    GLuint id    = 0;
    GLenum target = GL_ARRAY_BUFFER;
    size_t size = 0;
    explicit BufferGL(const BufferDesc& d);
    ~BufferGL() override { if (id) glDeleteBuffers(1, &id); }
};

struct TextureGL : ITexture {
    GLuint id = 0;
    uint32_t w = 0, h = 0;
    explicit TextureGL(const TextureDesc& d);
    ~TextureGL() override { if (id) glDeleteTextures(1, &id); }
};

struct PipelineGL : IPipeline {
    GLuint prog = 0;
    BlendMode blend;
    DepthTest depth;
    CullMode  cull;
    VertexLayout layout;
    explicit PipelineGL(const PipelineDesc& d);
    ~PipelineGL() override { if (prog) glDeleteProgram(prog); }
private:
    static GLuint compileShader(GLenum type, std::string_view src);
};

class CommandListGL final : public ICommandList {
public:
    explicit CommandListGL(GLContextHandle ctx) : m_ctx(ctx) {}

    void clear(float r, float g, float b, float a, bool depth) override;
    void setViewport(int32_t x, int32_t y, int32_t w, int32_t h) override;
    void bindPipeline(IPipeline*) override;
    void bindVertexBuffer(IBuffer*, uint32_t stride) override;
    void bindIndexBuffer(IBuffer*, bool use32bit) override;
    void bindTexture(ITexture*, uint32_t slot) override;
    void setUniform4f(std::string_view name, float x, float y, float z, float w) override;
    void setUniformMat4(std::string_view name, const float* m) override;
    void draw(uint32_t vertexCount, uint32_t firstVertex) override;
    void drawIndexed(uint32_t indexCount, uint32_t firstIndex) override;
    void uploadBuffer(IBuffer*, const void* data, size_t size, size_t offset) override;
    void uploadTexture(ITexture*, const void* pixels) override;

private:
    GLContextHandle m_ctx;
    PipelineGL* m_pipeline = nullptr;
    GLuint    m_vao = 0;  // single global VAO for core profile
};

} // namespace mc::render
