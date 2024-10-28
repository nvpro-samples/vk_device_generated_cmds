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


#pragma once

#define DRAW_UBOS_NUM 3

#include "cadscene_vk.hpp"
#include "resources.hpp"

#include <nvvk/buffers_vk.hpp>
#include <nvvk/commands_vk.hpp>
#include <nvvk/context_vk.hpp>
#include <nvvk/descriptorsets_vk.hpp>
#include <nvvk/error_vk.hpp>
#include <nvvk/resourceallocator_vk.hpp>
#include <nvvk/pipeline_vk.hpp>
#include <nvvk/profiler_vk.hpp>
#include <nvvk/renderpasses_vk.hpp>
#include <nvvk/shadermodulemanager_vk.hpp>
#include <nvvk/swapchain_vk.hpp>
#include <nvvk/memorymanagement_vk.hpp>
#include <nvvk/resourceallocator_vk.hpp>

namespace generatedcmds {

class ResourcesVK : public Resources
{
public:
  ResourcesVK() {}

  static ResourcesVK* get()
  {
    static ResourcesVK res;

    return &res;
  }
  static bool isAvailable();

  static void initImGui(const nvvk::Context& context);
  static void deinitImGui(const nvvk::Context& context);

  struct FrameBuffer
  {
    int  renderWidth  = 0;
    int  renderHeight = 0;
    int  supersample  = 0;
    bool useResolved  = false;
    bool vsync        = false;
    int  msaa         = 0;

    VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depthStencilFormat;

    VkViewport viewport;
    VkViewport viewportUI;
    VkRect2D   scissor;
    VkRect2D   scissorUI;

    nvvk::Image imgColor         = {};
    nvvk::Image imgColorResolved = {};
    nvvk::Image imgDepthStencil  = {};

    VkImageView viewColor         = VK_NULL_HANDLE;
    VkImageView viewColorResolved = VK_NULL_HANDLE;
    VkImageView viewDepthStencil  = VK_NULL_HANDLE;

    VkRenderingAttachmentInfo attachColor;
    VkRenderingAttachmentInfo attachColorUI;
    VkRenderingAttachmentInfo attachDepth;

    VkRenderingInfo               renderingInfo           = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    VkRenderingInfo               renderingInfoUI         = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    VkPipelineRenderingCreateInfo pipelineRenderingInfo   = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    VkPipelineRenderingCreateInfo pipelineRenderingInfoUI = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  };

  struct Common
  {
    nvvk::Buffer           viewBuffer;
    VkDescriptorBufferInfo viewInfo;

    nvvk::Buffer           animBuffer;
    VkDescriptorBufferInfo animInfo;
  };

  struct
  {
    nvvk::ShaderModuleID shaderModuleID = {};
    VkShaderModule       shader         = nullptr;
    VkPipeline           pipeline       = nullptr;
  } m_animShading;

  struct
  {
    VkPipeline  pipelines[NUM_MATERIAL_SHADERS]          = {};
    VkShaderEXT vertexShaderObjs[NUM_MATERIAL_SHADERS]   = {};
    VkShaderEXT fragmentShaderObjs[NUM_MATERIAL_SHADERS] = {};
  } m_drawShading;

  struct
  {
    nvvk::ShaderModuleID vertexIDs[NUM_MATERIAL_SHADERS]       = {};
    nvvk::ShaderModuleID fragmentIDs[NUM_MATERIAL_SHADERS]     = {};
    VkShaderModule       vertexShaders[NUM_MATERIAL_SHADERS]   = {};
    VkShaderModule       fragmentShaders[NUM_MATERIAL_SHADERS] = {};
  } m_drawShaderModules[NUM_BINDINGMODES];


  bool                      m_withinFrame = false;
  nvvk::ShaderModuleManager m_shaderManager;


  FrameBuffer m_framebuffer = {};
  Common      m_common;

  nvvk::SwapChain* m_swapChain = nullptr;
  nvvk::Context*   m_context   = nullptr;
  nvvk::ProfilerVK m_profilerVK;

