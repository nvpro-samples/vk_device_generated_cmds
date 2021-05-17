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


#include "resources_vkgen.hpp"
#include <algorithm>

namespace generatedcmds {

bool ResourcesVKGen::init(nvvk::Context* context, nvvk::SwapChain* swapChain, nvh::Profiler* profiler)
{
  bool valid = ResourcesVK::init(context, swapChain, profiler);
  return valid && m_context->hasDeviceExtension(VK_NV_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME);
}

void ResourcesVKGen::deinit()
{
  vkDestroyPipeline(m_device, m_drawGroupsPipeline, NULL);
  m_drawGroupsPipeline = NULL;

  ResourcesVK::deinit();
}

void ResourcesVKGen::initPipes()
{
  // the most likely use-case is that you want to re-use existing
  // graphics pipelines
  bool useReferences = USE_PIPELINE_REFERENCES != 0;

  // If we use references make sure those pipelines were created with this flag activated.
  // This member here influences the parent class initPipes.
  m_gfxStatePipelineFlags = useReferences ? VK_PIPELINE_CREATE_INDIRECT_BINDABLE_BIT_NV : 0;

  ResourcesVK::initPipes();

  if(m_drawGroupsPipeline)
  {
    vkDestroyPipeline(m_device, m_drawGroupsPipeline, NULL);
  }

  VkGraphicsPipelineShaderGroupsCreateInfoNV groupsCreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_SHADER_GROUPS_CREATE_INFO_NV};

  std::vector<VkPipelineShaderStageCreateInfo>   shaderStages;
  std::vector<VkGraphicsShaderGroupCreateInfoNV> shaderGroups;
  shaderStages.resize(NUM_MATERIAL_SHADERS * 2);
  shaderGroups.reserve(NUM_MATERIAL_SHADERS);

  m_gfxGen.createInfo.flags = VK_PIPELINE_CREATE_INDIRECT_BINDABLE_BIT_NV;
  m_gfxGen.clearShaders();
  m_gfxGen.setLayout(m_drawPush.getPipeLayout());
  m_gfxGen.addShader(m_drawShading[BINDINGMODE_PUSHADDRESS].vertexShaders[0], VK_SHADER_STAGE_VERTEX_BIT);
  m_gfxGen.addShader(m_drawShading[BINDINGMODE_PUSHADDRESS].fragmentShaders[0], VK_SHADER_STAGE_FRAGMENT_BIT);

  for(uint32_t m = 0; m < (useReferences ? 1 : NUM_MATERIAL_SHADERS); m++)
  {
    VkPipelineShaderStageCreateInfo& vstage = shaderStages[m * 2 + 0];
    vstage                                  = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vstage.pName                            = "main";
    vstage.stage                            = VK_SHADER_STAGE_VERTEX_BIT;
    vstage.module                           = m_drawShading[BINDINGMODE_PUSHADDRESS].vertexShaders[m];

    VkPipelineShaderStageCreateInfo& fstage = shaderStages[m * 2 + 1];
    fstage                                  = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fstage.pName                            = "main";
    fstage.stage                            = VK_SHADER_STAGE_FRAGMENT_BIT;
    fstage.module                           = m_drawShading[BINDINGMODE_PUSHADDRESS].fragmentShaders[m];

    VkGraphicsShaderGroupCreateInfoNV group = {VK_STRUCTURE_TYPE_GRAPHICS_SHADER_GROUP_CREATE_INFO_NV};
    group.stageCount                        = 2;
    group.pStages                           = &shaderStages[m * 2];
    group.pVertexInputState                 = m_gfxGen.createInfo.pVertexInputState;
    group.pTessellationState                = m_gfxGen.createInfo.pTessellationState;

    shaderGroups.push_back(group);
  }

  std::vector<VkPipeline> referencedPipelines;
  if(useReferences)
  {
    for(uint32_t m = 1; m < NUM_MATERIAL_SHADERS; m++)
    {
      referencedPipelines.push_back(m_drawShading[BINDINGMODE_PUSHADDRESS].pipelines[m]);
    }
    groupsCreateInfo.pPipelines    = referencedPipelines.data();
    groupsCreateInfo.pipelineCount = (uint32_t)referencedPipelines.size();
  }

  groupsCreateInfo.groupCount = (uint32_t)shaderGroups.size();
  groupsCreateInfo.pGroups    = shaderGroups.data();


  m_gfxGen.createInfo.pNext = &groupsCreateInfo;

  m_drawGroupsPipeline = m_gfxGen.createPipeline();
  assert(m_drawGroupsPipeline != VK_NULL_HANDLE);

  m_gfxGen.createInfo.pNext = nullptr;
  m_gfxGen.createInfo.flags = 0;
  m_gfxStatePipelineFlags   = 0;
}


}  // namespace generatedcmds
