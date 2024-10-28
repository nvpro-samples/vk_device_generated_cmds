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


#include <algorithm>
#include <assert.h>
#include <mutex>
#include <queue>

#include "renderer.hpp"
#include "resources_vk.hpp"
#include "threadpool.hpp"
#include <nvh/nvprint.hpp>
#include <nvpwindow.hpp>

#include "common.h"

#if 0
#include <emmintrin.h>
#define THREAD_BARRIER() _mm_mfence()
#else
#define THREAD_BARRIER() std::atomic_thread_fence(std::memory_order_seq_cst)
#endif

namespace generatedcmds {

//////////////////////////////////////////////////////////////////////////


class RendererThreadedVK : public Renderer
{
public:
  class TypeCmd : public Renderer::Type
  {
    bool        isAvailable(const nvvk::Context& context) override { return true; }
    const char* name() const override { return "threaded cmds"; }
    Renderer*   create() const override
    {
      RendererThreadedVK* renderer = new RendererThreadedVK();
      return renderer;
    }
    uint32_t priority() const override { return 10; }
  };

public:
  void init(const CadScene* scene, ResourcesVK* res, const Config& config, Stats& stats) override;
  void deinit() override;
  void draw(const Resources::Global& global, Stats& stats) override;

  RendererThreadedVK() {}

private:
  struct DrawSetup
  {
    std::vector<VkCommandBuffer> cmdbuffers;
  };


  struct ThreadJob
  {
    RendererThreadedVK* renderer;
    int                 index;

    nvvk::RingCommandPool m_pool;

    int                     m_frame;
    std::condition_variable m_hasWorkCond;
    std::mutex              m_hasWorkMutex;
    volatile int            m_hasWork;

    size_t                  m_scIdx;
    std::vector<DrawSetup*> m_scs;


    void resetFrame() { m_scIdx = 0; }

    DrawSetup* getFrameCommand()
    {
      DrawSetup* sc;
      if(m_scIdx + 1 > m_scs.size())
      {
        sc = new DrawSetup;
        m_scIdx++;
        m_scs.push_back(sc);
      }
      else
      {
        sc = m_scs[m_scIdx++];
      }

      sc->cmdbuffers.clear();
      return sc;
    }
  };


  std::vector<DrawItem>  m_drawItems;
  std::vector<uint32_t>  m_seqIndices;
  ResourcesVK*           m_resources;
  int                    m_numThreads;
  CadScene::IndexingBits m_indexingBits;
  std::vector<uint32_t>  m_combinedIndicesData;
  nvvk::Buffer           m_combinedIndices[nvvk::DEFAULT_RING_SIZE];
  void*                  m_combinedIndicesMappings[nvvk::DEFAULT_RING_SIZE];

  ThreadPool m_threadpool;

  bool     m_workerBatched;
  int      m_workingSet;
  int      m_frame;
  uint32_t m_cycleCurrent;

  ThreadJob* m_jobs;

  volatile uint32_t m_ready;
  volatile uint32_t m_stopThreads;
  volatile size_t   m_numCurItems;

  std::condition_variable m_readyCond;
  std::mutex              m_readyMutex;

  size_t                 m_numEnqueues;
  std::queue<DrawSetup*> m_drawQueue;

  std::mutex              m_workMutex;
  std::mutex              m_drawMutex;
  std::condition_variable m_drawMutexCondition;

  VkCommandBuffer m_primary;

  static void threadMaster(void* arg)
  {
    ThreadJob* job = (ThreadJob*)arg;
    job->renderer->RunThread(job->index);
  }

  bool getWork_ts(size_t& start, size_t& num)
  {
    std::lock_guard<std::mutex> lock(m_workMutex);
    bool                        hasWork = false;

    const size_t chunkSize = m_workingSet;
    size_t       total     = m_drawItems.size();

    if(m_numCurItems < total)
    {
      size_t batch = std::min(total - m_numCurItems, chunkSize);
      start        = m_numCurItems;
      num          = batch;
      m_numCurItems += batch;
      hasWork = true;
    }
    else
    {
      hasWork = false;
      start   = 0;
      num     = 0;
    }

    return hasWork;
  }

  void         RunThread(int index);
  unsigned int RunThreadFrame(ThreadJob& job);

  void enqueueShadeCommand_ts(DrawSetup* sc);

  void drawThreaded(const Resources::Global& global, VkCommandBuffer cmd, Stats& stats);

