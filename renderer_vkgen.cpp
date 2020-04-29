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
//#undef NDEBUG

#include <algorithm>
#include <assert.h>

#include "renderer.hpp"
#include "resources_vkgen.hpp"

#include <nvh/nvprint.hpp>
#include <nvmath/nvmath_glsltypes.h>

#include "common.h"


#if USE_SINGLE_GEOMETRY_BUFFERS
// can be 0 or 1
#define USE_PER_DRAW_VBO 1
#define USE_PER_DRAW_IBO 1
#else
// must be 1
#define USE_PER_DRAW_VBO 1
#define USE_PER_DRAW_IBO 1
#endif

namespace generatedcmds {

//////////////////////////////////////////////////////////////////////////

class RendererVKGen : public Renderer
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
    bool isAvailable(const nvvk::Context& context) const
    {
      return context.hasDeviceExtension(VK_NV_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME)
             && context.hasDeviceExtension(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)
             && load_VK_NV_device_generated_commands(context.m_instance, vkGetInstanceProcAddr, context.m_device, vkGetDeviceProcAddr);
    }
    const char* name() const { return "generated cmds"; }
    Renderer*   create() const
    {
      RendererVKGen* renderer = new RendererVKGen();
      renderer->m_mode        = MODE_DIRECT;

      return renderer;
    }
    unsigned int priority() const { return 30; }

    Resources* resources() { return ResourcesVKGen::get(); }
  };

  class TypeReuse : public Renderer::Type
  {
    bool isAvailable(const nvvk::Context& context) const
    {
      return context.hasDeviceExtension(VK_NV_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME)
             && context.hasDeviceExtension(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)
             && load_VK_NV_device_generated_commands(context.m_instance, vkGetInstanceProcAddr, context.m_device, vkGetDeviceProcAddr);
    }
    const char* name() const { return "preprocess,generated cmds"; }
    Renderer*   create() const
    {
      RendererVKGen* renderer = new RendererVKGen();
      renderer->m_mode        = MODE_PREPROCESS;

      return renderer;
    }
    unsigned int priority() const { return 30; }

    Resources* resources() { return ResourcesVKGen::get(); }
  };

public:
  void init(const CadScene* NV_RESTRICT scene, Resources* resources, const Renderer::Config& config, Stats& stats) override;
  void     deinit() override;
  void     draw(const Resources::Global& global, Stats& stats) override;
  uint32_t supportedBindingModes() override { return (1 << BINDINGMODE_PUSHADDRESS); };

  RendererVKGen() {}

