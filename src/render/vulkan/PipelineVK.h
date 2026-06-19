#pragma once
#include "VulkanUtils.h"
#include "../IRenderDevice.h"
#include <string>
#include <vector>
#include <map>

namespace mc::render {

class PipelineVK : public IPipeline {
public:
    PipelineVK(VkDevice device, const PipelineDesc& desc);
    ~PipelineVK() override;

    VkPipeline       getHandle() const { return m_pipeline; }
    VkPipelineLayout getLayout() const { return m_layout; }
    
    uint32_t getPushConstantOffset(std::string_view name) const;

private:
    void createLayout();
    void createGraphicsPipeline(const PipelineDesc& desc);
    std::vector<uint32_t> compileGLSL(ShaderStage stage, std::string_view source);

    VkDevice         m_device;
    VkPipelineLayout m_layout   = VK_NULL_HANDLE;
    VkPipeline       m_pipeline = VK_NULL_HANDLE;
    
    std::map<std::string, uint32_t> m_pushConstants;
};

} // namespace mc::render
