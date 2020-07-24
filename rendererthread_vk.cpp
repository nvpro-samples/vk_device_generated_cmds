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

#include <algorithm>
#include <assert.h>
#include <mutex>
#include <queue>

#include "renderer.hpp"
#include "resources_vk.hpp"
#include "threadpool.hpp"
#include <nvh/nvprint.hpp>
#include <nvmath/nvmath_glsltypes.h>
#include <nvpwindow.hpp>

#include "common.h"


namespace generatedcmds {

//////////////////////////////////////////////////////////////////////////


class RendererThreadedVK : public Renderer
{
public:
  enum Mode
  {
    MODE_CMD_MAINSUBMIT,
  };


  class TypeCmd : public Renderer::Type
  {
    bool        isAvailable(const nvvk::Context& context) const { return true; }
    const char* name() const { return "threaded cmds"; }
    Renderer*   create() const
    {
      RendererThreadedVK* renderer = new RendererThreadedVK();
      renderer->m_mode             = MODE_CMD_MAINSUBMIT;
      return renderer;
    }
    unsigned int priority() const { return 10; }

    Resources* resources() { return ResourcesVK::get(); }
  };

public:
  void init(const CadScene* NV_RESTRICT scene, Resources* res, const Config& config, Stats& stats) override;
  void deinit() override;
  void draw(const Resources::Global& global, Stats& stats) override;

  uint32_t supportedBindingModes() override { return (1 << BINDINGMODE_DSETS) | (1 << BINDINGMODE_PUSHADDRESS); };


  Mode m_mode;

  RendererThreadedVK()
      : m_mode(MODE_CMD_MAINSUBMIT)
  {
  }

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


  std::vector<DrawItem> m_drawItems;
  std::vector<uint32_t> m_seqIndices;
  ResourcesVK* NV_RESTRICT m_resources;
  int                      m_numThreads;

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
  void submitShadeCommand_ts(DrawSetup* sc);

  void drawThreaded(const Resources::Global& global, VkCommandBuffer cmd, Stats& stats);

  void fillCmdBuffer(VkCommandBuffer cmd, BindingMode bindingMode, const DrawItem* NV_RESTRICT drawItems, size_t drawCount)
  {
    const ResourcesVK* res   = m_resources;
    const CadSceneVK&  scene = res->m_scene;

    int lastMaterial = -1;
    int lastGeometry = -1;
    int lastMatrix   = -1;
    int lastObject   = -1;
    int lastShader   = -1;

    VkBufferDeviceAddressInfoEXT addressInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_EXT};
    addressInfo.buffer                       = scene.m_buffers.matrices;
    VkDeviceAddress matrixAddress            = vkGetBufferDeviceAddressEXT(res->m_device, &addressInfo);
    addressInfo.buffer                       = scene.m_buffers.materials;
    VkDeviceAddress materialAddress          = vkGetBufferDeviceAddressEXT(res->m_device, &addressInfo);

    if(bindingMode == BINDINGMODE_DSETS)
    {
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawBind.getPipeLayout(), DRAW_UBO_SCENE, 1,
                              res->m_drawBind.at(DRAW_UBO_SCENE).getSets(), 0, NULL);
    }
    else if(bindingMode == BINDINGMODE_PUSHADDRESS)
    {
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawPush.getPipeLayout(), DRAW_UBO_SCENE, 1,
                              res->m_drawPush.getSets(), 0, NULL);
    }

    for(size_t i = 0; i < drawCount; i++)
    {
      uint32_t        idx = m_config.permutated ? m_seqIndices[i] : uint32_t(i);
      const DrawItem& di  = drawItems[idx];

      if(di.shaderIndex != lastShader)
      {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, res->m_drawShading[bindingMode].pipelines[di.shaderIndex]);
      }

      if(lastGeometry != di.geometryIndex)
      {
        const CadSceneVK::Geometry& geo = scene.m_geometry[di.geometryIndex];

        vkCmdBindVertexBuffers(cmd, 0, 1, &geo.vbo.buffer, &geo.vbo.offset);
        vkCmdBindIndexBuffer(cmd, geo.ibo.buffer, geo.ibo.offset, VK_INDEX_TYPE_UINT32);

        lastGeometry = di.geometryIndex;
      }

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
      // drawcall
      vkCmdDrawIndexed(cmd, di.range.count, 1, uint32_t(di.range.offset / sizeof(uint32_t)), 0, 0);

      lastShader = di.shaderIndex;
    }
  }

  void setupCmdBuffer(DrawSetup& sc, nvvk::RingCommandPool& pool, const DrawItem* NV_RESTRICT drawItems, size_t drawCount)
  {
    const ResourcesVK* NV_RESTRICT res = m_resources;

    VkCommandBuffer cmd = pool.createCommandBuffer(
        m_mode == MODE_CMD_MAINSUBMIT ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
    res->cmdBegin(cmd, true, false, true);

    res->cmdDynamicState(cmd);
    fillCmdBuffer(cmd, m_config.bindingMode, drawItems, drawCount);

    vkEndCommandBuffer(cmd);
    sc.cmdbuffers.push_back(cmd);
  }
};