  void fillCmdBuffer(VkCommandBuffer cmd, BindingMode bindingMode, size_t begin, const DrawItem* drawItems, size_t drawCount)
  {
    const ResourcesVK* res   = m_resources;
    const CadSceneVK&  scene = res->m_scene;

    int lastMaterial = -1;
    int lastGeometry = -1;
    int lastMatrix   = -1;
    int lastObject   = -1;
    int lastShader   = -1;

    VkDeviceAddress matrixAddress   = scene.m_buffers.matrices.address;
    VkDeviceAddress materialAddress = scene.m_buffers.materials.address;

    switch(bindingMode)
    {
      case BINDINGMODE_DSETS:
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawBind.getPipeLayout(), DRAW_UBO_SCENE,
                                1, res->m_drawBind.at(DRAW_UBO_SCENE).getSets(), 0, nullptr);
        break;
      case BINDINGMODE_PUSHADDRESS:
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawPush.getPipeLayout(), 0, 1,
                                res->m_drawPush.getSets(), 0, nullptr);
        break;
      case BINDINGMODE_INDEX_BASEINSTANCE:
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawIndexed.getPipeLayout(), 0, 1,
                                res->m_drawIndexed.getSets(), 0, nullptr);
        break;
      case BINDINGMODE_INDEX_VERTEXATTRIB:
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawIndexed.getPipeLayout(), 0, 1,
                                res->m_drawIndexed.getSets(), 0, nullptr);

        {
          VkDeviceSize offset = {sizeof(uint32_t) * begin};
          VkDeviceSize size   = {VK_WHOLE_SIZE};
          VkDeviceSize stride = {sizeof(uint32_t)};
#if USE_DYNAMIC_VERTEX_STRIDE
          vkCmdBindVertexBuffers2(cmd, 1, 1, &m_combinedIndices[m_cycleCurrent].buffer, &offset, &size, &stride);
#else
          vkCmdBindVertexBuffers(cmd, 1, 1, &m_combinedIndices[m_cycleCurrent].buffer, &offset);
#endif
        }
        break;
    }

    if(m_config.shaderObjs)
    {
      const VkShaderStageFlagBits unusedStages[3] = {VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                                                     VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, VK_SHADER_STAGE_GEOMETRY_BIT};
      vkCmdBindShadersEXT(cmd, 3, unusedStages, nullptr);
    }

    for(size_t i = 0; i < drawCount; i++)
    {
      size_t          idx = m_config.permutated ? m_seqIndices[i + begin] : i + begin;
      const DrawItem& di  = drawItems[idx];

      if(di.shaderIndex != lastShader)
      {
        if(m_config.shaderObjs)
        {
          VkShaderStageFlagBits stages[2]  = {VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT};
          VkShaderEXT           shaders[2] = {res->m_drawShading.vertexShaderObjs[di.shaderIndex],
                                              res->m_drawShading.fragmentShaderObjs[di.shaderIndex]};
          vkCmdBindShadersEXT(cmd, 2, stages, shaders);
        }
        else
        {
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawShading.pipelines[di.shaderIndex]);
        }

        lastShader = di.shaderIndex;
      }

#if USE_DRAW_OFFSETS
      if(lastGeometry != int(scene.m_geometry[di.geometryIndex].allocation.chunkIndex))
      {
        const CadSceneVK::Geometry& geo = scene.m_geometry[di.geometryIndex];

        vkCmdBindIndexBuffer(cmd, geo.ibo.buffer, 0, VK_INDEX_TYPE_UINT32);
        VkDeviceSize offset = {0};
        VkDeviceSize size   = {VK_WHOLE_SIZE};
        VkDeviceSize stride = {sizeof(CadScene::Vertex)};
#if USE_DYNAMIC_VERTEX_STRIDE
        vkCmdBindVertexBuffers2(cmd, 0, 1, &geo.vbo.buffer, &offset, &size, &stride);
#else
        vkCmdBindVertexBuffers(cmd, 0, 1, &geo.vbo.buffer, &offset);
#endif
        lastGeometry = int(scene.m_geometry[di.geometryIndex].allocation.chunkIndex);
      }
#else
      if(lastGeometry != di.geometryIndex)
      {
        const CadSceneVK::Geometry& geo    = scene.m_geometry[di.geometryIndex];
        VkDeviceSize                stride = {sizeof(CadScene::Vertex)};

        vkCmdBindIndexBuffer(cmd, geo.ibo.buffer, geo.ibo.offset, VK_INDEX_TYPE_UINT32);
#if USE_DYNAMIC_VERTEX_STRIDE
        vkCmdBindVertexBuffers2(cmd, 0, 1, &geo.vbo.buffer, &geo.vbo.offset, &geo.vbo.range, &stride);
#else
        vkCmdBindVertexBuffers(cmd, 0, 1, &geo.vbo.buffer, &geo.vbo.offset);
#endif

        lastGeometry = di.geometryIndex;
      }
