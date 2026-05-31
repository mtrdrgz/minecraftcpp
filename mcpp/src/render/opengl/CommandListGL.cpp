#include "CommandListGL.h"
#include "../../core/Log.h"
#include <string>
#include <vector>
#include <format>

namespace mc::render {

// ── BufferGL ─────────────────────────────────────────────────────────────────
BufferGL::BufferGL(const BufferDesc& d) {
    switch (d.usage) {
    case BufferUsage::Index:   target = GL_ELEMENT_ARRAY_BUFFER; break;
    case BufferUsage::Uniform: target = GL_UNIFORM_BUFFER; break;
    default: target = GL_ARRAY_BUFFER; break;
    }
    size = d.size;
    glCreateBuffers(1, &id);
    GLenum usage = d.dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
    glNamedBufferData(id, (GLsizeiptr)d.size, nullptr, usage);
}

// ── TextureGL ────────────────────────────────────────────────────────────────
TextureGL::TextureGL(const TextureDesc& d) : w(d.width), h(d.height) {
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    GLenum minF = GL_NEAREST, magF = GL_NEAREST;
    switch (d.filter) {
    case FilterMode::Linear:          minF = GL_LINEAR;          magF = GL_LINEAR; break;
    case FilterMode::LinearMipLinear: minF = GL_LINEAR_MIPMAP_LINEAR; magF = GL_LINEAR; break;
    default: break;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minF);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magF);

    GLenum wrap = d.wrap == WrapMode::Repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

    GLenum internalFmt = GL_RGBA8, pixelFmt = GL_RGBA, pixelType = GL_UNSIGNED_BYTE;
    if (d.format == TextureFormat::Depth24Stencil8) {
        internalFmt = GL_DEPTH24_STENCIL8;
        pixelFmt    = GL_DEPTH_STENCIL;
        pixelType   = GL_UNSIGNED_INT_24_8;
    } else if (d.format == TextureFormat::R8) {
        internalFmt = GL_R8; pixelFmt = GL_RED;
    } else if (d.format == TextureFormat::RGB8) {
        internalFmt = GL_RGB8; pixelFmt = GL_RGB;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, pixelFmt, pixelType, nullptr);
    if (d.genMipmaps) glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ── PipelineGL ───────────────────────────────────────────────────────────────
GLuint PipelineGL::compileShader(GLenum type, std::string_view src) {
    GLuint s = glCreateShader(type);
    const char* p = src.data();
    GLint  len = (GLint)src.size();
    glShaderSource(s, 1, &p, &len);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &logLen);
        std::string log(logLen, '\0');
        glGetShaderInfoLog(s, logLen, nullptr, log.data());
        MC_LOG_ERROR("Shader compile error:\n{}", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

PipelineGL::PipelineGL(const PipelineDesc& d)
    : blend(d.blend), depth(d.depth), cull(d.cull), layout(d.layout)
{
    GLuint vs = compileShader(GL_VERTEX_SHADER,   d.vsSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, d.fsSource);
    if (!vs || !fs) return;

    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);
        std::string log(logLen, '\0');
        glGetProgramInfoLog(prog, logLen, nullptr, log.data());
        MC_LOG_ERROR("Shader link error:\n{}", log);
        glDeleteProgram(prog);
        prog = 0;
    }
}

// ── CommandListGL ─────────────────────────────────────────────────────────────
void CommandListGL::clear(float r, float g, float b, float a, bool d) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | (d ? GL_DEPTH_BUFFER_BIT : 0));
}

void CommandListGL::setViewport(int32_t x, int32_t y, int32_t w, int32_t h) {
    glViewport(x, y, w, h);
}

