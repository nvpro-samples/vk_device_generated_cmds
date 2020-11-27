/* Copyright (c) 2019-2020, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <algorithm>
#include <imgui/imgui_impl_vk.h>
#include <nvh/nvprint.hpp>

#include "resources_vk.hpp"

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

void ResourcesVK::blitFrame(const Global& global)
{
  VkCommandBuffer cmd = createTempCmdBuffer();

  nvh::Profiler::SectionID sec = m_profilerVK.beginSection("BltUI", cmd);

  VkImage imageBlitRead = m_framebuffer.imgColor;

  if(m_framebuffer.useResolved)
  {
    cmdImageTransition(cmd, m_framebuffer.imgColor, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
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

      imageBlitRead = m_framebuffer.imgColorResolved;

      vkCmdResolveImage(cmd, m_framebuffer.imgColor, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageBlitRead,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
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

      imageBlitRead = m_framebuffer.imgColorResolved;

      vkCmdBlitImage(cmd, m_framebuffer.imgColor, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imageBlitRead,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR);
    }
  }

  // It would be better to render the ui ontop of backbuffer
  // instead of using the "resolved" image here, as it would avoid an additional
  // blit. However, for the simplicity to pass a final image in the OpenGL mode
  // we avoid rendering to backbuffer directly.

  if(global.imguiDrawData)
  {
    VkRenderPassBeginInfo renderPassBeginInfo    = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassBeginInfo.renderPass               = m_framebuffer.passUI;
    renderPassBeginInfo.framebuffer              = m_framebuffer.fboUI;
    renderPassBeginInfo.renderArea.offset.x      = 0;
    renderPassBeginInfo.renderArea.offset.y      = 0;
    renderPassBeginInfo.renderArea.extent.width  = global.winWidth;
    renderPassBeginInfo.renderArea.extent.height = global.winHeight;
    renderPassBeginInfo.clearValueCount          = 0;
    renderPassBeginInfo.pClearValues             = nullptr;

    vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetViewport(cmd, 0, 1, &m_framebuffer.viewportUI);
    vkCmdSetScissor(cmd, 0, 1, &m_framebuffer.scissorUI);

    ImGui::RenderDrawDataVK(cmd, global.imguiDrawData);

    vkCmdEndRenderPass(cmd);

    // turns imageBlitRead to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
  }
  else
  {
    if(m_framebuffer.useResolved)
    {
      cmdImageTransition(cmd, m_framebuffer.imgColorResolved, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                         VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    }
    else
    {
      cmdImageTransition(cmd, m_framebuffer.imgColor, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
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
    cmdImageTransition(cmd, m_framebuffer.imgColorResolved, VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                       VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  }

  m_profilerVK.endSection(sec, cmd);

  vkEndCommandBuffer(cmd);
  submissionEnqueue(cmd);
}

bool ResourcesVK::init(nvvk::Context* context, nvvk::SwapChain* swapChain, nvh::Profiler* profiler)
{
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

  // Create the render passes
  {
    m_framebuffer.passClear    = createPass(true, m_framebuffer.msaa);
    m_framebuffer.passPreserve = createPass(false, m_framebuffer.msaa);
    m_framebuffer.passUI       = createPassUI(m_framebuffer.msaa);
  }
  // device mem allocator
  m_memAllocator.init(m_device, m_physical);
  {
    // common
    VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    m_common.viewBuffer = m_memAllocator.createBuffer(sizeof(SceneData), usageFlags, m_common.viewAID);
    m_common.viewInfo   = {m_common.viewBuffer, 0, sizeof(SceneData)};

    m_common.animBuffer = m_memAllocator.createBuffer(sizeof(AnimationData), usageFlags, m_common.animAID);
    m_common.animInfo   = {m_common.animBuffer, 0, sizeof(AnimationData)};
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

    VkPushConstantRange pushRanges[2];
    pushRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRanges[0].size       = sizeof(uint64_t);
    pushRanges[0].offset     = 0;
    pushRanges[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRanges[1].size       = sizeof(uint64_t);
    pushRanges[1].offset     = sizeof(uint64_t);

    m_drawPush.initPipeLayout(NV_ARRAY_SIZE(pushRanges), pushRanges);
  }

  {
    ImGui::InitVK(m_context->m_device, m_context->m_physicalDevice, m_context->m_queueGCT,
                  m_context->m_queueGCT.familyIndex, m_framebuffer.passUI);
  }

  return true;
}

void ResourcesVK::deinit()
{
  synchronize();

  ImGui::ShutdownVK();

  {
    vkDestroyBuffer(m_device, m_common.viewBuffer, NULL);
    m_memAllocator.free(m_common.viewAID);
    vkDestroyBuffer(m_device, m_common.animBuffer, NULL);
    m_memAllocator.free(m_common.animAID);
  }

  m_ringFences.deinit();
  m_ringCmdPool.deinit();

  deinitScene();
  deinitFramebuffer();
  deinitPipes();
  deinitPrograms();

  vkDestroyRenderPass(m_device, m_framebuffer.passClear, NULL);
  vkDestroyRenderPass(m_device, m_framebuffer.passPreserve, NULL);
  vkDestroyRenderPass(m_device, m_framebuffer.passUI, NULL);

  m_drawBind.deinit();
  m_drawPush.deinit();
  m_anim.deinit();

  m_profilerVK.deinit();
  m_memAllocator.deinit();
}

bool ResourcesVK::initPrograms(const std::string& path, const std::string& prepend)
{
  m_shaderManager.init(m_device);
  m_shaderManager.m_filetype       = nvh::ShaderFileManager::FILETYPE_GLSL;

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
      std::string defines = std::string("#define WIREMODE 0\n")
                            + nvh::ShaderFileManager::format("#define SHADER_PERMUTATION %d\n", m)
                            + nvh::ShaderFileManager::format("#define UNIFORMS_TECHNIQUE %d\n", i);

      m_drawShading[i].vertexIDs[m] = m_shaderManager.createShaderModule(VK_SHADER_STAGE_VERTEX_BIT, "scene.vert.glsl", defines);
      m_drawShading[i].fragmentIDs[m] =
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
}

void ResourcesVK::updatedPrograms()
{
  for(uint32_t i = 0; i < NUM_BINDINGMODES; i++)
  {
    for(uint32_t m = 0; m < NUM_MATERIAL_SHADERS; m++)
    {
      m_drawShading[i].vertexShaders[m]   = m_shaderManager.get(m_drawShading[i].vertexIDs[m]);
      m_drawShading[i].fragmentShaders[m] = m_shaderManager.get(m_drawShading[i].fragmentIDs[m]);
    }
  }
  m_animShading.shader = m_shaderManager.get(m_animShading.shaderModuleID);

  initPipes();
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

VkRenderPass ResourcesVK::createPass(bool clear, int msaa)
{
  VkResult result;

  VkAttachmentLoadOp loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;

  VkSampleCountFlagBits samplesUsed = getSampleCountFlagBits(msaa);

  // Create the render pass
  VkAttachmentDescription attachments[2] = {};
  attachments[0].format                  = VK_FORMAT_R8G8B8A8_UNORM;
  attachments[0].samples                 = samplesUsed;
  attachments[0].loadOp                  = loadOp;
  attachments[0].storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].initialLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  attachments[0].finalLayout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  attachments[0].flags                   = 0;

  VkFormat depthStencilFormat = nvvk::findDepthStencilFormat(m_physical);

  attachments[1].format              = depthStencilFormat;
  attachments[1].samples             = samplesUsed;
  attachments[1].loadOp              = loadOp;
  attachments[1].storeOp             = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[1].stencilLoadOp       = loadOp;
  attachments[1].stencilStoreOp      = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[1].initialLayout       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  attachments[1].finalLayout         = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  attachments[1].flags               = 0;
  VkSubpassDescription subpass       = {};
  subpass.pipelineBindPoint          = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.inputAttachmentCount       = 0;
  VkAttachmentReference colorRefs[1] = {{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
  subpass.colorAttachmentCount       = NV_ARRAY_SIZE(colorRefs);
  subpass.pColorAttachments          = colorRefs;
  VkAttachmentReference depthRefs[1] = {{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}};
  subpass.pDepthStencilAttachment    = depthRefs;
  VkRenderPassCreateInfo rpInfo      = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  rpInfo.attachmentCount             = NV_ARRAY_SIZE(attachments);
  rpInfo.pAttachments                = attachments;
  rpInfo.subpassCount                = 1;
  rpInfo.pSubpasses                  = &subpass;
  rpInfo.dependencyCount             = 0;

  VkRenderPass rp;
  result = vkCreateRenderPass(m_device, &rpInfo, NULL, &rp);
  assert(result == VK_SUCCESS);
  return rp;
}


VkRenderPass ResourcesVK::createPassUI(int msaa)
{
  // ui related
  // two cases:
  // if msaa we want to render into scene_color_resolved, which was DST_OPTIMAL
  // otherwise render into scene_color, which was VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
  VkImageLayout uiTargetLayout = msaa ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  // Create the ui render pass
  VkAttachmentDescription attachments[1] = {};
  attachments[0].format                  = VK_FORMAT_R8G8B8A8_UNORM;
  attachments[0].samples                 = VK_SAMPLE_COUNT_1_BIT;
  attachments[0].loadOp                  = VK_ATTACHMENT_LOAD_OP_LOAD;
  attachments[0].storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].initialLayout           = uiTargetLayout;
  attachments[0].finalLayout             = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;  // for blit operation
  attachments[0].flags                   = 0;

  VkSubpassDescription subpass       = {};
  subpass.pipelineBindPoint          = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.inputAttachmentCount       = 0;
  VkAttachmentReference colorRefs[1] = {{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
  subpass.colorAttachmentCount       = NV_ARRAY_SIZE(colorRefs);
  subpass.pColorAttachments          = colorRefs;
  subpass.pDepthStencilAttachment    = nullptr;
  VkRenderPassCreateInfo rpInfo      = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  rpInfo.attachmentCount             = NV_ARRAY_SIZE(attachments);
  rpInfo.pAttachments                = attachments;
  rpInfo.subpassCount                = 1;
  rpInfo.pSubpasses                  = &subpass;
  rpInfo.dependencyCount             = 0;

  VkRenderPass rp;
  VkResult     result = vkCreateRenderPass(m_device, &rpInfo, NULL, &rp);
  assert(result == VK_SUCCESS);
  return rp;
}


bool ResourcesVK::initFramebuffer(int winWidth, int winHeight, int msaa, bool vsync)
{
  VkResult result;
  int      supersample = 1;

  m_fboChangeID++;

  if(m_framebuffer.imgColor != 0)
  {
    deinitFramebuffer();
  }

  m_framebuffer.memAllocator.init(m_device, m_physical);

  int  oldMsaa     = m_framebuffer.msaa;
  bool oldResolved = m_framebuffer.supersample > 1;

  m_framebuffer.renderWidth  = winWidth * supersample;
  m_framebuffer.renderHeight = winHeight * supersample;
  m_framebuffer.supersample  = supersample;
  m_framebuffer.msaa         = msaa;
  m_framebuffer.vsync        = vsync;

  LOGI("framebuffer: %d x %d (%d msaa)\n", m_framebuffer.renderWidth, m_framebuffer.renderHeight, m_framebuffer.msaa);

  m_framebuffer.useResolved = supersample > 1 || msaa;

  if(oldMsaa != m_framebuffer.msaa || oldResolved != m_framebuffer.useResolved)
  {
    vkDestroyRenderPass(m_device, m_framebuffer.passClear, NULL);
    vkDestroyRenderPass(m_device, m_framebuffer.passPreserve, NULL);
    vkDestroyRenderPass(m_device, m_framebuffer.passUI, NULL);

    // recreate the render passes with new msaa setting
    m_framebuffer.passClear    = createPass(true, m_framebuffer.msaa);
    m_framebuffer.passPreserve = createPass(false, m_framebuffer.msaa);
    m_framebuffer.passUI       = createPassUI(m_framebuffer.msaa);
  }

  VkSampleCountFlagBits samplesUsed = getSampleCountFlagBits(m_framebuffer.msaa);

  // color
  VkImageCreateInfo cbImageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  cbImageInfo.imageType         = VK_IMAGE_TYPE_2D;
  cbImageInfo.format            = VK_FORMAT_R8G8B8A8_UNORM;
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

  m_framebuffer.imgColor = m_framebuffer.memAllocator.createImage(cbImageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // depth stencil
  VkFormat depthStencilFormat = nvvk::findDepthStencilFormat(m_physical);

  VkImageCreateInfo dsImageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  dsImageInfo.imageType         = VK_IMAGE_TYPE_2D;
  dsImageInfo.format            = depthStencilFormat;
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

  m_framebuffer.imgDepthStencil = m_framebuffer.memAllocator.createImage(dsImageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if(m_framebuffer.useResolved)
  {
    // resolve image
    VkImageCreateInfo resImageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    resImageInfo.imageType         = VK_IMAGE_TYPE_2D;
    resImageInfo.format            = VK_FORMAT_R8G8B8A8_UNORM;
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

    m_framebuffer.imgColorResolved = m_framebuffer.memAllocator.createImage(resImageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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

  cbImageViewInfo.image = m_framebuffer.imgColor;
  result                = vkCreateImageView(m_device, &cbImageViewInfo, NULL, &m_framebuffer.viewColor);
  assert(result == VK_SUCCESS);

  if(m_framebuffer.useResolved)
  {
    cbImageViewInfo.image = m_framebuffer.imgColorResolved;
    result                = vkCreateImageView(m_device, &cbImageViewInfo, NULL, &m_framebuffer.viewColorResolved);
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

  dsImageViewInfo.image = m_framebuffer.imgDepthStencil;
  result                = vkCreateImageView(m_device, &dsImageViewInfo, NULL, &m_framebuffer.viewDepthStencil);
  assert(result == VK_SUCCESS);
  // initial resource transitions
  {
    VkCommandBuffer cmd = createTempCmdBuffer();

    m_swapChain->cmdUpdateBarriers(cmd);

    cmdImageTransition(cmd, m_framebuffer.imgColor, VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_ACCESS_TRANSFER_READ_BIT,
                       VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    cmdImageTransition(cmd, m_framebuffer.imgDepthStencil, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0,
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                       VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    if(m_framebuffer.useResolved)
    {
      cmdImageTransition(cmd, m_framebuffer.imgColorResolved, VK_IMAGE_ASPECT_COLOR_BIT, 0,
                         VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    }

    vkEndCommandBuffer(cmd);

    submissionEnqueue(cmd);
    submissionExecute();
    synchronize();
    resetTempResources();
  }

  {
    // Create framebuffers
    VkImageView bindInfos[2];
    bindInfos[0] = m_framebuffer.viewColor;
    bindInfos[1] = m_framebuffer.viewDepthStencil;

    VkFramebuffer           fb;
    VkFramebufferCreateInfo fbInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbInfo.attachmentCount         = NV_ARRAY_SIZE(bindInfos);
    fbInfo.pAttachments            = bindInfos;
    fbInfo.width                   = m_framebuffer.renderWidth;
    fbInfo.height                  = m_framebuffer.renderHeight;
    fbInfo.layers                  = 1;

    fbInfo.renderPass = m_framebuffer.passClear;
    result            = vkCreateFramebuffer(m_device, &fbInfo, NULL, &fb);
    assert(result == VK_SUCCESS);
    m_framebuffer.fboScene = fb;
  }


  // ui related
  {
    VkImageView uiTarget = m_framebuffer.useResolved ? m_framebuffer.viewColorResolved : m_framebuffer.viewColor;

    // Create framebuffers
    VkImageView bindInfos[1];
    bindInfos[0] = uiTarget;

    VkFramebuffer           fb;
    VkFramebufferCreateInfo fbInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbInfo.attachmentCount         = NV_ARRAY_SIZE(bindInfos);
    fbInfo.pAttachments            = bindInfos;
    fbInfo.width                   = winWidth;
    fbInfo.height                  = winHeight;
    fbInfo.layers                  = 1;

    fbInfo.renderPass = m_framebuffer.passUI;
    result            = vkCreateFramebuffer(m_device, &fbInfo, NULL, &fb);
    assert(result == VK_SUCCESS);
    m_framebuffer.fboUI = fb;
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
    sc.extent.width  = m_framebuffer.renderWidth;
    sc.extent.height = m_framebuffer.renderHeight;

    m_framebuffer.viewport = vp;
    m_framebuffer.scissor  = sc;

    vp.width         = float(winWidth);
    vp.height        = float(winHeight);
    sc.extent.width  = winWidth;
    sc.extent.height = winHeight;

    m_framebuffer.viewportUI = vp;
    m_framebuffer.scissorUI  = sc;
  }


  if(m_framebuffer.msaa != oldMsaa)
  {
    ImGui::ReInitPipelinesVK(m_framebuffer.passUI);
  }
  if(m_framebuffer.msaa != oldMsaa && hasPipes())
  {
    // reinit pipelines
    initPipes();
  }

  return true;
}

void ResourcesVK::deinitFramebuffer()
{
  synchronize();

  vkDestroyImageView(m_device, m_framebuffer.viewColor, nullptr);
  vkDestroyImageView(m_device, m_framebuffer.viewDepthStencil, nullptr);
  m_framebuffer.viewColor        = VK_NULL_HANDLE;
  m_framebuffer.viewDepthStencil = VK_NULL_HANDLE;

  vkDestroyImage(m_device, m_framebuffer.imgColor, nullptr);
  vkDestroyImage(m_device, m_framebuffer.imgDepthStencil, nullptr);
  m_framebuffer.imgColor        = VK_NULL_HANDLE;
  m_framebuffer.imgDepthStencil = VK_NULL_HANDLE;

  if(m_framebuffer.imgColorResolved)
  {
    vkDestroyImageView(m_device, m_framebuffer.viewColorResolved, nullptr);
    m_framebuffer.viewColorResolved = VK_NULL_HANDLE;

    vkDestroyImage(m_device, m_framebuffer.imgColorResolved, nullptr);
    m_framebuffer.imgColorResolved = VK_NULL_HANDLE;
  }

  vkDestroyFramebuffer(m_device, m_framebuffer.fboScene, nullptr);
  m_framebuffer.fboScene = VK_NULL_HANDLE;

  vkDestroyFramebuffer(m_device, m_framebuffer.fboUI, nullptr);
  m_framebuffer.fboUI = VK_NULL_HANDLE;

  m_framebuffer.memAllocator.freeAll();
  m_framebuffer.memAllocator.deinit();
}

void ResourcesVK::initPipes()
{
  VkResult result;

  m_pipeChangeID++;

  if(hasPipes())
  {
    deinitPipes();
  }

  m_gfxState = nvvk::GraphicsPipelineState();

  m_gfxGen.createInfo.flags = m_gfxStatePipelineFlags;
  m_gfxGen.setDevice(m_device);
  m_gfxState.addAttributeDescription(
      nvvk::GraphicsPipelineState::makeVertexInputAttribute(VERTEX_POS_OCTNORMAL, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0));
  m_gfxState.addBindingDescription(nvvk::GraphicsPipelineState::makeVertexInputBinding(0, sizeof(CadScene::Vertex)));
  m_gfxState.inputAssemblyState.topology        = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  m_gfxState.depthStencilState.depthTestEnable  = true;
  m_gfxState.depthStencilState.depthWriteEnable = true;
  m_gfxState.depthStencilState.depthCompareOp   = VK_COMPARE_OP_LESS;

  m_gfxState.multisampleState.rasterizationSamples = getSampleCountFlagBits(m_framebuffer.msaa);

  m_gfxGen.setRenderPass(m_framebuffer.passPreserve);

  for(uint32_t i = 0; i < NUM_BINDINGMODES; i++)
  {
    for(uint32_t m = 0; m < NUM_MATERIAL_SHADERS; m++)
    {
      m_gfxGen.setLayout(i == 0 ? m_drawBind.getPipeLayout() : m_drawPush.getPipeLayout());

      m_gfxGen.clearShaders();
      m_gfxGen.addShader(m_drawShading[i].vertexShaders[m], VK_SHADER_STAGE_VERTEX_BIT);
      m_gfxGen.addShader(m_drawShading[i].fragmentShaders[m], VK_SHADER_STAGE_FRAGMENT_BIT);

      m_drawShading[i].pipelines[m] = m_gfxGen.createPipeline();
      assert(m_drawShading[i].pipelines[m] != VK_NULL_HANDLE);
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
    result = vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &m_animShading.pipeline);
    assert(result == VK_SUCCESS);
  }
}

void ResourcesVK::deinitPipes()
{
  for(uint32_t i = 0; i < NUM_BINDINGMODES; i++)
  {
    for(uint32_t m = 0; m < NUM_MATERIAL_SHADERS; m++)
    {
      vkDestroyPipeline(m_device, m_drawShading[i].pipelines[m], NULL);
      m_drawShading[i].pipelines[m] = VK_NULL_HANDLE;
    }
  }
  vkDestroyPipeline(m_device, m_animShading.pipeline, NULL);
  m_animShading.pipeline = VK_NULL_HANDLE;
}

void ResourcesVK::cmdDynamicState(VkCommandBuffer cmd) const
{
  vkCmdSetViewport(cmd, 0, 1, &m_framebuffer.viewport);
  vkCmdSetScissor(cmd, 0, 1, &m_framebuffer.scissor);
}

void ResourcesVK::cmdBeginRenderPass(VkCommandBuffer cmd, bool clear, bool hasSecondary) const
{
  VkRenderPassBeginInfo renderPassBeginInfo    = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  renderPassBeginInfo.renderPass               = clear ? m_framebuffer.passClear : m_framebuffer.passPreserve;
  renderPassBeginInfo.framebuffer              = m_framebuffer.fboScene;
  renderPassBeginInfo.renderArea.offset.x      = 0;
  renderPassBeginInfo.renderArea.offset.y      = 0;
  renderPassBeginInfo.renderArea.extent.width  = m_framebuffer.renderWidth;
  renderPassBeginInfo.renderArea.extent.height = m_framebuffer.renderHeight;
  renderPassBeginInfo.clearValueCount          = 2;
  VkClearValue clearValues[2];
  clearValues[0].color.float32[0]     = 0.2f;
  clearValues[0].color.float32[1]     = 0.2f;
  clearValues[0].color.float32[2]     = 0.2f;
  clearValues[0].color.float32[3]     = 0.0f;
  clearValues[1].depthStencil.depth   = 1.0f;
  clearValues[1].depthStencil.stencil = 0;
  renderPassBeginInfo.pClearValues    = clearValues;
  vkCmdBeginRenderPass(cmd, &renderPassBeginInfo,
                       hasSecondary ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE);
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
    memBarrier.image                = m_framebuffer.imgColor;
    memBarrier.subresourceRange     = colorRange;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_FALSE,
                         0, NULL, 0, NULL, 1, &memBarrier);
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
    memBarrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    memBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    memBarrier.oldLayout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    memBarrier.newLayout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    memBarrier.image         = m_framebuffer.imgDepthStencil;
    memBarrier.subresourceRange = depthStencilRange;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                         VK_FALSE, 0, NULL, 0, NULL, 1, &memBarrier);
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

  vkCmdPipelineBarrier(cmd, srcPipe, dstPipe, VK_FALSE, 0, NULL, 0, NULL, 1, &memBarrier);
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
  if(secondary)
  {
    inheritInfo.renderPass  = secondaryInClear ? m_framebuffer.passClear : m_framebuffer.passPreserve;
    inheritInfo.framebuffer = m_framebuffer.fboScene;
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
  cfg.singleAllocation = USE_SINGLE_GEOMETRY_BUFFERS;
  cfg.needAddress      = true;
  m_scene.init(cadscene, m_device, m_physical, m_queue, m_queueFamily, cfg);


  {
    //////////////////////////////////////////////////////////////////////////
    // Allocation phase
    //////////////////////////////////////////////////////////////////////////

    m_drawBind.at(DRAW_UBO_SCENE).initPool(1);
    m_drawBind.at(DRAW_UBO_MATRIX).initPool(1);
    m_drawBind.at(DRAW_UBO_MATERIAL).initPool(1);

    m_drawPush.initPool(1);

    //////////////////////////////////////////////////////////////////////////
    // Update phase
    //////////////////////////////////////////////////////////////////////////
    std::vector<VkWriteDescriptorSet> updateDescriptors;

    updateDescriptors.push_back(m_drawBind.at(DRAW_UBO_SCENE).makeWrite(0, 0, &m_common.viewInfo));
    updateDescriptors.push_back(m_drawBind.at(DRAW_UBO_MATRIX).makeWrite(0, 0, &m_scene.m_infos.matricesSingle));
    updateDescriptors.push_back(m_drawBind.at(DRAW_UBO_MATERIAL).makeWrite(0, 0, &m_scene.m_infos.materialsSingle));

    updateDescriptors.push_back(m_drawPush.makeWrite(0, DRAW_UBO_SCENE, &m_common.viewInfo));

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
  m_scene.deinit();
}

void ResourcesVK::synchronize()
{
  vkDeviceWaitIdle(m_device);
}

void ResourcesVK::animation(const Global& global)
{
  VkCommandBuffer cmd = createTempCmdBuffer();

  vkCmdUpdateBuffer(cmd, m_common.animBuffer, 0, sizeof(AnimationData), (const uint32_t*)&global.animUbo);
  {
    VkBufferMemoryBarrier memBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    memBarrier.srcAccessMask         = VK_ACCESS_TRANSFER_WRITE_BIT;
    memBarrier.dstAccessMask         = VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
    memBarrier.buffer                = m_common.animBuffer;
    memBarrier.size                  = sizeof(AnimationData);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_FALSE, 0, NULL,
                         1, &memBarrier, 0, NULL);
  }

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_animShading.pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_anim.getPipeLayout(), 0, 1, m_anim.getSets(), 0, 0);
  vkCmdDispatch(cmd, (m_numMatrices + ANIMATION_WORKGROUPSIZE - 1) / ANIMATION_WORKGROUPSIZE, 1, 1);

  {
    VkBufferMemoryBarrier memBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    memBarrier.srcAccessMask         = VK_ACCESS_SHADER_WRITE_BIT;
    memBarrier.dstAccessMask         = VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
    memBarrier.buffer                = m_scene.m_buffers.matrices;
    memBarrier.size                  = sizeof(CadScene::MatrixNode) * m_numMatrices;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_FALSE, 0,
                         NULL, 1, &memBarrier, 0, NULL);
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
  vkCmdCopyBuffer(cmd, m_scene.m_buffers.matricesOrig, m_scene.m_buffers.matrices, 1, &copy);
  vkEndCommandBuffer(cmd);

  submissionEnqueue(cmd);
}
}  // namespace generatedcmds
