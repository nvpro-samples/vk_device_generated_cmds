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

#include "renderer.hpp"
#include "resources_vk.hpp"

#include <nvh/nvprint.hpp>

#include "common.h"

namespace generatedcmds {

//////////////////////////////////////////////////////////////////////////

class RendererVKGenNV : public Renderer
{
public:
  enum Mode
  {
    MODE_DIRECT,      // direct execute & generate
    MODE_PREPROCESS,  // separate pre-process step
  };

  Mode m_mode;

  class TypeDirect : public Renderer::Type
  {
    VkPhysicalDeviceDeviceGeneratedCommandsPropertiesNV props = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_NV};

    bool isAvailable(const nvvk::Context& context)
    {
      VkPhysicalDeviceProperties2 props2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
      props2.pNext                       = &props;
      vkGetPhysicalDeviceProperties2(context.m_physicalDevice, &props2);

      return context.hasDeviceExtension(VK_NV_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME);
    }
    const char* name() const override { return "generated cmds nv"; }
    Renderer*   create() const override
    {
      RendererVKGenNV* renderer = new RendererVKGenNV();
      renderer->m_mode          = MODE_DIRECT;

      return renderer;
    }
    uint32_t priority() const override { return 30; }
    uint32_t supportedBindingModes() const override
    {
      return (1 << BINDINGMODE_PUSHADDRESS) | (1 << BINDINGMODE_INDEX_BASEINSTANCE) | (1 << BINDINGMODE_INDEX_VERTEXATTRIB);
    };
    bool     supportsShaderObjs() const override { return false; };
    uint32_t supportedShaderBinds() const override { return props.maxGraphicsShaderGroupCount; }
  };

  class TypeReuse : public Renderer::Type
  {
    VkPhysicalDeviceDeviceGeneratedCommandsPropertiesNV props = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_NV};

    bool isAvailable(const nvvk::Context& context)
    {
      VkPhysicalDeviceProperties2 props2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
      props2.pNext                       = &props;
      vkGetPhysicalDeviceProperties2(context.m_physicalDevice, &props2);

      return context.hasDeviceExtension(VK_NV_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME);
    }
    const char* name() const { return "preprocess,generated cmds nv"; }
    Renderer*   create() const
    {
      RendererVKGenNV* renderer = new RendererVKGenNV();
      renderer->m_mode          = MODE_PREPROCESS;

      return renderer;
    }
    uint32_t priority() const override { return 30; }
    uint32_t supportedBindingModes() const override
    {
      return (1 << BINDINGMODE_PUSHADDRESS) | (1 << BINDINGMODE_INDEX_BASEINSTANCE);
    };
    bool     supportsShaderObjs() const override { return false; };
    uint32_t supportedShaderBinds() const override { return props.maxGraphicsShaderGroupCount; }
  };

public:
  void init(const CadScene* scene, ResourcesVK* resources, const Renderer::Config& config, Stats& stats) override;
  void deinit() override;
  void draw(const Resources::Global& global, Stats& stats) override;

  RendererVKGenNV() {}

