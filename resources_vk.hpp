/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
 * SPDX-FileCopyrightText: Copyright (c) 2019-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#define DRAW_UBOS_NUM 3

// only use one big buffer for all geometries, otherwise individual
#define USE_SINGLE_GEOMETRY_BUFFERS 1

#include "cadscene_vk.hpp"
#include "resources.hpp"

#include <nvvk/buffers_vk.hpp>
#include <nvvk/commands_vk.hpp>
#include <nvvk/context_vk.hpp>
#include <nvvk/descriptorsets_vk.hpp>
#include <nvvk/error_vk.hpp>
#include <nvvk/memorymanagement_vk.hpp>
#include <nvvk/pipeline_vk.hpp>
#include <nvvk/profiler_vk.hpp>
#include <nvvk/renderpasses_vk.hpp>
#include <nvvk/shadermodulemanager_vk.hpp>
#include <nvvk/swapchain_vk.hpp>

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


  struct FrameBuffer
  {
    int  renderWidth  = 0;
    int  renderHeight = 0;
    int  supersample  = 0;
    bool useResolved  = false;
    bool vsync        = false;
    int  msaa         = 0;

    VkViewport viewport;
    VkViewport viewportUI;
    VkRect2D   scissor;
    VkRect2D   scissorUI;

    VkRenderPass passClear    = VK_NULL_HANDLE;
    VkRenderPass passPreserve = VK_NULL_HANDLE;
    VkRenderPass passUI       = VK_NULL_HANDLE;

    VkFramebuffer fboScene = VK_NULL_HANDLE;
    VkFramebuffer fboUI    = VK_NULL_HANDLE;

    VkImage imgColor         = VK_NULL_HANDLE;
    VkImage imgColorResolved = VK_NULL_HANDLE;
    VkImage imgDepthStencil  = VK_NULL_HANDLE;

    VkImageView viewColor         = VK_NULL_HANDLE;
    VkImageView viewColorResolved = VK_NULL_HANDLE;
    VkImageView viewDepthStencil  = VK_NULL_HANDLE;

    nvvk::DeviceMemoryAllocator memAllocator;
  };

  struct Common
  {
    nvvk::AllocationID     viewAID;
    VkBuffer               viewBuffer;
    VkDescriptorBufferInfo viewInfo;

    nvvk::AllocationID     animAID;
    VkBuffer               animBuffer;
    VkDescriptorBufferInfo animInfo;
  };

  struct
  {
    nvvk::ShaderModuleID shaderModuleID;
    VkShaderModule       shader;
    VkPipeline           pipeline;
  } m_animShading;

  struct
  {
    nvvk::ShaderModuleID vertexIDs[NUM_MATERIAL_SHADERS];
    nvvk::ShaderModuleID fragmentIDs[NUM_MATERIAL_SHADERS];
    VkShaderModule       vertexShaders[NUM_MATERIAL_SHADERS];
    VkShaderModule       fragmentShaders[NUM_MATERIAL_SHADERS];
    VkPipeline           pipelines[NUM_MATERIAL_SHADERS];
  } m_drawShading[NUM_BINDINGMODES];


  bool                      m_withinFrame = false;
  nvvk::ShaderModuleManager m_shaderManager;

  FrameBuffer m_framebuffer;
  Common      m_common;

  nvvk::SwapChain* m_swapChain;
  nvvk::Context*   m_context;
  nvvk::ProfilerVK m_profilerVK;

  VkDevice                    m_device = VK_NULL_HANDLE;
  VkPhysicalDevice            m_physical;
  VkQueue                     m_queue;
  uint32_t                    m_queueFamily;
  nvvk::DeviceMemoryAllocator m_memAllocator;
  nvvk::RingFences            m_ringFences;
  nvvk::RingCommandPool       m_ringCmdPool;
  nvvk::BatchSubmission       m_submission;
  bool                        m_submissionWaitForRead;

  VkPipelineCreateFlags                        m_gfxStatePipelineFlags = 0;  // hack for derived overrides
  nvvk::GraphicsPipelineState                  m_gfxState;
  nvvk::GraphicsPipelineGenerator              m_gfxGen{m_gfxState};
  nvvk::TDescriptorSetContainer<DRAW_UBOS_NUM> m_drawBind;
  nvvk::DescriptorSetContainer                 m_drawPush;
  nvvk::DescriptorSetContainer                 m_anim;

  uint32_t   m_numMatrices;
  CadSceneVK m_scene;

  size_t m_pipeChangeID;
  size_t m_fboChangeID;


  bool init(nvvk::Context* context, nvvk::SwapChain* swapChain, nvh::Profiler* profiler) override;
  void deinit() override;

  virtual void initPipes();
  void         deinitPipes();
  bool         hasPipes() { return m_animShading.pipeline != 0; }

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

  VkRenderPass createPass(bool clear, int msaa);
  VkRenderPass createPassUI(int msaa);

  VkCommandBuffer createCmdBuffer(VkCommandPool pool, bool singleshot, bool primary, bool secondaryInClear) const;
  VkCommandBuffer createTempCmdBuffer(bool primary = true, bool secondaryInClear = false);


  // submit for batched execution
  void submissionEnqueue(VkCommandBuffer cmdbuffer) { m_submission.enqueue(cmdbuffer); }
  void submissionEnqueue(uint32_t num, const VkCommandBuffer* cmdbuffers) { m_submission.enqueue(num, cmdbuffers); }
  // perform queue submit
  void submissionExecute(VkFence fence = NULL, bool useImageReadWait = false, bool useImageWriteSignals = false);

  // synchronizes to queue
  void resetTempResources();

  void cmdBeginRenderPass(VkCommandBuffer cmd, bool clear, bool hasSecondary = false) const;
  void cmdPipelineBarrier(VkCommandBuffer cmd) const;
  void cmdDynamicState(VkCommandBuffer cmd) const;
  void cmdImageTransition(VkCommandBuffer    cmd,
                          VkImage            img,
                          VkImageAspectFlags aspects,
                          VkAccessFlags      src,
                          VkAccessFlags      dst,
                          VkImageLayout      oldLayout,
                          VkImageLayout      newLayout) const;
  void cmdBegin(VkCommandBuffer cmd, bool singleshot, bool primary, bool secondaryInClear) const;
};

}  // namespace generatedcmds
