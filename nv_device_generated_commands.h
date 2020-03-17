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

#pragma once

#include <vulkan/vulkan_core.h>

#ifndef VK_NV_device_generated_commands
#define VK_NV_device_generated_commands 1
#define VK_NV_device_generated_commands_custom 1

VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkIndirectCommandsLayoutNV)

#define VK_NV_DEVICE_GENERATED_COMMANDS_SPEC_VERSION 3
#define VK_NV_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME "VK_NV_device_generated_commands"

#define VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV                          ((VkObjectType)1000277000)

#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_NV  ((VkStructureType)1000277000)
#define VK_STRUCTURE_TYPE_GRAPHICS_SHADER_GROUP_CREATE_INFO_NV              ((VkStructureType)1000277001)
#define VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_SHADER_GROUPS_CREATE_INFO_NV    ((VkStructureType)1000277002)
#define VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV                 ((VkStructureType)1000277003)
#define VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_NV           ((VkStructureType)1000277004)
#define VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV                        ((VkStructureType)1000277005)
#define VK_STRUCTURE_TYPE_GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_NV    ((VkStructureType)1000277006)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_NV  ((VkStructureType)1000277007)

#define VK_PIPELINE_CREATE_INDIRECT_BINDABLE_BIT_NV                         ((VkPipelineCreateFlagBits)0x00040000)

#define VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV                            ((VkAccessFlagBits)0x00020000)
#define VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV                           ((VkAccessFlagBits)0x00040000)

#define VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV                         ((VkPipelineStageFlagBits)0x00020000)

//////////////////////////////////////////////////////////////////////////

typedef struct VkPhysicalDeviceDeviceGeneratedCommandsFeaturesNV
{
  VkStructureType sType;
  const void*     pNext;

  VkBool32        deviceGeneratedCommands;

} VkPhysicalDeviceDeviceGeneratedCommandsFeaturesNV;

typedef struct VkPhysicalDeviceDeviceGeneratedCommandsPropertiesNV
{
  VkStructureType sType;
  const void*     pNext;

  uint32_t        maxGraphicsShaderGroupCount;

  uint32_t        maxIndirectSequenceCount;
  uint32_t        maxIndirectCommandsTokenCount;
  uint32_t        maxIndirectCommandsStreamCount;

  uint32_t        maxIndirectCommandsTokenOffset;
  uint32_t        maxIndirectCommandsStreamStride;

  uint32_t        minSequencesCountBufferOffsetAlignment;
  uint32_t        minSequencesIndexBufferOffsetAlignment;
  uint32_t        minIndirectCommandsBufferOffsetAlignment;
} VkPhysicalDeviceDeviceGeneratedCommandsPropertiesNV;

//////////////////////////////////////////////////////////////////////////

typedef struct VkGraphicsShaderGroupCreateInfoNV
{
  // A shadergroup, is a set of unique shader combinations (VS,FS,...) etc.
  // that all are stored within a single graphics pipeline that share
  // most of the state.
  // Must not mix mesh with traditional pipeline.
  VkStructureType sType;
  const void*     pNext;

  // overrides createInfo from original graphicsPipeline
  uint32_t                                          stageCount;
  const VkPipelineShaderStageCreateInfo*            pStages;
  const VkPipelineVertexInputStateCreateInfo*       pVertexInputState;
  const VkPipelineTessellationStateCreateInfo*      pTessellationState;
} VkGraphicsShaderGroupCreateInfoNV;

typedef struct VkGraphicsPipelineShaderGroupsCreateInfoNV
{
  // extend regular VkGraphicsPipelineCreateInfo
  // If bound via vkCmdBindPipeline will behave as if pGroups[0] is active,
  // otherwise bind using vkCmdBindPipelineShaderGroup with proper index
  VkStructureType                          sType;
  const void*                              pNext;

  uint32_t                                 groupCount;
  const VkGraphicsShaderGroupCreateInfoNV* pGroups;
  
  uint32_t                                 pipelineCount;
  const VkPipeline*                        pPipelines;
} VkGraphicsPipelineShaderGroupsCreateInfoNV;


