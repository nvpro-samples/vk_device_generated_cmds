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


#include "nv_device_generated_commands.h"
#include <assert.h>

/* /////////////////////////////////// */
#if VK_NV_device_generated_commands && VK_NV_device_generated_commands_custom
static PFN_vkCmdPreprocessGeneratedCommandsNV         pfn_vkCmdPreprocessGeneratedCommandsNV         = 0;
static PFN_vkCmdExecuteGeneratedCommandsNV            pfn_vkCmdExecuteGeneratedCommandsNV            = 0;
static PFN_vkGetGeneratedCommandsMemoryRequirementsNV pfn_vkGetGeneratedCommandsMemoryRequirementsNV = 0;
static PFN_vkCreateIndirectCommandsLayoutNV           pfn_vkCreateIndirectCommandsLayoutNV           = 0;
static PFN_vkDestroyIndirectCommandsLayoutNV          pfn_vkDestroyIndirectCommandsLayoutNV          = 0;
static PFN_vkCmdBindPipelineShaderGroupNV             pfn_vkCmdBindPipelineShaderGroup               = 0;

VKAPI_ATTR void VKAPI_CALL vkCmdPreprocessGeneratedCommandsNV(VkCommandBuffer                  commandBuffer,
                                                              const VkGeneratedCommandsInfoNV* pGeneratedCommandsInfo)
{
  assert(pfn_vkCmdPreprocessGeneratedCommandsNV);
  pfn_vkCmdPreprocessGeneratedCommandsNV(commandBuffer, pGeneratedCommandsInfo);
}
VKAPI_ATTR void VKAPI_CALL vkCmdExecuteGeneratedCommandsNV(VkCommandBuffer                  commandBuffer,
                                                           VkBool32                         isPreprocessed,
                                                           const VkGeneratedCommandsInfoNV* pGenerateCommandsInfo)
{
  assert(pfn_vkCmdExecuteGeneratedCommandsNV);
  pfn_vkCmdExecuteGeneratedCommandsNV(commandBuffer, isPreprocessed, pGenerateCommandsInfo);
}
VKAPI_ATTR void VKAPI_CALL vkGetGeneratedCommandsMemoryRequirementsNV(VkDevice                                           device,
                                                            const VkGeneratedCommandsMemoryRequirementsInfoNV* pInfo,
                                                            VkMemoryRequirements2* pMemoryRequirements)
{
  assert(pfn_vkGetGeneratedCommandsMemoryRequirementsNV);
  pfn_vkGetGeneratedCommandsMemoryRequirementsNV(device, pInfo, pMemoryRequirements);
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateIndirectCommandsLayoutNV(VkDevice                                    device,
                                                                const VkIndirectCommandsLayoutCreateInfoNV* pCreateInfo,
                                                                const VkAllocationCallbacks*                pAllocator,
                                                                VkIndirectCommandsLayoutNV* pIndirectCommandsLayout)
{
  assert(pfn_vkCreateIndirectCommandsLayoutNV);
  return pfn_vkCreateIndirectCommandsLayoutNV(device, pCreateInfo, pAllocator, pIndirectCommandsLayout);
}
VKAPI_ATTR void VKAPI_CALL vkDestroyIndirectCommandsLayoutNV(VkDevice                     device,
                                                             VkIndirectCommandsLayoutNV   indirectCommandsLayout,
                                                             const VkAllocationCallbacks* pAllocator)
{
  assert(pfn_vkDestroyIndirectCommandsLayoutNV);
  pfn_vkDestroyIndirectCommandsLayoutNV(device, indirectCommandsLayout, pAllocator);
}
VKAPI_ATTR void VKAPI_CALL vkCmdBindPipelineShaderGroupNV(VkCommandBuffer     commandBuffer,
                                                          VkPipelineBindPoint pipelineBindPoint,
                                                          VkPipeline          pipeline,
                                                          uint32_t            groupIndex)
{
  assert(pfn_vkCmdBindPipelineShaderGroup);
  pfn_vkCmdBindPipelineShaderGroup(commandBuffer, pipelineBindPoint, pipeline, groupIndex);
}

int load_VK_NV_device_generated_commands(VkInstance instance, PFN_vkGetInstanceProcAddr getInstanceProcAddr, VkDevice device, PFN_vkGetDeviceProcAddr getDeviceProcAddr)
{
  pfn_vkCmdPreprocessGeneratedCommandsNV =
      (PFN_vkCmdPreprocessGeneratedCommandsNV)getDeviceProcAddr(device, "vkCmdPreprocessGeneratedCommandsNV");
  pfn_vkCmdExecuteGeneratedCommandsNV =
      (PFN_vkCmdExecuteGeneratedCommandsNV)getDeviceProcAddr(device, "vkCmdExecuteGeneratedCommandsNV");
  pfn_vkGetGeneratedCommandsMemoryRequirementsNV =
      (PFN_vkGetGeneratedCommandsMemoryRequirementsNV)getDeviceProcAddr(device,
                                                                        "vkGetGeneratedCommandsMemoryRequirementsNV");
  pfn_vkCreateIndirectCommandsLayoutNV =
      (PFN_vkCreateIndirectCommandsLayoutNV)getDeviceProcAddr(device, "vkCreateIndirectCommandsLayoutNV");
  pfn_vkDestroyIndirectCommandsLayoutNV =
      (PFN_vkDestroyIndirectCommandsLayoutNV)getDeviceProcAddr(device, "vkDestroyIndirectCommandsLayoutNV");
  pfn_vkCmdBindPipelineShaderGroup =
      (PFN_vkCmdBindPipelineShaderGroupNV)getDeviceProcAddr(device, "vkCmdBindPipelineShaderGroupNV");
  int success = 1;
  success     = success && (pfn_vkCmdPreprocessGeneratedCommandsNV != 0);
  success     = success && (pfn_vkCmdExecuteGeneratedCommandsNV != 0);
  success     = success && (pfn_vkGetGeneratedCommandsMemoryRequirementsNV != 0);
  success     = success && (pfn_vkCreateIndirectCommandsLayoutNV != 0);
  success     = success && (pfn_vkDestroyIndirectCommandsLayoutNV != 0);
  success     = success && (pfn_vkCmdBindPipelineShaderGroup != 0);
  return success;
}
#endif