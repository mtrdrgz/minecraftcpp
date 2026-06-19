#include "CommandListVK.h"
#include "BufferVK.h"
#include "TextureVK.h"
#include "PipelineVK.h"
#include "../../core/Log.h"

namespace mc::render {

CommandListVK::CommandListVK(VkCommandBuffer cmd) : m_cmd(cmd) {
}

void CommandListVK::clear(float r, float g, float b, float a, bool depth) {
    // Clear is typically handled at the start of a render pass via VkRenderPassBeginInfo
    // or vkCmdBeginRendering clear values. 
    // In this simplified version, we assume the render pass is already configured with clear.
}

void CommandListVK::setViewport(int32_t x, int32_t y, int32_t w, int32_t h) {
    VkViewport viewport{};
    viewport.x        = static_cast<float>(x);
    viewport.y        = static_cast<float>(y);
    viewport.width    = static_cast<float>(w);
    viewport.height   = static_cast<float>(h);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(m_cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { x, y };
    scissor.extent = { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
    vkCmdSetScissor(m_cmd, 0, 1, &scissor);
}

void CommandListVK::bindPipeline(IPipeline* pipeline) {
    m_currentPipeline = pipeline;
    if (pipeline) {
        auto* vkPipe = static_cast<PipelineVK*>(pipeline);
        vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipe->getHandle());
    }
}

void CommandListVK::bindVertexBuffer(IBuffer* buffer, uint32_t stride) {
    if (!buffer) return;
    VkBuffer     vkb    = static_cast<BufferVK*>(buffer)->getHandle();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(m_cmd, 0, 1, &vkb, &offset);
}

void CommandListVK::bindIndexBuffer(IBuffer* buffer, bool use32bit) {
    if (!buffer) return;
    VkBuffer vkb = static_cast<BufferVK*>(buffer)->getHandle();
    vkCmdBindIndexBuffer(m_cmd, vkb, 0, use32bit ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
}

void CommandListVK::bindTexture(ITexture* texture, uint32_t slot) {
    if (!texture || !m_currentPipeline) return;
    // Binding textures in Vulkan requires descriptor sets.
    // This typically involves a descriptor pool and updating sets.
    // TODO: Implement descriptor set binding logic.
}

void CommandListVK::setUniform4f(std::string_view name, float x, float y, float z, float w) {
    if (!m_currentPipeline) return;
    auto* vkPipe = static_cast<PipelineVK*>(m_currentPipeline);
    
    float data[4] = { x, y, z, w };
    uint32_t offset = vkPipe->getPushConstantOffset(name);
    
    vkCmdPushConstants(m_cmd, vkPipe->getLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, offset, sizeof(data), data);
}

void CommandListVK::setUniformMat4(std::string_view name, const float* m) {
    if (!m_currentPipeline) return;
    auto* vkPipe = static_cast<PipelineVK*>(m_currentPipeline);
    
    uint32_t offset = vkPipe->getPushConstantOffset(name);
    vkCmdPushConstants(m_cmd, vkPipe->getLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, offset, 64, m);
}

void CommandListVK::draw(uint32_t vertexCount, uint32_t firstVertex) {
    vkCmdDraw(m_cmd, vertexCount, 1, firstVertex, 0);
}

void CommandListVK::drawIndexed(uint32_t indexCount, uint32_t firstIndex) {
    vkCmdDrawIndexed(m_cmd, indexCount, 1, firstIndex, 0, 0);
}

void CommandListVK::uploadBuffer(IBuffer* buffer, const void* data, size_t size, size_t offset) {
    if (!buffer) return;
    
    // For simplicity, we use the immediate upload from BufferVK.
    // This works if the buffer is host-visible.
    static_cast<BufferVK*>(buffer)->upload(data, size, offset);
}

void CommandListVK::uploadTexture(ITexture* texture, const void* pixels) {
    if (!texture || !pixels) return;
    
    // Texture upload requires a staging buffer and layout transitions.
    // TODO: Implement staging upload for textures using a staging buffer pool.
}

} // namespace mc::render