private:
  struct DrawSequence
  {
    VkBindShaderGroupIndirectCommandNV  shader;
    uint32_t                            _pad;
    VkDeviceAddress                     pushMatrix;
    VkDeviceAddress                     pushMaterial;
    VkBindIndexBufferIndirectCommandNV  ibo;
    VkBindVertexBufferIndirectCommandNV vbo;
    VkDrawIndexedIndirectCommand        drawIndexed;
  };

  struct DrawSetup
  {
    nvvk::Buffer combinedIndices;

    std::vector<VkIndirectCommandsStreamNV> inputs;

    VkIndirectCommandsLayoutNV indirectCmdsLayout;

    nvvk::Buffer inputBuffer;
    size_t       inputSequenceIndexOffset;

    nvvk::Buffer preprocessBuffer;
    VkDeviceSize preprocessSize;

    uint32_t sequencesCount;
  };


  ResourcesVK*           m_resources;
  CadScene::IndexingBits m_indexingBits;

  DrawSetup  m_draw;
  VkPipeline m_indirectPipeline = nullptr;

  VkGeneratedCommandsInfoNV getGeneratedCommandsInfo();

  void cmdPreprocess(VkCommandBuffer cmd);
  void cmdExecute(VkCommandBuffer cmd, VkBool32 isPreprocessed);

  void initShaderGroupsPipeline();

  void initIndirectCommandsLayout(const Renderer::Config& config);
  void deinitIndirectCommandsLayout()
  {
    vkDestroyIndirectCommandsLayoutNV(m_resources->m_device, m_draw.indirectCmdsLayout, nullptr);
  }

  void setupInputInterleaved(const DrawItem* drawItems, size_t drawCount, Stats& stats)
  {
    ResourcesVK*      res   = m_resources;
    const CadSceneVK& scene = res->m_scene;

    // setup staging buffer for filling
    ScopeStaging staging(res->m_resourceAllocator, res->m_queue, res->m_queueFamily);

    m_draw.sequencesCount = drawCount;

    // compute input buffer space requirements
    VkPhysicalDeviceProperties2 phyProps = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    VkPhysicalDeviceDeviceGeneratedCommandsPropertiesNV genProps = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_NV};
    phyProps.pNext = &genProps;
    vkGetPhysicalDeviceProperties2(res->m_physical, &phyProps);


    // create input buffer

    size_t alignSeqIndexMask = genProps.minSequencesIndexBufferOffsetAlignment - 1;
    size_t inputBufferSize   = ((sizeof(DrawSequence) * drawCount) + alignSeqIndexMask) & (~alignSeqIndexMask);
    size_t seqindexOffset    = inputBufferSize;

    if(m_config.permutated)
    {
      inputBufferSize += sizeof(uint32_t) * drawCount;
    }

    inputBufferSize += 32;  // +32 in case num == 0

    m_draw.inputBuffer    = res->m_resourceAllocator.createBuffer(inputBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    uint8_t* inputMapping = staging.uploadT<uint8_t>(m_draw.inputBuffer.buffer, 0, inputBufferSize);

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

    // fill sequence
    DrawSequence* sequences = (DrawSequence*)inputMapping;
    for(unsigned int i = 0; i < drawCount; i++)
    {
      const DrawItem&             di  = drawItems[i];
      const CadSceneVK::Geometry& geo = scene.m_geometry[di.geometryIndex];

      DrawSequence& seq = sequences[i];

      seq.shader.groupIndex = di.shaderIndex;

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

    if(m_config.permutated)
    {
      m_draw.inputSequenceIndexOffset = seqindexOffset;

      // fill index permutation (random, worst-case)
      uint32_t* permutation = (uint32_t*)(inputMapping + seqindexOffset);
      fillRandomPermutation(drawCount, permutation, drawItems, stats);
    }

    // setup input stream
    VkIndirectCommandsStreamNV input;
    input.buffer = m_draw.inputBuffer.buffer;
    input.offset = 0;
    m_draw.inputs.push_back(input);
  }

  void setupInputSeparate(const DrawItem* drawItems, size_t drawCount, Stats& stats)
  {
    ResourcesVK*      res   = m_resources;
    const CadSceneVK& scene = res->m_scene;

    // setup staging buffer for filling
    ScopeStaging staging(res->m_resourceAllocator, res->m_queue, res->m_queueFamily);

    m_draw.sequencesCount = drawCount;

    // compute input buffer
    VkPhysicalDeviceProperties2 phyProps = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    VkPhysicalDeviceDeviceGeneratedCommandsPropertiesNV genProps = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_NV};
    phyProps.pNext = &genProps;
    vkGetPhysicalDeviceProperties2(res->m_physical, &phyProps);

    size_t alignSeqIndexMask = genProps.minSequencesIndexBufferOffsetAlignment - 1;
    size_t alignMask         = genProps.minIndirectCommandsBufferOffsetAlignment - 1;

    size_t totalSize  = 0;
    size_t pipeOffset = totalSize;
    totalSize = totalSize + ((sizeof(VkBindShaderGroupIndirectCommandNV) * drawCount + alignMask) & (~alignMask));
    size_t iboOffset = totalSize;
    totalSize = totalSize + ((sizeof(VkBindIndexBufferIndirectCommandNV) * drawCount + alignMask) & (~alignMask));
    size_t vboOffset = totalSize;
    totalSize = totalSize + ((sizeof(VkBindVertexBufferIndirectCommandNV) * drawCount + alignMask) & (~alignMask));
    size_t matrixOffset   = totalSize;
    totalSize             = totalSize + ((sizeof(VkDeviceAddress) * drawCount + alignMask) & (~alignMask));
    size_t materialOffset = totalSize;
    totalSize             = totalSize + ((sizeof(VkDeviceAddress) * drawCount + alignMask) & (~alignMask));
    size_t drawOffset     = totalSize;
    totalSize             = totalSize + ((sizeof(VkDrawIndexedIndirectCommand) * drawCount + alignMask) & (~alignMask));
    size_t seqindexOffset = totalSize;

    if(m_config.permutated)
    {
      totalSize += sizeof(uint32_t) * drawCount;
    }
    totalSize += 32;  // +32 in case num == 0

    m_draw.inputBuffer    = res->m_resourceAllocator.createBuffer(totalSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    uint8_t* inputMapping = staging.uploadT<uint8_t>(m_draw.inputBuffer.buffer, 0, totalSize);

    VkBindShaderGroupIndirectCommandNV*  shaders = (VkBindShaderGroupIndirectCommandNV*)(inputMapping + pipeOffset);
    VkBindVertexBufferIndirectCommandNV* vbos    = (VkBindVertexBufferIndirectCommandNV*)(inputMapping + vboOffset);
    VkBindIndexBufferIndirectCommandNV*  ibos    = (VkBindIndexBufferIndirectCommandNV*)(inputMapping + iboOffset);
    VkDeviceAddress*                     pushMatrices  = (VkDeviceAddress*)(inputMapping + matrixOffset);
    VkDeviceAddress*                     pushMaterials = (VkDeviceAddress*)(inputMapping + materialOffset);
    VkDrawIndexedIndirectCommand*        draws         = (VkDrawIndexedIndirectCommand*)(inputMapping + drawOffset);

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

    // let's record all token inputs for every drawcall
    for(unsigned int i = 0; i < drawCount; i++)
    {
      const DrawItem&             di  = drawItems[i];
      const CadSceneVK::Geometry& geo = scene.m_geometry[di.geometryIndex];

      shaders[i].groupIndex = di.shaderIndex;

      VkBindIndexBufferIndirectCommandNV& ibo = ibos[i];
      ibo.bufferAddress                       = nvvk::getBufferDeviceAddress(res->m_device, geo.ibo.buffer);
      ibo.indexType                           = VK_INDEX_TYPE_UINT32;

      VkBindVertexBufferIndirectCommandNV& vbo = vbos[i];
      vbo.bufferAddress                        = nvvk::getBufferDeviceAddress(res->m_device, geo.vbo.buffer);
      vbo.stride                               = sizeof(CadScene::Vertex);

#if USE_DRAW_OFFSETS
      ibo.size = scene.m_geometryMem.getChunk(geo.allocation).iboSize;
      vbo.size = scene.m_geometryMem.getChunk(geo.allocation).vboSize;
#else
      ibo.bufferAddress += geo.ibo.offset;
      vbo.bufferAddress += geo.vbo.offset;

      ibo.size = geo.ibo.range;
      vbo.size = geo.vbo.range;
#endif

      pushMatrices[i]  = matrixAddress + sizeof(CadScene::MatrixNode) * di.matrixIndex;
      pushMaterials[i] = materialAddress + sizeof(CadScene::Material) * di.materialIndex;

      VkDrawIndexedIndirectCommand& drawIndexed = draws[i];
      drawIndexed.indexCount                    = di.range.count;
      drawIndexed.instanceCount                 = 1;
      drawIndexed.firstInstance                 = m_indexingBits.packIndices(di.matrixIndex, di.materialIndex);
      drawIndexed.firstIndex                    = uint32_t(di.range.offset / sizeof(uint32_t));
      drawIndexed.vertexOffset                  = 0;
#if USE_DRAW_OFFSETS
      drawIndexed.firstIndex += geo.ibo.offset / sizeof(uint32_t);
#endif
#if USE_DRAW_OFFSETS
      drawIndexed.vertexOffset = geo.vbo.offset / sizeof(CadScene::Vertex);
#endif
      if(m_config.bindingMode == BINDINGMODE_INDEX_BASEINSTANCE)
      {
        drawIndexed.firstInstance = m_indexingBits.packIndices(di.matrixIndex, di.materialIndex);
      }
      else if(m_config.bindingMode == BINDINGMODE_INDEX_VERTEXATTRIB)
      {
        drawIndexed.firstInstance = i;
        combinedIndicesMapping[i] = m_indexingBits.packIndices(di.matrixIndex, di.materialIndex);
      }
    }
    if(m_config.permutated)
    {
      m_draw.inputSequenceIndexOffset = seqindexOffset;

      // fill index permutation (random, worst-case)
      uint32_t* permutation = (uint32_t*)(inputMapping + seqindexOffset);
      fillRandomPermutation(drawCount, permutation, drawItems, stats);
    }

    // setup input streams
    VkIndirectCommandsStreamNV input;
    input.buffer = m_draw.inputBuffer.buffer;

    if(m_config.maxShaders > 1)
    {
      input.offset = pipeOffset;
      m_draw.inputs.push_back(input);
    }
    {
      input.offset = iboOffset;
      m_draw.inputs.push_back(input);
    }
    {
      input.offset = vboOffset;
      m_draw.inputs.push_back(input);
    }
    if(m_config.bindingMode == BINDINGMODE_PUSHADDRESS)
    {
      input.offset = matrixOffset;
      m_draw.inputs.push_back(input);

      input.offset = materialOffset;
      m_draw.inputs.push_back(input);
    }
    {
      input.offset = drawOffset;
      m_draw.inputs.push_back(input);
    }
  }

  void setupPreprocess(Stats& stats)
  {
    ResourcesVK* res = m_resources;

    VkGeneratedCommandsMemoryRequirementsInfoNV memInfo = {VK_STRUCTURE_TYPE_GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_NV};
    VkPipelineBindPoint bindPoint  = VK_PIPELINE_BIND_POINT_GRAPHICS;
    memInfo.maxSequencesCount      = m_draw.sequencesCount;
    memInfo.indirectCommandsLayout = m_draw.indirectCmdsLayout;
    memInfo.pipeline               = m_indirectPipeline;
    memInfo.pipelineBindPoint      = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkMemoryRequirements2 memReqs = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
    vkGetGeneratedCommandsMemoryRequirementsNV(res->m_device, &memInfo, &memReqs);

    m_draw.preprocessSize = memReqs.memoryRequirements.size;
    m_draw.preprocessBuffer.buffer =
        nvvk::createBuffer(res->m_device, nvvk::makeBufferCreateInfo(m_draw.preprocessSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT));

    nvvk::MemAllocateInfo memAllocInfo(memReqs.memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_draw.preprocessBuffer.memHandle = res->m_memoryAllocator.allocMemory(memAllocInfo);

    nvvk::MemAllocator::MemInfo allocatedMemInfo = res->m_memoryAllocator.getMemoryInfo(m_draw.preprocessBuffer.memHandle);
    vkBindBufferMemory(res->m_device, m_draw.preprocessBuffer.buffer, allocatedMemInfo.memory, allocatedMemInfo.offset);
    m_draw.preprocessBuffer.address = nvvk::getBufferDeviceAddress(res->m_device, m_draw.preprocessBuffer.buffer);

    stats.preprocessSizeKB = (m_draw.preprocessSize + 1023) / 1024;
    stats.sequences        = m_draw.sequencesCount;
  }

  void deleteData()
  {
    m_resources->m_resourceAllocator.destroy(m_draw.inputBuffer);
    m_resources->m_resourceAllocator.destroy(m_draw.preprocessBuffer);
    m_resources->m_resourceAllocator.destroy(m_draw.combinedIndices);
  }
};


static RendererVKGenNV::TypeDirect s_type_cmdbuffergen_vk;
static RendererVKGenNV::TypeReuse  s_type_cmdbuffergen2_vk;

void RendererVKGenNV::initIndirectCommandsLayout(const Renderer::Config& config)
{
  ResourcesVK* res = m_resources;

  std::vector<VkIndirectCommandsLayoutTokenNV> inputInfos;
  std::vector<uint32_t>                        inputStrides;

  uint32_t numInputs = 0;

  if(m_config.maxShaders > 1)
  {
    VkIndirectCommandsLayoutTokenNV input = {VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV, 0,
                                             VK_INDIRECT_COMMANDS_TOKEN_TYPE_SHADER_GROUP_NV};
    input.stream                          = config.interleaved ? 0 : numInputs;
    input.offset                          = config.interleaved ? offsetof(DrawSequence, shader) : 0;
    inputInfos.push_back(input);
    inputStrides.push_back(sizeof(VkBindShaderGroupIndirectCommandNV));
    numInputs++;
  }
  {
    VkIndirectCommandsLayoutTokenNV input = {VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV, 0,
                                             VK_INDIRECT_COMMANDS_TOKEN_TYPE_INDEX_BUFFER_NV};
    input.stream                          = config.interleaved ? 0 : numInputs;
    input.offset                          = config.interleaved ? offsetof(DrawSequence, ibo) : 0;
    inputInfos.push_back(input);
    inputStrides.push_back(sizeof(VkBindIndexBufferIndirectCommandNV));
    numInputs++;
  }
  {
    VkIndirectCommandsLayoutTokenNV input = {VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV, 0,
                                             VK_INDIRECT_COMMANDS_TOKEN_TYPE_VERTEX_BUFFER_NV};
    input.vertexBindingUnit               = 0;
    input.vertexDynamicStride             = USE_DYNAMIC_VERTEX_STRIDE ? VK_TRUE : VK_FALSE;
    input.stream                          = config.interleaved ? 0 : numInputs;
    input.offset                          = config.interleaved ? offsetof(DrawSequence, vbo) : 0;
    inputInfos.push_back(input);
    inputStrides.push_back(sizeof(VkBindVertexBufferIndirectCommandNV));
    numInputs++;
  }
  if(config.bindingMode == BINDINGMODE_PUSHADDRESS)
  {
    VkIndirectCommandsLayoutTokenNV input = {VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV, 0,
                                             VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_NV};
    input.pushconstantPipelineLayout      = res->m_drawPush.getPipeLayout();
    input.pushconstantShaderStageFlags    = VK_SHADER_STAGE_VERTEX_BIT;
    input.pushconstantOffset              = 0;
    input.pushconstantSize                = sizeof(VkDeviceAddress);
    input.stream                          = config.interleaved ? 0 : numInputs;
    input.offset                          = config.interleaved ? offsetof(DrawSequence, pushMatrix) : 0;
    inputInfos.push_back(input);
    inputStrides.push_back(sizeof(VkDeviceAddress));
    numInputs++;

    input.pushconstantShaderStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    input.pushconstantOffset           = sizeof(VkDeviceAddress);
    input.pushconstantSize             = sizeof(VkDeviceAddress);
    input.stream                       = config.interleaved ? 0 : numInputs;
    input.offset                       = config.interleaved ? offsetof(DrawSequence, pushMaterial) : 0;
    inputInfos.push_back(input);
    inputStrides.push_back(sizeof(VkDeviceAddress));
    numInputs++;
  }
  {
    VkIndirectCommandsLayoutTokenNV input = {VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV, 0,
                                             VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_NV};
    input.stream                          = config.interleaved ? 0 : numInputs;
    input.offset                          = config.interleaved ? offsetof(DrawSequence, drawIndexed) : 0;
    inputInfos.push_back(input);
    inputStrides.push_back(sizeof(VkDrawIndexedIndirectCommand));
    numInputs++;
  }

  uint32_t interleavedStride = sizeof(DrawSequence);

  VkIndirectCommandsLayoutCreateInfoNV genInfo = {VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_NV};
  genInfo.tokenCount                           = (uint32_t)inputInfos.size();
  genInfo.pTokens                              = inputInfos.data();
  genInfo.streamCount                          = config.interleaved ? 1 : numInputs;
  genInfo.pStreamStrides                       = config.interleaved ? &interleavedStride : inputStrides.data();

  if(config.permutated)
  {
    genInfo.flags |= VK_INDIRECT_COMMANDS_LAYOUT_USAGE_INDEXED_SEQUENCES_BIT_NV;
  }
  if(config.unordered)
  {
    genInfo.flags |= VK_INDIRECT_COMMANDS_LAYOUT_USAGE_UNORDERED_SEQUENCES_BIT_NV;
  }
  if(m_mode == MODE_PREPROCESS)
  {
    genInfo.flags |= VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_NV;
  }

  VkResult result;
  result = vkCreateIndirectCommandsLayoutNV(res->m_device, &genInfo, nullptr, &m_draw.indirectCmdsLayout);
  assert(result == VK_SUCCESS);
}

void RendererVKGenNV::initShaderGroupsPipeline()
{
  ResourcesVK* res = m_resources;

  VkGraphicsPipelineShaderGroupsCreateInfoNV groupsCreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_SHADER_GROUPS_CREATE_INFO_NV};

  VkPipelineShaderStageCreateInfo   shaderStages[2];
  VkGraphicsShaderGroupCreateInfoNV shaderGroups[1];

  res->m_gfxGen.clearShaders();
  res->m_gfxGen.addShader(res->m_drawShaderModules[m_config.bindingMode].vertexShaders[0], VK_SHADER_STAGE_VERTEX_BIT);
  res->m_gfxGen.addShader(res->m_drawShaderModules[m_config.bindingMode].fragmentShaders[0], VK_SHADER_STAGE_FRAGMENT_BIT);

  // first shadergroup must match
  {
    VkPipelineShaderStageCreateInfo& vstage = shaderStages[0];
    vstage                                  = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vstage.pName                            = "main";
    vstage.stage                            = VK_SHADER_STAGE_VERTEX_BIT;
    vstage.module                           = res->m_drawShaderModules[m_config.bindingMode].vertexShaders[0];

    VkPipelineShaderStageCreateInfo& fstage = shaderStages[1];
    fstage                                  = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fstage.pName                            = "main";
    fstage.stage                            = VK_SHADER_STAGE_FRAGMENT_BIT;
    fstage.module                           = res->m_drawShaderModules[m_config.bindingMode].fragmentShaders[0];

    VkGraphicsShaderGroupCreateInfoNV& group = shaderGroups[0];
    group                                    = {VK_STRUCTURE_TYPE_GRAPHICS_SHADER_GROUP_CREATE_INFO_NV};
    group.stageCount                         = 2;
    group.pStages                            = &shaderStages[0];
    group.pVertexInputState                  = res->m_gfxGen.createInfo.pVertexInputState;
    group.pTessellationState                 = res->m_gfxGen.createInfo.pTessellationState;
  }

  std::vector<VkPipeline> referencedPipelines;
  {
    // first group is already here, import the others
    for(uint32_t m = 1; m < m_config.maxShaders; m++)
    {
      referencedPipelines.push_back(res->m_drawShading.pipelines[m]);
    }
    groupsCreateInfo.pPipelines    = referencedPipelines.data();
    groupsCreateInfo.pipelineCount = (uint32_t)referencedPipelines.size();
  }

  groupsCreateInfo.groupCount = 1;
  groupsCreateInfo.pGroups    = shaderGroups;

  groupsCreateInfo.pNext         = res->m_gfxGen.createInfo.pNext;
  res->m_gfxGen.createInfo.pNext = &groupsCreateInfo;

  m_indirectPipeline = res->m_gfxGen.createPipeline();
}