private:
  struct DrawSequence
  {
    VkBindShaderGroupIndirectCommandNV  shader;
    VkDeviceAddress                     pushMatrix;
    VkDeviceAddress                     pushMaterial;
    uint32_t                            _pad;
    VkBindIndexBufferIndirectCommandNV  ibo;
    VkBindVertexBufferIndirectCommandNV vbo;
    VkDrawIndexedIndirectCommand        drawIndexed;
  };

  struct DrawSetup
  {
    std::vector<VkIndirectCommandsStreamNV> inputs;

    VkIndirectCommandsLayoutNV indirectCmdsLayout;

    VkBuffer inputBuffer;
    size_t   inputSequenceIndexOffset;

    VkBuffer     preprocessBuffer;
    VkDeviceSize preprocessSize;

    uint32_t sequencesCount;

    VkCommandBuffer cmdBuffer;

    size_t fboChangeID;
    size_t pipeChangeID;
  };

  ResourcesVKGen* NV_RESTRICT m_resources;
  VkCommandPool               m_cmdPool;
  nvvk::DeviceMemoryAllocator m_memoryAllocator;

  DrawSetup m_draw;

  VkGeneratedCommandsInfoNV getGeneratedCommandsInfo();

  void cmdPreprocess(VkCommandBuffer cmd);
  void cmdExecute(VkCommandBuffer cmd, VkBool32 isPreprocessed);

  void initGenerator(const Renderer::Config& config);
  void deinitGenerator() { vkDestroyIndirectCommandsLayoutNV(m_resources->m_device, m_draw.indirectCmdsLayout, NULL); }

  void setupInputInterleaved(const DrawItem* NV_RESTRICT drawItems, size_t drawCount, Stats& stats)
  {
    ResourcesVKGen*   res     = m_resources;
    const CadSceneVK& sceneVK = res->m_scene;

    m_draw.sequencesCount = drawCount;

    // compute input buffer space requirements
    VkPhysicalDeviceProperties2                         phyProps = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    VkPhysicalDeviceDeviceGeneratedCommandsPropertiesNV genProps = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_NV};
    phyProps.pNext                                               = &genProps;
    vkGetPhysicalDeviceProperties2(res->m_physical, &phyProps);

    size_t alignSeqIndexMask = genProps.minSequencesIndexBufferOffsetAlignment - 1;

    size_t totalSize      = ((sizeof(DrawSequence) * drawCount) + alignSeqIndexMask) & (~alignSeqIndexMask);
    size_t seqindexOffset = totalSize;

    if(m_config.permutated)
    {
      totalSize += sizeof(uint32_t) * drawCount;
    }

    // +32 in case num == 0
    totalSize += 32;

    // create input buffer
    nvvk::AllocationID aid;
    m_draw.inputBuffer = m_memoryAllocator.createBuffer(totalSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, aid);

    // setup staging buffer for filling
    nvvk::StagingMemoryManager staging(res->m_device, res->m_physical);
    nvvk::ScopeCommandBuffer   cmd(res->m_device, res->m_queueFamily, res->m_queue);

    uint8_t* mapping = staging.cmdToBufferT<uint8_t>(cmd, m_draw.inputBuffer, 0, totalSize);


    VkBufferDeviceAddressInfoEXT addressInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT};
    addressInfo.buffer                       = sceneVK.m_buffers.matrices;
    VkDeviceAddress matrixAddress            = vkGetBufferDeviceAddressEXT(res->m_device, &addressInfo);
    addressInfo.buffer                       = sceneVK.m_buffers.materials;
    VkDeviceAddress materialAddress          = vkGetBufferDeviceAddressEXT(res->m_device, &addressInfo);

    // fill sequence
    DrawSequence* sequences = (DrawSequence*)mapping;
    for(unsigned int i = 0; i < drawCount; i++)
    {
      const DrawItem&             di    = drawItems[i];
      const CadSceneVK::Geometry& vkgeo = sceneVK.m_geometry[di.geometryIndex];

      DrawSequence& seq = sequences[i];

      seq.shader.groupIndex = di.shaderIndex;

      addressInfo.buffer    = vkgeo.ibo.buffer;
      seq.ibo.bufferAddress = vkGetBufferDeviceAddressEXT(res->m_device, &addressInfo) + vkgeo.ibo.offset;
      seq.ibo.size          = vkgeo.ibo.range;
      seq.ibo.indexType     = VK_INDEX_TYPE_UINT32;

      addressInfo.buffer    = vkgeo.vbo.buffer;
      seq.vbo.bufferAddress = vkGetBufferDeviceAddressEXT(res->m_device, &addressInfo) + vkgeo.vbo.offset;
      seq.vbo.size          = vkgeo.vbo.range;
      seq.vbo.stride        = sizeof(CadScene::Vertex);

      seq.pushMatrix   = matrixAddress + sizeof(CadScene::MatrixNode) * di.matrixIndex;
      seq.pushMaterial = materialAddress + sizeof(CadScene::Material) * di.materialIndex;

      seq.drawIndexed.indexCount    = di.range.count;
      seq.drawIndexed.firstIndex    = uint32_t(di.range.offset / sizeof(uint32_t));
      seq.drawIndexed.instanceCount = 1;
      seq.drawIndexed.firstInstance = 0;
      seq.drawIndexed.vertexOffset  = 0;
#if !USE_PER_DRAW_IBO
      seq.drawIndexed.firstIndex += vkgeo.ibo.offset / sizeof(uint32_t);
#endif
#if !USE_PER_DRAW_VBO
      seq.drawIndexed.vertexOffset = vkgeo.vbo.offset / sizeof(CadScene::Vertex);
#endif
    }

    if(m_config.permutated)
    {
      m_draw.inputSequenceIndexOffset = seqindexOffset;

      // fill index permutation (random, worst-case)
      uint32_t* permutation = (uint32_t*)(mapping + seqindexOffset);
      fillRandomPermutation(drawCount, permutation, drawItems, stats);
    }

    // setup input stream
    VkIndirectCommandsStreamNV input;
    input.buffer = m_draw.inputBuffer;
    input.offset = 0;
    m_draw.inputs.push_back(input);
  }

  void setupInputSeparate(const DrawItem* NV_RESTRICT drawItems, size_t drawCount, Stats& stats)
  {
    ResourcesVKGen*   res   = m_resources;
    const CadSceneVK& scene = res->m_scene;

    m_draw.sequencesCount = drawCount;

    // compute input buffer space requirements
    VkPhysicalDeviceProperties2                         phyProps = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    VkPhysicalDeviceDeviceGeneratedCommandsPropertiesNV genProps = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_NV};
    phyProps.pNext                                               = &genProps;
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

    // +32 in case num == 0
    totalSize += 32;

    // create input buffer
    nvvk::AllocationID aid;
    m_draw.inputBuffer = m_memoryAllocator.createBuffer(totalSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, aid);

    // setup staging buffer for filling
    nvvk::StagingMemoryManager staging(res->m_device, res->m_physical);
    nvvk::ScopeCommandBuffer   cmd(res->m_device, res->m_queueFamily, res->m_queue);

    uint8_t* mapping = staging.cmdToBufferT<uint8_t>(cmd, m_draw.inputBuffer, 0, totalSize);

    VkBindShaderGroupIndirectCommandNV*  shaders       = (VkBindShaderGroupIndirectCommandNV*)(mapping + pipeOffset);
    VkBindVertexBufferIndirectCommandNV* vbos          = (VkBindVertexBufferIndirectCommandNV*)(mapping + vboOffset);
    VkBindIndexBufferIndirectCommandNV*  ibos          = (VkBindIndexBufferIndirectCommandNV*)(mapping + iboOffset);
    VkDeviceAddress*                     pushMatrices  = (VkDeviceAddress*)(mapping + matrixOffset);
    VkDeviceAddress*                     pushMaterials = (VkDeviceAddress*)(mapping + materialOffset);
    VkDrawIndexedIndirectCommand*        draws         = (VkDrawIndexedIndirectCommand*)(mapping + drawOffset);

    VkBufferDeviceAddressInfoEXT addressInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT};
    addressInfo.buffer                       = scene.m_buffers.matrices;
    VkDeviceAddress matrixAddress            = vkGetBufferDeviceAddressEXT(res->m_device, &addressInfo);
    addressInfo.buffer                       = scene.m_buffers.materials;
    VkDeviceAddress materialAddress          = vkGetBufferDeviceAddressEXT(res->m_device, &addressInfo);

    // let's record all token inputs for every drawcall
    for(unsigned int i = 0; i < drawCount; i++)
    {
      const DrawItem&             di  = drawItems[i];
      const CadSceneVK::Geometry& geo = scene.m_geometry[di.geometryIndex];

      shaders[i].groupIndex = di.shaderIndex;

      VkBindIndexBufferIndirectCommandNV& ibo = ibos[i];
      addressInfo.buffer                      = geo.ibo.buffer;
      ibo.bufferAddress = vkGetBufferDeviceAddressEXT(res->m_device, &addressInfo) + geo.ibo.offset;
      ibo.size          = geo.ibo.range;
      ibo.indexType     = VK_INDEX_TYPE_UINT32;

      VkBindVertexBufferIndirectCommandNV& vbo = vbos[i];
      addressInfo.buffer                       = geo.vbo.buffer;
      vbo.bufferAddress = vkGetBufferDeviceAddressEXT(res->m_device, &addressInfo) + geo.vbo.offset;
      vbo.size          = geo.vbo.range;
      vbo.stride        = sizeof(CadScene::Vertex);

      pushMatrices[i]  = matrixAddress + sizeof(CadScene::MatrixNode) * di.matrixIndex;
      pushMaterials[i] = materialAddress + sizeof(CadScene::Material) * di.materialIndex;

      VkDrawIndexedIndirectCommand& drawIndexed = draws[i];
      drawIndexed.indexCount                    = di.range.count;
      drawIndexed.firstIndex                    = uint32_t(di.range.offset / sizeof(uint32_t));
      drawIndexed.instanceCount                 = 1;
      drawIndexed.firstInstance                 = 0;
      drawIndexed.vertexOffset                  = 0;
#if !USE_PER_DRAW_IBO
      drawIndexed.firstIndex += geo.ibo.offset / sizeof(uint32_t);
#endif
#if !USE_PER_DRAW_VBO
      drawIndexed.vertexOffset = geo.vbo.offset / sizeof(CadScene::Vertex);
#endif
    }

    if(m_config.permutated)
    {
      m_draw.inputSequenceIndexOffset = seqindexOffset;

      // fill index permutation (random, worst-case)
      uint32_t* permutation = (uint32_t*)(mapping + seqindexOffset);
      fillRandomPermutation(drawCount, permutation, drawItems, stats);
    }

    // setup input streams
    VkIndirectCommandsStreamNV input;
    input.buffer = m_draw.inputBuffer;

    {
      input.offset = pipeOffset;
      m_draw.inputs.push_back(input);
    }

    if(USE_PER_DRAW_IBO)
    {
      input.offset = iboOffset;
      m_draw.inputs.push_back(input);
    }

    if(USE_PER_DRAW_VBO)
    {
      input.offset = vboOffset;
      m_draw.inputs.push_back(input);
    }
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
    ResourcesVKGen* res = m_resources;

    VkGeneratedCommandsMemoryRequirementsInfoNV memInfo = {VK_STRUCTURE_TYPE_GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_NV};
    VkPipelineBindPoint                         bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    memInfo.maxSequencesCount                             = m_draw.sequencesCount;
    memInfo.indirectCommandsLayout                        = m_draw.indirectCmdsLayout;
    memInfo.pipeline                                      = res->m_drawGroupsPipeline;
    memInfo.pipelineBindPoint                             = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkMemoryRequirements2 memReqs = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
    vkGetGeneratedCommandsMemoryRequirementsNV(res->m_device, &memInfo, &memReqs);

    m_draw.preprocessSize = memReqs.memoryRequirements.size;
    m_draw.preprocessBuffer = nvvk::createBuffer(res->m_device,nvvk::makeBufferCreateInfo(m_draw.preprocessSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT));

    // attempt device local first
    nvvk::AllocationID aid = m_memoryAllocator.alloc(memReqs.memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(!aid.isValid())
    {
      aid = m_memoryAllocator.alloc(memReqs.memoryRequirements, 0);
    }
    assert(aid.isValid());
    nvvk::Allocation allocation = m_memoryAllocator.getAllocation(aid);
    vkBindBufferMemory(res->m_device, m_draw.preprocessBuffer, allocation.mem, allocation.offset);

    stats.preprocessSizeKB = (m_draw.preprocessSize + 1023) / 1024;
  }

  void deleteData()
  {
    vkDestroyBuffer(m_resources->m_device, m_draw.inputBuffer, NULL);
    vkDestroyBuffer(m_resources->m_device, m_draw.preprocessBuffer, NULL);
    m_memoryAllocator.freeAll();
    m_memoryAllocator.deinit();
  }
};


