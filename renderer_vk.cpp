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


/* Contact ckubisch@nvidia.com (Christoph Kubisch) for feedback */


#include <algorithm>
#include <assert.h>

#include "renderer.hpp"
#include "resources_vk.hpp"

#include <nvh/nvprint.hpp>

#include "common.h"


namespace generatedcmds {

//////////////////////////////////////////////////////////////////////////


class RendererVK : public Renderer
{
public:
  class TypeCmd : public Renderer::Type
  {
    bool isAvailable(const nvvk::Context& context) override { return true; }

    const char* name() const override { return "re-used cmds"; }
    Renderer*   create() const override
    {
      RendererVK* renderer = new RendererVK();
      return renderer;
    }
    uint32_t priority() const override { return 8; }
  };

public:
  void init(const CadScene* scene, ResourcesVK* resources, const Config& config, Stats& stats) override;
  void deinit() override;
  void draw(const Resources::Global& global, Stats& stats) override;

  RendererVK() {}

private:
  struct DrawSetup
  {
    VkCommandBuffer cmdBuffer;
    nvvk::Buffer    combinedIndices;
  };

  std::vector<DrawItem>  m_drawItems;
  std::vector<uint32_t>  m_seqIndices;
  CadScene::IndexingBits m_indexingBits;
  VkCommandPool          m_cmdPool;
  DrawSetup              m_draw;
  ResourcesVK*           m_resources;

  void fillCmdBuffer(VkCommandBuffer cmd, const DrawItem* drawItems, size_t drawCount)
  {
    ResourcesVK*      res         = m_resources;
    const CadSceneVK& scene       = res->m_scene;
    BindingMode       bindingMode = m_config.bindingMode;

    int lastMaterial = -1;
    int lastGeometry = -1;
    int lastMatrix   = -1;
    int lastObject   = -1;
    int lastShader   = -1;

    VkDeviceAddress matrixAddress   = scene.m_buffers.matrices.address;
    VkDeviceAddress materialAddress = scene.m_buffers.materials.address;

    // setup staging buffer for filling
    ScopeStaging staging(res->m_resourceAllocator, res->m_queue, res->m_queueFamily);

    size_t    combinedIndicesSize    = bindingMode == BINDINGMODE_INDEX_VERTEXATTRIB ? sizeof(uint32_t) * drawCount : 0;
    uint32_t* combinedIndicesMapping = nullptr;
    if(combinedIndicesSize)
    {
      m_draw.combinedIndices = res->m_resourceAllocator.createBuffer(combinedIndicesSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
      combinedIndicesMapping = staging.uploadT<uint32_t>(m_draw.combinedIndices.buffer, 0, combinedIndicesSize);
    }

    switch(bindingMode)
    {
      case BINDINGMODE_DSETS:
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawBind.getPipeLayout(), DRAW_UBO_SCENE,
                                1, res->m_drawBind.at(DRAW_UBO_SCENE).getSets(), 0, nullptr);
        break;
      case BINDINGMODE_PUSHADDRESS:
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawPush.getPipeLayout(), 0, 1,
                                res->m_drawPush.getSets(), 0, nullptr);
        break;
      case BINDINGMODE_INDEX_BASEINSTANCE:
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawIndexed.getPipeLayout(), 0, 1,
                                res->m_drawIndexed.getSets(), 0, nullptr);
        break;
      case BINDINGMODE_INDEX_VERTEXATTRIB:
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawIndexed.getPipeLayout(), 0, 1,
                                res->m_drawIndexed.getSets(), 0, nullptr);

        {
          VkDeviceSize offset = {0};
          VkDeviceSize size   = {VK_WHOLE_SIZE};
          VkDeviceSize stride = {sizeof(uint32_t)};
#if USE_DYNAMIC_VERTEX_STRIDE
          vkCmdBindVertexBuffers2(cmd, 1, 1, &m_draw.combinedIndices.buffer, &offset, &size, &stride);
#else
          vkCmdBindVertexBuffers(cmd, 1, 1, &m_draw.combinedIndices.buffer, &offset);
#endif
        }
        break;
    }

    if(m_config.shaderObjs)
    {
      const VkShaderStageFlagBits unusedStages[3] = {VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                                                     VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, VK_SHADER_STAGE_GEOMETRY_BIT};
      vkCmdBindShadersEXT(cmd, 3, unusedStages, nullptr);
    }

    for(size_t i = 0; i < drawCount; i++)
    {
      uint32_t        idx = m_config.permutated ? m_seqIndices[i] : uint32_t(i);
      const DrawItem& di  = drawItems[idx];

      if(di.shaderIndex != lastShader)
      {
        if(m_config.shaderObjs)
        {
          VkShaderStageFlagBits stages[2]  = {VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT};
          VkShaderEXT           shaders[2] = {res->m_drawShading.vertexShaderObjs[di.shaderIndex],
                                              res->m_drawShading.fragmentShaderObjs[di.shaderIndex]};
          vkCmdBindShadersEXT(cmd, 2, stages, shaders);
        }
        else
        {
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawShading.pipelines[di.shaderIndex]);
        }

        lastShader = di.shaderIndex;
      }

