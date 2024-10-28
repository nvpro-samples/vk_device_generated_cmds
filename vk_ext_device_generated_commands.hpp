/*
* Copyright (c) 2024, NVIDIA CORPORATION.  All rights reserved.
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
* SPDX-FileCopyrightText: Copyright (c) 2024, NVIDIA CORPORATION.
* SPDX-License-Identifier: Apache-2.0
*/

#include <vulkan/vulkan_core.h>

#ifndef VK_EXT_device_generated_commands
#define VK_EXT_device_generated_commands 1
#define VK_EXT_DEVICE_GENERATED_COMMANDS_SPEC_VERSION 1
#define VK_EXT_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME "VK_EXT_device_generated_commands"
#define VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT ((VkShaderCreateFlagBitsEXT)0x00000080)
#define VK_BUFFER_USAGE_2_PREPROCESS_BUFFER_BIT_EXT ((VkBufferUsageFlagBits2KHR)0x0000000080000000ULL)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_EXT ((VkStructureType)1000572000)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_EXT ((VkStructureType)1000572001)
#define VK_STRUCTURE_TYPE_GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_EXT ((VkStructureType)1000572002)
#define VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_CREATE_INFO_EXT ((VkStructureType)1000572003)
#define VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_EXT ((VkStructureType)1000572004)
#define VK_STRUCTURE_TYPE_WRITE_INDIRECT_EXECUTION_SET_EXT ((VkStructureType)1000572005)
#define VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_EXT ((VkStructureType)1000572006)
#define VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_EXT ((VkStructureType)1000572007)
#define VK_STRUCTURE_TYPE_WRITE_INDIRECT_EXECUTION_SET_PIPELINE_EXT ((VkStructureType)1000572008)
#define VK_STRUCTURE_TYPE_WRITE_INDIRECT_EXECUTION_SET_SHADER_EXT ((VkStructureType)1000572009)
#define VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_PIPELINE_INFO_EXT ((VkStructureType)1000572010)
#define VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_SHADER_INFO_EXT ((VkStructureType)1000572011)
#define VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_SHADER_LAYOUT_INFO_EXT ((VkStructureType)1000572012)
#define VK_STRUCTURE_TYPE_GENERATED_COMMANDS_PIPELINE_INFO_EXT ((VkStructureType)1000572013)
#define VK_STRUCTURE_TYPE_GENERATED_COMMANDS_SHADER_INFO_EXT ((VkStructureType)1000572014)
#define VK_PIPELINE_STAGE_2_COMMAND_PREPROCESS_BIT_EXT VK_PIPELINE_STAGE_2_COMMAND_PREPROCESS_BIT_NV
#define VK_ACCESS_2_COMMAND_PREPROCESS_READ_BIT_EXT VK_ACCESS_2_COMMAND_PREPROCESS_READ_BIT_NV
#define VK_ACCESS_2_COMMAND_PREPROCESS_WRITE_BIT_EXT VK_ACCESS_2_COMMAND_PREPROCESS_WRITE_BIT_NV
#define VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_EXT ((VkIndirectCommandsTokenTypeEXT)1000328000)
#define VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_COUNT_EXT ((VkIndirectCommandsTokenTypeEXT)1000328001)
#define VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_NV_EXT ((VkIndirectCommandsTokenTypeEXT)1000202002)
#define VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_COUNT_NV_EXT ((VkIndirectCommandsTokenTypeEXT)1000202003)
#define VK_INDIRECT_COMMANDS_TOKEN_TYPE_TRACE_RAYS2_EXT ((VkIndirectCommandsTokenTypeEXT)1000386004)
#define VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_EXT ((VkObjectType)1000572000)
#define VK_OBJECT_TYPE_INDIRECT_EXECUTION_SET_EXT ((VkObjectType)1000572001)
#define VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT ((VkPipelineCreateFlagBits2KHR)0x0000004000000000ULL)
#define VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_EXT VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV
#define VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_EXT VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV
#define VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_EXT VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV

