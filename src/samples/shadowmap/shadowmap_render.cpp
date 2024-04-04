#include "shadowmap_render.h"

#include <geom/vk_mesh.h>
#include <vk_pipeline.h>
#include <vk_buffers.h>
#include <iostream>

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <vulkan/vulkan_core.h>


/// RESOURCE ALLOCATION

void SimpleShadowmapRender::AllocateResources()
{
  mainViewDepth = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "main_view_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment
  });

  shadowMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{2048, 2048, 1},
    .name = "shadow_map",
    .format = vk::Format::eD16Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
  });

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});
  constants = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "constants"
  });

  m_uboMappedMem = constants.map();

  m_LightViewDepthMap = m_context->createImage({
    .extent     = vk::Extent3D{ 2048, 2048, 1 },
    .name       = "LightViewDepthMap",
    .format     = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment
  });

  m_LightViewDepthMomentsMap = m_context->createImage({
    .extent     = vk::Extent3D{ 2048, 2048, 1 },
    .name       = "LightViewDepthMomentsMap",
    .format     = vk::Format::eR32G32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage |vk::ImageUsageFlagBits::eSampled
  });

  m_LightViewDepthMomentsBlurredMap = m_context->createImage({
    .extent     = vk::Extent3D{ 2048, 2048, 1 },
    .name       = "LightViewDepthMomentsBlurredMap",
    .format     = vk::Format::eR32G32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled
  });

  m_BlurCoeff = m_context->createBuffer({
    .size        = 16 * BLUR_WINDOW_SIZE,
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name        = "Coeff"
  });

  auto mappedBlurCoeff = (float *)m_BlurCoeff.map();

  constexpr auto PI    = M_PI;
  constexpr auto SIGMA = 1.84089642f;

  for (size_t i = 0; i < BLUR_WINDOW_SIZE; ++i)
  {
    auto x = static_cast<float>(BLUR_WINDOW_SIZE_HALF) - i;
    auto coeff                                = 1.f / sqrtf(2.f * PI * SIGMA * SIGMA) * expf(-1.f / (2.f * SIGMA * SIGMA) * x * x);
    mappedBlurCoeff[i * (16 / sizeof(float))] = coeff;
  }
}

void SimpleShadowmapRender::LoadScene(const char* path, bool transpose_inst_matrices)
{
  m_pScnMgr->LoadSceneXML(path, transpose_inst_matrices);

  // TODO: Make a separate stage
  loadShaders();
  PreparePipelines();

  auto loadedCam = m_pScnMgr->GetCamera(0);
  m_cam.fov = loadedCam.fov;
  m_cam.pos = float3(loadedCam.pos);
  m_cam.up  = float3(loadedCam.up);
  m_cam.lookAt = float3(loadedCam.lookAt);
  m_cam.tdist  = loadedCam.farPlane;
}

void SimpleShadowmapRender::DeallocateResources()
{
  mainViewDepth.reset(); // TODO: Make an etna method to reset all the resources
  shadowMap.reset();
  m_swapchain.Cleanup();
  vkDestroySurfaceKHR(GetVkInstance(), m_surface, nullptr);  

  constants = etna::Buffer();
}





/// PIPELINES CREATION

void SimpleShadowmapRender::PreparePipelines()
{
  // create full screen quad for debug purposes
  // 
  m_pQuad = std::make_unique<QuadRenderer>(QuadRenderer::CreateInfo{ 
      .format = static_cast<vk::Format>(m_swapchain.GetFormat()),
      .rect = { 0, 0, 512, 512 }, 
    });
  SetupSimplePipeline();
}

void SimpleShadowmapRender::loadShaders()
{
  etna::create_program("simple_material",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_shadow.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});
  etna::create_program("simple_shadow", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});

  etna::create_program("DepthMomentsMap", {
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/depth_moments_map.frag.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"
  });

  etna::create_program("Blur", {
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/blur.comp.spv"
  });

  etna::create_program("VarianceShading", {
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/variance_shading.frag.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"
  });
}

void SimpleShadowmapRender::SetupSimplePipeline()
{
  etna::VertexShaderInputDescription sceneVertexInputDesc
    {
      .bindings = {etna::VertexShaderInputDescription::Binding
        {
          .byteStreamDescription = m_pScnMgr->GetVertexStreamDescription()
        }}
    };

  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_basicForwardPipeline = pipelineManager.createGraphicsPipeline("simple_material",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
  m_shadowPipeline = pipelineManager.createGraphicsPipeline("simple_shadow",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .depthAttachmentFormat = vk::Format::eD16Unorm
        }
    });

  m_DepthMomentsMap = pipelineManager.createGraphicsPipeline("DepthMomentsMap", {
    .vertexShaderInput = sceneVertexInputDesc,
    .fragmentShaderOutput = {
      .colorAttachmentFormats = { vk::Format::eR32G32Sfloat },
      .depthAttachmentFormat = vk::Format::eD32Sfloat
    }
  });

  m_Blur = pipelineManager.createComputePipeline("Blur", {
  });

  m_VarianceShading = pipelineManager.createGraphicsPipeline("VarianceShading", {
    .vertexShaderInput = sceneVertexInputDesc,
    .fragmentShaderOutput = {
        .colorAttachmentFormats = { static_cast<vk::Format>(m_swapchain.GetFormat()) },
        .depthAttachmentFormat  = vk::Format::eD32Sfloat
    }
  });
}