#if USE_DRAW_OFFSETS
      if(lastGeometry != int(scene.m_geometry[di.geometryIndex].allocation.chunkIndex))
      {
        const CadSceneVK::Geometry& geo = scene.m_geometry[di.geometryIndex];

        vkCmdBindIndexBuffer(cmd, geo.ibo.buffer, 0, VK_INDEX_TYPE_UINT32);
        VkDeviceSize offset = {0};
        VkDeviceSize size   = {VK_WHOLE_SIZE};
        VkDeviceSize stride = {sizeof(CadScene::Vertex)};
#if USE_DYNAMIC_VERTEX_STRIDE
        vkCmdBindVertexBuffers2(cmd, 0, 1, &geo.vbo.buffer, &offset, &size, &stride);
#else
        vkCmdBindVertexBuffers(cmd, 0, 1, &geo.vbo.buffer, &offset);
#endif
        lastGeometry = int(scene.m_geometry[di.geometryIndex].allocation.chunkIndex);
      }
#else
      if(lastGeometry != di.geometryIndex)
      {
        const CadSceneVK::Geometry& geo    = scene.m_geometry[di.geometryIndex];
        VkDeviceSize                stride = {sizeof(CadScene::Vertex)};

        vkCmdBindIndexBuffer(cmd, geo.ibo.buffer, geo.ibo.offset, VK_INDEX_TYPE_UINT32);
#if USE_DYNAMIC_VERTEX_STRIDE
        vkCmdBindVertexBuffers2(cmd, 0, 1, &geo.vbo.buffer, &geo.vbo.offset, &geo.vbo.range, &stride);
#else
        vkCmdBindVertexBuffers(cmd, 0, 1, &geo.vbo.buffer, &geo.vbo.offset);
#endif

        lastGeometry = di.geometryIndex;
      }
#endif

      uint32_t firstInstance = 0;

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
      else if(bindingMode == BINDINGMODE_INDEX_BASEINSTANCE)
      {
        firstInstance = m_indexingBits.packIndices(di.matrixIndex, di.materialIndex);
      }
      else if(bindingMode == BINDINGMODE_INDEX_VERTEXATTRIB)
      {
        firstInstance             = i;
        combinedIndicesMapping[i] = m_indexingBits.packIndices(di.matrixIndex, di.materialIndex);
      }

      // drawcall
#if USE_DRAW_OFFSETS
      const CadSceneVK::Geometry& geo = scene.m_geometry[di.geometryIndex];
      vkCmdDrawIndexed(cmd, di.range.count, 1, uint32_t(di.range.offset + geo.ibo.offset / sizeof(uint32_t)),
                       geo.vbo.offset / sizeof(CadScene::Vertex), firstInstance);
#else
      vkCmdDrawIndexed(cmd, di.range.count, 1, uint32_t(di.range.offset / sizeof(uint32_t)), 0, firstInstance);
#endif

      lastShader = di.shaderIndex;
    }
  }

  void setupCmdBuffer(const DrawItem* drawItems, size_t drawCount)
  {
    const ResourcesVK* res = m_resources;

    VkCommandBuffer cmd = res->createCmdBuffer(m_cmdPool, false, false, true);

    if(m_config.shaderObjs)
    {
      res->cmdShaderObjectState(cmd);
    }
    else
    {
      res->cmdDynamicPipelineState(cmd);
    }

    fillCmdBuffer(cmd, drawItems, drawCount);

    vkEndCommandBuffer(cmd);
    m_draw.cmdBuffer = cmd;
  }

  void deleteCmdBuffer() { vkFreeCommandBuffers(m_resources->m_device, m_cmdPool, 1, &m_draw.cmdBuffer); }
};


static RendererVK::TypeCmd s_type_cmdbuffer_vk;

void RendererVK::init(const CadScene* scene, ResourcesVK* resources, const Config& config, Stats& stats)
{
  ResourcesVK* res = (ResourcesVK*)resources;
  m_resources      = res;
  m_scene          = scene;
  m_config         = config;

  stats.cmdBuffers = 1;

  m_indexingBits = m_scene->getIndexingBits();

  res->initPipelinesOrShaders(config.bindingMode, 0, config.shaderObjs);

  VkResult                result;
  VkCommandPoolCreateInfo cmdPoolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  cmdPoolInfo.queueFamilyIndex        = 0;
  result                              = vkCreateCommandPool(res->m_device, &cmdPoolInfo, nullptr, &m_cmdPool);
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
  m_resources->m_resourceAllocator.destroy(m_draw.combinedIndices);

  deleteCmdBuffer();
  vkDestroyCommandPool(m_resources->m_device, m_cmdPool, nullptr);
}

void RendererVK::draw(const Resources::Global& global, Stats& stats)
{
  ResourcesVK* res = m_resources;

  VkCommandBuffer primary = res->createTempCmdBuffer();
  {
    nvvk::ProfilerVK::Section profile(res->m_profilerVK, "Render", primary);
    {
      nvvk::ProfilerVK::Section profile(res->m_profilerVK, "Draw", primary);

      vkCmdUpdateBuffer(primary, res->m_common.viewBuffer.buffer, 0, sizeof(SceneData), (const uint32_t*)&global.sceneUbo);
      res->cmdPipelineBarrier(primary);

      // clear via pass
      res->cmdBeginRendering(primary, true);
      vkCmdExecuteCommands(primary, 1, &m_draw.cmdBuffer);
      vkCmdEndRendering(primary);
    }
  }
  vkEndCommandBuffer(primary);
  res->submissionEnqueue(primary);
}

}  // namespace generatedcmds