typedef struct VkPhysicalDeviceDeviceGeneratedCommandsFeaturesEXT
{
  VkStructureType sType;
  void*           pNext;
  VkBool32        deviceGeneratedCommandsEXT;
  VkBool32        dynamicGeneratedPipelineLayout;
} VkPhysicalDeviceDeviceGeneratedCommandsFeaturesEXT;

typedef VkFlags VkIndirectCommandsInputModeFlagsEXT;

typedef struct VkPhysicalDeviceDeviceGeneratedCommandsPropertiesEXT
{
  VkStructureType                     sType;
  void*                               pNext;
  uint32_t                            maxIndirectPipelineCount;
  uint32_t                            maxIndirectShaderObjectCount;
  uint32_t                            maxIndirectSequenceCount;
  uint32_t                            maxIndirectCommandsTokenCount;
  uint32_t                            maxIndirectCommandsTokenOffset;
  uint32_t                            maxIndirectCommandsIndirectStride;
  VkIndirectCommandsInputModeFlagsEXT supportedIndirectCommandsInputModes;
  VkShaderStageFlags                  supportedIndirectCommandsShaderStages;
  VkShaderStageFlags                  supportedIndirectCommandsShaderStagesPipelineBinding;
  VkShaderStageFlags                  supportedIndirectCommandsShaderStagesShaderBinding;
  VkBool32                            deviceGeneratedCommandsTransformFeedback;
  VkBool32                            deviceGeneratedCommandsMultiDrawIndirectCount;
} VkPhysicalDeviceDeviceGeneratedCommandsPropertiesEXT;

VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkIndirectCommandsLayoutEXT)

VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkIndirectExecutionSetEXT)

typedef struct VkGeneratedCommandsMemoryRequirementsInfoEXT
{
  VkStructureType             sType;
  void*                       pNext;
  VkIndirectExecutionSetEXT   indirectExecutionSet;
  VkIndirectCommandsLayoutEXT indirectCommandsLayout;
  uint32_t                    maxSequenceCount;
  uint32_t                    maxDrawCount;
} VkGeneratedCommandsMemoryRequirementsInfoEXT;

typedef struct VkIndirectExecutionSetPipelineInfoEXT
{
  VkStructureType sType;
  void const*     pNext;
  VkPipeline      initialPipeline;
  uint32_t        maxPipelineCount;
} VkIndirectExecutionSetPipelineInfoEXT;

typedef struct VkIndirectExecutionSetShaderLayoutInfoEXT
{
  VkStructureType              sType;
  void const*                  pNext;
  uint32_t                     setLayoutCount;
  VkDescriptorSetLayout const* pSetLayouts;
} VkIndirectExecutionSetShaderLayoutInfoEXT;

typedef struct VkIndirectExecutionSetShaderInfoEXT
{
  VkStructureType                                  sType;
  void const*                                      pNext;
  uint32_t                                         shaderCount;
  VkShaderEXT const*                               pInitialShaders;
  VkIndirectExecutionSetShaderLayoutInfoEXT const* pSetLayoutInfos;
  uint32_t                                         maxShaderCount;
  uint32_t                                         pushConstantRangeCount;
  VkPushConstantRange const*                       pPushConstantRanges;
} VkIndirectExecutionSetShaderInfoEXT;

typedef union VkIndirectExecutionSetInfoEXT
{
  VkIndirectExecutionSetPipelineInfoEXT const* pPipelineInfo;
  VkIndirectExecutionSetShaderInfoEXT const*   pShaderInfo;
} VkIndirectExecutionSetInfoEXT;

typedef enum VkIndirectExecutionSetInfoTypeEXT
{
  VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT      = 0,
  VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT = 1,
  VK_INDIRECT_EXECUTION_SET_INFO_TYPE_MAX_ENUM_EXT       = 0x7FFFFFFF
} VkIndirectExecutionSetInfoTypeEXT;

typedef struct VkIndirectExecutionSetCreateInfoEXT
{
  VkStructureType                   sType;
  void const*                       pNext;
  VkIndirectExecutionSetInfoTypeEXT type;
  VkIndirectExecutionSetInfoEXT     info;
} VkIndirectExecutionSetCreateInfoEXT;

