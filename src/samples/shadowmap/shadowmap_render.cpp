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

  m_Depth = m_context->createImage({
    .extent     = { 2048, 2048, 1 },
    .name       = "rsm depth",
    .format     = vk::Format::eD16Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
  });

  m_Position = m_context->createImage({
    .extent     = { 2048, 2048, 1 },
    .name       = "rsm position",
    .format     = vk::Format::eR16G16B16A16Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });

  m_Normal = m_context->createImage({
    .extent     = { 2048, 2048, 1 },
    .name       = "rsm normal",
    .format     = vk::Format::eR16G16B16A16Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
   });

  m_Flux = m_context->createImage({
    .extent     = { 2048, 2048, 1 },
    .name       = "rsm flux",
    .format     = vk::Format::eR16G16B16A16Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });

  m_Sampler = etna::Sampler({
    .filter      = vk::Filter::eLinear,
    .addressMode = vk::SamplerAddressMode::eClampToBorder,
    .name        = "rsm sampler"
  });
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
  etna::create_program("simple_material", {
    VK_GRAPHICS_BASIC_ROOT "/resources/shaders/simple.vert.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_shadow.frag.spv"
  });

  etna::create_program("simple_shadow", {
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"
  });

  etna::create_program("maps generation rsm", {
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/maps_generation_rsm.frag.spv"
  });

  etna::create_program("shadow rsm", {
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv",
    VK_GRAPHICS_BASIC_ROOT"/resources/shaders/shadow_rsm.frag.spv"
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

  m_MapsGenerationRSM = pipelineManager.createGraphicsPipeline("maps generation rsm", {
    .vertexShaderInput    = sceneVertexInputDesc,
    .rasterizationConfig = {
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eNone,
      .frontFace = vk::FrontFace::eCounterClockwise,
      .lineWidth = 1.f,
    },
    .blendingConfig = {
      .attachments = {
        {
          .blendEnable = vk::False,
          .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        },
        {
          .blendEnable = vk::False,
          .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        },
        {
          .blendEnable = vk::False,
          .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        }
      }
    },
    .fragmentShaderOutput = {
      .colorAttachmentFormats = {
        vk::Format::eR16G16B16A16Sfloat,
        vk::Format::eR16G16B16A16Sfloat,
        vk::Format::eR16G16B16A16Sfloat
      },
      .depthAttachmentFormat  = vk::Format::eD16Unorm
    }
  });

  m_ShadowRSM = pipelineManager.createGraphicsPipeline("shadow rsm", {
    .vertexShaderInput    = sceneVertexInputDesc,
    .rasterizationConfig = {
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eBack,
      .frontFace = vk::FrontFace::eCounterClockwise,
      .lineWidth = 1.f,
    },
    .fragmentShaderOutput = {
      .colorAttachmentFormats = {
        static_cast<vk::Format>(m_swapchain.GetFormat())
      },
      .depthAttachmentFormat  = vk::Format::eD32Sfloat
    }
  });
}


/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::DrawSceneCmd(VkCommandBuffer a_cmdBuff, const float4x4 &a_wvp, VkPipelineLayout a_pipelineLayout, VkShaderStageFlags a_flags)
{
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
    pushConst2M.color = m_Colors[i];
    vkCmdPushConstants(a_cmdBuff, a_pipelineLayout, a_flags, 0, sizeof(pushConst2M), &pushConst2M);

    auto mesh_info = m_pScnMgr->GetMeshInfo(inst.mesh_id);
    vkCmdDrawIndexed(a_cmdBuff, mesh_info.m_indNum, 1, mesh_info.m_indexOffset, mesh_info.m_vertexOffset, 0);
  }
}

void SimpleShadowmapRender::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  const VkShaderStageFlags vertex_stage = VK_SHADER_STAGE_VERTEX_BIT;
  const VkShaderStageFlags vertex_fragment_stage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  if (m_Enabled)
  {
    {
      auto MapsGenerationRSM = etna::get_shader_program("maps generation rsm");

      auto set = etna::create_descriptor_set(
        MapsGenerationRSM.getDescriptorLayoutId(0),
        a_cmdBuff,
        {
          { 0, constants.genBinding() }
        }
      );

      VkDescriptorSet sets[] = { set.getVkSet() };

      etna::RenderTargetState renderTargets(
        a_cmdBuff,
        { 0, 0, 2048, 2048 },
        {
          { .image = m_Position.get(), .view = m_Position.getView({}) },
          { .image = m_Normal.get(), .view = m_Normal.getView({}) },
          { .image = m_Flux.get(), .view = m_Flux.getView({}) }
        },
        { .image = m_Depth.get(), .view = m_Depth.getView({}) }
      );

      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_MapsGenerationRSM.getVkPipeline());
      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_MapsGenerationRSM.getVkPipelineLayout(), 0, std::size(sets), sets, 0, VK_NULL_HANDLE);

      DrawSceneCmd(a_cmdBuff, m_lightMatrix, m_MapsGenerationRSM.getVkPipelineLayout(), vertex_fragment_stage);
    }

    {
      auto ShadowRSM = etna::get_shader_program("shadow rsm");

      auto set = etna::create_descriptor_set(
        ShadowRSM.getDescriptorLayoutId(0),
        a_cmdBuff,
        {
          { 0, constants.genBinding() },
          { 1, m_Depth.genBinding(m_Sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal) },
          { 2, m_Position.genBinding(m_Sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal) },
          { 3, m_Normal.genBinding(m_Sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal) },
          { 4, m_Flux.genBinding(m_Sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal) }
        }
      );

      VkDescriptorSet sets[] = { set.getVkSet() };

      etna::RenderTargetState renderTargets(
        a_cmdBuff,
        {0, 0, m_width, m_height},
        {
          { .image = a_targetImage, .view = a_targetImageView }
        },
        { .image = mainViewDepth.get(), .view = mainViewDepth.getView({}) }
      );

      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ShadowRSM.getVkPipeline());
      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ShadowRSM.getVkPipelineLayout(), 0, std::size(sets), sets, 0, VK_NULL_HANDLE);

      DrawSceneCmd(a_cmdBuff, m_worldViewProj, m_ShadowRSM.getVkPipelineLayout(), vertex_fragment_stage);
    }
  }
  else
  {
    //// draw scene to shadowmap
    //
    {
      etna::RenderTargetState renderTargets(a_cmdBuff, {0, 0, 2048, 2048}, {}, {.image = shadowMap.get(), .view = shadowMap.getView({})});

      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.getVkPipeline());
      DrawSceneCmd(a_cmdBuff, m_lightMatrix, m_shadowPipeline.getVkPipelineLayout(), vertex_stage);
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

      DrawSceneCmd(a_cmdBuff, m_worldViewProj, m_basicForwardPipeline.getVkPipelineLayout(), vertex_fragment_stage);

    }
  }

  etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eBottomOfPipe, vk::AccessFlags2(), vk::ImageLayout::ePresentSrcKHR, vk::ImageAspectFlagBits::eColor);

  etna::finish_frame(a_cmdBuff);

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}
