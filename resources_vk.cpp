/*
 * Copyright (c) 2019-2024, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2019-2024 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */


#include "vk_ext_device_generated_commands.hpp"
#include "resources_vk.hpp"
#include "imgui/backends/imgui_vk_extra.h"
#include "nvh/nvprint.hpp"
#include <algorithm>

namespace generatedcmds {

/////////////////////////////////////////////////////////////////////////////////


void ResourcesVK::submissionExecute(VkFence fence, bool useImageReadWait, bool useImageWriteSignals)
{
  if(useImageReadWait && m_submissionWaitForRead)
  {
    VkSemaphore semRead = m_swapChain->getActiveReadSemaphore();
    if(semRead)
    {
      m_submission.enqueueWait(semRead, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }
    m_submissionWaitForRead = false;
  }

  if(useImageWriteSignals)
  {
    VkSemaphore semWritten = m_swapChain->getActiveWrittenSemaphore();
    if(semWritten)
    {
      m_submission.enqueueSignal(semWritten);
    }
  }

  m_submission.execute(fence);
}

void ResourcesVK::beginFrame()
{
  assert(!m_withinFrame);
  m_withinFrame           = true;
  m_submissionWaitForRead = true;
  m_ringFences.setCycleAndWait(m_frame);
  m_ringCmdPool.setCycle(m_ringFences.getCycleIndex());
}

void ResourcesVK::endFrame()
{
  submissionExecute(m_ringFences.getFence(), true, true);
  assert(m_withinFrame);
  m_withinFrame = false;
}

void ResourcesVK::initImGui(const nvvk::Context& context)
{
  VkPipelineRenderingCreateInfo pipelineRendering = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};

  VkFormat colorFormat                      = VK_FORMAT_R8G8B8A8_UNORM;
  pipelineRendering.colorAttachmentCount    = 1;
  pipelineRendering.pColorAttachmentFormats = &colorFormat;

  ImGui::InitVK(context.m_instance, context.m_device, context.m_physicalDevice, context.m_queueGCT,
                context.m_queueGCT.familyIndex, pipelineRendering);
}
void ResourcesVK::deinitImGui(const nvvk::Context& context)
{
  ImGui::ShutdownVK();
}

void ResourcesVK::blitFrame(const Global& global)
{
  VkCommandBuffer cmd = createTempCmdBuffer();

  nvh::Profiler::SectionID sec = m_profilerVK.beginSection("BltUI", cmd);

  VkImage imageBlitRead = m_framebuffer.imgColor.image;

  if(m_framebuffer.useResolved)
  {
    cmdImageTransition(cmd, m_framebuffer.imgColor.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                       VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    if(m_framebuffer.msaa)
    {
      VkImageResolve region            = {0};
      region.extent.width              = global.winWidth;
      region.extent.height             = global.winHeight;
      region.extent.depth              = 1;
      region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      region.dstSubresource.layerCount = 1;
      region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      region.srcSubresource.layerCount = 1;

      vkCmdResolveImage(cmd, m_framebuffer.imgColor.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        m_framebuffer.imgColorResolved.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

      imageBlitRead = m_framebuffer.imgColorResolved.image;
    }
    else
    {
      // downsample to resolved
      VkImageBlit region               = {0};
      region.dstOffsets[1].x           = global.winWidth;
      region.dstOffsets[1].y           = global.winHeight;
      region.dstOffsets[1].z           = 1;
      region.srcOffsets[1].x           = m_framebuffer.renderWidth;
      region.srcOffsets[1].y           = m_framebuffer.renderHeight;
      region.srcOffsets[1].z           = 1;
      region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      region.dstSubresource.layerCount = 1;
      region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      region.srcSubresource.layerCount = 1;

      imageBlitRead = m_framebuffer.imgColorResolved.image;

      vkCmdBlitImage(cmd, m_framebuffer.imgColor.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageBlitRead,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR);
    }
  }

  if(global.imguiDrawData)
  {
    if(imageBlitRead != m_framebuffer.imgColor.image)
    {
      cmdImageTransition(cmd, imageBlitRead, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    vkCmdBeginRendering(cmd, &m_framebuffer.renderingInfoUI);

    vkCmdSetViewport(cmd, 0, 1, &m_framebuffer.viewportUI);
    vkCmdSetScissor(cmd, 0, 1, &m_framebuffer.scissorUI);

    ImGui_ImplVulkan_RenderDrawData(global.imguiDrawData, cmd);

    vkCmdEndRendering(cmd);

    cmdImageTransition(cmd, imageBlitRead, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  }
  else
  {
    if(m_framebuffer.useResolved)
    {
      cmdImageTransition(cmd, m_framebuffer.imgColorResolved.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                         VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    }
    else
    {
      cmdImageTransition(cmd, m_framebuffer.imgColor.image, VK_IMAGE_ASPECT_COLOR_BIT,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    }
  }


  {
    // blit to vk backbuffer
    VkImageBlit region               = {0};
    region.dstOffsets[1].x           = global.winWidth;
    region.dstOffsets[1].y           = global.winHeight;
    region.dstOffsets[1].z           = 1;
    region.srcOffsets[1].x           = global.winWidth;
    region.srcOffsets[1].y           = global.winHeight;
    region.srcOffsets[1].z           = 1;
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.layerCount = 1;
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.layerCount = 1;

    cmdImageTransition(cmd, m_swapChain->getActiveImage(), VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkCmdBlitImage(cmd, imageBlitRead, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_swapChain->getActiveImage(),
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST);

    cmdImageTransition(cmd, m_swapChain->getActiveImage(), VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, 0,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  }

  if(m_framebuffer.useResolved)
  {
    cmdImageTransition(cmd, m_framebuffer.imgColorResolved.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                       VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  }

  m_profilerVK.endSection(sec, cmd);

  vkEndCommandBuffer(cmd);
  submissionEnqueue(cmd);
}

bool ResourcesVK::init(nvvk::Context* context, nvvk::SwapChain* swapChain, nvh::Profiler* profiler)
{
  m_gfxStateFlags2CreateInfo.flags = 0;

  m_fboChangeID  = 0;
  m_pipeChangeID = 0;

  m_context   = context;
  m_swapChain = swapChain;

  m_device      = m_context->m_device;
  m_physical    = m_context->m_physicalDevice;
  m_queue       = m_context->m_queueGCT.queue;
  m_queueFamily = m_context->m_queueGCT.familyIndex;

  initAlignedSizes((uint32_t)m_context->m_physicalInfo.properties10.limits.minUniformBufferOffsetAlignment);

  // profiler
  m_profilerVK = nvvk::ProfilerVK(profiler);
  m_profilerVK.init(m_device, m_physical);

  // submission queue
  m_submission.init(m_queue);

  // fences
  m_ringFences.init(m_device);

  // temp cmd pool
  m_ringCmdPool.init(m_device, m_queueFamily, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

  // device mem allocator
  m_memoryAllocator.init(m_device, m_physical);
  m_memoryAllocator.setAllocateFlags(VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, true);
  m_resourceAllocator.init(m_device, m_physical, &m_memoryAllocator);

  {
    // common
    VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    m_common.viewBuffer = m_resourceAllocator.createBuffer(sizeof(SceneData), usageFlags);
    m_common.viewInfo   = {m_common.viewBuffer.buffer, 0, sizeof(SceneData)};

    m_common.animBuffer = m_resourceAllocator.createBuffer(sizeof(AnimationData), usageFlags);
    m_common.animInfo   = {m_common.animBuffer.buffer, 0, sizeof(AnimationData)};
  }

  // animation
  {
    m_anim.init(m_device);
    m_anim.addBinding(ANIM_UBO, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0);
    m_anim.addBinding(ANIM_SSBO_MATRIXOUT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0);
    m_anim.addBinding(ANIM_SSBO_MATRIXORIG, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0);
    m_anim.initLayout();
    m_anim.initPipeLayout();
    m_anim.initPool(1);
  }

  // drawing
  {
    m_drawBind.init(m_device);

    m_drawBind.at(DRAW_UBO_SCENE).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);
    m_drawBind.at(DRAW_UBO_SCENE).initLayout();

    m_drawBind.at(DRAW_UBO_MATRIX).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT, 0);
    m_drawBind.at(DRAW_UBO_MATRIX).initLayout();

    m_drawBind.at(DRAW_UBO_MATERIAL).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
    m_drawBind.at(DRAW_UBO_MATERIAL).initLayout();
    m_drawBind.initPipeLayout(0);
  }

  {
    m_drawPush.init(m_device);

    m_drawPush.addBinding(DRAW_UBO_SCENE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);
    m_drawPush.initLayout();

    m_pushRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    m_pushRanges[0].size       = sizeof(uint64_t);
    m_pushRanges[0].offset     = 0;
    m_pushRanges[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    m_pushRanges[1].size       = sizeof(uint64_t);
    m_pushRanges[1].offset     = sizeof(uint64_t);

    m_drawPush.initPipeLayout(NV_ARRAY_SIZE(m_pushRanges), m_pushRanges);
  }

  {
    m_drawIndexed.init(m_device);

    m_drawIndexed.addBinding(DRAW_UBO_SCENE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                             VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);
    m_drawIndexed.addBinding(DRAW_SSBO_MATRIX, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, 0);
    m_drawIndexed.addBinding(DRAW_SSBO_MATERIAL, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    m_drawIndexed.initLayout();

    m_drawIndexed.initPipeLayout();
  }

  return true;
}

void ResourcesVK::deinit()
{
  synchronize();

  {
    m_resourceAllocator.destroy(m_common.viewBuffer);
    m_resourceAllocator.destroy(m_common.animBuffer);
  }

  m_ringFences.deinit();
  m_ringCmdPool.deinit();

  deinitScene();
  deinitFramebuffer();
  deinitPipelinesOrShaders();
  deinitPrograms();

  m_drawBind.deinit();
  m_drawPush.deinit();
  m_drawIndexed.deinit();
  m_anim.deinit();

  m_profilerVK.deinit();
  m_resourceAllocator.deinit();
  m_memoryAllocator.deinit();
}

bool ResourcesVK::initPrograms(const std::string& path, const std::string& prepend)
{
  m_shaderManager.init(m_device, m_context->m_apiMajor, m_context->m_apiMinor);
  m_shaderManager.m_filetype        = nvh::ShaderFileManager::FILETYPE_GLSL;
  m_shaderManager.m_keepModuleSPIRV = true;

  m_shaderManager.addDirectory(path);
  m_shaderManager.addDirectory(std::string("GLSL_" PROJECT_NAME));
  m_shaderManager.addDirectory(path + std::string(PROJECT_RELDIRECTORY));

  m_shaderManager.registerInclude("common.h");

  m_shaderManager.m_prepend = prepend;

  ///////////////////////////////////////////////////////////////////////////////////////////
  for(uint32_t i = 0; i < NUM_BINDINGMODES; i++)
  {
    for(uint32_t m = 0; m < NUM_MATERIAL_SHADERS; m++)
    {
      std::string defines =
          nvh::ShaderFileManager::format("#define SHADER_PERMUTATION %d\n", m)
          + nvh::ShaderFileManager::format("#define UNIFORMS_MULTISETSDYNAMIC %d\n", BINDINGMODE_DSETS)
          + nvh::ShaderFileManager::format("#define UNIFORMS_PUSHCONSTANTS_ADDRESS %d\n", BINDINGMODE_PUSHADDRESS)
          + nvh::ShaderFileManager::format("#define UNIFORMS_INDEX_BASEINSTANCE %d\n", BINDINGMODE_INDEX_BASEINSTANCE)
          + nvh::ShaderFileManager::format("#define UNIFORMS_INDEX_VERTEXATTRIB %d\n", BINDINGMODE_INDEX_VERTEXATTRIB)
          + nvh::ShaderFileManager::format("#define UNIFORMS_TECHNIQUE %d\n", i);

      m_drawShaderModules[i].vertexIDs[m] =
          m_shaderManager.createShaderModule(VK_SHADER_STAGE_VERTEX_BIT, "scene.vert.glsl", defines);
      m_drawShaderModules[i].fragmentIDs[m] =
          m_shaderManager.createShaderModule(VK_SHADER_STAGE_FRAGMENT_BIT, "scene.frag.glsl", defines);
    }
  }

  m_animShading.shaderModuleID = m_shaderManager.createShaderModule(VK_SHADER_STAGE_COMPUTE_BIT, "animation.comp.glsl");

  bool valid = m_shaderManager.areShaderModulesValid();

  if(valid)
  {
    updatedPrograms();
  }

  return valid;
}

void ResourcesVK::reloadPrograms(const std::string& prepend)
{
  m_shaderManager.m_prepend = prepend;
  m_shaderManager.reloadShaderModules();
  updatedPrograms();
  initPipelinesOrShaders(m_lastBindingMode, m_lastPipeFlags, m_lastUseShaderObjs, true);
}

void ResourcesVK::updatedPrograms()
{
  for(uint32_t i = 0; i < NUM_BINDINGMODES; i++)
  {
    for(uint32_t m = 0; m < NUM_MATERIAL_SHADERS; m++)
    {
      m_drawShaderModules[i].vertexShaders[m]   = m_shaderManager.get(m_drawShaderModules[i].vertexIDs[m]);
      m_drawShaderModules[i].fragmentShaders[m] = m_shaderManager.get(m_drawShaderModules[i].fragmentIDs[m]);
    }
  }
  m_animShading.shader = m_shaderManager.get(m_animShading.shaderModuleID);
}

void ResourcesVK::deinitPrograms()
{
  m_shaderManager.deinit();
}

static VkSampleCountFlagBits getSampleCountFlagBits(int msaa)
{
  switch(msaa)
  {
    case 2:
      return VK_SAMPLE_COUNT_2_BIT;
    case 4:
      return VK_SAMPLE_COUNT_4_BIT;
    case 8:
      return VK_SAMPLE_COUNT_8_BIT;
    default:
      return VK_SAMPLE_COUNT_1_BIT;
  }
}

bool ResourcesVK::initFramebuffer(int winWidth, int winHeight, int msaa, bool vsync)
{
  VkResult result;
  int      supersample = 1;

  m_fboChangeID++;

  if(m_framebuffer.imgColor.image != 0)
  {
    deinitFramebuffer();
  }


  int  oldMsaa     = m_framebuffer.msaa;
  bool oldResolved = m_framebuffer.supersample > 1;

  m_framebuffer.renderWidth  = winWidth * supersample;
  m_framebuffer.renderHeight = winHeight * supersample;
  m_framebuffer.supersample  = supersample;
  m_framebuffer.msaa         = msaa;
  m_framebuffer.vsync        = vsync;

  LOGI("framebuffer: %d x %d (%d msaa)\n", m_framebuffer.renderWidth, m_framebuffer.renderHeight, m_framebuffer.msaa);

  m_framebuffer.useResolved = supersample > 1 || msaa;

  VkSampleCountFlagBits samplesUsed = getSampleCountFlagBits(m_framebuffer.msaa);
  m_framebuffer.depthStencilFormat  = nvvk::findDepthStencilFormat(m_physical);

  // color
  VkImageCreateInfo cbImageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  cbImageInfo.imageType         = VK_IMAGE_TYPE_2D;
  cbImageInfo.format            = m_framebuffer.colorFormat;
  cbImageInfo.extent.width      = m_framebuffer.renderWidth;
  cbImageInfo.extent.height     = m_framebuffer.renderHeight;
  cbImageInfo.extent.depth      = 1;
  cbImageInfo.mipLevels         = 1;
  cbImageInfo.arrayLayers       = 1;
  cbImageInfo.samples           = samplesUsed;
  cbImageInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
  cbImageInfo.usage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  cbImageInfo.flags             = 0;
  cbImageInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;

  m_framebuffer.imgColor = m_resourceAllocator.createImage(cbImageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VkImageCreateInfo dsImageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  dsImageInfo.imageType         = VK_IMAGE_TYPE_2D;
  dsImageInfo.format            = m_framebuffer.depthStencilFormat;
  dsImageInfo.extent.width      = m_framebuffer.renderWidth;
  dsImageInfo.extent.height     = m_framebuffer.renderHeight;
  dsImageInfo.extent.depth      = 1;
  dsImageInfo.mipLevels         = 1;
  dsImageInfo.arrayLayers       = 1;
  dsImageInfo.samples           = samplesUsed;
  dsImageInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
  dsImageInfo.usage             = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  dsImageInfo.flags             = 0;
  dsImageInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;

  m_framebuffer.imgDepthStencil = m_resourceAllocator.createImage(dsImageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if(m_framebuffer.useResolved)
  {
    // resolve image
    VkImageCreateInfo resImageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    resImageInfo.imageType         = VK_IMAGE_TYPE_2D;
    resImageInfo.format            = m_framebuffer.colorFormat;
    resImageInfo.extent.width      = winWidth;
    resImageInfo.extent.height     = winHeight;
    resImageInfo.extent.depth      = 1;
    resImageInfo.mipLevels         = 1;
    resImageInfo.arrayLayers       = 1;
    resImageInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    resImageInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    resImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    resImageInfo.flags         = 0;
    resImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    m_framebuffer.imgColorResolved = m_resourceAllocator.createImage(resImageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }

  // views after allocation handling

  VkImageViewCreateInfo cbImageViewInfo           = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  cbImageViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
  cbImageViewInfo.format                          = cbImageInfo.format;
  cbImageViewInfo.components.r                    = VK_COMPONENT_SWIZZLE_R;
  cbImageViewInfo.components.g                    = VK_COMPONENT_SWIZZLE_G;
  cbImageViewInfo.components.b                    = VK_COMPONENT_SWIZZLE_B;
  cbImageViewInfo.components.a                    = VK_COMPONENT_SWIZZLE_A;
  cbImageViewInfo.flags                           = 0;
  cbImageViewInfo.subresourceRange.levelCount     = 1;
  cbImageViewInfo.subresourceRange.baseMipLevel   = 0;
  cbImageViewInfo.subresourceRange.layerCount     = 1;
  cbImageViewInfo.subresourceRange.baseArrayLayer = 0;
  cbImageViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;

  cbImageViewInfo.image = m_framebuffer.imgColor.image;
  result                = vkCreateImageView(m_device, &cbImageViewInfo, nullptr, &m_framebuffer.viewColor);
  assert(result == VK_SUCCESS);

  if(m_framebuffer.useResolved)
  {
    cbImageViewInfo.image = m_framebuffer.imgColorResolved.image;
    result                = vkCreateImageView(m_device, &cbImageViewInfo, nullptr, &m_framebuffer.viewColorResolved);
    assert(result == VK_SUCCESS);
  }

  VkImageViewCreateInfo dsImageViewInfo           = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  dsImageViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
  dsImageViewInfo.format                          = dsImageInfo.format;
  dsImageViewInfo.components.r                    = VK_COMPONENT_SWIZZLE_R;
  dsImageViewInfo.components.g                    = VK_COMPONENT_SWIZZLE_G;
  dsImageViewInfo.components.b                    = VK_COMPONENT_SWIZZLE_B;
  dsImageViewInfo.components.a                    = VK_COMPONENT_SWIZZLE_A;
  dsImageViewInfo.flags                           = 0;
  dsImageViewInfo.subresourceRange.levelCount     = 1;
  dsImageViewInfo.subresourceRange.baseMipLevel   = 0;
  dsImageViewInfo.subresourceRange.layerCount     = 1;
  dsImageViewInfo.subresourceRange.baseArrayLayer = 0;
  dsImageViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;

  dsImageViewInfo.image = m_framebuffer.imgDepthStencil.image;
  result                = vkCreateImageView(m_device, &dsImageViewInfo, nullptr, &m_framebuffer.viewDepthStencil);
  assert(result == VK_SUCCESS);
  // initial resource transitions
  {
    VkCommandBuffer cmd = createTempCmdBuffer();

    m_swapChain->cmdUpdateBarriers(cmd);

    cmdImageTransition(cmd, m_framebuffer.imgColor.image, VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_ACCESS_TRANSFER_READ_BIT,
                       VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    cmdImageTransition(cmd, m_framebuffer.imgDepthStencil.image, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                       0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                       VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    if(m_framebuffer.useResolved)
    {
      cmdImageTransition(cmd, m_framebuffer.imgColorResolved.image, VK_IMAGE_ASPECT_COLOR_BIT, 0,
                         VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    }

    vkEndCommandBuffer(cmd);

    submissionEnqueue(cmd);
    submissionExecute();
    synchronize();
    resetTempResources();
  }

  {
    VkViewport vp;
    VkRect2D   sc;
    vp.x        = 0;
    vp.y        = 0;
    vp.width    = float(m_framebuffer.renderWidth);
    vp.height   = float(m_framebuffer.renderHeight);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    sc.offset.x      = 0;
    sc.offset.y      = 0;
    sc.extent.width  = uint32_t(m_framebuffer.renderWidth);
    sc.extent.height = uint32_t(m_framebuffer.renderHeight);

    m_framebuffer.viewport = vp;
    m_framebuffer.scissor  = sc;

    vp.width         = float(winWidth);
    vp.height        = float(winHeight);
    sc.extent.width  = winWidth;
    sc.extent.height = winHeight;

    m_framebuffer.viewportUI = vp;
    m_framebuffer.scissorUI  = sc;
  }

  {
    VkRenderingAttachmentInfo     attachColor           = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    VkRenderingAttachmentInfo     attachDepth           = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    VkRenderingInfo               renderingInfo         = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    VkPipelineRenderingCreateInfo pipelineRenderingInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};

    attachColor.imageLayout                 = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachColor.imageView                   = m_framebuffer.viewColor;
    attachColor.loadOp                      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachColor.storeOp                     = VK_ATTACHMENT_STORE_OP_STORE;
    attachColor.clearValue.color.float32[0] = 0.2f;
    attachColor.clearValue.color.float32[1] = 0.2f;
    attachColor.clearValue.color.float32[2] = 0.2f;
    attachColor.clearValue.color.float32[3] = 0.0f;

    attachDepth.imageLayout                   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachDepth.imageView                     = m_framebuffer.viewDepthStencil;
    attachDepth.loadOp                        = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachDepth.storeOp                       = VK_ATTACHMENT_STORE_OP_STORE;
    attachDepth.clearValue.depthStencil.depth = 1.0f;

    pipelineRenderingInfo.colorAttachmentCount    = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &m_framebuffer.colorFormat;
    pipelineRenderingInfo.depthAttachmentFormat   = m_framebuffer.depthStencilFormat;

    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &m_framebuffer.attachColor;
    renderingInfo.pDepthAttachment     = &m_framebuffer.attachDepth;
    renderingInfo.renderArea.extent    = m_framebuffer.scissor.extent;
    renderingInfo.layerCount           = 1;

    m_framebuffer.attachColor           = attachColor;
    m_framebuffer.attachDepth           = attachDepth;
    m_framebuffer.renderingInfo         = renderingInfo;
    m_framebuffer.pipelineRenderingInfo = pipelineRenderingInfo;
  }

  {
    VkRenderingAttachmentInfo     attachColorUI           = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    VkRenderingInfo               renderingInfoUI         = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    VkPipelineRenderingCreateInfo pipelineRenderingInfoUI = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};

    attachColorUI.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachColorUI.imageView   = m_framebuffer.useResolved ? m_framebuffer.viewColorResolved : m_framebuffer.viewColor;
    attachColorUI.loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachColorUI.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

    pipelineRenderingInfoUI.colorAttachmentCount    = 1;
    pipelineRenderingInfoUI.pColorAttachmentFormats = &m_framebuffer.colorFormat;

    renderingInfoUI.colorAttachmentCount = 1;
    renderingInfoUI.renderArea.extent    = m_framebuffer.scissor.extent;
    renderingInfoUI.pColorAttachments    = &m_framebuffer.attachColorUI;
    renderingInfoUI.layerCount           = 1;

    m_framebuffer.attachColorUI           = attachColorUI;
    m_framebuffer.renderingInfoUI         = renderingInfoUI;
    m_framebuffer.pipelineRenderingInfoUI = pipelineRenderingInfoUI;
  }

  if(m_framebuffer.msaa != oldMsaa && hasPipes())
  {
    // reinit pipelines
    initPipelinesOrShaders(m_lastBindingMode, m_lastPipeFlags, m_lastUseShaderObjs, true);
  }

  return true;
}

void ResourcesVK::deinitFramebuffer()
{
  synchronize();

  vkDestroyImageView(m_device, m_framebuffer.viewColor, nullptr);
  vkDestroyImageView(m_device, m_framebuffer.viewDepthStencil, nullptr);
  m_framebuffer.viewColor        = nullptr;
  m_framebuffer.viewDepthStencil = nullptr;

  m_resourceAllocator.destroy(m_framebuffer.imgColor);
  m_resourceAllocator.destroy(m_framebuffer.imgDepthStencil);

  if(m_framebuffer.imgColorResolved.image)
  {
    vkDestroyImageView(m_device, m_framebuffer.viewColorResolved, nullptr);
    m_framebuffer.viewColorResolved = nullptr;

    m_resourceAllocator.destroy(m_framebuffer.imgColorResolved);
  }
}

void ResourcesVK::initPipelinesOrShaders(BindingMode bindingMode, VkPipelineCreateFlags2KHR pipeFlags, bool useShaderObjs, bool force)
{
  VkResult result;

  m_gfxState                                       = nvvk::GraphicsPipelineState();
  m_gfxState.inputAssemblyState.topology           = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  m_gfxState.depthStencilState.depthTestEnable     = true;
  m_gfxState.depthStencilState.depthWriteEnable    = true;
  m_gfxState.depthStencilState.depthCompareOp      = VK_COMPARE_OP_LESS;
  m_gfxState.multisampleState.rasterizationSamples = getSampleCountFlagBits(m_framebuffer.msaa);
  m_gfxState.rasterizationState.cullMode           = VK_CULL_MODE_NONE;
#if USE_DYNAMIC_VERTEX_STRIDE
  m_gfxState.addDynamicStateEnable(VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE);
#endif

  m_gfxState.addAttributeDescription(
      nvvk::GraphicsPipelineState::makeVertexInputAttribute(VERTEX_POS_OCTNORMAL, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0));
  m_gfxState.addBindingDescription(nvvk::GraphicsPipelineState::makeVertexInputBinding(0, sizeof(CadScene::Vertex)));

  if(bindingMode == BINDINGMODE_INDEX_VERTEXATTRIB)
  {
    m_gfxState.addAttributeDescription(
        nvvk::GraphicsPipelineState::makeVertexInputAttribute(VERTEX_COMBINED_INDEX, 1, VK_FORMAT_R32_UINT, 0));
    m_gfxState.addBindingDescription(
        nvvk::GraphicsPipelineState::makeVertexInputBinding(1, sizeof(uint32_t), VK_VERTEX_INPUT_RATE_INSTANCE));
  }

  m_gfxGen.createInfo.pNext = nullptr;
  m_gfxGen.setDevice(m_device);
  m_gfxGen.setPipelineRenderingCreateInfo(m_framebuffer.pipelineRenderingInfo);

  switch(bindingMode)
  {
    case BINDINGMODE_DSETS:
      m_gfxGen.setLayout(m_drawBind.getPipeLayout());
      break;
    case BINDINGMODE_PUSHADDRESS:
      m_gfxGen.setLayout(m_drawPush.getPipeLayout());
      break;
    case BINDINGMODE_INDEX_BASEINSTANCE:
    case BINDINGMODE_INDEX_VERTEXATTRIB:
      m_gfxGen.setLayout(m_drawIndexed.getPipeLayout());
      break;
  }

  m_gfxStateFlags2CreateInfo       = {VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR};
  m_gfxStateFlags2CreateInfo.flags = pipeFlags;
  if(pipeFlags)
  {
    // insert into chain
    m_gfxStateFlags2CreateInfo.pNext = m_gfxGen.createInfo.pNext;
    m_gfxGen.createInfo.pNext        = &m_gfxStateFlags2CreateInfo;
  }

  *(nvvk::GraphicsPipelineState*)&m_gfxStateShaderObjects = m_gfxState;
  m_gfxStateShaderObjects.addViewport(m_framebuffer.viewport);
  m_gfxStateShaderObjects.addScissor(m_framebuffer.scissor);
  m_gfxStateShaderObjects.update();

  if(!force && (bindingMode == m_lastBindingMode && pipeFlags == m_lastPipeFlags && useShaderObjs == m_lastUseShaderObjs))
    return;

  m_lastBindingMode   = bindingMode;
  m_lastPipeFlags     = pipeFlags;
  m_lastUseShaderObjs = useShaderObjs;

  m_pipeChangeID++;

  if(hasPipes())
  {
    deinitPipelinesOrShaders();
  }

  if(useShaderObjs)
  {
    VkShaderCreateInfoEXT createInfo = {VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT};
    createInfo.codeType              = VK_SHADER_CODE_TYPE_SPIRV_EXT;
    createInfo.pName                 = "main";

    if(pipeFlags & VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT)
    {
      createInfo.flags = VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT;
    }

    if(bindingMode == BINDINGMODE_DSETS)
    {
      VkDescriptorSetLayout layouts[DRAW_UBOS_NUM] = {m_drawBind.at(0).getLayout(), m_drawBind.at(1).getLayout(),
                                                      m_drawBind.at(2).getLayout()};

      createInfo.setLayoutCount = DRAW_UBOS_NUM;
      createInfo.pSetLayouts    = layouts;
    }
    else if(bindingMode == BINDINGMODE_PUSHADDRESS)
    {
      createInfo.setLayoutCount = 1;
      createInfo.pSetLayouts    = &m_drawPush.getLayout();

      createInfo.pushConstantRangeCount = NV_ARRAY_SIZE(m_pushRanges);
      createInfo.pPushConstantRanges    = m_pushRanges;
    }
    else if(bindingMode == BINDINGMODE_INDEX_BASEINSTANCE || bindingMode == BINDINGMODE_INDEX_VERTEXATTRIB)
    {
      createInfo.setLayoutCount = 1;
      createInfo.pSetLayouts    = &m_drawIndexed.getLayout();
    }

    for(uint32_t m = 0; m < NUM_MATERIAL_SHADERS; m++)
    {
      createInfo.stage     = VK_SHADER_STAGE_VERTEX_BIT;
      createInfo.nextStage = VK_SHADER_STAGE_FRAGMENT_BIT;
      m_shaderManager.getSPIRV(m_drawShaderModules[bindingMode].vertexIDs[m], &createInfo.codeSize,
                               (const uint32_t**)&createInfo.pCode);

      result = vkCreateShadersEXT(m_device, 1, &createInfo, nullptr, &m_drawShading.vertexShaderObjs[m]);
      assert(result == VK_SUCCESS);

      createInfo.stage     = VK_SHADER_STAGE_FRAGMENT_BIT;
      createInfo.nextStage = 0;
      m_shaderManager.getSPIRV(m_drawShaderModules[bindingMode].fragmentIDs[m], &createInfo.codeSize,
                               (const uint32_t**)&createInfo.pCode);

      result = vkCreateShadersEXT(m_device, 1, &createInfo, nullptr, &m_drawShading.fragmentShaderObjs[m]);
      assert(result == VK_SUCCESS);
    }
  }
  else
  {
    for(uint32_t m = 0; m < NUM_MATERIAL_SHADERS; m++)
    {
      m_gfxGen.clearShaders();
      m_gfxGen.addShader(m_drawShaderModules[bindingMode].vertexShaders[m], VK_SHADER_STAGE_VERTEX_BIT);
      m_gfxGen.addShader(m_drawShaderModules[bindingMode].fragmentShaders[m], VK_SHADER_STAGE_FRAGMENT_BIT);

      m_drawShading.pipelines[m] = m_gfxGen.createPipeline();
      assert(m_drawShading.pipelines[m] != nullptr);
    }
  }

  //////////////////////////////////////////////////////////////////////////

  {
    VkComputePipelineCreateInfo     pipelineInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    VkPipelineShaderStageCreateInfo stageInfo    = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stageInfo.stage                              = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.pName                              = "main";
    stageInfo.module                             = m_animShading.shader;

    pipelineInfo.layout = m_anim.getPipeLayout();
    pipelineInfo.stage  = stageInfo;
    result = vkCreateComputePipelines(m_device, nullptr, 1, &pipelineInfo, nullptr, &m_animShading.pipeline);
    assert(result == VK_SUCCESS);
  }
}

void ResourcesVK::deinitPipelinesOrShaders()
{
  for(uint32_t m = 0; m < NUM_MATERIAL_SHADERS; m++)
  {
    if(m_drawShading.pipelines[m])
      vkDestroyPipeline(m_device, m_drawShading.pipelines[m], nullptr);
    m_drawShading.pipelines[m] = nullptr;
    if(m_drawShading.vertexShaderObjs[m])
      vkDestroyShaderEXT(m_device, m_drawShading.vertexShaderObjs[m], nullptr);
    m_drawShading.vertexShaderObjs[m] = nullptr;
    if(m_drawShading.fragmentShaderObjs[m])
      vkDestroyShaderEXT(m_device, m_drawShading.fragmentShaderObjs[m], nullptr);
    m_drawShading.fragmentShaderObjs[m] = nullptr;
  }
  vkDestroyPipeline(m_device, m_animShading.pipeline, nullptr);
  m_animShading.pipeline = nullptr;
}

void ResourcesVK::cmdDynamicPipelineState(VkCommandBuffer cmd) const
{
  vkCmdSetViewport(cmd, 0, 1, &m_framebuffer.viewport);
  vkCmdSetScissor(cmd, 0, 1, &m_framebuffer.scissor);
}

void ResourcesVK::cmdShaderObjectState(VkCommandBuffer cmd) const
{
  m_gfxStateShaderObjects.cmdSetPipelineState(cmd);
}

void ResourcesVK::cmdBeginRendering(VkCommandBuffer cmd, bool hasSecondary) const
{
  VkRenderingInfo renderingInfo = m_framebuffer.renderingInfo;

  renderingInfo.flags = hasSecondary ? VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT : 0;

  vkCmdBeginRendering(cmd, &renderingInfo);
}

void ResourcesVK::cmdPipelineBarrier(VkCommandBuffer cmd) const
{
  // color transition
  {
    VkImageSubresourceRange colorRange;
    memset(&colorRange, 0, sizeof(colorRange));
    colorRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    colorRange.baseMipLevel   = 0;
    colorRange.levelCount     = VK_REMAINING_MIP_LEVELS;
    colorRange.baseArrayLayer = 0;
    colorRange.layerCount     = 1;

    VkImageMemoryBarrier memBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    memBarrier.srcAccessMask        = VK_ACCESS_TRANSFER_READ_BIT;
    memBarrier.dstAccessMask        = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    memBarrier.oldLayout            = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    memBarrier.newLayout            = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    memBarrier.image                = m_framebuffer.imgColor.image;
    memBarrier.subresourceRange     = colorRange;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_FALSE,
                         0, nullptr, 0, nullptr, 1, &memBarrier);
  }

  // Prepare the depth+stencil for reading.
  {
    VkImageSubresourceRange depthStencilRange;
    memset(&depthStencilRange, 0, sizeof(depthStencilRange));
    depthStencilRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    depthStencilRange.baseMipLevel   = 0;
    depthStencilRange.levelCount     = VK_REMAINING_MIP_LEVELS;
    depthStencilRange.baseArrayLayer = 0;
    depthStencilRange.layerCount     = 1;

    VkImageMemoryBarrier memBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};

    memBarrier.image            = m_framebuffer.imgDepthStencil.image;
    memBarrier.subresourceRange = depthStencilRange;

    memBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    memBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    memBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                         VK_FALSE, 0, nullptr, 0, nullptr, 1, &memBarrier);
  }
}

void ResourcesVK::cmdImageTransition(VkCommandBuffer    cmd,
                                     VkImage            img,
                                     VkImageAspectFlags aspects,
                                     VkAccessFlags      src,
                                     VkAccessFlags      dst,
                                     VkImageLayout      oldLayout,
                                     VkImageLayout      newLayout) const
{

  VkPipelineStageFlags srcPipe = nvvk::makeAccessMaskPipelineStageFlags(src);
  VkPipelineStageFlags dstPipe = nvvk::makeAccessMaskPipelineStageFlags(dst);

  VkImageSubresourceRange range;
  memset(&range, 0, sizeof(range));
  range.aspectMask     = aspects;
  range.baseMipLevel   = 0;
  range.levelCount     = VK_REMAINING_MIP_LEVELS;
  range.baseArrayLayer = 0;
  range.layerCount     = VK_REMAINING_ARRAY_LAYERS;

  VkImageMemoryBarrier memBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  memBarrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  memBarrier.dstAccessMask        = dst;
  memBarrier.srcAccessMask        = src;
  memBarrier.oldLayout            = oldLayout;
  memBarrier.newLayout            = newLayout;
  memBarrier.image                = img;
  memBarrier.subresourceRange     = range;

  vkCmdPipelineBarrier(cmd, srcPipe, dstPipe, VK_FALSE, 0, nullptr, 0, nullptr, 1, &memBarrier);
}

VkCommandBuffer ResourcesVK::createCmdBuffer(VkCommandPool pool, bool singleshot, bool primary, bool secondaryInClear) const
{
  VkResult result;
  bool     secondary = !primary;

  // Create the command buffer.
  VkCommandBufferAllocateInfo cmdInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cmdInfo.commandPool                 = pool;
  cmdInfo.level                       = primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
  cmdInfo.commandBufferCount          = 1;
  VkCommandBuffer cmd;
  result = vkAllocateCommandBuffers(m_device, &cmdInfo, &cmd);
  assert(result == VK_SUCCESS);

  cmdBegin(cmd, singleshot, primary, secondaryInClear);

  return cmd;
}

VkCommandBuffer ResourcesVK::createTempCmdBuffer(bool primary /*=true*/, bool secondaryInClear /*=false*/)
{
  VkCommandBuffer cmd =
      m_ringCmdPool.createCommandBuffer(primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY, false);
  cmdBegin(cmd, true, primary, secondaryInClear);
  return cmd;
}

void ResourcesVK::cmdBegin(VkCommandBuffer cmd, bool singleshot, bool primary, bool secondaryInClear) const
{
  VkResult result;
  bool     secondary = !primary;

  VkCommandBufferInheritanceInfo inheritInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
  VkCommandBufferInheritanceRenderingInfo inheritRenderInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO};

  if(secondary)
  {
    inheritInfo.pNext                         = &inheritRenderInfo;
    inheritRenderInfo.rasterizationSamples    = getSampleCountFlagBits(m_framebuffer.msaa);
    inheritRenderInfo.colorAttachmentCount    = 1;
    inheritRenderInfo.pColorAttachmentFormats = &m_framebuffer.colorFormat;
    inheritRenderInfo.depthAttachmentFormat   = m_framebuffer.depthStencilFormat;
    inheritRenderInfo.flags                   = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
  }

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  // the sample is resubmitting re-use commandbuffers to the queue while they may still be executed by GPU
  // we only use fences to prevent deleting commandbuffers that are still in flight
  beginInfo.flags = singleshot ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
  // the sample's secondary buffers always are called within passes as they contain drawcalls
  beginInfo.flags |= secondary ? VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT : 0;
  beginInfo.pInheritanceInfo = &inheritInfo;

  result = vkBeginCommandBuffer(cmd, &beginInfo);
  assert(result == VK_SUCCESS);
}

void ResourcesVK::resetTempResources()
{
  synchronize();
  m_ringFences.reset();
  m_ringCmdPool.reset();
}

bool ResourcesVK::initScene(const CadScene& cadscene)
{
  VkResult result = VK_SUCCESS;

  m_numMatrices = uint(cadscene.m_matrices.size());

  CadSceneVK::Config cfg;
#if USE_SINGLE_GEOMETRY_ALLOCATION
  cfg.singleAllocation = true;
#endif
  m_scene.init(cadscene, m_resourceAllocator, m_queue, m_queueFamily, cfg);


  {
    //////////////////////////////////////////////////////////////////////////
    // Allocation phase
    //////////////////////////////////////////////////////////////////////////

    m_drawBind.at(DRAW_UBO_SCENE).initPool(1);
    m_drawBind.at(DRAW_UBO_MATRIX).initPool(1);
    m_drawBind.at(DRAW_UBO_MATERIAL).initPool(1);

    m_drawPush.initPool(1);

    m_drawIndexed.initPool(1);

    //////////////////////////////////////////////////////////////////////////
    // Update phase
    //////////////////////////////////////////////////////////////////////////
    std::vector<VkWriteDescriptorSet> updateDescriptors;

    updateDescriptors.push_back(m_drawBind.at(DRAW_UBO_SCENE).makeWrite(0, 0, &m_common.viewInfo));
    updateDescriptors.push_back(m_drawBind.at(DRAW_UBO_MATRIX).makeWrite(0, 0, &m_scene.m_infos.matricesSingle));
    updateDescriptors.push_back(m_drawBind.at(DRAW_UBO_MATERIAL).makeWrite(0, 0, &m_scene.m_infos.materialsSingle));

    updateDescriptors.push_back(m_drawPush.makeWrite(0, DRAW_UBO_SCENE, &m_common.viewInfo));

    updateDescriptors.push_back(m_drawIndexed.makeWrite(0, DRAW_UBO_SCENE, &m_common.viewInfo));
    updateDescriptors.push_back(m_drawIndexed.makeWrite(0, DRAW_SSBO_MATRIX, &m_scene.m_infos.matrices));
    updateDescriptors.push_back(m_drawIndexed.makeWrite(0, DRAW_SSBO_MATERIAL, &m_scene.m_infos.materials));

    updateDescriptors.push_back(m_anim.makeWrite(0, ANIM_UBO, &m_common.animInfo));
    updateDescriptors.push_back(m_anim.makeWrite(0, ANIM_SSBO_MATRIXOUT, &m_scene.m_infos.matrices));
    updateDescriptors.push_back(m_anim.makeWrite(0, ANIM_SSBO_MATRIXORIG, &m_scene.m_infos.matricesOrig));

    vkUpdateDescriptorSets(m_device, updateDescriptors.size(), updateDescriptors.data(), 0, 0);
  }

  return true;
}

void ResourcesVK::deinitScene()
{
  // guard by synchronization as some stuff is unsafe to delete while in use
  synchronize();

  m_drawBind.deinitPools();
  m_drawPush.deinitPool();
  m_drawIndexed.deinitPool();
  m_scene.deinit();
}

void ResourcesVK::synchronize()
{
  vkDeviceWaitIdle(m_device);
}

void ResourcesVK::animation(const Global& global)
{
  VkCommandBuffer cmd = createTempCmdBuffer();

  vkCmdUpdateBuffer(cmd, m_common.animBuffer.buffer, 0, sizeof(AnimationData), (const uint32_t*)&global.animUbo);
  {
    VkBufferMemoryBarrier memBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    memBarrier.srcAccessMask         = VK_ACCESS_TRANSFER_WRITE_BIT;
    memBarrier.dstAccessMask         = VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
    memBarrier.buffer                = m_common.animBuffer.buffer;
    memBarrier.size                  = sizeof(AnimationData);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_FALSE, 0,
                         nullptr, 1, &memBarrier, 0, nullptr);
  }

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_animShading.pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_anim.getPipeLayout(), 0, 1, m_anim.getSets(), 0, 0);
  vkCmdDispatch(cmd, (m_numMatrices + ANIMATION_WORKGROUPSIZE - 1) / ANIMATION_WORKGROUPSIZE, 1, 1);

  {
    VkBufferMemoryBarrier memBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    memBarrier.srcAccessMask         = VK_ACCESS_SHADER_WRITE_BIT;
    memBarrier.dstAccessMask         = VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
    memBarrier.buffer                = m_scene.m_buffers.matrices.buffer;
    memBarrier.size                  = sizeof(CadScene::MatrixNode) * m_numMatrices;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_FALSE, 0,
                         nullptr, 1, &memBarrier, 0, nullptr);
  }

  vkEndCommandBuffer(cmd);

  submissionEnqueue(cmd);
}

void ResourcesVK::animationReset()
{
  VkCommandBuffer cmd = createTempCmdBuffer();
  VkBufferCopy    copy;
  copy.size      = sizeof(MatrixData) * m_numMatrices;
  copy.dstOffset = 0;
  copy.srcOffset = 0;
  vkCmdCopyBuffer(cmd, m_scene.m_buffers.matricesOrig.buffer, m_scene.m_buffers.matrices.buffer, 1, &copy);
  vkEndCommandBuffer(cmd);

  submissionEnqueue(cmd);
}
}  // namespace generatedcmds
