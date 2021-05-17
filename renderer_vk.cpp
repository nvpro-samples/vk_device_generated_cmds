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


/* Contact ckubisch@nvidia.com (Christoph Kubisch) for feedback */


#include <algorithm>
#include <assert.h>

#include "renderer.hpp"
#include "resources_vk.hpp"

#include <nvh/nvprint.hpp>
#include <nvmath/nvmath_glsltypes.h>

#include "common.h"


namespace generatedcmds {

//////////////////////////////////////////////////////////////////////////


class RendererVK : public Renderer
{
public:
  enum Mode
  {
    MODE_CMD_SINGLE,
  };


  class TypeCmd : public Renderer::Type
  {
    bool isAvailable(const nvvk::Context& context) const { return true; }

    const char* name() const { return "re-used cmds"; }
    Renderer*   create() const
    {
      RendererVK* renderer = new RendererVK();
      renderer->m_mode     = MODE_CMD_SINGLE;
      return renderer;
    }
    unsigned int priority() const { return 8; }

    Resources* resources() { return ResourcesVK::get(); }
  };

public:
  void     init(const CadScene* NV_RESTRICT scene, Resources* resources, const Config& config, Stats& stats) override;
  void     deinit() override;
  void     draw(const Resources::Global& global, Stats& stats) override;
  uint32_t supportedBindingModes() override { return (1 << BINDINGMODE_DSETS) | (1 << BINDINGMODE_PUSHADDRESS); };


  Mode m_mode;

  RendererVK()
      : m_mode(MODE_CMD_SINGLE)
  {
  }

private:
  struct DrawSetup
  {
    BindingMode     bindingMode;
    VkCommandBuffer cmdBuffer;
    size_t          fboChangeID;
    size_t          pipeChangeID;
  };

  std::vector<DrawItem> m_drawItems;
  std::vector<uint32_t> m_seqIndices;
  VkCommandPool         m_cmdPool;
  DrawSetup             m_draw;
  ResourcesVK* NV_RESTRICT m_resources;

  void fillCmdBuffer(VkCommandBuffer cmd, const DrawItem* NV_RESTRICT drawItems, size_t drawCount)
  {
    const ResourcesVK* res         = m_resources;
    const CadSceneVK&  scene       = res->m_scene;
    BindingMode        bindingMode = m_config.bindingMode;

    int lastMaterial = -1;
    int lastGeometry = -1;
    int lastMatrix   = -1;
    int lastObject   = -1;
    int lastShader   = -1;

#if USE_VULKAN_1_2_BUFFER_ADDRESS
    VkBufferDeviceAddressInfo addressInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
#define vkGetBufferDeviceAddressUSED    vkGetBufferDeviceAddress
#else
    VkBufferDeviceAddressInfoEXT addressInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_EXT};
#define vkGetBufferDeviceAddressUSED    vkGetBufferDeviceAddressEXT
#endif
    addressInfo.buffer                       = scene.m_buffers.matrices;
    VkDeviceAddress matrixAddress            = vkGetBufferDeviceAddressUSED(res->m_device, &addressInfo);
    addressInfo.buffer                       = scene.m_buffers.materials;
    VkDeviceAddress materialAddress          = vkGetBufferDeviceAddressUSED(res->m_device, &addressInfo);