#endif

      uint32_t firstInstance = 0;

      if(bindingMode == BINDINGMODE_DSETS)
      {
        if(lastMatrix != di.matrixIndex)
        {
          uint32_t offset = di.matrixIndex * res->m_alignedMatrixSize;
          vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawBind.getPipeLayout(),
                                  DRAW_UBO_MATRIX, 1, res->m_drawBind.at(DRAW_UBO_MATRIX).getSets(), 1, &offset);
          lastMatrix = di.matrixIndex;
        }

        if(lastMaterial != di.materialIndex)
        {
          uint32_t offset = di.materialIndex * res->m_alignedMaterialSize;
          vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawBind.getPipeLayout(),
                                  DRAW_UBO_MATERIAL, 1, res->m_drawBind.at(DRAW_UBO_MATERIAL).getSets(), 1, &offset);
          lastMaterial = di.materialIndex;
        }
      }
      else if(bindingMode == BINDINGMODE_PUSHADDRESS)
      {
        if(lastMatrix != di.matrixIndex)
        {
          VkDeviceAddress address = matrixAddress + sizeof(CadScene::MatrixNode) * di.matrixIndex;

          vkCmdPushConstants(cmd, res->m_drawPush.getPipeLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &address);

          lastMatrix = di.matrixIndex;
        }

        if(lastMaterial != di.materialIndex)
        {
          VkDeviceAddress address = materialAddress + sizeof(CadScene::Material) * di.materialIndex;

          vkCmdPushConstants(cmd, res->m_drawPush.getPipeLayout(), VK_SHADER_STAGE_FRAGMENT_BIT,
                             sizeof(VkDeviceAddress), sizeof(VkDeviceAddress), &address);

          lastMaterial = di.materialIndex;
        }
      }
      else if(bindingMode == BINDINGMODE_INDEX_BASEINSTANCE)
      {
        firstInstance = m_indexingBits.packIndices(di.matrixIndex, di.materialIndex);
      }
      else if(bindingMode == BINDINGMODE_INDEX_VERTEXATTRIB)
      {
        firstInstance                    = i;
        m_combinedIndicesData[begin + i] = m_indexingBits.packIndices(di.matrixIndex, di.materialIndex);
      }

      // drawcall
#if USE_DRAW_OFFSETS
      const CadSceneVK::Geometry& geo = scene.m_geometry[di.geometryIndex];
      vkCmdDrawIndexed(cmd, di.range.count, 1, uint32_t(di.range.offset + geo.ibo.offset / sizeof(uint32_t)),
                       geo.vbo.offset / sizeof(CadScene::Vertex), firstInstance);
#else
      vkCmdDrawIndexed(cmd, di.range.count, 1, uint32_t(di.range.offset / sizeof(uint32_t)), 0, firstInstance);
#endif

      lastShader = di.shaderIndex;
    }

    if(m_combinedIndicesData.size())
    {
      // copy
      uint32_t* mapping = (uint32_t*)m_combinedIndicesMappings[m_cycleCurrent];
      memcpy(mapping + begin, m_combinedIndicesData.data() + begin, sizeof(uint32_t) * drawCount);
    }
  }

  void setupCmdBuffer(DrawSetup& sc, nvvk::RingCommandPool& pool, size_t begin, const DrawItem* drawItems, size_t drawCount)
  {
    const ResourcesVK* res = m_resources;

    VkCommandBuffer cmd = pool.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY, false);
    res->cmdBegin(cmd, true, false, true);

    if(m_config.shaderObjs)
    {
      res->cmdShaderObjectState(cmd);
    }
    else
    {
      res->cmdDynamicPipelineState(cmd);
    }

    fillCmdBuffer(cmd, m_config.bindingMode, begin, drawItems, drawCount);

    vkEndCommandBuffer(cmd);
    sc.cmdbuffers.push_back(cmd);
  }
};


static RendererThreadedVK::TypeCmd s_type_cmdmain_vk;

