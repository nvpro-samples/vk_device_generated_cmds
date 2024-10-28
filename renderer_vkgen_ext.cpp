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

//#undef NDEBUG

#include <algorithm>
#include <assert.h>
#include <array>

#include "renderer.hpp"
#include "resources_vk.hpp"
#include "vk_ext_device_generated_commands.hpp"

#include <nvh/nvprint.hpp>

#include "common.h"

namespace generatedcmds {

//////////////////////////////////////////////////////////////////////////

class RendererVKGenEXT : public Renderer
{
public:
  enum Mode
  {
    MODE_DIRECT,      // direct execute & generate
    MODE_PREPROCESS,  // separate pre-process step
  };

  Mode m_mode;

  class TypeGen : public Renderer::Type
  {
    VkPhysicalDeviceDeviceGeneratedCommandsPropertiesEXT props = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_EXT};

    bool isAvailable(const nvvk::Context& context) override
    {
      VkPhysicalDeviceProperties2 props2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
      props2.pNext                       = &props;
      vkGetPhysicalDeviceProperties2(context.m_physicalDevice, &props2);

      return context.hasDeviceExtension(VK_EXT_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME);
    }
    const char* name() const override { return "generated cmds ext"; }
    Renderer*   create() const override
    {
      RendererVKGenEXT* renderer = new RendererVKGenEXT();
      renderer->m_mode           = MODE_DIRECT;

      return renderer;
    }
    uint32_t priority() const override { return 20; }
    uint32_t supportedBindingModes() const override
    {
      return (1 << BINDINGMODE_PUSHADDRESS) | (1 << BINDINGMODE_INDEX_BASEINSTANCE) | (1 << BINDINGMODE_INDEX_VERTEXATTRIB);
    }
    bool supportsShaderObjs() const override
    {
      VkShaderStageFlags flagsRequired = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
      return ((props.supportedIndirectCommandsShaderStagesShaderBinding & flagsRequired) == flagsRequired);
    }
    uint32_t supportedShaderBinds() const override
    {
      VkShaderStageFlags flagsRequired = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

      bool supportsPipelines = ((props.supportedIndirectCommandsShaderStagesPipelineBinding & flagsRequired) == flagsRequired);

      if(!supportsPipelines)
      {
        return 0;
      }
      else if(supportsShaderObjs() && supportsPipelines)
      {
        return std::min(props.maxIndirectPipelineCount, props.maxIndirectShaderObjectCount / 2);
      }
      else
      {
        return props.maxIndirectPipelineCount;
      }
    }
  };

  class TypePreprocess : public Renderer::Type
  {
    VkPhysicalDeviceDeviceGeneratedCommandsPropertiesEXT props = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_EXT};

    bool isAvailable(const nvvk::Context& context)
    {
      VkPhysicalDeviceProperties2 props2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
      props2.pNext                       = &props;
      vkGetPhysicalDeviceProperties2(context.m_physicalDevice, &props2);

      return context.hasDeviceExtension(VK_EXT_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME);
    }
    const char* name() const override { return "preprocess,generated cmds ext"; }
    Renderer*   create() const override
    {
      RendererVKGenEXT* renderer = new RendererVKGenEXT();
      renderer->m_mode           = MODE_PREPROCESS;

      return renderer;
    }
    uint32_t priority() const override { return 20; }
    uint32_t supportedBindingModes() const override
    {
      return (1 << BINDINGMODE_PUSHADDRESS) | (1 << BINDINGMODE_INDEX_BASEINSTANCE) | (1 << BINDINGMODE_INDEX_VERTEXATTRIB);
    };
    bool supportsShaderObjs() const override
    {
      VkShaderStageFlags flagsRequired = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
      return ((props.supportedIndirectCommandsShaderStagesShaderBinding & flagsRequired) == flagsRequired);
    }
    uint32_t supportedShaderBinds() const override
    {
      VkShaderStageFlags flagsRequired = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

      bool supportsPipelines = ((props.supportedIndirectCommandsShaderStagesPipelineBinding & flagsRequired) == flagsRequired);

      if(!supportsPipelines)
      {
        return 0;
      }
      else if(supportsShaderObjs() && supportsPipelines)
      {
        return std::min(props.maxIndirectPipelineCount, props.maxIndirectShaderObjectCount / 2);
      }
      else
      {
        return props.maxIndirectPipelineCount;
      }
    }
  };

public:
  void init(const CadScene* scene, ResourcesVK* resources, const Renderer::Config& config, Stats& stats) override;
  void deinit() override;
  void draw(const Resources::Global& global, Stats& stats) override;


  RendererVKGenEXT() {}

