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



#pragma once

#include "resources_vk.hpp"

namespace generatedcmds {

class ResourcesVKGen : public ResourcesVK
{
public:
  // To use the extension, we extend the resources for vulkan
  // by the object table. In this table we will register all the
  // binding/resources we need for rendering the scene.
  // When it comes to resources, that is the only difference to
  // unextended Vulkan.

  VkPipeline m_drawGroupsPipeline;

  bool init(nvvk::Context* context, nvvk::SwapChain* swapChain, nvh::Profiler* profiler) override;
  void deinit() override;

  void initPipes() override;

  ResourcesVKGen() {}

  static ResourcesVKGen* get()
  {
    static ResourcesVKGen res;

    return &res;
  }
};
}  // namespace generatedcmds
