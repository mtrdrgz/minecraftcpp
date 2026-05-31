#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include <memory>
#include <string>

namespace mc::render {

enum class BufferUsage   { Vertex, Index, Uniform, Staging };
enum class TextureFormat { RGBA8, RGB8, R8, Depth24Stencil8 };
enum class FilterMode    { Nearest, Linear, LinearMipLinear };
enum class WrapMode      { Repeat, ClampEdge };
enum class ShaderStage   { Vertex, Fragment };
enum class PrimitiveType { Triangles, TriangleStrip, Lines };
enum class BlendMode     { None, Alpha, Additive };
enum class DepthTest     { Disabled, ReadWrite, ReadOnly };
enum class CullMode      { None, Back, Front };

enum class VertexLayout {
    PositionTexColor, // For chunks: {vec3, vec2, rgba8_unorm}
    Gui,              // For UI: {vec3, vec2, vec4}
    Simple            // For entities: {vec3}
};

struct BufferDesc {
    size_t      size;
    BufferUsage usage;
    bool        dynamic = false; // CPU-updatable each frame
};

struct TextureDesc {
    uint32_t      width, height;
    TextureFormat format;
    uint32_t      mipLevels  = 1;
    FilterMode    filter     = FilterMode::LinearMipLinear;
    WrapMode      wrap       = WrapMode::Repeat;
    bool          genMipmaps = false;
};

struct PipelineDesc {
    std::string_view vsSource;
    std::string_view fsSource;
    BlendMode        blend     = BlendMode::None;
    DepthTest        depth     = DepthTest::ReadWrite;
    CullMode         cull      = CullMode::Back;
    PrimitiveType    primitive = PrimitiveType::Triangles;
    VertexLayout     layout    = VertexLayout::PositionTexColor;
};

// Opaque handles
struct IBuffer   { virtual ~IBuffer() = default; };
struct ITexture  { virtual ~ITexture() = default; };
struct IPipeline { virtual ~IPipeline() = default; };

// Abstract draw command recorder
class ICommandList {
public:
    virtual ~ICommandList() = default;

    virtual void clear(float r, float g, float b, float a = 1.f, bool depth = true) = 0;
    virtual void setViewport(int32_t x, int32_t y, int32_t w, int32_t h) = 0;
    virtual void bindPipeline(IPipeline*) = 0;
    virtual void bindVertexBuffer(IBuffer*, uint32_t stride) = 0;
    virtual void bindIndexBuffer(IBuffer*, bool use32bit = false) = 0;
    virtual void bindTexture(ITexture*, uint32_t slot) = 0;
    virtual void setUniform4f(std::string_view name, float x, float y, float z, float w) = 0;
    virtual void setUniformMat4(std::string_view name, const float* m) = 0;
    virtual void draw(uint32_t vertexCount, uint32_t firstVertex = 0) = 0;
    virtual void drawIndexed(uint32_t indexCount, uint32_t firstIndex = 0) = 0;
    virtual void uploadBuffer(IBuffer*, const void* data, size_t size, size_t offset = 0) = 0;
    virtual void uploadTexture(ITexture*, const void* pixels) = 0;
};

// The main device
class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    virtual IBuffer*      createBuffer(const BufferDesc&)   = 0;
    virtual ITexture*     createTexture(const TextureDesc&)  = 0;
    virtual IPipeline*    createPipeline(const PipelineDesc&) = 0;

    virtual void destroyBuffer(IBuffer*)   = 0;
    virtual void destroyTexture(ITexture*) = 0;
    virtual void destroyPipeline(IPipeline*) = 0;

    virtual ICommandList* beginFrame(int32_t viewW, int32_t viewH) = 0;
    virtual void          endFrame()   = 0;
    virtual void          waitIdle()   = 0;
    virtual void          setVsync(bool) = 0;
    virtual std::string_view backendName() const = 0;
};

} // namespace mc::render