static RendererVKGen::TypeDirect s_type_cmdbuffergen_vk;
static RendererVKGen::TypeReuse  s_type_cmdbuffergen2_vk;

void RendererVKGen::initGenerator(const Renderer::Config& config)
{
  ResourcesVKGen* res = m_resources;

  std::vector<VkIndirectCommandsLayoutTokenNV> inputInfos;
  std::vector<uint32_t>                        inputStrides;

  uint32_t numInputs = 0;

  {
    VkIndirectCommandsLayoutTokenNV input = {VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV, 0,
                                             VK_INDIRECT_COMMANDS_TOKEN_TYPE_SHADER_GROUP_NV};
    input.stream                          = config.interleaved ? 0 : numInputs;
    input.offset                          = config.interleaved ? offsetof(DrawSequence, shader) : 0;
    inputInfos.push_back(input);
    inputStrides.push_back(sizeof(VkBindShaderGroupIndirectCommandNV));
    numInputs++;
  }
  if(USE_PER_DRAW_IBO)
  {
    VkIndirectCommandsLayoutTokenNV input = {VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV, 0,
                                             VK_INDIRECT_COMMANDS_TOKEN_TYPE_INDEX_BUFFER_NV};
    input.stream                          = config.interleaved ? 0 : numInputs;
    input.offset                          = config.interleaved ? offsetof(DrawSequence, ibo) : 0;
    inputInfos.push_back(input);
    inputStrides.push_back(sizeof(VkBindIndexBufferIndirectCommandNV));
    numInputs++;
  }
  if(USE_PER_DRAW_VBO)
  {
    VkIndirectCommandsLayoutTokenNV input = {VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV, 0,
                                             VK_INDIRECT_COMMANDS_TOKEN_TYPE_VERTEX_BUFFER_NV};
    input.vertexBindingUnit               = 0;
    input.vertexDynamicStride             = VK_FALSE;
    input.stream                          = config.interleaved ? 0 : numInputs;
    input.offset                          = config.interleaved ? offsetof(DrawSequence, vbo) : 0;
    inputInfos.push_back(input);
    inputStrides.push_back(sizeof(VkBindVertexBufferIndirectCommandNV));
    numInputs++;
  }
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
  result = vkCreateIndirectCommandsLayoutNV(res->m_device, &genInfo, NULL, &m_draw.indirectCmdsLayout);
  assert(result == VK_SUCCESS);
}

