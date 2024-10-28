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


#pragma once

#include "cadscene.hpp"

#include <nvvk/buffers_vk.hpp>
#include <nvvk/commands_vk.hpp>
#include <nvvk/resourceallocator_vk.hpp>

// ScopeStaging handles uploads and other staging operations.
// not efficient because it blocks/syncs operations

struct ScopeStaging
{
  ScopeStaging(nvvk::ResourceAllocator& resAllocator, VkQueue queue_, uint32_t queueFamily)
      : staging(*resAllocator.getStaging())
      , cmdPool(resAllocator.getDevice(), queueFamily)
      , queue(queue_)
      , cmd(VK_NULL_HANDLE)
  {
  }
  ~ScopeStaging() { submit(); }

  VkCommandBuffer             cmd;
  nvvk::StagingMemoryManager& staging;
  nvvk::CommandPool           cmdPool;
  VkQueue                     queue;

  VkCommandBuffer getCmd()
  {
    cmd = cmd ? cmd : cmdPool.createCommandBuffer();
    return cmd;
  }

  void submit()
  {
    if(cmd)
    {
      cmdPool.submitAndWait(cmd, queue);
      cmd = VK_NULL_HANDLE;
      staging.releaseResources();
    }
  }

  void uploadAutoSubmit(const VkDescriptorBufferInfo& binding, const void* data)
  {
    if(cmd && (data == nullptr || !staging.fitsInAllocated(binding.range)))
    {
      submit();
    }
    if(data && binding.range)
    {
      staging.cmdToBuffer(getCmd(), binding.buffer, binding.offset, binding.range, data);
    }
  }

  void* upload(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const void* data = nullptr)
  {
    return staging.cmdToBuffer(getCmd(), buffer, offset, size, data);
  }

  template <class T>
  T* uploadT(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const void* data = nullptr)
  {
    return (T*)staging.cmdToBuffer(getCmd(), buffer, offset, size, data);
  }
};


// GeometryMemoryVK manages vbo/ibo etc. in chunks
// allows to reduce number of bindings and be more memory efficient

struct GeometryMemoryVK
{
  typedef size_t Index;


  struct Allocation
  {
    Index        chunkIndex;
    VkDeviceSize vboOffset;
    VkDeviceSize iboOffset;
  };

  struct Chunk
  {
    nvvk::Buffer vbo;
    nvvk::Buffer ibo;

    VkDeviceSize vboSize;
    VkDeviceSize iboSize;
  };


  VkDevice                 m_device = VK_NULL_HANDLE;
  nvvk::ResourceAllocator* m_resourceAllocator;
  std::vector<Chunk>       m_chunks;

  void init(nvvk::ResourceAllocator* resourceAllocator, VkDeviceSize vboStride, VkDeviceSize maxChunk);
  void deinit();
  void alloc(VkDeviceSize vboSize, VkDeviceSize iboSize, Allocation& allocation);
  void finalize();

  const Chunk& getChunk(const Allocation& allocation) const { return m_chunks[allocation.chunkIndex]; }

  const Chunk& getChunk(Index index) const { return m_chunks[index]; }

  VkDeviceSize getVertexSize() const
  {
    VkDeviceSize size = 0;
    for(size_t i = 0; i < m_chunks.size(); i++)
    {
      size += m_chunks[i].vboSize;
    }
    return size;
  }

  VkDeviceSize getIndexSize() const
  {
    VkDeviceSize size = 0;
    for(size_t i = 0; i < m_chunks.size(); i++)
    {
      size += m_chunks[i].iboSize;
    }
    return size;
  }

  VkDeviceSize getChunkCount() const { return m_chunks.size(); }

private:
  VkDeviceSize m_alignment;
  VkDeviceSize m_vboAlignment;
  VkDeviceSize m_maxVboChunk;
  VkDeviceSize m_maxIboChunk;

  Index getActiveIndex() { return (m_chunks.size() - 1); }

  Chunk& getActiveChunk()
  {
    assert(!m_chunks.empty());
    return m_chunks[getActiveIndex()];
  }
};


class CadSceneVK
{
public:
  struct Geometry
  {
    GeometryMemoryVK::Allocation allocation;

    VkDescriptorBufferInfo vbo;
    VkDescriptorBufferInfo ibo;
  };

  struct Buffers
  {
    nvvk::Buffer materials    = {};
    nvvk::Buffer matrices     = {};
    nvvk::Buffer matricesOrig = {};
  };

  struct Infos
  {
    VkDescriptorBufferInfo materialsSingle, materials, matricesSingle, matrices, matricesOrig;
  };

  struct Config
  {
    bool singleAllocation = false;
  };

  VkDevice m_device = VK_NULL_HANDLE;

  Config m_config;

  Buffers m_buffers;
  Infos   m_infos;

  std::vector<Geometry>    m_geometry;
  GeometryMemoryVK         m_geometryMem;
  nvvk::ResourceAllocator* m_resourceAllocator = nullptr;


  void init(const CadScene& cadscene, nvvk::ResourceAllocator& resourceAllocator, VkQueue queue, uint32_t queueFamilyIndex, const Config& config);
  void deinit();
};