    if(bindingMode == BINDINGMODE_DSETS)
    {
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawBind.getPipeLayout(), DRAW_UBO_SCENE, 1,
                              res->m_drawBind.at(DRAW_UBO_SCENE).getSets(), 0, NULL);
    }
    else if(bindingMode == BINDINGMODE_PUSHADDRESS)
    {
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawPush.getPipeLayout(), DRAW_UBO_SCENE, 1,
                              res->m_drawPush.getSets(), 0, NULL);
    }

    for(size_t i = 0; i < drawCount; i++)
    {
      uint32_t        idx = m_config.permutated ? m_seqIndices[i] : uint32_t(i);
      const DrawItem& di  = drawItems[idx];

      if(di.shaderIndex != lastShader)
      {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawShading[bindingMode].pipelines[di.shaderIndex]);
      }

      if(lastGeometry != di.geometryIndex)
      {
        const CadSceneVK::Geometry& geo = scene.m_geometry[di.geometryIndex];

        vkCmdBindVertexBuffers(cmd, 0, 1, &geo.vbo.buffer, &geo.vbo.offset);
        vkCmdBindIndexBuffer(cmd, geo.ibo.buffer, geo.ibo.offset, VK_INDEX_TYPE_UINT32);

        lastGeometry = di.geometryIndex;
      }

      if(bindingMode == BINDINGMODE_DSETS)
      {
        if(lastMatrix != di.matrixIndex)
        {
          uint32_t offset = di.matrixIndex * res->m_alignedMatrixSize;
          vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawBind.getPipeLayout(),
                                  DRAW_UBO_MATRIX, 1, res->m_drawBind.at(DRAW_UBO_MATRIX).getSets(), 1, &offset);
          lastMatrix = di.matrixIndex;
        }

        if(lastMaterial != di.materialIndex)
        {
          uint32_t offset = di.materialIndex * res->m_alignedMaterialSize;
          vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawBind.getPipeLayout(),
                                  DRAW_UBO_MATERIAL, 1, res->m_drawBind.at(DRAW_UBO_MATERIAL).getSets(), 1, &offset);
          lastMaterial = di.materialIndex;
        }
      }
      else if(bindingMode == BINDINGMODE_PUSHADDRESS)
      {
        if(lastMatrix != di.matrixIndex)
        {
          VkDeviceAddress address = matrixAddress + sizeof(CadScene::MatrixNode) * di.matrixIndex;

          vkCmdPushConstants(cmd, res->m_drawPush.getPipeLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &address);

          lastMatrix = di.matrixIndex;
        }

        if(lastMaterial != di.materialIndex)
        {
          VkDeviceAddress address = materialAddress + sizeof(CadScene::Material) * di.materialIndex;

          vkCmdPushConstants(cmd, res->m_drawPush.getPipeLayout(), VK_SHADER_STAGE_FRAGMENT_BIT,
                             sizeof(VkDeviceAddress), sizeof(VkDeviceAddress), &address);

          lastMaterial = di.materialIndex;
        }
      }

      // drawcall
      vkCmdDrawIndexed(cmd, di.range.count, 1, uint32_t(di.range.offset / sizeof(uint32_t)), 0, 0);

      lastShader = di.shaderIndex;
    }
  }

  void setupCmdBuffer(const DrawItem* NV_RESTRICT drawItems, size_t drawCount)
  {
    const ResourcesVK* res = m_resources;

    VkCommandBuffer cmd = res->createCmdBuffer(m_cmdPool, false, false, true);
    res->cmdDynamicState(cmd);

    fillCmdBuffer(cmd, drawItems, drawCount);

    vkEndCommandBuffer(cmd);
    m_draw.cmdBuffer = cmd;

    m_draw.fboChangeID  = res->m_fboChangeID;
    m_draw.pipeChangeID = res->m_pipeChangeID;
  }

  void deleteCmdBuffer() { vkFreeCommandBuffers(m_resources->m_device, m_cmdPool, 1, &m_draw.cmdBuffer); }
};


static RendererVK::TypeCmd s_type_cmdbuffer_vk;

void RendererVK::init(const CadScene* NV_RESTRICT scene, Resources* resources, const Config& config, Stats& stats)
{
  ResourcesVK* NV_RESTRICT res = (ResourcesVK*)resources;
  m_resources                  = res;
  m_scene                      = scene;
  m_config                     = config;

  stats.cmdBuffers = 1;

  VkResult                result;
  VkCommandPoolCreateInfo cmdPoolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  cmdPoolInfo.queueFamilyIndex        = 0;
  result                              = vkCreateCommandPool(res->m_device, &cmdPoolInfo, NULL, &m_cmdPool);
  assert(result == VK_SUCCESS);

  fillDrawItems(m_drawItems, scene, config, stats);
  if(config.permutated)
  {
    m_seqIndices.resize(m_drawItems.size());
    fillRandomPermutation(m_drawItems.size(), m_seqIndices.data(), m_drawItems.data(), stats);
  }

  setupCmdBuffer(m_drawItems.data(), m_drawItems.size());
}

void RendererVK::deinit()
{
  deleteCmdBuffer();
  vkDestroyCommandPool(m_resources->m_device, m_cmdPool, NULL);
}

void RendererVK::draw(const Resources::Global& global, Stats& stats)
{
  ResourcesVK* NV_RESTRICT res = m_resources;

  if(m_draw.pipeChangeID != res->m_pipeChangeID || m_draw.fboChangeID != res->m_fboChangeID)
  {
    deleteCmdBuffer();
    setupCmdBuffer(m_drawItems.data(), m_drawItems.size());
  }


  VkCommandBuffer primary = res->createTempCmdBuffer();
  {
    nvvk::ProfilerVK::Section profile(res->m_profilerVK, "Render", primary);
    {
      nvvk::ProfilerVK::Section profile(res->m_profilerVK, "Draw", primary);

      vkCmdUpdateBuffer(primary, res->m_common.viewBuffer, 0, sizeof(SceneData), (const uint32_t*)&global.sceneUbo);
      res->cmdPipelineBarrier(primary);

      // clear via pass
      res->cmdBeginRenderPass(primary, true, true);
      vkCmdExecuteCommands(primary, 1, &m_draw.cmdBuffer);
      vkCmdEndRenderPass(primary);
    }
  }
  vkEndCommandBuffer(primary);
  res->submissionEnqueue(primary);
}

}  // namespace generatedcmds