void RendererVKGenNV::init(const CadScene* scene, ResourcesVK* resources, const Renderer::Config& config, Stats& stats)
{
  ResourcesVK* res = (ResourcesVK*)resources;
  m_resources      = res;
  m_scene          = scene;
  m_config         = config;

  stats.cmdBuffers = 1;

  m_indexingBits = scene->getIndexingBits();

  std::vector<DrawItem> drawItems;
  fillDrawItems(drawItems, scene, config, stats);

  res->initPipelinesOrShaders(m_config.bindingMode,
                              m_config.maxShaders > 1 ? VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_NV : 0, false);

  if(m_config.maxShaders > 1)
  {
    initShaderGroupsPipeline();
  }

  initIndirectCommandsLayout(config);
  if(config.interleaved)
  {
    setupInputInterleaved(drawItems.data(), drawItems.size(), stats);
  }
  else
  {
    setupInputSeparate(drawItems.data(), drawItems.size(), stats);
  }
  setupPreprocess(stats);
}

void RendererVKGenNV::deinit()
{
  deleteData();
  deinitIndirectCommandsLayout();
  vkDestroyPipeline(m_resources->m_device, m_indirectPipeline, nullptr);
}


VkGeneratedCommandsInfoNV RendererVKGenNV::getGeneratedCommandsInfo()
{
  ResourcesVK*              res  = m_resources;
  VkGeneratedCommandsInfoNV info = {VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV};
  info.pipeline                  = m_indirectPipeline ? m_indirectPipeline : res->m_drawShading.pipelines[0];
  info.pipelineBindPoint         = VK_PIPELINE_BIND_POINT_GRAPHICS;
  info.indirectCommandsLayout    = m_draw.indirectCmdsLayout;
  info.sequencesCount            = m_draw.sequencesCount;
  info.streamCount               = (uint32_t)m_draw.inputs.size();
  info.pStreams                  = m_draw.inputs.data();
  info.preprocessBuffer          = m_draw.preprocessBuffer.buffer;
  info.preprocessSize            = m_draw.preprocessSize;
  if(m_config.permutated)
  {
    info.sequencesIndexBuffer = m_draw.inputBuffer.buffer;
    info.sequencesIndexOffset = m_draw.inputSequenceIndexOffset;
  }

  return info;
}