void RendererVKGen::init(const CadScene* NV_RESTRICT scene, Resources* resources, const Renderer::Config& config, Stats& stats)
{
  ResourcesVKGen* res = (ResourcesVKGen*)resources;
  m_resources         = res;
  m_scene             = scene;
  m_config            = config;

  stats.cmdBuffers = 1;

  m_memoryAllocator.init(res->m_device, res->m_physical);

  std::vector<DrawItem> drawItems;

  fillDrawItems(drawItems, scene, config, stats);

  initGenerator(config);
  if(config.interleaved)
  {
    setupInputInterleaved(drawItems.data(), drawItems.size(), stats);
  }
  else
  {
    setupInputSeparate(drawItems.data(), drawItems.size(), stats);
  }
  setupPreprocess(stats);

  {
    VkResult                result;
    VkCommandPoolCreateInfo cmdPoolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cmdPoolInfo.queueFamilyIndex        = 0;
    result                              = vkCreateCommandPool(res->m_device, &cmdPoolInfo, NULL, &m_cmdPool);
    assert(result == VK_SUCCESS);
  }
  if(m_mode == MODE_PREPROCESS)
  {
    m_draw.cmdBuffer = res->createCmdBuffer(m_cmdPool, false, false, false);
    cmdExecute(m_draw.cmdBuffer, VK_TRUE);
    vkEndCommandBuffer(m_draw.cmdBuffer);
  }
}