/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::DrawSceneCmd(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp, VkPipelineLayout a_pipelineLayout)
{
  VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT);

  VkDeviceSize zero_offset = 0u;
  VkBuffer vertexBuf = m_pScnMgr->GetVertexBuffer();
  VkBuffer indexBuf  = m_pScnMgr->GetIndexBuffer();
  
  vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
  vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);

  pushConst2M.projView = a_wvp;
  for (uint32_t i = 0; i < m_pScnMgr->InstancesNum(); ++i)
  {
    auto inst         = m_pScnMgr->GetInstanceInfo(i);
    pushConst2M.model = m_pScnMgr->GetInstanceMatrix(i);
    vkCmdPushConstants(a_cmdBuff, a_pipelineLayout,
      stageFlags, 0, sizeof(pushConst2M), &pushConst2M);

    auto mesh_info = m_pScnMgr->GetMeshInfo(inst.mesh_id);
    vkCmdDrawIndexed(a_cmdBuff, mesh_info.m_indNum, 1, mesh_info.m_indexOffset, mesh_info.m_vertexOffset, 0);
  }
}

void SimpleShadowmapRender::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  //// draw scene to shadowmap
  //
  {
    etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0, 2048, 2048}, {}, {.image = shadowMap.get(), .view = shadowMap.getView({})});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.getVkPipeline());
    DrawSceneCmd(a_cmdBuff, m_lightMatrix, m_shadowPipeline.getVkPipelineLayout());
  }

  //// draw final scene to screen
  //
  {
    auto simpleMaterialInfo = etna::get_shader_program("simple_material");

    auto set = etna::create_descriptor_set(simpleMaterialInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, constants.genBinding()},
      etna::Binding {1, shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
    });

    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0, m_width, m_height},
      {{.image = a_targetImage, .view = a_targetImageView}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({})});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_basicForwardPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_basicForwardPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    DrawSceneCmd(a_cmdBuff, m_worldViewProj, m_basicForwardPipeline.getVkPipelineLayout());
  }

  if(m_input.drawFSQuad)
    m_pQuad->RecordCommands(a_cmdBuff, a_targetImage, a_targetImageView, shadowMap, defaultSampler);

  etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eBottomOfPipe,
    vk::AccessFlags2(), vk::ImageLayout::ePresentSrcKHR,
    vk::ImageAspectFlagBits::eColor);

  etna::finish_frame(a_cmdBuff);

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}

void SimpleShadowmapRender::BuildVarianceShadingCommandBuffer(VkCommandBuffer commandBuffer, VkImage targetImage, VkImageView targetImageView)
{
  vkResetCommandBuffer(commandBuffer, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo));

  { 
    etna::RenderTargetState renderTargets(commandBuffer, { 0, 0, 2048, 2048 },
      { { .image = m_LightViewDepthMomentsMap.get(), .view = m_LightViewDepthMomentsMap.getView({}) } },
      { .image = m_LightViewDepthMap.get(), .view = m_LightViewDepthMap.getView({}) }
    );

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DepthMomentsMap.getVkPipeline());
    DrawSceneCmd(commandBuffer, m_lightMatrix, m_DepthMomentsMap.getVkPipelineLayout());
  }

  etna::set_state(commandBuffer, m_LightViewDepthMomentsMap.get(), vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eGeneral, vk::ImageAspectFlagBits::eColor);

  etna::set_state(commandBuffer, m_LightViewDepthMomentsBlurredMap.get(), vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite, vk::ImageLayout::eGeneral, vk::ImageAspectFlagBits::eColor);

  etna::flush_barriers(commandBuffer);

  {
    auto blur = etna::get_shader_program("Blur");

    auto set = etna::create_descriptor_set(blur.getDescriptorLayoutId(0), commandBuffer, {
        etna::Binding{ 0, m_LightViewDepthMomentsMap.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral) },
        etna::Binding{ 1, m_LightViewDepthMomentsBlurredMap.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral) },
        etna::Binding{ 2, m_BlurCoeff.genBinding() }
    });

    VkDescriptorSet vkSet = set.getVkSet();

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_Blur.getVkPipeline());
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_Blur.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    vkCmdDispatch(commandBuffer, 2048 / WORK_GROUP_SIZE, 2048 / WORK_GROUP_SIZE, 1);
  }

  //etna::set_state(commandBuffer, m_LightViewDepthMomentsMap.get(), vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferRead, vk::ImageLayout::eTransferSrcOptimal, vk::ImageAspectFlagBits::eColor);

  //etna::set_state(commandBuffer, m_LightViewDepthMomentsBlurredMap.get(), vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eTransferDstOptimal, vk::ImageAspectFlagBits::eColor);

  etna::set_state(commandBuffer, m_LightViewDepthMomentsBlurredMap.get(), vk::PipelineStageFlagBits2::eAllGraphics, vk::AccessFlagBits2::eShaderSampledRead, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor);

  etna::flush_barriers(commandBuffer);

  {
    auto varianceShading = etna::get_shader_program("VarianceShading");

    auto set = etna::create_descriptor_set(varianceShading.getDescriptorLayoutId(0), commandBuffer, {
        etna::Binding{ 0, constants.genBinding() },
        etna::Binding{ 1, m_LightViewDepthMomentsBlurredMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal) } 
    });

    auto vkSet = static_cast<VkDescriptorSet>(set.getVkSet());

    etna::RenderTargetState renderTargets(commandBuffer, { 0, 0, m_width, m_height },
        { { .image = targetImage, .view = targetImageView } },
        { .image = mainViewDepth.get(), .view = mainViewDepth.getView({}) }
    );

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_VarianceShading.getVkPipeline());
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_VarianceShading.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    DrawSceneCmd(commandBuffer, m_worldViewProj, m_VarianceShading.getVkPipelineLayout());
  }

  etna::set_state(commandBuffer, targetImage, vk::PipelineStageFlagBits2::eBottomOfPipe, vk::AccessFlags2(), vk::ImageLayout::ePresentSrcKHR, vk::ImageAspectFlagBits::eColor);

  etna::finish_frame(commandBuffer);

  VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));
}