static RendererThreadedVK::TypeCmd s_type_cmdmain_vk;

void RendererThreadedVK::init(const CadScene* NV_RESTRICT scene, Resources* resources, const Config& config, Stats& stats)
{
  ResourcesVK* NV_RESTRICT res = (ResourcesVK*)resources;
  m_resources                  = res;
  m_scene                      = scene;
  m_config                     = config;

  fillDrawItems(m_drawItems, scene, config, stats);
  if(config.permutated)
  {
    m_seqIndices.resize(m_drawItems.size());
    fillRandomPermutation(m_drawItems.size(), m_seqIndices.data(), m_drawItems.data(), stats);
  }

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

  NV_BARRIER();
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

  NV_BARRIER();

  for(uint32_t i = 0; i < m_config.workerThreads; i++)
  {
    for(size_t s = 0; s < m_jobs[i].m_scs.size(); s++)
    {
      delete m_jobs[i].m_scs[s];
    }
    m_jobs[i].m_pool.deinit();
  }

  delete[] m_jobs;

  m_threadpool.deinit();

  m_drawItems.clear();
}

void RendererThreadedVK::enqueueShadeCommand_ts(DrawSetup* sc)
{
  std::unique_lock<std::mutex> lock(m_drawMutex);

  m_drawQueue.push(sc);
  m_drawMutexCondition.notify_one();
}


void RendererThreadedVK::submitShadeCommand_ts(DrawSetup* sc)
{
  {
    std::unique_lock<std::mutex> lock(m_drawMutex);
    NV_BARRIER();
    VkSubmitInfo submitInfo       = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = (uint32_t)sc->cmdbuffers.size();
    submitInfo.pCommandBuffers    = sc->cmdbuffers.data();
    vkQueueSubmit(m_resources->m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    NV_BARRIER();
  }

  sc->cmdbuffers.clear();
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

  if(m_workerBatched)
  {
    DrawSetup* sc = job.getFrameCommand();
    while(getWork_ts(begin, num))
    {
      setupCmdBuffer(*sc, job.m_pool, &m_drawItems[begin], num);
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
      setupCmdBuffer(*sc, job.m_pool, &m_drawItems[begin], num);

      if(!sc->cmdbuffers.empty())
      {
        enqueueShadeCommand_ts(sc);
        dispatches += 1;
      }
      tnum += num;
    }
  }

  // NULL signals we are done
  enqueueShadeCommand_ts(NULL);

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
    //NV_BARRIER();

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
      LOGI("thread %d: work %6d [us] dispatches %5.1f\n", tid, uint32_t(timeWork), avgdispatch);
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

  NV_BARRIER();

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
      DrawSetup* sc       = NULL;
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
          NV_BARRIER();
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

  NV_BARRIER();
}

void RendererThreadedVK::draw(const Resources::Global& global, Stats& stats)
{
  ResourcesVK* res = m_resources;

  VkCommandBuffer primary = res->createTempCmdBuffer();
  {
    nvvk::ProfilerVK::Section profile(res->m_profilerVK, "Render", primary);
    {
      nvvk::ProfilerVK::Section profile(res->m_profilerVK, "Draw", primary);

      vkCmdUpdateBuffer(primary, res->m_common.viewBuffer, 0, sizeof(SceneData), (const uint32_t*)&global.sceneUbo);
      res->cmdPipelineBarrier(primary);
      res->cmdBeginRenderPass(primary, true, true);

      drawThreaded(global, primary, stats);

      vkCmdEndRenderPass(primary);
    }
  }
  vkEndCommandBuffer(primary);
  res->submissionEnqueue(primary);
}


}  // namespace generatedcmds