typedef struct VkGeneratedCommandsInfoEXT
{
  VkStructureType             sType;
  void const*                 pNext;
  VkShaderStageFlags          shaderStages;
  VkIndirectExecutionSetEXT   indirectExecutionSet;
  VkIndirectCommandsLayoutEXT indirectCommandsLayout;
  VkDeviceAddress             indirectAddress;
  VkDeviceSize                indirectAddressSize;
  VkDeviceAddress             preprocessAddress;
  VkDeviceSize                preprocessSize;
  uint32_t                    maxSequenceCount;
  VkDeviceAddress             sequenceCountAddress;
  uint32_t                    maxDrawCount;
} VkGeneratedCommandsInfoEXT;

typedef struct VkWriteIndirectExecutionSetPipelineEXT
{
  VkStructureType sType;
  void const*     pNext;
  uint32_t        index;
  VkPipeline      pipeline;
} VkWriteIndirectExecutionSetPipelineEXT;

typedef struct VkWriteIndirectExecutionSetShaderEXT
{
  VkStructureType sType;
  void const*     pNext;
  uint32_t        index;
  VkShaderEXT     shader;
} VkWriteIndirectExecutionSetShaderEXT;

typedef struct VkIndirectCommandsVertexBufferTokenEXT
{
  uint32_t vertexBindingUnit;
} VkIndirectCommandsVertexBufferTokenEXT;

typedef enum VkIndirectCommandsInputModeFlagBitsEXT
{
  VK_INDIRECT_COMMANDS_INPUT_MODE_VULKAN_INDEX_BUFFER_EXT = 0x00000001,
  VK_INDIRECT_COMMANDS_INPUT_MODE_DXGI_INDEX_BUFFER_EXT   = 0x00000002,
  VK_INDIRECT_COMMANDS_INPUT_MODE_FLAG_BITS_MAX_ENUM_EXT  = 0x7FFFFFFF
} VkIndirectCommandsInputModeFlagBitsEXT;

typedef struct VkIndirectCommandsIndexBufferTokenEXT
{
  VkIndirectCommandsInputModeFlagBitsEXT mode;
} VkIndirectCommandsIndexBufferTokenEXT;

typedef struct VkIndirectCommandsPushConstantTokenEXT
{
  VkPushConstantRange updateRange;
} VkIndirectCommandsPushConstantTokenEXT;

typedef struct VkIndirectCommandsExecutionSetTokenEXT
{
  VkIndirectExecutionSetInfoTypeEXT type;
  VkShaderStageFlags                shaderStages;
} VkIndirectCommandsExecutionSetTokenEXT;

typedef union VkIndirectCommandsTokenDataEXT
{
  VkIndirectCommandsPushConstantTokenEXT const* pPushConstant;
  VkIndirectCommandsVertexBufferTokenEXT const* pVertexBuffer;
  VkIndirectCommandsIndexBufferTokenEXT const*  pIndexBuffer;
  VkIndirectCommandsExecutionSetTokenEXT const* pExecutionSet;
} VkIndirectCommandsTokenDataEXT;

typedef enum VkIndirectCommandsTokenTypeEXT
{
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_EXECUTION_SET_EXT      = 0,
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_EXT      = 1,
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_SEQUENCE_INDEX_EXT     = 2,
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_INDEX_BUFFER_EXT       = 3,
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_VERTEX_BUFFER_EXT      = 4,
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_EXT       = 5,
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_EXT               = 6,
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_COUNT_EXT = 7,
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_COUNT_EXT         = 8,
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_DISPATCH_EXT           = 9,
  VK_INDIRECT_COMMANDS_TOKEN_TYPE_MAX_ENUM_EXT           = 0x7FFFFFFF
} VkIndirectCommandsTokenTypeEXT;

typedef struct VkIndirectCommandsLayoutTokenEXT
{
  VkStructureType                sType;
  void const*                    pNext;
  VkIndirectCommandsTokenTypeEXT type;
  VkIndirectCommandsTokenDataEXT data;
  uint32_t                       offset;
} VkIndirectCommandsLayoutTokenEXT;