  VkDevice                    m_device = VK_NULL_HANDLE;
  VkPhysicalDevice            m_physical;
  VkQueue                     m_queue;
  uint32_t                    m_queueFamily;
  nvvk::DeviceMemoryAllocator m_memoryAllocator;
  nvvk::ResourceAllocator     m_resourceAllocator;
  nvvk::RingFences            m_ringFences;
  nvvk::RingCommandPool       m_ringCmdPool;
  nvvk::BatchSubmission       m_submission;
  bool                        m_submissionWaitForRead;

  VkPipelineCreateFlags2CreateInfoKHR m_gfxStateFlags2CreateInfo;
  nvvk::GraphicsPipelineState         m_gfxState;
  nvvk::GraphicsPipelineGenerator     m_gfxGen{m_gfxState};
  nvvk::GraphicShaderObjectPipeline   m_gfxStateShaderObjects;

  nvvk::TDescriptorSetContainer<DRAW_UBOS_NUM> m_drawBind;
  nvvk::DescriptorSetContainer                 m_drawPush;
  nvvk::DescriptorSetContainer                 m_drawIndexed;
  nvvk::DescriptorSetContainer                 m_anim;
  VkPushConstantRange                          m_pushRanges[2];

  BindingMode               m_lastBindingMode   = NUM_BINDINGMODES;
  VkPipelineCreateFlags2KHR m_lastPipeFlags     = ~0;
  bool                      m_lastUseShaderObjs = false;

  uint32_t   m_numMatrices;
  CadSceneVK m_scene;

  size_t m_pipeChangeID;
  size_t m_fboChangeID;


  bool init(nvvk::Context* context, nvvk::SwapChain* swapChain, nvh::Profiler* profiler) override;
  void deinit() override;

  void initPipelinesOrShaders(BindingMode bindingMode, VkPipelineCreateFlags2KHR pipeFlags, bool useShaderObjs, bool force = false);
  void deinitPipelinesOrShaders();
  bool hasPipes() { return m_animShading.pipeline != 0; }

  bool initPrograms(const std::string& path, const std::string& prepend) override;
  void reloadPrograms(const std::string& prepend) override;

  void updatedPrograms();
  void deinitPrograms();

  bool initFramebuffer(int width, int height, int msaa, bool vsync) override;
  void deinitFramebuffer();

  bool initScene(const CadScene&) override;
  void deinitScene() override;

  void synchronize() override;

  void beginFrame() override;
  void blitFrame(const Global& global) override;
  void endFrame() override;

  void animation(const Global& global) override;
  void animationReset() override;

  //////////////////////////////////////////////////////////////////////////

  VkCommandBuffer createCmdBuffer(VkCommandPool pool, bool singleshot, bool primary, bool secondaryInClear) const;
  VkCommandBuffer createTempCmdBuffer(bool primary = true, bool secondaryInClear = false);


  // submit for batched execution
  void submissionEnqueue(VkCommandBuffer cmdbuffer) { m_submission.enqueue(cmdbuffer); }
  void submissionEnqueue(uint32_t num, const VkCommandBuffer* cmdbuffers) { m_submission.enqueue(num, cmdbuffers); }
  // perform queue submit
  void submissionExecute(VkFence fence = nullptr, bool useImageReadWait = false, bool useImageWriteSignals = false);

  // synchronizes to queue
  void resetTempResources();


  void cmdShaderObjectState(VkCommandBuffer cmd) const;
  void cmdDynamicPipelineState(VkCommandBuffer cmd) const;
  void cmdImageTransition(VkCommandBuffer    cmd,
                          VkImage            img,
                          VkImageAspectFlags aspects,
                          VkAccessFlags      src,
                          VkAccessFlags      dst,
                          VkImageLayout      oldLayout,
                          VkImageLayout      newLayout) const;

  void cmdBegin(VkCommandBuffer cmd, bool singleshot, bool primary, bool secondaryInClear) const;
  void cmdBeginRendering(VkCommandBuffer cmd, bool hasSecondary = false) const;

  void cmdPipelineBarrier(VkCommandBuffer cmd) const;
};

}  // namespace generatedcmds