void RendererVKGenNV::cmdExecute(VkCommandBuffer cmd, VkBool32 isPreprocessed)
{
  ResourcesVK*      res     = m_resources;
  const CadSceneVK& sceneVK = res->m_scene;

  res->cmdDynamicPipelineState(cmd);

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

  if(m_indirectPipeline)
  {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_indirectPipeline);
  }
  else
  {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawShading.pipelines[0]);
  }

  // The previously generated commands will be executed here.
  // The current state of the command buffer is inherited just like a usual work provoking command.
  VkGeneratedCommandsInfoNV info = getGeneratedCommandsInfo();
  vkCmdExecuteGeneratedCommandsNV(cmd, isPreprocessed, &info);
  // after this function the state is undefined, you must rebind PSO as well as other
  // state that could have been touched
}

void RendererVKGenNV::cmdPreprocess(VkCommandBuffer primary)
{
  // If we were regenerating commands into the same preprocessBuffer in the same frame
  // then we would have to insert a barrier that ensures rendering of the preprocesBuffer
  // had completed.
  // Similar applies, if were modifying the input buffers, appropriate barriers would have to
  // be set here.
  //
  // vkCmdPipelineBarrier(primary, whateverModifiedInputs, VK_PIPELINE_STAGE_COMMAND_PROCESS_BIT_NV,  ...);
  //  barrier.dstAccessMask = VK_ACCESS_COMMAND_PROCESS_READ_BIT_NV;
  //
  // It is not required in this sample, as the blitting synchronizes each frame, and we
  // do not actually modify the input tokens dynamically.
  //
  VkGeneratedCommandsInfoNV info = getGeneratedCommandsInfo();
  vkCmdPreprocessGeneratedCommandsNV(primary, &info);
}

void RendererVKGenNV::draw(const Resources::Global& global, Stats& stats)
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
        barrier.srcAccessMask   = VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV;
        barrier.dstAccessMask   = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        vkCmdPipelineBarrier(primary, VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
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