private:
  struct DrawSequence
  {
    union
    {
      uint32_t pipeline;
      struct
      {
        uint32_t shaderVertex;
        uint32_t shaderFragment;
      };
    };

    VkDeviceAddress                      pushMatrix;
    VkDeviceAddress                      pushMaterial;
    VkBindIndexBufferIndirectCommandEXT  ibo;
    VkBindVertexBufferIndirectCommandEXT vbo;
    VkDrawIndexedIndirectCommand         drawIndexed;
  };

  struct DrawSequenceBinned
  {
    union
    {
      uint32_t pipeline;
      struct
      {
        uint32_t shaderVertex;
        uint32_t shaderFragment;
      };
    };

    VkDeviceAddress                       pushMatrix;
    VkDeviceAddress                       pushMaterial;
    VkBindIndexBufferIndirectCommandEXT   ibo;
    VkBindVertexBufferIndirectCommandEXT  vbo;
    VkDrawIndirectCountIndirectCommandEXT drawIndirectCount;
  };

  struct DrawSetup
  {
    VkIndirectCommandsLayoutEXT indirectCmdsLayout;

    nvvk::Buffer combinedIndices;

    nvvk::Buffer inputBuffer = {};
    VkDeviceSize inputSize   = 0;

    nvvk::Buffer preprocessBuffer = {};
    VkDeviceSize preprocessSize   = 0;

    // only used for binning
    nvvk::Buffer    drawIndirectBuffer = {};
    VkDeviceAddress drawIndirectSize   = 0;

    uint32_t sequencesCount    = 0;
    uint32_t drawIndirectCount = 0;

    VkCommandBuffer cmdStateBuffer = nullptr;
  };

  ResourcesVK*           m_resources;
  VkCommandPool          m_cmdPool;
  CadScene::IndexingBits m_indexingBits;

  DrawSetup                 m_draw;
  VkIndirectExecutionSetEXT m_indirectExecutionSet = nullptr;

  VkGeneratedCommandsInfoEXT getGeneratedCommandsInfo();

  void cmdStates(VkCommandBuffer cmd);
  void cmdPreprocess(VkCommandBuffer cmd);
  void cmdExecute(VkCommandBuffer cmd, VkBool32 isPreprocessed);

  void initIndirectExecutionSet();

  void initIndirectCommandsLayout(const Renderer::Config& config);
  void deinitIndirectCommandsLayout()
  {
    vkDestroyIndirectCommandsLayoutEXT(m_resources->m_device, m_draw.indirectCmdsLayout, nullptr);
  }

  void setupInputInterleaved(const DrawItem* drawItems, size_t drawCount, Stats& stats)
  {
    ResourcesVK*      res   = m_resources;
    const CadSceneVK& scene = res->m_scene;

    ScopeStaging staging(res->m_resourceAllocator, res->m_queue, res->m_queueFamily);

    m_draw.sequencesCount = drawCount;

    // compute input buffer space requirements
    VkPhysicalDeviceProperties2 phyProps = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    VkPhysicalDeviceDeviceGeneratedCommandsPropertiesEXT genProps = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_EXT};
    phyProps.pNext = &genProps;
    vkGetPhysicalDeviceProperties2(res->m_physical, &phyProps);

    // create input buffer
    m_draw.inputSize = sizeof(DrawSequence) * drawCount;
    m_draw.inputSize += 32;  // if drawCount == 0
    m_draw.inputBuffer    = res->m_resourceAllocator.createBuffer(m_draw.inputSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
                                                                                        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    uint8_t* inputMapping = staging.uploadT<uint8_t>(m_draw.inputBuffer.buffer, 0, m_draw.inputSize);

    // create combined indices buffer
    size_t combinedIndicesSize = m_config.bindingMode == BINDINGMODE_INDEX_VERTEXATTRIB ? sizeof(uint32_t) * drawCount : 0;
    uint32_t* combinedIndicesMapping = nullptr;
    if(combinedIndicesSize)
    {
      m_draw.combinedIndices = res->m_resourceAllocator.createBuffer(combinedIndicesSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
      combinedIndicesMapping = staging.uploadT<uint32_t>(m_draw.combinedIndices.buffer, 0, combinedIndicesSize);
    }

    // prepare filling

    VkDeviceAddress matrixAddress   = scene.m_buffers.matrices.address;
    VkDeviceAddress materialAddress = scene.m_buffers.materials.address;

    std::vector<uint32_t> seqIndices;
    if(m_config.permutated)
    {
      seqIndices.resize(drawCount);
      fillRandomPermutation(seqIndices.size(), seqIndices.data(), drawItems, stats);
    }

    // fill sequence
    DrawSequence* sequences = (DrawSequence*)inputMapping;
    for(size_t i = 0; i < drawCount; i++)
    {
      const uint32_t seqIndex = seqIndices.size() ? seqIndices[i] : i;

      const DrawItem&             di  = drawItems[seqIndex];
      const CadSceneVK::Geometry& geo = scene.m_geometry[di.geometryIndex];

      DrawSequence& seq = sequences[i];

      if(m_config.shaderObjs)
      {
        seq.shaderVertex   = di.shaderIndex * 2 + 0;
        seq.shaderFragment = di.shaderIndex * 2 + 1;
      }
      else
      {
        seq.pipeline = di.shaderIndex;
      }

      assert(di.shaderIndex < m_config.maxShaders);

      seq.ibo.bufferAddress = nvvk::getBufferDeviceAddress(res->m_device, geo.ibo.buffer);
      seq.ibo.indexType     = VK_INDEX_TYPE_UINT32;

      seq.vbo.bufferAddress = nvvk::getBufferDeviceAddress(res->m_device, geo.vbo.buffer);
      seq.vbo.stride        = sizeof(CadScene::Vertex);

#if USE_DRAW_OFFSETS
      seq.ibo.size = scene.m_geometryMem.getChunk(geo.allocation).iboSize;
      seq.vbo.size = scene.m_geometryMem.getChunk(geo.allocation).vboSize;
#else
      seq.ibo.bufferAddress += geo.ibo.offset;
      seq.vbo.bufferAddress += geo.vbo.offset;

      seq.ibo.size = geo.ibo.range;
      seq.vbo.size = geo.vbo.range;
#endif

      seq.pushMatrix   = matrixAddress + sizeof(CadScene::MatrixNode) * di.matrixIndex;
      seq.pushMaterial = materialAddress + sizeof(CadScene::Material) * di.materialIndex;

      seq.drawIndexed.indexCount    = di.range.count;
      seq.drawIndexed.instanceCount = 1;
      seq.drawIndexed.firstInstance = 0;
      seq.drawIndexed.firstIndex    = uint32_t(di.range.offset / sizeof(uint32_t));
      seq.drawIndexed.vertexOffset  = 0;
#if USE_DRAW_OFFSETS
      seq.drawIndexed.firstIndex += geo.ibo.offset / sizeof(uint32_t);
#endif
#if USE_DRAW_OFFSETS
      seq.drawIndexed.vertexOffset += geo.vbo.offset / sizeof(CadScene::Vertex);
#endif
      if(m_config.bindingMode == BINDINGMODE_INDEX_BASEINSTANCE)
      {
        seq.drawIndexed.firstInstance = m_indexingBits.packIndices(di.matrixIndex, di.materialIndex);
      }
      else if(m_config.bindingMode == BINDINGMODE_INDEX_VERTEXATTRIB)
      {
        seq.drawIndexed.firstInstance = i;
        combinedIndicesMapping[i]     = m_indexingBits.packIndices(di.matrixIndex, di.materialIndex);
      }
    }
  }

  void setupInputBinned(const DrawItem* drawItems, size_t drawCount, Stats& stats)
  {
    ResourcesVK*      res   = m_resources;
    const CadSceneVK& scene = res->m_scene;

    // setup staging buffer for filling
    ScopeStaging staging(res->m_resourceAllocator, res->m_queue, res->m_queueFamily);

    // compute input buffer space requirements
    VkPhysicalDeviceProperties2 phyProps = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    VkPhysicalDeviceDeviceGeneratedCommandsPropertiesEXT genProps = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_EXT};
    phyProps.pNext = &genProps;
    vkGetPhysicalDeviceProperties2(res->m_physical, &phyProps);


    // create draw indirect buffer
    m_draw.drawIndirectSize = sizeof(VkDrawIndexedIndirectCommand) * drawCount;
    m_draw.drawIndirectSize += 32;  // if drawCount == 0

    m_draw.drawIndirectBuffer = res->m_resourceAllocator.createBuffer(
        m_draw.drawIndirectSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    uint8_t* indirectMapping = staging.uploadT<uint8_t>(m_draw.drawIndirectBuffer.buffer, 0, m_draw.drawIndirectSize);

    stats.indirectSizeKB                = (uint32_t(m_draw.drawIndirectSize) + 1023) / 1024;
    VkDeviceAddress drawIndirectAddress = m_draw.drawIndirectBuffer.address;

    // create combined indices buffer
    size_t combinedIndicesSize = m_config.bindingMode == BINDINGMODE_INDEX_VERTEXATTRIB ? sizeof(uint32_t) * drawCount : 0;
    uint32_t* combinedIndicesMapping = nullptr;
    if(combinedIndicesSize)
    {
      m_draw.combinedIndices = res->m_resourceAllocator.createBuffer(combinedIndicesSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
      combinedIndicesMapping = staging.uploadT<uint32_t>(m_draw.combinedIndices.buffer, 0, combinedIndicesSize);
    }

    // preparing filling

    VkDeviceAddress matrixAddress   = scene.m_buffers.matrices.address;
    VkDeviceAddress materialAddress = scene.m_buffers.materials.address;

    std::vector<uint32_t> seqIndices;
    if(m_config.permutated)
    {
      seqIndices.resize(drawCount);
      fillRandomPermutation(seqIndices.size(), seqIndices.data(), drawItems, stats);
    }

    DrawSequenceBinned lastSeq      = {0};
    size_t             seqDrawStart = 0;
    uint32_t           seqDrawCount = 0;

    // fill sequence and drawindirects
    std::vector<DrawSequenceBinned> seqBinned;
    seqBinned.reserve(drawCount);

    VkDrawIndexedIndirectCommand* drawIndirects = (VkDrawIndexedIndirectCommand*)indirectMapping;
    for(size_t i = 0; i < drawCount; i++)
    {
      const uint32_t seqIndex = seqIndices.size() ? seqIndices[i] : i;

      const DrawItem&             di  = drawItems[seqIndex];
      const CadSceneVK::Geometry& geo = scene.m_geometry[di.geometryIndex];

      DrawSequenceBinned seq = {0};

      if(m_config.maxShaders > 1)
      {
        if(m_config.shaderObjs)
        {
          seq.shaderVertex   = di.shaderIndex * 2 + 0;
          seq.shaderFragment = di.shaderIndex * 2 + 1;
        }
        else
        {
          seq.pipeline = di.shaderIndex;
        }
      }

      seq.ibo.bufferAddress = nvvk::getBufferDeviceAddress(res->m_device, geo.ibo.buffer);
      seq.ibo.size          = scene.m_geometryMem.getChunk(geo.allocation).iboSize;
      seq.ibo.indexType     = VK_INDEX_TYPE_UINT32;

      seq.vbo.bufferAddress = nvvk::getBufferDeviceAddress(res->m_device, geo.vbo.buffer);
      seq.vbo.size          = scene.m_geometryMem.getChunk(geo.allocation).vboSize;
      seq.vbo.stride        = sizeof(CadScene::Vertex);

      if(m_config.bindingMode == BINDINGMODE_PUSHADDRESS)
      {
        seq.pushMatrix   = matrixAddress + sizeof(CadScene::MatrixNode) * di.matrixIndex;
        seq.pushMaterial = materialAddress + sizeof(CadScene::Material) * di.materialIndex;
      }

      if(seqDrawCount && (memcmp(&lastSeq, &seq, sizeof(seq)) != 0))
      {
        lastSeq.drawIndirectCount.bufferAddress = drawIndirectAddress + sizeof(VkDrawIndexedIndirectCommand) * seqDrawStart;
        lastSeq.drawIndirectCount.commandCount = seqDrawCount;
        lastSeq.drawIndirectCount.stride       = uint32_t(sizeof(VkDrawIndexedIndirectCommand));
        seqBinned.push_back(lastSeq);

        m_draw.drawIndirectCount = std::max(m_draw.drawIndirectCount, seqDrawCount);

        seqDrawCount = 0;
        seqDrawStart = i;
      }

      lastSeq = seq;

      VkDrawIndexedIndirectCommand& drawIndexed = drawIndirects[i];
      drawIndexed.indexCount                    = di.range.count;
      drawIndexed.instanceCount                 = 1;
      drawIndexed.firstInstance                 = 0;
      drawIndexed.firstIndex                    = uint32_t(di.range.offset / sizeof(uint32_t));
      drawIndexed.vertexOffset                  = 0;
      drawIndexed.firstIndex += geo.ibo.offset / sizeof(uint32_t);
      drawIndexed.vertexOffset += geo.vbo.offset / sizeof(CadScene::Vertex);

      if(m_config.bindingMode == BINDINGMODE_INDEX_BASEINSTANCE)
      {
        drawIndexed.firstInstance = m_indexingBits.packIndices(di.matrixIndex, di.materialIndex);
      }
      else if(m_config.bindingMode == BINDINGMODE_INDEX_VERTEXATTRIB)
      {
        drawIndexed.firstInstance = i;
        combinedIndicesMapping[i] = m_indexingBits.packIndices(di.matrixIndex, di.materialIndex);
      }

      seqDrawCount++;
    }

    if(seqDrawCount)
    {
      lastSeq.drawIndirectCount.bufferAddress = drawIndirectAddress + sizeof(VkDrawIndexedIndirectCommand) * seqDrawStart;
      lastSeq.drawIndirectCount.commandCount = seqDrawCount;
      lastSeq.drawIndirectCount.stride       = uint32_t(sizeof(VkDrawIndexedIndirectCommand));
      seqBinned.push_back(lastSeq);

      m_draw.drawIndirectCount = std::max(m_draw.drawIndirectCount, seqDrawCount);
    }

    m_draw.sequencesCount = uint32_t(seqBinned.size());

    // input buffer
    m_draw.inputBuffer =
        res->m_resourceAllocator.createBuffer(sizeof(DrawSequenceBinned) * seqBinned.size() + 32,
                                              VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    m_draw.inputSize = sizeof(DrawSequenceBinned) * seqBinned.size();

    staging.upload(m_draw.inputBuffer.buffer, 0, m_draw.inputSize, seqBinned.data());
  }

  void setupPreprocess(Stats& stats)
  {
    ResourcesVK* res = m_resources;

    VkGeneratedCommandsMemoryRequirementsInfoEXT memInfo = {VK_STRUCTURE_TYPE_GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_EXT};
    memInfo.maxSequenceCount       = m_draw.sequencesCount;
    memInfo.maxDrawCount           = m_draw.drawIndirectCount;
    memInfo.indirectCommandsLayout = m_draw.indirectCmdsLayout;
    memInfo.indirectExecutionSet   = m_indirectExecutionSet;

    VkMemoryRequirements2 memReqs = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
    vkGetGeneratedCommandsMemoryRequirementsEXT(res->m_device, &memInfo, &memReqs);

    m_draw.preprocessSize = memReqs.memoryRequirements.size;

    VkBufferCreateInfo               bufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    VkBufferUsageFlags2CreateInfoKHR bufferFlags2     = {VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO_KHR};
    bufferCreateInfo.size                             = m_draw.preprocessSize;
    bufferFlags2.usage = VK_BUFFER_USAGE_2_PREPROCESS_BUFFER_BIT_EXT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT_KHR
                         | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR;
    bufferCreateInfo.pNext = &bufferFlags2;

    VkResult result = vkCreateBuffer(res->m_device, &bufferCreateInfo, nullptr, &m_draw.preprocessBuffer.buffer);
    assert(result == VK_SUCCESS);

    nvvk::MemAllocateInfo memAllocInfo(memReqs.memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_draw.preprocessBuffer.memHandle = res->m_memoryAllocator.allocMemory(memAllocInfo);
    nvvk::MemAllocator::MemInfo allocatedMemInfo = res->m_memoryAllocator.getMemoryInfo(m_draw.preprocessBuffer.memHandle);
    vkBindBufferMemory(res->m_device, m_draw.preprocessBuffer.buffer, allocatedMemInfo.memory, allocatedMemInfo.offset);
    m_draw.preprocessBuffer.address = nvvk::getBufferDeviceAddress(res->m_device, m_draw.preprocessBuffer.buffer);

    printf("preprocess Address: %llX\n", m_draw.preprocessBuffer.address);

    stats.preprocessSizeKB = (m_draw.preprocessSize + 1023) / 1024;
    stats.sequences        = m_draw.sequencesCount;
  }

  void deleteData()
  {
    m_resources->m_resourceAllocator.destroy(m_draw.inputBuffer);
    m_resources->m_resourceAllocator.destroy(m_draw.preprocessBuffer);
    m_resources->m_resourceAllocator.destroy(m_draw.drawIndirectBuffer);
    m_resources->m_resourceAllocator.destroy(m_draw.combinedIndices);
  }

  void initStateCommandBuffer()
  {
    {
      VkResult                result;
      VkCommandPoolCreateInfo cmdPoolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
      cmdPoolInfo.queueFamilyIndex        = 0;
      result = vkCreateCommandPool(m_resources->m_device, &cmdPoolInfo, nullptr, &m_cmdPool);
      assert(result == VK_SUCCESS);
    }
    {
      m_draw.cmdStateBuffer = m_resources->createCmdBuffer(m_cmdPool, false, false, false);
      {
        cmdStates(m_draw.cmdStateBuffer);
      }
    }
  }

  void deinitStateCommandBuffer()
  {
    vkFreeCommandBuffers(m_resources->m_device, m_cmdPool, 1, &m_draw.cmdStateBuffer);
    vkDestroyCommandPool(m_resources->m_device, m_cmdPool, nullptr);
  }
};


static RendererVKGenEXT::TypeGen        s_type_cmdbuffergen_vk;
static RendererVKGenEXT::TypePreprocess s_type_cmdbuffergen2_vk;


void RendererVKGenEXT::initIndirectCommandsLayout(const Renderer::Config& config)
{
  ResourcesVK* res = m_resources;

  // FIXME BUG workaround "static"

  static std::array<VkIndirectCommandsLayoutTokenEXT, 6> inputInfos;
  static struct
  {
    VkIndirectCommandsVertexBufferTokenEXT vertexBuffer;
    VkIndirectCommandsIndexBufferTokenEXT  indexBuffer;
    VkIndirectCommandsPushConstantTokenEXT pushConstantVertex;
    VkIndirectCommandsPushConstantTokenEXT pushConstantFragment;
    VkIndirectCommandsExecutionSetTokenEXT executionSet;
  } inputData;

  uint32_t numInputs = 0;

  if(m_config.maxShaders > 1)
  {
    VkIndirectCommandsLayoutTokenEXT input = {VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_EXT, 0,
                                              VK_INDIRECT_COMMANDS_TOKEN_TYPE_EXECUTION_SET_EXT};

    inputData.executionSet.type         = m_config.shaderObjs ? VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT :
                                                                VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT;
    inputData.executionSet.shaderStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    input.offset = config.binned ? offsetof(DrawSequenceBinned, pipeline) : offsetof(DrawSequence, pipeline);
    input.data.pExecutionSet = &inputData.executionSet;
    inputInfos[numInputs]    = input;
    numInputs++;
  }
  {
    VkIndirectCommandsLayoutTokenEXT input = {VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_EXT, 0,
                                              VK_INDIRECT_COMMANDS_TOKEN_TYPE_INDEX_BUFFER_EXT};

    inputData.indexBuffer.mode = VK_INDIRECT_COMMANDS_INPUT_MODE_VULKAN_INDEX_BUFFER_EXT;

    input.offset            = config.binned ? offsetof(DrawSequenceBinned, ibo) : offsetof(DrawSequence, ibo);
    input.data.pIndexBuffer = &inputData.indexBuffer;
    inputInfos[numInputs]   = input;
    numInputs++;
  }
  {
    VkIndirectCommandsLayoutTokenEXT input   = {VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_EXT, 0,
                                                VK_INDIRECT_COMMANDS_TOKEN_TYPE_VERTEX_BUFFER_EXT};
    inputData.vertexBuffer.vertexBindingUnit = 0;

    input.offset             = config.binned ? offsetof(DrawSequenceBinned, vbo) : offsetof(DrawSequence, vbo);
    input.data.pVertexBuffer = &inputData.vertexBuffer;
    inputInfos[numInputs]    = input;
    numInputs++;
  }
  if(config.bindingMode == BINDINGMODE_PUSHADDRESS)
  {
    VkIndirectCommandsLayoutTokenEXT input = {VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_EXT, 0,
                                              VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_EXT};

    inputData.pushConstantVertex.updateRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    inputData.pushConstantVertex.updateRange.offset     = 0;
    inputData.pushConstantVertex.updateRange.size       = sizeof(VkDeviceAddress);

    input.offset = config.binned ? offsetof(DrawSequenceBinned, pushMatrix) : offsetof(DrawSequence, pushMatrix);
    input.data.pPushConstant = &inputData.pushConstantVertex;
    inputInfos[numInputs]    = input;
    numInputs++;

    inputData.pushConstantFragment.updateRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    inputData.pushConstantFragment.updateRange.offset     = sizeof(VkDeviceAddress);
    inputData.pushConstantFragment.updateRange.size       = sizeof(VkDeviceAddress);

    input.offset = config.binned ? offsetof(DrawSequenceBinned, pushMaterial) : offsetof(DrawSequence, pushMaterial);
    input.data.pPushConstant = &inputData.pushConstantFragment;
    inputInfos[numInputs]    = input;
    numInputs++;
  }

  if(config.binned)
  {
    VkIndirectCommandsLayoutTokenEXT input = {VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_EXT, 0,
                                              VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_COUNT_EXT};
    input.offset                           = offsetof(DrawSequenceBinned, drawIndirectCount);
    inputInfos[numInputs]                  = input;
    numInputs++;
  }
  else
  {
    VkIndirectCommandsLayoutTokenEXT input = {VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_EXT, 0,
                                              VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_EXT};
    input.offset                           = offsetof(DrawSequence, drawIndexed);
    inputInfos[numInputs]                  = input;
    numInputs++;
  }

  uint32_t interleavedStride = config.binned ? sizeof(DrawSequenceBinned) : sizeof(DrawSequence);

  assert(numInputs <= inputInfos.size());

  VkIndirectCommandsLayoutCreateInfoEXT genInfo = {VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_EXT};
  genInfo.tokenCount                            = numInputs;
  genInfo.pTokens                               = inputInfos.data();
  genInfo.indirectStride                        = interleavedStride;
  genInfo.pipelineLayout                        = res->m_drawPush.getPipeLayout();

  if(config.unordered)
  {
    genInfo.flags |= VK_INDIRECT_COMMANDS_LAYOUT_USAGE_UNORDERED_SEQUENCES_BIT_EXT;
  }
  if(m_mode == MODE_PREPROCESS)
  {
    genInfo.flags |= VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT;
  }

  VkResult result;
  result = vkCreateIndirectCommandsLayoutEXT(res->m_device, &genInfo, nullptr, &m_draw.indirectCmdsLayout);
  assert(result == VK_SUCCESS);
}

void RendererVKGenEXT::initIndirectExecutionSet()
{
  ResourcesVK* res = m_resources;

  if(m_config.shaderObjs)
  {
    VkIndirectExecutionSetShaderInfoEXT execSetShaderInfo = {VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_SHADER_INFO_EXT};
    VkIndirectExecutionSetShaderLayoutInfoEXT execShaderLayoutInfos[2];
    VkShaderEXT initialShaders[2] = {res->m_drawShading.vertexShaderObjs[0], res->m_drawShading.fragmentShaderObjs[0]};

    execSetShaderInfo.maxShaderCount  = 2 * m_config.maxShaders;
    execSetShaderInfo.shaderCount     = 2;
    execSetShaderInfo.pInitialShaders = initialShaders;

    execShaderLayoutInfos[0]                = {VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_SHADER_LAYOUT_INFO_EXT};
    execShaderLayoutInfos[0].setLayoutCount = 1;

    if(m_config.bindingMode == BINDINGMODE_PUSHADDRESS)
    {
      execShaderLayoutInfos[0].pSetLayouts     = &res->m_drawPush.getLayout();
      execSetShaderInfo.pushConstantRangeCount = NV_ARRAY_SIZE(res->m_pushRanges);
      execSetShaderInfo.pPushConstantRanges    = res->m_pushRanges;
    }
    else if(m_config.bindingMode == BINDINGMODE_INDEX_BASEINSTANCE)
    {
      execShaderLayoutInfos[0].pSetLayouts = &res->m_drawIndexed.getLayout();
    }
    // both stages use same layouts
    execShaderLayoutInfos[1] = execShaderLayoutInfos[0];

    execSetShaderInfo.pSetLayoutInfos = execShaderLayoutInfos;

    VkIndirectExecutionSetCreateInfoEXT execSetCreateInfo = {VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_CREATE_INFO_EXT};
    execSetCreateInfo.type                                = VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT;
    execSetCreateInfo.info.pShaderInfo                    = &execSetShaderInfo;

    vkCreateIndirectExecutionSetEXT(res->m_device, &execSetCreateInfo, nullptr, &m_indirectExecutionSet);

    std::vector<VkWriteIndirectExecutionSetShaderEXT> indirectShaders;

    // pump the pipelines in
    for(uint32_t m = 0; m < m_config.maxShaders; m++)
    {
      VkWriteIndirectExecutionSetShaderEXT writeSet = {VK_STRUCTURE_TYPE_WRITE_INDIRECT_EXECUTION_SET_SHADER_EXT};
      writeSet.index                                = m * 2 + 0;
      writeSet.shader                               = res->m_drawShading.vertexShaderObjs[m];
      indirectShaders.push_back(writeSet);

      writeSet.index  = m * 2 + 1;
      writeSet.shader = res->m_drawShading.fragmentShaderObjs[m];
      indirectShaders.push_back(writeSet);
    }

    vkUpdateIndirectExecutionSetShaderEXT(res->m_device, m_indirectExecutionSet, uint32_t(indirectShaders.size()),
                                          indirectShaders.data());
  }
  else
  {
    VkIndirectExecutionSetPipelineInfoEXT execSetPipelineInfo = {VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_PIPELINE_INFO_EXT};
    execSetPipelineInfo.initialPipeline  = res->m_drawShading.pipelines[0];
    execSetPipelineInfo.maxPipelineCount = m_config.maxShaders;

    VkIndirectExecutionSetCreateInfoEXT execSetCreateInfo = {VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_CREATE_INFO_EXT};
    execSetCreateInfo.type                                = VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT;
    execSetCreateInfo.info.pPipelineInfo                  = &execSetPipelineInfo;

    vkCreateIndirectExecutionSetEXT(res->m_device, &execSetCreateInfo, nullptr, &m_indirectExecutionSet);

    std::vector<VkWriteIndirectExecutionSetPipelineEXT> indirectPipes;

    // pump the pipelines in
    for(uint32_t m = 0; m < m_config.maxShaders; m++)
    {
      VkWriteIndirectExecutionSetPipelineEXT writeSet = {VK_STRUCTURE_TYPE_WRITE_INDIRECT_EXECUTION_SET_PIPELINE_EXT};
      writeSet.index                                  = m;
      writeSet.pipeline                               = res->m_drawShading.pipelines[m];
      indirectPipes.push_back(writeSet);
    }

    vkUpdateIndirectExecutionSetPipelineEXT(res->m_device, m_indirectExecutionSet, uint32_t(indirectPipes.size()),
                                            indirectPipes.data());
  }
}

void RendererVKGenEXT::init(const CadScene* scene, ResourcesVK* resources, const Renderer::Config& config, Stats& stats)
{
  ResourcesVK* res = (ResourcesVK*)resources;
  m_resources      = res;
  m_scene          = scene;
  m_config         = config;

  m_indexingBits = scene->getIndexingBits();

  stats.cmdBuffers = 1;

  std::vector<DrawItem> drawItems;
  fillDrawItems(drawItems, scene, config, stats);

  res->initPipelinesOrShaders(m_config.bindingMode,
                              m_config.maxShaders > 1 ? VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT : 0, m_config.shaderObjs);

  if(m_config.maxShaders > 1)
  {
    initIndirectExecutionSet();
  }

  initIndirectCommandsLayout(config);
  if(config.binned)
  {
    setupInputBinned(drawItems.data(), drawItems.size(), stats);
  }
  else
  {
    setupInputInterleaved(drawItems.data(), drawItems.size(), stats);
  }
  setupPreprocess(stats);

  if(m_mode == MODE_PREPROCESS)
  {
    initStateCommandBuffer();
  }
}

void RendererVKGenEXT::deinit()
{
  if(m_mode == MODE_PREPROCESS)
  {
    deinitStateCommandBuffer();
  }

  deleteData();
  deinitIndirectCommandsLayout();
  vkDestroyIndirectExecutionSetEXT(m_resources->m_device, m_indirectExecutionSet, nullptr);
}

VkGeneratedCommandsInfoEXT RendererVKGenEXT::getGeneratedCommandsInfo()
{
  ResourcesVK*               res  = m_resources;
  VkGeneratedCommandsInfoEXT info = {VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_EXT};
  info.indirectExecutionSet       = m_indirectExecutionSet;
  info.indirectCommandsLayout     = m_draw.indirectCmdsLayout;
  info.maxSequenceCount           = m_draw.sequencesCount;
  info.maxDrawCount               = m_draw.drawIndirectCount;
  info.preprocessAddress          = m_draw.preprocessBuffer.address;
  info.preprocessSize             = m_draw.preprocessSize;
  info.indirectAddress            = m_draw.inputBuffer.address;
  info.indirectAddressSize        = m_draw.inputSize;

  return info;
}

void RendererVKGenEXT::cmdStates(VkCommandBuffer cmd)
{
  ResourcesVK*      res     = m_resources;
  const CadSceneVK& sceneVK = res->m_scene;

  if(m_config.shaderObjs)
  {
    res->cmdShaderObjectState(cmd);
  }
  else
  {
    res->cmdDynamicPipelineState(cmd);
  }

  switch(m_config.bindingMode)
  {
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
    VkShaderStageFlagBits stages[2] = {VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT};
    VkShaderEXT shaders[2] = {res->m_drawShading.vertexShaderObjs[0], res->m_drawShading.fragmentShaderObjs[0]};
    vkCmdBindShadersEXT(cmd, 2, stages, shaders);

    const VkShaderStageFlagBits unusedStages[3] = {VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                                                   VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, VK_SHADER_STAGE_GEOMETRY_BIT};
    vkCmdBindShadersEXT(cmd, 3, unusedStages, nullptr);
  }
  else
  {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawShading.pipelines[0]);
  }
}

void RendererVKGenEXT::cmdExecute(VkCommandBuffer cmd, VkBool32 isPreprocessed)
{
  ResourcesVK*      res     = m_resources;
  const CadSceneVK& sceneVK = res->m_scene;

  cmdStates(cmd);

  // The previously generated commands will be executed here.
  // The current state of the command buffer is inherited just like a usual work provoking command.
  VkGeneratedCommandsInfoEXT info = getGeneratedCommandsInfo();
  vkCmdExecuteGeneratedCommandsEXT(cmd, isPreprocessed, &info);
  // after this function the state is undefined, you must rebind PSO as well as other
  // state that could have been touched
}

void RendererVKGenEXT::cmdPreprocess(VkCommandBuffer primary)
{
  // If we were regenerating commands into the same preprocessBuffer in the same frame
  // then we would have to insert a barrier that ensures rendering of the preprocesBuffer
  // had completed.
  // Similar applies, if were modifying the input buffers, appropriate barriers would have to
  // be set here.
  //
  // vkCmdPipelineBarrier(primary, whateverModifiedInputs, VK_PIPELINE_STAGE_COMMAND_PROCESS_BIT_EXT,  ...);
  //  barrier.dstAccessMask = VK_ACCESS_COMMAND_PROCESS_READ_BIT_EXT;
  //
  // It is not required in this sample, as the blitting synchronizes each frame, and we
  // do not actually modify the input tokens dynamically.
  //
  VkGeneratedCommandsInfoEXT         info         = getGeneratedCommandsInfo();
  VkGeneratedCommandsPipelineInfoEXT infoPipeline = {VK_STRUCTURE_TYPE_GENERATED_COMMANDS_PIPELINE_INFO_EXT};
  VkGeneratedCommandsShaderInfoEXT   infoShader   = {VK_STRUCTURE_TYPE_GENERATED_COMMANDS_SHADER_INFO_EXT};

  VkShaderEXT shaders[2] = {m_resources->m_drawShading.vertexShaderObjs[0], m_resources->m_drawShading.fragmentShaderObjs[0]};

  if(m_config.shaderObjs)
  {
    infoShader.pShaders    = shaders;
    infoShader.shaderCount = 2;
    info.pNext             = &infoShader;
  }
  else
  {
    infoPipeline.pipeline = m_resources->m_drawShading.pipelines[0];
    info.pNext            = &infoPipeline;
  }

  vkCmdPreprocessGeneratedCommandsEXT(primary, &info, m_draw.cmdStateBuffer);
}

void RendererVKGenEXT::draw(const Resources::Global& global, Stats& stats)
{
  const CadScene* scene = m_scene;
  ResourcesVK*    res   = m_resources;

  // generic state setup
  VkCommandBuffer primary = res->createTempCmdBuffer();

  {
    nvvk::ProfilerVK::Section profile(res->m_profilerVK, "Render", primary);

    if(m_mode != MODE_DIRECT)
    {
      nvvk::ProfilerVK::Section profile(res->m_profilerVK, "Pre", primary);
      cmdPreprocess(primary);

      {
        // we need to ensure the preprocessing of commands has completed, before we can execute them
        VkMemoryBarrier barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask   = VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_EXT;
        barrier.dstAccessMask   = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        vkCmdPipelineBarrier(primary, VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_EXT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                             0, 1, &barrier, 0, nullptr, 0, nullptr);
      }
    }
    {
      nvvk::ProfilerVK::Section profile(res->m_profilerVK, "Draw", primary);
      vkCmdUpdateBuffer(primary, res->m_common.viewBuffer.buffer, 0, sizeof(SceneData), (const uint32_t*)&global.sceneUbo);
      res->cmdPipelineBarrier(primary);

      // clear via pass
      res->cmdBeginRendering(primary);
      cmdExecute(primary, m_mode == MODE_PREPROCESS);
      vkCmdEndRendering(primary);
    }
  }

  vkEndCommandBuffer(primary);
  res->submissionEnqueue(primary);
}

}  // namespace generatedcmds