void CommandListGL::bindPipeline(IPipeline* p) {
    m_pipeline = static_cast<PipelineGL*>(p);
    if (!m_pipeline || !m_pipeline->prog) return;

    glUseProgram(m_pipeline->prog);
    if (m_pipeline->depth == DepthTest::Disabled) glDisable(GL_DEPTH_TEST);
    else { glEnable(GL_DEPTH_TEST); glDepthMask(m_pipeline->depth == DepthTest::ReadWrite ? GL_TRUE : GL_FALSE); }

    if (m_pipeline->blend == BlendMode::None) glDisable(GL_BLEND);
    else { glEnable(GL_BLEND); if (m_pipeline->blend == BlendMode::Alpha) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); else glBlendFunc(GL_SRC_ALPHA, GL_ONE); }

    if (m_pipeline->cull == CullMode::None) glDisable(GL_CULL_FACE);
    else { glEnable(GL_CULL_FACE); glCullFace(m_pipeline->cull == CullMode::Back ? GL_BACK : GL_FRONT); }

    if (!m_vao) glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    GLenum err = glGetError();
    if (err) MC_LOG_ERROR("bindPipeline error: 0x{:X}", err);
}

void CommandListGL::bindVertexBuffer(IBuffer* b, uint32_t stride) {
    auto* gl = static_cast<BufferGL*>(b);
    if (!gl) return;

    if (!m_vao) glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, gl->id);

    if (!m_pipeline) return;

    if (m_pipeline->layout == VertexLayout::PositionTexColor) {
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)12);
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)20);
    } else if (m_pipeline->layout == VertexLayout::Gui) {
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)12);
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)20);
    } else {
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    }
}

void CommandListGL::bindIndexBuffer(IBuffer* b, bool) {
    auto* gl = static_cast<BufferGL*>(b);
    if (!gl) return;
    if (!m_vao) glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl->id);
}

void CommandListGL::bindTexture(ITexture* t, uint32_t slot) {
    glActiveTexture(GL_TEXTURE0 + slot);
    if (t) glBindTexture(GL_TEXTURE_2D, static_cast<TextureGL*>(t)->id);
    else glBindTexture(GL_TEXTURE_2D, 0);
}

void CommandListGL::setUniform4f(std::string_view name, float x, float y, float z, float w) {
    if (!m_pipeline || !m_pipeline->prog) return;
    GLint loc = glGetUniformLocation(m_pipeline->prog, std::string(name).c_str());
    if (loc >= 0) glUniform4f(loc, x, y, z, w);
}

void CommandListGL::setUniformMat4(std::string_view name, const float* m) {
    if (!m_pipeline || !m_pipeline->prog) return;
    GLint loc = glGetUniformLocation(m_pipeline->prog, std::string(name).c_str());
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, m);
}

void CommandListGL::draw(uint32_t count, uint32_t first) {
    glDrawArrays(GL_TRIANGLES, (GLint)first, (GLsizei)count);
    GLenum err = glGetError();
    if (err) MC_LOG_ERROR("glDrawArrays error: 0x{:X}", err);
}

void CommandListGL::drawIndexed(uint32_t count, uint32_t first) {
    glDrawElements(GL_TRIANGLES, (GLsizei)count, GL_UNSIGNED_INT, (void*)(uintptr_t)(first * sizeof(uint32_t)));
    GLenum err = glGetError();
    if (err) MC_LOG_ERROR("glDrawElements error: 0x{:X}", err);
}

void CommandListGL::uploadBuffer(IBuffer* b, const void* data, size_t size, size_t offset) {
    auto* gl = static_cast<BufferGL*>(b);
    if (!gl || !gl->id || !data) return;
    if (offset + size > gl->size) {
        gl->size = offset + size;
        glNamedBufferData(gl->id, (GLsizeiptr)gl->size, nullptr, GL_DYNAMIC_DRAW);
    }
    glNamedBufferSubData(gl->id, (GLintptr)offset, (GLsizeiptr)size, data);
}

void CommandListGL::uploadTexture(ITexture* t, const void* pixels) {
    auto* gl = static_cast<TextureGL*>(t);
    glBindTexture(GL_TEXTURE_2D, gl->id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gl->w, gl->h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
}

} // namespace mc::render
