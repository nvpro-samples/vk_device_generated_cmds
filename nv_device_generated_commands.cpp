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