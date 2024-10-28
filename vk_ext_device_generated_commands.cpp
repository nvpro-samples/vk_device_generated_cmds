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

#include <nvvk/extensions_vk.hpp>
#include "vk_ext_device_generated_commands.hpp"

static PFN_vkGetGeneratedCommandsMemoryRequirementsEXT s_vkGetGeneratedCommandsMemoryRequirementsEXT = nullptr;
static PFN_vkCmdPreprocessGeneratedCommandsEXT         s_vkCmdPreprocessGeneratedCommandsEXT         = nullptr;
static PFN_vkCmdExecuteGeneratedCommandsEXT            s_vkCmdExecuteGeneratedCommandsEXT            = nullptr;
static PFN_vkCreateIndirectCommandsLayoutEXT           s_vkCreateIndirectCommandsLayoutEXT           = nullptr;
static PFN_vkDestroyIndirectCommandsLayoutEXT          s_vkDestroyIndirectCommandsLayoutEXT          = nullptr;
static PFN_vkCreateIndirectExecutionSetEXT             s_vkCreateIndirectExecutionSetEXT             = nullptr;
static PFN_vkDestroyIndirectExecutionSetEXT            s_vkDestroyIndirectExecutionSetEXT            = nullptr;
static PFN_vkUpdateIndirectExecutionSetPipelineEXT     s_vkUpdateIndirectExecutionSetPipelineEXT     = nullptr;
static PFN_vkUpdateIndirectExecutionSetShaderEXT       s_vkUpdateIndirectExecutionSetShaderEXT       = nullptr;

#ifndef NVVK_HAS_VK_EXT_device_generated_commands

