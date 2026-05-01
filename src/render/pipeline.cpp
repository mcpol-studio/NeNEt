#include "pipeline.h"

#include "shader.h"
#include "vertex.h"

#include <spdlog/spdlog.h>

#include <array>
#include <stdexcept>

namespace nenet {

CubePipeline::CubePipeline(VkDevice device, VkFormat colorFormat, VkFormat depthFormat)
    : device_(device) {
    const auto shaderDir = executableDir() / "shaders";
    VkShaderModule vert = loadShaderModule(device_, shaderDir / "cube.vert.spv");
    VkShaderModule frag = loadShaderModule(device_, shaderDir / "cube.frag.spv");

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(PushConstants);

    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 0;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dslInfo{};
    dslInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslInfo.bindingCount = 1;
    dslInfo.pBindings = &samplerBinding;
    if (vkCreateDescriptorSetLayout(device_, &dslInfo, nullptr, &descriptorLayout_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDescriptorSetLayout 失败 (cube)");
    }

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorLayout_;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pcRange;
    if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &layout_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreatePipelineLayout 失败");
    }

    std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    auto vBinding = Vertex::binding();
    auto vAttribs = Vertex::attributes();
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &vBinding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(vAttribs.size());
    vertexInput.pVertexAttributeDescriptions = vAttribs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth{};
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS;
    depth.depthBoundsTestEnable = VK_FALSE;
    depth.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorAttach{};
    colorAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorAttach.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorAttach;

    std::array<VkDynamicState, 2> dynStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
    dyn.pDynamicStates = dynStates.data();

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;
    renderingInfo.depthAttachmentFormat = depthFormat;

    VkGraphicsPipelineCreateInfo pipeInfo{};
    pipeInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeInfo.pNext = &renderingInfo;
    pipeInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipeInfo.pStages = stages.data();
    pipeInfo.pVertexInputState = &vertexInput;
    pipeInfo.pInputAssemblyState = &inputAssembly;
    pipeInfo.pViewportState = &viewport;
    pipeInfo.pRasterizationState = &raster;
    pipeInfo.pMultisampleState = &ms;
    pipeInfo.pDepthStencilState = &depth;
    pipeInfo.pColorBlendState = &colorBlend;
    pipeInfo.pDynamicState = &dyn;
    pipeInfo.layout = layout_;

    const VkResult res = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &pipeline_);

    vkDestroyShaderModule(device_, vert, nullptr);
    vkDestroyShaderModule(device_, frag, nullptr);

    if (res != VK_SUCCESS) {
        throw std::runtime_error("vkCreateGraphicsPipelines 失败 code=" + std::to_string(res));
    }
    spdlog::info("Cube pipeline 已创建");
}

CubePipeline::~CubePipeline() {
    if (pipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, pipeline_, nullptr);
    if (layout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, layout_, nullptr);
    if (descriptorLayout_ != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device_, descriptorLayout_, nullptr);
    spdlog::info("Cube pipeline 已销毁");
}

}