void RendererVKGen::deinit()
{
  if(m_mode == MODE_PREPROCESS)
  {
    vkFreeCommandBuffers(m_resources->m_device, m_cmdPool, 1, &m_draw.cmdBuffer);
  }
  vkDestroyCommandPool(m_resources->m_device, m_cmdPool, NULL);

  deleteData();
  deinitGenerator();
}


VkGeneratedCommandsInfoNV RendererVKGen::getGeneratedCommandsInfo()
{
  ResourcesVKGen*           res  = m_resources;
  VkGeneratedCommandsInfoNV info = {VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV};
  info.pipeline                  = res->m_drawGroupsPipeline;
  info.pipelineBindPoint         = VK_PIPELINE_BIND_POINT_GRAPHICS;
  info.indirectCommandsLayout    = m_draw.indirectCmdsLayout;
  info.sequencesCount            = m_draw.sequencesCount;
  info.streamCount               = (uint32_t)m_draw.inputs.size();
  info.pStreams                  = m_draw.inputs.data();
  info.preprocessBuffer          = m_draw.preprocessBuffer;
  info.preprocessSize            = m_draw.preprocessSize;
  if(m_config.permutated)
  {
    info.sequencesIndexBuffer = m_draw.inputBuffer;
    info.sequencesIndexOffset = m_draw.inputSequenceIndexOffset;
  }

  return info;
}

