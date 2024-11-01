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


#include "config.h"
#include "cadscene_vk.hpp"

#include <algorithm>
#include <inttypes.h>
#include <nvh/nvprint.hpp>


static inline VkDeviceSize alignedSize(VkDeviceSize sz, VkDeviceSize align)
{
  return ((sz + align - 1) / (align)) * align;
}


void GeometryMemoryVK::init(nvvk::ResourceAllocator* resourceAllocator, VkDeviceSize vboStride, VkDeviceSize maxChunk)
{
  m_resourceAllocator = resourceAllocator;
  m_alignment         = 16;
  m_vboAlignment      = 16;

  m_maxVboChunk = maxChunk;
  m_maxIboChunk = maxChunk;
}

void GeometryMemoryVK::deinit()
{
  for(size_t i = 0; i < m_chunks.size(); i++)
  {
    Chunk chunk = getChunk(i);
    m_resourceAllocator->destroy(chunk.vbo);
    m_resourceAllocator->destroy(chunk.ibo);
  }
  m_chunks            = std::vector<Chunk>();
  m_device            = nullptr;
  m_resourceAllocator = nullptr;
}

void GeometryMemoryVK::alloc(VkDeviceSize vboSize, VkDeviceSize iboSize, Allocation& allocation)
{
  vboSize = alignedSize(vboSize, m_vboAlignment);
  iboSize = alignedSize(iboSize, m_alignment);

  if(m_chunks.empty() || getActiveChunk().vboSize + vboSize > m_maxVboChunk || getActiveChunk().iboSize + iboSize > m_maxIboChunk)
  {
    finalize();
    Chunk chunk = {};
    m_chunks.push_back(chunk);
  }

  Chunk& chunk = getActiveChunk();

  allocation.chunkIndex = getActiveIndex();
  allocation.vboOffset  = chunk.vboSize;
  allocation.iboOffset  = chunk.iboSize;

  chunk.vboSize += vboSize;
  chunk.iboSize += iboSize;
}

void GeometryMemoryVK::finalize()
{
  if(m_chunks.empty())
  {
    return;
  }

  Chunk& chunk = getActiveChunk();

  uint32_t flags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

  chunk.vbo = m_resourceAllocator->createBuffer(chunk.vboSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | flags);
  chunk.ibo = m_resourceAllocator->createBuffer(chunk.iboSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | flags);
}

void CadSceneVK::init(const CadScene& cadscene, nvvk::ResourceAllocator& resourceAllocator, VkQueue queue, uint32_t queueFamilyIndex, const Config& config)
{
  VkDeviceSize MB = 1024 * 1024;

  m_resourceAllocator = &resourceAllocator;
  m_config            = config;
  m_geometry.resize(cadscene.m_geometry.size(), {0});

  if(m_geometry.empty())
    return;

  {
    // allocation phase
    m_geometryMem.init(&resourceAllocator, sizeof(CadScene::Vertex), config.singleAllocation ? VkDeviceSize(4096) * MB : 256 * MB);

    for(size_t g = 0; g < cadscene.m_geometry.size(); g++)
    {
      const CadScene::Geometry& cadgeom = cadscene.m_geometry[g];
      Geometry&                 geom    = m_geometry[g];

      m_geometryMem.alloc(cadgeom.vboSize, cadgeom.iboSize, geom.allocation);
    }

    m_geometryMem.finalize();

    LOGI("Size of vertex data: %11" PRId64 "\n", uint64_t(m_geometryMem.getVertexSize()));
    LOGI("Size of index data:  %11" PRId64 "\n", uint64_t(m_geometryMem.getIndexSize()));
    LOGI("Size of data:        %11" PRId64 "\n", uint64_t(m_geometryMem.getVertexSize() + m_geometryMem.getIndexSize()));
    LOGI("Chunks:              %11d\n", uint32_t(m_geometryMem.getChunkCount()));
  }

  ScopeStaging staging(resourceAllocator, queue, queueFamilyIndex);

  for(size_t g = 0; g < cadscene.m_geometry.size(); g++)
  {
    const CadScene::Geometry&      cadgeom = cadscene.m_geometry[g];
    Geometry&                      geom    = m_geometry[g];
    const GeometryMemoryVK::Chunk& chunk   = m_geometryMem.getChunk(geom.allocation);

    // upload and assignment phase
    geom.vbo.buffer = chunk.vbo.buffer;
    geom.vbo.offset = geom.allocation.vboOffset;
    geom.vbo.range  = cadgeom.vboSize;
    staging.uploadAutoSubmit(geom.vbo, cadgeom.vboData);

    geom.ibo.buffer = chunk.ibo.buffer;
    geom.ibo.offset = geom.allocation.iboOffset;
    geom.ibo.range  = cadgeom.iboSize;
    staging.uploadAutoSubmit(geom.ibo, cadgeom.iboData);
  }

  VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  usageFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

  VkDeviceSize materialsSize = cadscene.m_materials.size() * sizeof(CadScene::Material);
  VkDeviceSize matricesSize  = cadscene.m_matrices.size() * sizeof(CadScene::MatrixNode);

  m_buffers.materials    = resourceAllocator.createBuffer(materialsSize, usageFlags);
  m_buffers.matrices     = resourceAllocator.createBuffer(matricesSize, usageFlags);
  m_buffers.matricesOrig = resourceAllocator.createBuffer(matricesSize, usageFlags | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

  m_infos.materialsSingle = {m_buffers.materials.buffer, 0, sizeof(CadScene::Material)};
  m_infos.materials       = {m_buffers.materials.buffer, 0, materialsSize};
  m_infos.matricesSingle  = {m_buffers.matrices.buffer, 0, sizeof(CadScene::MatrixNode)};
  m_infos.matrices        = {m_buffers.matrices.buffer, 0, matricesSize};
  m_infos.matricesOrig    = {m_buffers.matricesOrig.buffer, 0, matricesSize};

  staging.uploadAutoSubmit(m_infos.materials, cadscene.m_materials.data());
  staging.uploadAutoSubmit(m_infos.matrices, cadscene.m_matrices.data());
  staging.uploadAutoSubmit(m_infos.matricesOrig, cadscene.m_matrices.data());

  staging.uploadAutoSubmit({}, nullptr);
}

void CadSceneVK::deinit()
{
  m_resourceAllocator->destroy(m_buffers.materials);
  m_resourceAllocator->destroy(m_buffers.matrices);
  m_resourceAllocator->destroy(m_buffers.matricesOrig);
  m_geometry.clear();
  m_geometryMem.deinit();
}