typedef VkFlags VkIndirectCommandsLayoutUsageFlagsEXT;

typedef struct VkIndirectCommandsLayoutCreateInfoEXT
{
  VkStructureType                         sType;
  void const*                             pNext;
  VkIndirectCommandsLayoutUsageFlagsEXT   flags;
  VkShaderStageFlags                      shaderStages;
  uint32_t                                indirectStride;
  VkPipelineLayout                        pipelineLayout;
  uint32_t                                tokenCount;
  VkIndirectCommandsLayoutTokenEXT const* pTokens;
} VkIndirectCommandsLayoutCreateInfoEXT;

typedef struct VkDrawIndirectCountIndirectCommandEXT
{
  VkDeviceAddress bufferAddress;
  uint32_t        stride;
  uint32_t        commandCount;
} VkDrawIndirectCountIndirectCommandEXT;

typedef struct VkBindVertexBufferIndirectCommandEXT
{
  VkDeviceAddress bufferAddress;
  uint32_t        size;
  uint32_t        stride;
} VkBindVertexBufferIndirectCommandEXT;

typedef struct VkBindIndexBufferIndirectCommandEXT
{
  VkDeviceAddress bufferAddress;
  uint32_t        size;
  VkIndexType     indexType;
} VkBindIndexBufferIndirectCommandEXT;

typedef enum VkIndirectCommandsLayoutUsageFlagBitsEXT
{
  VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT = 0x00000001,
  VK_INDIRECT_COMMANDS_LAYOUT_USAGE_UNORDERED_SEQUENCES_BIT_EXT = 0x00000002,
  VK_INDIRECT_COMMANDS_LAYOUT_USAGE_FLAG_BITS_MAX_ENUM_EXT      = 0x7FFFFFFF
} VkIndirectCommandsLayoutUsageFlagBitsEXT;

typedef struct VkGeneratedCommandsPipelineInfoEXT
{
  VkStructureType sType;
  void*           pNext;
  VkPipeline      pipeline;
} VkGeneratedCommandsPipelineInfoEXT;

typedef struct VkGeneratedCommandsShaderInfoEXT
{
  VkStructureType    sType;
  void*              pNext;
  uint32_t           shaderCount;
  VkShaderEXT const* pShaders;
} VkGeneratedCommandsShaderInfoEXT;

typedef void(VKAPI_PTR* PFN_vkGetGeneratedCommandsMemoryRequirementsEXT)(VkDevice device,
                                                                         const VkGeneratedCommandsMemoryRequirementsInfoEXT* pInfo,
                                                                         VkMemoryRequirements2* pMemoryRequirements);
typedef void(VKAPI_PTR* PFN_vkCmdPreprocessGeneratedCommandsEXT)(VkCommandBuffer commandBuffer,
                                                                 const VkGeneratedCommandsInfoEXT* pGeneratedCommandsInfo,
                                                                 VkCommandBuffer stateCommandBuffer);
typedef void(VKAPI_PTR* PFN_vkCmdExecuteGeneratedCommandsEXT)(VkCommandBuffer                   commandBuffer,
                                                              VkBool32                          isPreprocessed,
                                                              const VkGeneratedCommandsInfoEXT* pGeneratedCommandsInfo);
typedef VkResult(VKAPI_PTR* PFN_vkCreateIndirectCommandsLayoutEXT)(VkDevice device,
                                                                   const VkIndirectCommandsLayoutCreateInfoEXT* pCreateInfo,
                                                                   const VkAllocationCallbacks* pAllocator,
                                                                   VkIndirectCommandsLayoutEXT* pIndirectCommandsLayout);
typedef void(VKAPI_PTR* PFN_vkDestroyIndirectCommandsLayoutEXT)(VkDevice                     device,
                                                                VkIndirectCommandsLayoutEXT  indirectCommandsLayout,
                                                                const VkAllocationCallbacks* pAllocator);
typedef VkResult(VKAPI_PTR* PFN_vkCreateIndirectExecutionSetEXT)(VkDevice                                   device,
                                                                 const VkIndirectExecutionSetCreateInfoEXT* pCreateInfo,
                                                                 const VkAllocationCallbacks*               pAllocator,
                                                                 VkIndirectExecutionSetEXT* pIndirectExecutionSet);