void RendererVKGen::cmdExecute(VkCommandBuffer cmd, VkBool32 isPreprocessed)
{
  ResourcesVKGen*   res     = m_resources;
  const CadSceneVK& sceneVK = res->m_scene;

  res->cmdDynamicState(cmd);

  if(isPreprocessed)
  {
    // we need to ensure the preprocessing of commands has completed, before we can execute them
    VkMemoryBarrier barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask   = VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV;
    barrier.dstAccessMask   = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 1,
                         &barrier, 0, NULL, 0, NULL);
  }

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawPush.getPipeLayout(), 0, 1,
                          res->m_drawPush.getSets(), 0, NULL);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawGroupsPipeline);

#if USE_SINGLE_GEOMETRY_BUFFERS
  if(!USE_PER_DRAW_IBO)
  {
    vkCmdBindIndexBuffer(cmd, sceneVK.m_geometryMem.getChunk(0).ibo, 0, VK_INDEX_TYPE_UINT32);
  }
  if(!USE_PER_DRAW_VBO)
  {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &sceneVK.m_geometryMem.getChunk(0).vbo, &offset);
  }
#endif

  // The previously generated commands will be executed here.
  // The current state of the command buffer is inherited just like a usual work provoking command.
  VkGeneratedCommandsInfoNV info = getGeneratedCommandsInfo();
  vkCmdExecuteGeneratedCommandsNV(cmd, isPreprocessed, &info);
  // after this function the state is undefined, you must rebind PSO as well as other
  // state that could have been touched
}

void RendererVKGen::cmdPreprocess(VkCommandBuffer primary)
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

void RendererVKGen::draw(const Resources::Global& global, Stats& stats)
{
  const CadScene* NV_RESTRICT scene = m_scene;
  ResourcesVKGen* NV_RESTRICT res   = m_resources;


  if(m_mode == MODE_PREPROCESS)
  {
    if(m_draw.pipeChangeID != res->m_pipeChangeID || m_draw.fboChangeID != res->m_fboChangeID)
    {
      vkResetCommandBuffer(m_draw.cmdBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
      res->cmdBegin(m_draw.cmdBuffer, false, false, false);
      cmdExecute(m_draw.cmdBuffer, VK_TRUE);
      vkEndCommandBuffer(m_draw.cmdBuffer);
    }

    m_draw.fboChangeID  = res->m_fboChangeID;
    m_draw.pipeChangeID = res->m_pipeChangeID;
  }

  // generic state setup
  VkCommandBuffer primary = res->createTempCmdBuffer();

  {
    nvvk::ProfilerVK::Section profile(res->m_profilerVK, "Render", primary);

    if(m_mode != MODE_DIRECT)
    {
      nvvk::ProfilerVK::Section profile(res->m_profilerVK, "Pre", primary);
      cmdPreprocess(primary);
    }
    {
      nvvk::ProfilerVK::Section profile(res->m_profilerVK, "Draw", primary);
      vkCmdUpdateBuffer(primary, res->m_common.viewBuffer, 0, sizeof(SceneData), (const uint32_t*)&global.sceneUbo);
      res->cmdPipelineBarrier(primary);

      // clear via pass
      res->cmdBeginRenderPass(primary, true, true);
      if(m_mode == MODE_DIRECT)
      {
        cmdExecute(primary, VK_FALSE);
      }
      else
      {
        vkCmdExecuteCommands(primary, 1, &m_draw.cmdBuffer);
      }
      vkCmdEndRenderPass(primary);
    }
  }

  vkEndCommandBuffer(primary);
  res->submissionEnqueue(primary);
}

}  // namespace generatedcmds