//////////////////////////////////////////////////////////////////////////

typedef struct VkBindShaderGroupIndirectCommandNV
{
  uint32_t        groupIndex;
} VkBindShaderGroupIndirectCommandNV;

typedef struct VkBindIndexBufferIndirectCommandNV
{
  VkDeviceAddress bufferAddress;
  uint32_t        size;
  VkIndexType     indexType;
} VkBindIndexBufferIndirectCommandNV;

typedef struct VkBindVertexBufferIndirectCommandNV
{
  VkDeviceAddress bufferAddress;
  uint32_t        size;
  uint32_t        stride;
} VkBindVertexBufferIndirectCommandNV;

typedef struct VkSetStateFlagsIndirectCommandNV
{
  uint32_t        data;
} VkSetStateFlagsIndirectCommandNV;

typedef enum VkIndirectStateFlagBitsNV
{
  VK_INDIRECT_STATE_FLAG_FRONTFACE_BIT_NV = 0x00000001,  // CCW if not set
  VK_INDIRECT_STATE_FLAG_BITS_NV = 0x7FFFFFFF
} VkIndirectStateFlagBitsNV;
typedef VkFlags VkIndirectStateFlagsNV;

//////////////////////////////////////////////////////////////////////////

typedef enum VkIndirectCommandsTokenTypeNV
{
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_SHADER_GROUP_NV  = 0,  // VkBindShaderGroupIndirectCommandNV (if used must be first token)
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_STATE_FLAGS_NV   = 1,  // VkSetStateFlagsIndirectCommandNV
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_INDEX_BUFFER_NV  = 2,  // VkBindIndexBufferIndirectCommandNV
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_VERTEX_BUFFER_NV = 3,  // VkBindVertexBufferIndirectCommandNV
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_NV = 4,  // u32[] raw data
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_NV  = 5,  // VkDrawIndexedIndirectCommand
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_NV          = 6,  // VkDrawIndirectCommand
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_TASKS_NV    = 7,  // VkDrawMeshTasksIndirectCommandNV
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_BEGIN_RANGE_NV = VK_INDIRECT_COMMANDS_TOKEN_TYPE_SHADER_GROUP_NV,
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_END_RANGE_NV   = VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_TASKS_NV,
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_RANGE_SIZE_NV =
      (VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_TASKS_NV - VK_INDIRECT_COMMANDS_TOKEN_TYPE_SHADER_GROUP_NV + 1),
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_MAX_ENUM_NV = 0x7FFFFFFF
} VkIndirectCommandsTokenTypeNV;

typedef enum VkIndirectCommandsLayoutUsageFlagBitsNV
{
  VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_NV  = 0x00000001, // vkCmdPreprocessGeneratedCommands and isPreprocessed is used during execution
  VK_INDIRECT_COMMANDS_LAYOUT_USAGE_INDEXED_SEQUENCES_BIT_NV    = 0x00000002, // sequenceIndexBuffer is used during processing
  VK_INDIRECT_COMMANDS_LAYOUT_USAGE_UNORDERED_SEQUENCES_BIT_NV  = 0x00000004, // implementation can re-order drawcalls
  VK_INDIRECT_COMMANDS_LAYOUT_USAGE_FLAG_BITS_MAX_ENUM_NV       = 0x7FFFFFFF
} VkIndirectCommandsLayoutUsageFlagBitsNV;
typedef VkFlags VkIndirectCommandsLayoutUsageFlagsNV;

typedef struct VkIndirectCommandsStreamNV
{
  VkBuffer                        buffer;
  VkDeviceSize                    offset;
} VkIndirectCommandsStreamNV;