void RendererThreadedVK::init(const CadScene* scene, ResourcesVK* resources, const Config& config, Stats& stats)
{
  ResourcesVK* res = (ResourcesVK*)resources;
  m_resources      = res;
  m_scene          = scene;
  m_config         = config;

  res->initPipelinesOrShaders(config.bindingMode, 0, config.shaderObjs);

  fillDrawItems(m_drawItems, scene, config, stats);
  if(config.permutated)
  {
    m_seqIndices.resize(m_drawItems.size());
    fillRandomPermutation(m_drawItems.size(), m_seqIndices.data(), m_drawItems.data(), stats);
  }

  if(m_config.bindingMode == BINDINGMODE_INDEX_VERTEXATTRIB)
  {
    m_combinedIndicesData.resize(m_drawItems.size());
    for(uint32_t i = 0; i < nvvk::DEFAULT_RING_SIZE; i++)
    {
      m_combinedIndices[i] =
          res->m_resourceAllocator.createBuffer(sizeof(uint32_t) * m_drawItems.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

      m_combinedIndicesMappings[i] = res->m_resourceAllocator.map(m_combinedIndices[i]);
    }
  }

  m_indexingBits = m_scene->getIndexingBits();

  m_threadpool.init(m_config.workerThreads);

  // make jobs
  m_ready       = 0;
  m_jobs        = new ThreadJob[m_config.workerThreads];
  m_stopThreads = 0;

  for(uint32_t i = 0; i < m_config.workerThreads; i++)
  {
    ThreadJob& job = m_jobs[i];
    job.index      = i;
    job.renderer   = this;
    job.m_hasWork  = -1;
    job.m_frame    = 0;

    job.m_pool.init(res->m_device, res->m_context->m_queueGCT);

    m_threadpool.activateJob(i, threadMaster, &m_jobs[i]);
  }

  m_frame = 0;
}

void RendererThreadedVK::deinit()
{
  m_stopThreads = 1;
  m_ready       = 0;

  THREAD_BARRIER();
  for(uint32_t i = 0; i < m_config.workerThreads; i++)
  {
    std::unique_lock<std::mutex> lock(m_jobs[i].m_hasWorkMutex);
    m_jobs[i].m_hasWork = m_frame;
    m_jobs[i].m_hasWorkCond.notify_one();
  }
  m_drawMutexCondition.notify_all();

  std::this_thread::yield();

  {
    std::unique_lock<std::mutex> lock(m_readyMutex);
    while(m_ready < m_config.workerThreads)
    {
      m_readyCond.wait(lock);
    }
  }

  THREAD_BARRIER();

  for(uint32_t i = 0; i < m_config.workerThreads; i++)
  {
    for(size_t s = 0; s < m_jobs[i].m_scs.size(); s++)
    {
      delete m_jobs[i].m_scs[s];
    }
    m_jobs[i].m_pool.deinit();
  }

  for(uint32_t i = 0; i < nvvk::DEFAULT_RING_SIZE; i++)
  {
    if(m_combinedIndices[i].memHandle)
    {
      m_resources->m_resourceAllocator.unmap(m_combinedIndices[i]);
      m_resources->m_resourceAllocator.destroy(m_combinedIndices[i]);
    }
  }

  delete[] m_jobs;

  m_threadpool.deinit();

  m_drawItems.clear();
  m_combinedIndicesData.clear();
}

void RendererThreadedVK::enqueueShadeCommand_ts(DrawSetup* sc)
{
  std::unique_lock<std::mutex> lock(m_drawMutex);

  m_drawQueue.push(sc);
  m_drawMutexCondition.notify_one();
}

unsigned int RendererThreadedVK::RunThreadFrame(ThreadJob& job)
{
  unsigned int dispatches = 0;

  bool   first = true;
  size_t tnum  = 0;
  size_t begin = 0;
  size_t num   = 0;

  size_t offset = 0;

  job.resetFrame();
  job.m_pool.setCycle(m_cycleCurrent);

  if(m_workerBatched || true)
  {
    DrawSetup* sc = job.getFrameCommand();
    while(getWork_ts(begin, num))
    {
      setupCmdBuffer(*sc, job.m_pool, begin, m_drawItems.data(), num);
      tnum += num;
    }
    if(!sc->cmdbuffers.empty())
    {
      enqueueShadeCommand_ts(sc);
      dispatches += 1;
    }
  }
  else
  {
    while(getWork_ts(begin, num))
    {
      DrawSetup* sc = job.getFrameCommand();
      setupCmdBuffer(*sc, job.m_pool, begin, m_drawItems.data(), num);

      if(!sc->cmdbuffers.empty())
      {
        enqueueShadeCommand_ts(sc);
        dispatches += 1;
      }
      tnum += num;
    }
  }

  // nullptr signals we are done
  enqueueShadeCommand_ts(nullptr);

  return dispatches;
}

void RendererThreadedVK::RunThread(int tid)
{
  ThreadJob& job = m_jobs[tid];

  double timeWork    = 0;
  double timeFrame   = 0;
  int    timerFrames = 0;
  size_t dispatches  = 0;

  double timePrint = NVPSystem::getTime();

  while(!m_stopThreads)
  {
    double beginFrame = NVPSystem::getTime();
    timeFrame -= NVPSystem::getTime();
    {
      std::unique_lock<std::mutex> lock(job.m_hasWorkMutex);
      while(job.m_hasWork != job.m_frame)
      {
        job.m_hasWorkCond.wait(lock);
      }
    }

    if(m_stopThreads)
    {
      break;
    }

    double beginWork = NVPSystem::getTime();
    timeWork -= NVPSystem::getTime();

    dispatches += RunThreadFrame(job);

    job.m_frame++;

    timeWork += NVPSystem::getTime();

    double currentTime = NVPSystem::getTime();
    timeFrame += currentTime;

    timerFrames++;

    if(timerFrames && (currentTime - timePrint) > 2.0)
    {
      timeFrame /= double(timerFrames);
      timeWork /= double(timerFrames);

      timeFrame *= 1000000.0;
      timeWork *= 1000000.0;

      timePrint = currentTime;

      float avgdispatch = float(double(dispatches) / double(timerFrames));

#if 1
      LOGI("thread %d: work %6d [us] cmdbuffers %5.1f (avg)\n", tid, uint32_t(timeWork), avgdispatch);
#endif
      timeFrame = 0;
      timeWork  = 0;

      timerFrames = 0;
      dispatches  = 0;
    }
  }

  {
    std::unique_lock<std::mutex> lock(m_readyMutex);
    m_ready++;
    m_readyCond.notify_all();
  }
}


void RendererThreadedVK::drawThreaded(const Resources::Global& global, VkCommandBuffer primary, Stats& stats)
{
  ResourcesVK* res = m_resources;

  m_workingSet    = global.workingSet;
  m_workerBatched = global.workerBatched;
  m_numCurItems   = 0;
  m_numEnqueues   = 0;
  m_cycleCurrent  = res->m_ringFences.getCycleIndex();

  stats.cmdBuffers = 0;

  // generate & cmdbuffers in parallel

  THREAD_BARRIER();

  // start to dispatch threads
  for(uint32_t i = 0; i < m_config.workerThreads; i++)
  {
    {
      std::unique_lock<std::mutex> lock(m_jobs[i].m_hasWorkMutex);
      m_jobs[i].m_hasWork = m_frame;
    }
    m_jobs[i].m_hasWorkCond.notify_one();
  }

  // collect secondaries here
  {
    int numTerminated = 0;
    while(true)
    {
      bool       hadEntry = false;
      DrawSetup* sc       = nullptr;
      {
        std::unique_lock<std::mutex> lock(m_drawMutex);
        if(m_drawQueue.empty())
        {
          m_drawMutexCondition.wait(lock);
        }
        if(!m_drawQueue.empty())
        {

          sc = m_drawQueue.front();
          m_drawQueue.pop();

          hadEntry = true;
        }
      }

      if(hadEntry)
      {
        if(sc)
        {
          m_numEnqueues++;
          THREAD_BARRIER();
          vkCmdExecuteCommands(primary, (uint32_t)sc->cmdbuffers.size(), sc->cmdbuffers.data());
          stats.cmdBuffers += (uint32_t)sc->cmdbuffers.size();
          sc->cmdbuffers.clear();
        }
        else
        {
          numTerminated++;
        }
      }

      if(numTerminated == m_config.workerThreads)
      {
        break;
      }
      std::this_thread::yield();
    }
  }

  m_frame++;

  THREAD_BARRIER();
}

void RendererThreadedVK::draw(const Resources::Global& global, Stats& stats)
{
  ResourcesVK* res = m_resources;

  VkCommandBuffer primary = res->createTempCmdBuffer();
  {
    nvvk::ProfilerVK::Section profile(res->m_profilerVK, "Render", primary);
    {
      nvvk::ProfilerVK::Section profile(res->m_profilerVK, "Draw", primary);

      vkCmdUpdateBuffer(primary, res->m_common.viewBuffer.buffer, 0, sizeof(SceneData), (const uint32_t*)&global.sceneUbo);
      res->cmdPipelineBarrier(primary);
      res->cmdBeginRendering(primary, true);

      drawThreaded(global, primary, stats);


      vkCmdEndRendering(primary);
    }
  }
  vkEndCommandBuffer(primary);
  res->submissionEnqueue(primary);
}


}  // namespace generatedcmds
