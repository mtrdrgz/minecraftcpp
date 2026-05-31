#include "PipelineVK.h"
#include "../../core/Log.h"
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>
#include <stdexcept>

namespace mc::render {

static bool g_glslangInitialized = false;

PipelineVK::PipelineVK(VkDevice device, const PipelineDesc& desc) 
    : m_device(device) 
{
    if (!g_glslangInitialized) {
        glslang::InitializeProcess();
        g_glslangInitialized = true;
    }

    m_pushConstants["uModel"] = 0;
    m_pushConstants["uViewProj"] = 64;

    createLayout();
    createGraphicsPipeline(desc);
}

PipelineVK::~PipelineVK() {
    if (m_pipeline) vkDestroyPipeline(m_device, m_pipeline, nullptr);
    if (m_layout) vkDestroyPipelineLayout(m_device, m_layout, nullptr);
}

uint32_t PipelineVK::getPushConstantOffset(std::string_view name) const {
    auto it = m_pushConstants.find(std::string(name));
    if (it != m_pushConstants.end()) {
        return it->second;
    }
    return 0;
}

void PipelineVK::createLayout() {
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset     = 0;
    pushRange.size       = 128; // Enough for 2 mat4s

    VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &pushRange;
    layoutInfo.setLayoutCount         = 0; 

    VK_CHECK(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_layout));
}

void PipelineVK::createGraphicsPipeline(const PipelineDesc& desc) {
    auto vsSpirv = compileGLSL(ShaderStage::Vertex, desc.vsSource);
    auto fsSpirv = compileGLSL(ShaderStage::Fragment, desc.fsSource);

    if (vsSpirv.empty() || fsSpirv.empty()) {
        throw std::runtime_error("Failed to compile shaders for Vulkan pipeline");
    }

    VkShaderModuleCreateInfo vsInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    vsInfo.codeSize = vsSpirv.size() * sizeof(uint32_t);
    vsInfo.pCode    = vsSpirv.data();
    VkShaderModule vsModule;
    VK_CHECK(vkCreateShaderModule(m_device, &vsInfo, nullptr, &vsModule));

    VkShaderModuleCreateInfo fsInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    fsInfo.codeSize = fsSpirv.size() * sizeof(uint32_t);
    fsInfo.pCode    = fsSpirv.data();
    VkShaderModule fsModule;
    VK_CHECK(vkCreateShaderModule(m_device, &fsInfo, nullptr, &fsModule));

    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vsModule;
    shaderStages[0].pName  = "main";
    shaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fsModule;
    shaderStages[1].pName  = "main";

    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

    if (desc.layout == VertexLayout::PositionTexColor) {
        VkVertexInputBindingDescription b{};
        b.binding=0; b.stride=24; b.inputRate=VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDescriptions.push_back(b);
        attributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0});
        attributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32_SFLOAT, 12});
        attributeDescriptions.push_back({2, 0, VK_FORMAT_R8G8B8A8_UNORM, 20});
    } else if (desc.layout == VertexLayout::Gui) {
        VkVertexInputBindingDescription b{};
        b.binding=0; b.stride=36; b.inputRate=VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDescriptions.push_back(b);
        attributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0});
        attributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32_SFLOAT, 12});
        attributeDescriptions.push_back({2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 20});
    } else {
        VkVertexInputBindingDescription b{};
        b.binding=0; b.stride=12; b.inputRate=VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDescriptions.push_back(b);
        attributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0});
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertexInputInfo.vertexBindingDescriptionCount   = (uint32_t)bindingDescriptions.size();
    vertexInputInfo.pVertexBindingDescriptions       = bindingDescriptions.data();
    vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
    vertexInputInfo.pVertexAttributeDescriptions     = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportState.viewportCount = 1; viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL; rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = (desc.cull == CullMode::Back) ? VK_CULL_MODE_BACK_BIT : (desc.cull == CullMode::Front ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_NONE);
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depthStencil.depthTestEnable = (desc.depth != DepthTest::Disabled);
    depthStencil.depthWriteEnable = (desc.depth == DepthTest::ReadWrite);
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (desc.blend != BlendMode::None) {
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo colorBlending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    colorBlending.attachmentCount = 1; colorBlending.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicState.dynamicStateCount = 2; dynamicState.pDynamicStates = dynamicStates;

    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkPipelineRenderingCreateInfo renderingInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    renderingInfo.colorAttachmentCount = 1; renderingInfo.pColorAttachmentFormats = &colorFormat;
    renderingInfo.depthAttachmentFormat = VK_FORMAT_D24_UNORM_S8_UINT;

    VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipelineInfo.pNext = &renderingInfo; pipelineInfo.stageCount = 2; pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo; pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState; pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling; pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending; pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_layout;

    VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline));
    vkDestroyShaderModule(m_device, vsModule, nullptr);
    vkDestroyShaderModule(m_device, fsModule, nullptr);
}

std::vector<uint32_t> PipelineVK::compileGLSL(ShaderStage stage, std::string_view src) {
    EShLanguage lang = (stage == ShaderStage::Vertex) ? EShLangVertex : EShLangFragment;
    glslang::TShader shader(lang);
    const char* s = src.data(); int l = (int)src.size(); shader.setStringsWithLengths(&s, &l, 1);
    shader.setEnvInput(glslang::EShSourceGlsl, lang, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);
    TBuiltInResource res = *GetDefaultResources();
    if (!shader.parse(&res, 100, false, EShMsgDefault)) {
        MC_LOG_ERROR("Shader parse error:\n{}\n{}", shader.getInfoLog(), shader.getInfoDebugLog());
        return {};
    }
    glslang::TProgram program; program.addShader(&shader);
    if (!program.link(EShMsgDefault)) {
        MC_LOG_ERROR("Shader link error:\n{}\n{}", program.getInfoLog(), program.getInfoDebugLog());
        return {};
    }
    std::vector<uint32_t> spirv; glslang::GlslangToSpv(*program.getIntermediate(lang), spirv);
    return spirv;
}

} // namespace mc::render