typedef struct VkIndirectCommandsLayoutTokenNV
{
  VkStructureType                 sType;
  const void*                     pNext;

  VkIndirectCommandsTokenTypeNV   tokenType;

  // sequenceIndex = sequenceIndexBuffer ? sequenceIndexBuffer[sequenceIndex] : sequenceIndex;
  // tokenData = &tokenStream[stream].bytes[ (sequenceIndex) * tokenStream[stream].stride + localOffset]
  uint32_t                        stream;
  uint32_t                        offset;  // each token must be "naturally" aligned (4,8 or 16)
  
  // for VK_INDIRECT_COMMANDS_TOKEN_TYPE_VERTEX_BUFFER_NV
  uint32_t                        vertexBindingUnit;
  VkBool32                        vertexDynamicStride;
  // for VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_NV
  VkPipelineLayout                pushconstantPipelineLayout;
  VkShaderStageFlags              pushconstantShaderStageFlags;
  uint32_t                        pushconstantOffset;
  uint32_t                        pushconstantSize;
  // for VK_INDIRECT_COMMANDS_TOKEN_TYPE_STATE_FLAGS_NV
  VkIndirectStateFlagsNV          indirectStateFlags;
  // for VK_INDIRECT_COMMANDS_TOKEN_TYPE_INDEX_BUFFER_NV
  uint32_t                        indexTypeCount; // optional, allows to
  const VkIndexType*              pIndexTypes;    // remap DX enum values
  const uint32_t*                 pIndexTypeValues;

} VkIndirectCommandsLayoutTokenNV;

typedef struct VkIndirectCommandsLayoutCreateInfoNV
{
  VkStructureType                        sType;
  const void*                            pNext;

  VkIndirectCommandsLayoutUsageFlagsNV   flags;

  VkPipelineBindPoint                    pipelineBindPoint; // must be graphics for now
  
  uint32_t                               tokenCount;
  const VkIndirectCommandsLayoutTokenNV* pTokens;

  uint32_t                               streamCount;
  const uint32_t*                        pStreamStrides;
} VkIndirectCommandsLayoutCreateInfoNV;

//////////////////////////////////////////////////////////////////////////

typedef struct VkGeneratedCommandsInfoNV
{
  VkStructureType                  sType;
  const void*                      pNext;

  VkPipelineBindPoint              pipelineBindPoint; // must be graphics for now
  VkPipeline                       pipeline;          // must match bound pipeline at vkCmdExecuteGeneratedCommandsNV time

  VkIndirectCommandsLayoutNV       indirectCommandsLayout;

  uint32_t                         streamCount;
  const VkIndirectCommandsStreamNV* pStreams;

  uint32_t                         sequencesCount;  // used sequencesCount or maximum
    
  VkBuffer                         preprocessBuffer; // mandatory
  VkDeviceSize                     preprocessOffset;
  VkDeviceSize                     preprocessSize;   // use vkGetGeneratedCommandsMemoryRequirementsNV for sizing

  VkBuffer                         sequencesCountBuffer; // optional (gpu provided count <= sequencesCount)
  VkDeviceSize                     sequencesCountOffset; //    must use VK_INDIRECT_COMMANDS_LAYOUT_USAGE_BUFFERED_COUNT_BIT_NV
  VkBuffer                         sequencesIndexBuffer; // optional (re-order/subset sequences)
  VkDeviceSize                     sequencesIndexOffset; //    must use VK_INDIRECT_COMMANDS_LAYOUT_USAGE_INDEXED_SEQUENCES_BIT_NV
} VkGeneratedCommandsInfoNV;

typedef struct VkGeneratedCommandsMemoryRequirementsInfoNV
{
  VkStructureType                     sType;
  const void*                         pNext;

  VkPipelineBindPoint                 pipelineBindPoint;
  VkPipeline                          pipeline;
  
  VkIndirectCommandsLayoutNV          indirectCommandsLayout;

  uint32_t                            maxSequencesCount;
} VkGeneratedCommandsMemoryRequirementsInfoNV;

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////

// Some implementations may need to use scratch memory for internal preprocessing of generated commands.
// If you want to have a fixed memory limit, try calling the function with changing "maxSequenceCount"
// until the memory size is close to the desired value.
// You can recycle this memory with different execution/preprocessing operations, but then must synchronize using 
// barriers and ACCESS_COMMAND_PROCESS_WRITE/READ_BIT