VKAPI_ATTR void VKAPI_CALL vkGetGeneratedCommandsMemoryRequirementsEXT(VkDevice device,
                                                                       VkGeneratedCommandsMemoryRequirementsInfoEXT const* pInfo,
                                                                       VkMemoryRequirements2* pMemoryRequirements)
{
  s_vkGetGeneratedCommandsMemoryRequirementsEXT(device, pInfo, pMemoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL vkCmdPreprocessGeneratedCommandsEXT(VkCommandBuffer                   commandBuffer,
                                                               VkGeneratedCommandsInfoEXT const* pGeneratedCommandsInfo,
                                                               VkCommandBuffer                   stateCommandBuffer)
{
  s_vkCmdPreprocessGeneratedCommandsEXT(commandBuffer, pGeneratedCommandsInfo, stateCommandBuffer);
}

VKAPI_ATTR void VKAPI_CALL vkCmdExecuteGeneratedCommandsEXT(VkCommandBuffer                   commandBuffer,
                                                            VkBool32                          isPreprocessed,
                                                            VkGeneratedCommandsInfoEXT const* pGeneratedCommandsInfo)
{
  s_vkCmdExecuteGeneratedCommandsEXT(commandBuffer, isPreprocessed, pGeneratedCommandsInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateIndirectCommandsLayoutEXT(VkDevice device,
                                                                 VkIndirectCommandsLayoutCreateInfoEXT const* pCreateInfo,
                                                                 VkAllocationCallbacks const* pAllocator,
                                                                 VkIndirectCommandsLayoutEXT* pIndirectCommandsLayout)
{
  return s_vkCreateIndirectCommandsLayoutEXT(device, pCreateInfo, pAllocator, pIndirectCommandsLayout);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyIndirectCommandsLayoutEXT(VkDevice                     device,
                                                              VkIndirectCommandsLayoutEXT  indirectCommandsLayout,
                                                              VkAllocationCallbacks const* pAllocator)
{
  s_vkDestroyIndirectCommandsLayoutEXT(device, indirectCommandsLayout, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateIndirectExecutionSetEXT(VkDevice                                   device,
                                                               VkIndirectExecutionSetCreateInfoEXT const* pCreateInfo,
                                                               VkAllocationCallbacks const*               pAllocator,
                                                               VkIndirectExecutionSetEXT* pIndirectExecutionSet)
{
  return s_vkCreateIndirectExecutionSetEXT(device, pCreateInfo, pAllocator, pIndirectExecutionSet);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyIndirectExecutionSetEXT(VkDevice                     device,
                                                            VkIndirectExecutionSetEXT    indirectExecutionSet,
                                                            VkAllocationCallbacks const* pAllocator)
{
  s_vkDestroyIndirectExecutionSetEXT(device, indirectExecutionSet, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL vkUpdateIndirectExecutionSetPipelineEXT(VkDevice                  device,
                                                                   VkIndirectExecutionSetEXT indirectExecutionSet,
                                                                   uint32_t                  executionSetWriteCount,
                                                                   VkWriteIndirectExecutionSetPipelineEXT const* pExecutionSetWrites)
{
  s_vkUpdateIndirectExecutionSetPipelineEXT(device, indirectExecutionSet, executionSetWriteCount, pExecutionSetWrites);
}

VKAPI_ATTR void VKAPI_CALL vkUpdateIndirectExecutionSetShaderEXT(VkDevice                  device,
                                                                 VkIndirectExecutionSetEXT indirectExecutionSet,
                                                                 uint32_t                  executionSetWriteCount,
                                                                 VkWriteIndirectExecutionSetShaderEXT const* pExecutionSetWrites)
{
  s_vkUpdateIndirectExecutionSetShaderEXT(device, indirectExecutionSet, executionSetWriteCount, pExecutionSetWrites);
}
#endif

VkBool32 load_VK_EXT_device_generated_commands(VkInstance instance, VkDevice device)
{
  s_vkGetGeneratedCommandsMemoryRequirementsEXT = nullptr;
  s_vkCmdPreprocessGeneratedCommandsEXT         = nullptr;
  s_vkCmdExecuteGeneratedCommandsEXT            = nullptr;
  s_vkCreateIndirectCommandsLayoutEXT           = nullptr;
  s_vkDestroyIndirectCommandsLayoutEXT          = nullptr;
  s_vkCreateIndirectExecutionSetEXT             = nullptr;
  s_vkDestroyIndirectExecutionSetEXT            = nullptr;
  s_vkUpdateIndirectExecutionSetPipelineEXT     = nullptr;
  s_vkUpdateIndirectExecutionSetShaderEXT       = nullptr;

  s_vkGetGeneratedCommandsMemoryRequirementsEXT =
      (PFN_vkGetGeneratedCommandsMemoryRequirementsEXT)vkGetDeviceProcAddr(device, "vkGetGeneratedCommandsMemoryRequirementsEXT");
  s_vkCmdPreprocessGeneratedCommandsEXT =
      (PFN_vkCmdPreprocessGeneratedCommandsEXT)vkGetDeviceProcAddr(device, "vkCmdPreprocessGeneratedCommandsEXT");
  s_vkCmdExecuteGeneratedCommandsEXT =
      (PFN_vkCmdExecuteGeneratedCommandsEXT)vkGetDeviceProcAddr(device, "vkCmdExecuteGeneratedCommandsEXT");
  s_vkCreateIndirectCommandsLayoutEXT =
      (PFN_vkCreateIndirectCommandsLayoutEXT)vkGetDeviceProcAddr(device, "vkCreateIndirectCommandsLayoutEXT");
  s_vkDestroyIndirectCommandsLayoutEXT =
      (PFN_vkDestroyIndirectCommandsLayoutEXT)vkGetDeviceProcAddr(device, "vkDestroyIndirectCommandsLayoutEXT");
  s_vkCreateIndirectExecutionSetEXT =
      (PFN_vkCreateIndirectExecutionSetEXT)vkGetDeviceProcAddr(device, "vkCreateIndirectExecutionSetEXT");
  s_vkDestroyIndirectExecutionSetEXT =
      (PFN_vkDestroyIndirectExecutionSetEXT)vkGetDeviceProcAddr(device, "vkDestroyIndirectExecutionSetEXT");
  s_vkUpdateIndirectExecutionSetPipelineEXT =
      (PFN_vkUpdateIndirectExecutionSetPipelineEXT)vkGetDeviceProcAddr(device, "vkUpdateIndirectExecutionSetPipelineEXT");
  s_vkUpdateIndirectExecutionSetShaderEXT =
      (PFN_vkUpdateIndirectExecutionSetShaderEXT)vkGetDeviceProcAddr(device, "vkUpdateIndirectExecutionSetShaderEXT");

  return s_vkGetGeneratedCommandsMemoryRequirementsEXT && s_vkCmdPreprocessGeneratedCommandsEXT
         && s_vkCmdExecuteGeneratedCommandsEXT && s_vkCreateIndirectCommandsLayoutEXT
         && s_vkDestroyIndirectCommandsLayoutEXT && s_vkCreateIndirectExecutionSetEXT && s_vkDestroyIndirectExecutionSetEXT
         && s_vkUpdateIndirectExecutionSetPipelineEXT && s_vkUpdateIndirectExecutionSetShaderEXT;
}
