#pragma once
#include "../IRenderDevice.h"
#include "VulkanUtils.h"

namespace mc::render {

class CommandListVK : public ICommandList {
public:
    CommandListVK(VkCommandBuffer cmd);
    ~CommandListVK() override = default;

    void clear(float r, float g, float b, float a = 1.f, bool depth = true) override;
    void setViewport(int32_t x, int32_t y, int32_t w, int32_t h) override;
    void bindPipeline(IPipeline* pipeline) override;
    void bindVertexBuffer(IBuffer* buffer, uint32_t stride) override;
    void bindIndexBuffer(IBuffer* buffer, bool use32bit = false) override;
    void bindTexture(ITexture* texture, uint32_t slot) override;
    void setUniform4f(std::string_view name, float x, float y, float z, float w) override;
    void setUniformMat4(std::string_view name, const float* m) override;
    void draw(uint32_t vertexCount, uint32_t firstVertex = 0) override;
    void drawIndexed(uint32_t indexCount, uint32_t firstIndex = 0) override;
    void uploadBuffer(IBuffer* buffer, const void* data, size_t size, size_t offset = 0) override;
    void uploadTexture(ITexture* texture, const void* pixels) override;

    VkCommandBuffer getHandle() const { return m_cmd; }

private:
    VkCommandBuffer m_cmd;
    IPipeline*      m_currentPipeline = nullptr;
};

} // namespace mc::render