typedef void(VKAPI_PTR* PFN_vkGetGeneratedCommandsMemoryRequirementsNV)(VkDevice device,
                                                                        const VkGeneratedCommandsMemoryRequirementsInfoNV* pInfo,
                                                                        VkMemoryRequirements2* pMemoryRequirements);

// Preprocessing is an optional operation that allows to speedup the execution step.
// It can be done on async compute queue and its results can be re-used.
// If preprocessing is used, then the contents of pGenerateCommandsInfo as
// well as the data it references must match at preprocess and execution time.
// must use VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_NV
typedef void(VKAPI_PTR* PFN_vkCmdPreprocessGeneratedCommandsNV)(VkCommandBuffer                  commandBuffer,
                                                                const VkGeneratedCommandsInfoNV* pGenerateCommandsInfo);

// If isPreprocessed is true then above function must have been called with identical content prior this operation on
// the device's timeline.
typedef void(VKAPI_PTR* PFN_vkCmdExecuteGeneratedCommandsNV)(VkCommandBuffer commandBuffer,
                                                             VkBool32        isPreprocessed,  // optional, must use VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_NV
                                                             const VkGeneratedCommandsInfoNV* pGenerateCommandsInfo);

// allows shadergroup binding as regular commandbuffer operation
typedef void(VKAPI_PTR* PFN_vkCmdBindPipelineShaderGroupNV)(VkCommandBuffer     commandBuffer,
                                                            VkPipelineBindPoint pipelineBindPoint,
                                                            VkPipeline          pipeline,
                                                            uint32_t            groupIndex);

typedef VkResult(VKAPI_PTR* PFN_vkCreateIndirectCommandsLayoutNV)(VkDevice                                    device,
                                                                  const VkIndirectCommandsLayoutCreateInfoNV* pCreateInfo,
                                                                  const VkAllocationCallbacks* pAllocator,
                                                                  VkIndirectCommandsLayoutNV*  pIndirectCommandsLayout);

typedef void(VKAPI_PTR* PFN_vkDestroyIndirectCommandsLayoutNV)(VkDevice                     device,
                                                               VkIndirectCommandsLayoutNV   indirectCommandsLayout,
                                                               const VkAllocationCallbacks* pAllocator);


#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR void VKAPI_CALL vkGetGeneratedCommandsMemoryRequirementsNV(VkDevice device,
                                                                      const VkGeneratedCommandsMemoryRequirementsInfoNV* pInfo,
                                                                      VkMemoryRequirements2* pMemoryRequirements);

VKAPI_ATTR void VKAPI_CALL vkCmdPreprocessGeneratedCommandsNV(VkCommandBuffer                  commandBuffer,
                                                              const VkGeneratedCommandsInfoNV* pGenerateCommandsInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdExecuteGeneratedCommandsNV(VkCommandBuffer                  commandBuffer,
                                                           VkBool32                         isPreprocessed,
                                                           const VkGeneratedCommandsInfoNV* pGenerateCommandsInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdBindPipelineShaderGroupNV(VkCommandBuffer     commandBuffer,
                                                          VkPipelineBindPoint pipelineBindPoint,
                                                          VkPipeline          pipeline,
                                                          uint32_t            groupIndex);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateIndirectCommandsLayoutNV(VkDevice                                    device,
                                                                const VkIndirectCommandsLayoutCreateInfoNV* pCreateInfo,
                                                                const VkAllocationCallbacks*                pAllocator,
                                                                VkIndirectCommandsLayoutNV* pIndirectCommandsLayout);

VKAPI_ATTR void VKAPI_CALL vkDestroyIndirectCommandsLayoutNV(VkDevice                     device,
                                                             VkIndirectCommandsLayoutNV   indirectCommandsLayout,
                                                             const VkAllocationCallbacks* pAllocator);

#endif
#endif

int load_VK_NV_device_generated_commands(VkInstance instance, PFN_vkGetInstanceProcAddr getInstanceProcAddr, VkDevice device, PFN_vkGetDeviceProcAddr getDeviceProcAddr);