typedef void(VKAPI_PTR* PFN_vkDestroyIndirectExecutionSetEXT)(VkDevice                     device,
                                                              VkIndirectExecutionSetEXT    indirectExecutionSet,
                                                              const VkAllocationCallbacks* pAllocator);
typedef void(VKAPI_PTR* PFN_vkUpdateIndirectExecutionSetPipelineEXT)(VkDevice                  device,
                                                                     VkIndirectExecutionSetEXT indirectExecutionSet,
                                                                     uint32_t                  executionSetWriteCount,
                                                                     const VkWriteIndirectExecutionSetPipelineEXT* pExecutionSetWrites);
typedef void(VKAPI_PTR* PFN_vkUpdateIndirectExecutionSetShaderEXT)(VkDevice                  device,
                                                                   VkIndirectExecutionSetEXT indirectExecutionSet,
                                                                   uint32_t                  executionSetWriteCount,
                                                                   const VkWriteIndirectExecutionSetShaderEXT* pExecutionSetWrites);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR void VKAPI_CALL vkGetGeneratedCommandsMemoryRequirementsEXT(VkDevice device,
                                                                       VkGeneratedCommandsMemoryRequirementsInfoEXT const* pInfo,
                                                                       VkMemoryRequirements2* pMemoryRequirements);

VKAPI_ATTR void VKAPI_CALL vkCmdPreprocessGeneratedCommandsEXT(VkCommandBuffer                   commandBuffer,
                                                               VkGeneratedCommandsInfoEXT const* pGeneratedCommandsInfo,
                                                               VkCommandBuffer                   stateCommandBuffer);

VKAPI_ATTR void VKAPI_CALL vkCmdExecuteGeneratedCommandsEXT(VkCommandBuffer                   commandBuffer,
                                                            VkBool32                          isPreprocessed,
                                                            VkGeneratedCommandsInfoEXT const* pGeneratedCommandsInfo);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateIndirectCommandsLayoutEXT(VkDevice device,
                                                                 VkIndirectCommandsLayoutCreateInfoEXT const* pCreateInfo,
                                                                 VkAllocationCallbacks const* pAllocator,
                                                                 VkIndirectCommandsLayoutEXT* pIndirectCommandsLayout);

VKAPI_ATTR void VKAPI_CALL vkDestroyIndirectCommandsLayoutEXT(VkDevice                     device,
                                                              VkIndirectCommandsLayoutEXT  indirectCommandsLayout,
                                                              VkAllocationCallbacks const* pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateIndirectExecutionSetEXT(VkDevice                                   device,
                                                               VkIndirectExecutionSetCreateInfoEXT const* pCreateInfo,
                                                               VkAllocationCallbacks const*               pAllocator,
                                                               VkIndirectExecutionSetEXT* pIndirectExecutionSet);

VKAPI_ATTR void VKAPI_CALL vkDestroyIndirectExecutionSetEXT(VkDevice                     device,
                                                            VkIndirectExecutionSetEXT    indirectExecutionSet,
                                                            VkAllocationCallbacks const* pAllocator);

VKAPI_ATTR void VKAPI_CALL vkUpdateIndirectExecutionSetPipelineEXT(VkDevice                  device,
                                                                   VkIndirectExecutionSetEXT indirectExecutionSet,
                                                                   uint32_t                  executionSetWriteCount,
                                                                   VkWriteIndirectExecutionSetPipelineEXT const* pExecutionSetWrites);

VKAPI_ATTR void VKAPI_CALL vkUpdateIndirectExecutionSetShaderEXT(VkDevice                  device,
                                                                 VkIndirectExecutionSetEXT indirectExecutionSet,
                                                                 uint32_t                  executionSetWriteCount,
                                                                 VkWriteIndirectExecutionSetShaderEXT const* pExecutionSetWrites);
#endif
#endif

VkBool32 load_VK_EXT_device_generated_commands(VkInstance instance, VkDevice device);
