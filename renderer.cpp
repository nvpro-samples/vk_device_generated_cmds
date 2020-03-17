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


#include <assert.h>
#include <algorithm>
#include "renderer.hpp"
#include <nvpwindow.hpp>

#include <nvmath/nvmath_glsltypes.h>

#include "common.h"

#pragma pack(1)


namespace generatedcmds
{
  //////////////////////////////////////////////////////////////////////////

  static void AddItem(std::vector<Renderer::DrawItem>& drawItems, const Renderer::Config& config, const Renderer::DrawItem& di)
  {
    if (di.range.count) {
      drawItems.push_back(di);
    }
  }

  static void FillCache(std::vector<Renderer::DrawItem>& drawItems, const Renderer::Config& config, const CadScene::Object& obj, const CadScene::Geometry& geo, bool solid, int objectIndex)
  {
    int begin = 0;
    const CadScene::DrawRangeCache &cache = solid ? obj.cacheSolid : obj.cacheWire;

    for (size_t s = 0; s < cache.state.size(); s++)
    {
      const CadScene::DrawStateInfo &state = cache.state[s];
      for (int d = 0; d < cache.stateCount[s]; d++) {
        // evict
        Renderer::DrawItem di;
        di.geometryIndex = obj.geometryIndex;
        di.matrixIndex = state.matrixIndex;
        di.materialIndex = state.materialIndex;
        di.shaderIndex   = state.materialIndex % config.maxShaders;

        di.solid = solid;
        di.range.offset = cache.offsets[begin + d];
        di.range.count = cache.counts[begin + d];

        AddItem(drawItems, config, di);
      }
      begin += cache.stateCount[s];
    }
  }
  
  static void FillIndividual( std::vector<Renderer::DrawItem>& drawItems, const Renderer::Config& config, const CadScene::Object& obj, const CadScene::Geometry& geo, bool solid, int objectIndex )
  {
    for (size_t p = 0; p < obj.parts.size(); p++){
      const CadScene::ObjectPart&   part = obj.parts[p];
      const CadScene::GeometryPart& mesh = geo.parts[p];

      if (!part.active) continue;

      Renderer::DrawItem di;
      di.geometryIndex = obj.geometryIndex;
      di.matrixIndex   = part.matrixIndex;
      di.materialIndex = part.materialIndex;
      di.shaderIndex   = part.materialIndex % config.maxShaders;

      di.solid = solid;
      di.range = mesh.indexSolid;

      AddItem(drawItems, config, di);
    }
  }

  void Renderer::fillDrawItems( std::vector<DrawItem>& drawItems, const CadScene* NV_RESTRICT scene, const Config& config, Stats& stats )
  {
    bool solid = true;
    bool wire = false;

    size_t maxObjects = scene->m_objects.size();
    size_t from = std::min(maxObjects - 1, size_t(config.objectFrom));
    maxObjects = std::min(maxObjects, from + size_t(config.objectNum));

    for (size_t i = from; i < maxObjects; i++){
      const CadScene::Object& obj = scene->m_objects[i];
      const CadScene::Geometry& geo = scene->m_geometry[obj.geometryIndex];

      if (config.strategy == STRATEGY_GROUPS){
        if (solid)  FillCache(drawItems, config, obj, geo, true,  int(i));
        if (wire)   FillCache(drawItems, config, obj, geo, false, int(i));
      }
      else if (config.strategy == STRATEGY_INDIVIDUAL) {
        if (solid)  FillIndividual(drawItems, config, obj, geo, true, int(i));
        if (wire)   FillIndividual(drawItems, config, obj, geo, false, int(i));
      }
    }

    if(config.sorted)
    {
      std::sort(drawItems.begin(), drawItems.end(), DrawItem_compare_groups);
    }

    int shaderIndex = -1;
    for (size_t i = 0; i < drawItems.size(); i++) {
      stats.drawCalls++;
      stats.drawTriangles += drawItems[i].range.count / 3;
      if (drawItems[i].shaderIndex != shaderIndex) {
        stats.shaderBindings++;
        shaderIndex = drawItems[i].shaderIndex;
      }
    }
  }

  void Renderer::fillRandomPermutation(uint32_t drawCount, uint32_t* permutation, const DrawItem* drawItems, Stats& stats)
  {
    srand(634523);
    for(uint32_t i = 0; i < drawCount; i++)
    {
      permutation[i] = i;
    }
    if(drawCount)
    {
      // not exactly a good way to generate random 32bit ;)
      for(uint32_t i = drawCount - 1; i > 0; i--)
      {
        uint32_t r = 0;
        r |= (rand() & 0xFF) << 0;
        r |= (rand() & 0xFF) << 8;
        r |= (rand() & 0xFF) << 16;
        r |= (rand() & 0xFF) << 24;

        uint32_t other = r % (i + 1);
        std::swap(permutation[i], permutation[other]);
      }

      int shaderIndex = -1;
      stats.shaderBindings = 0;
      for(uint32_t i = 0; i < drawCount; i++)
      {
        uint32_t idx = permutation[i];
        if (drawItems[idx].shaderIndex != shaderIndex) {
          stats.shaderBindings++;
          shaderIndex = drawItems[idx].shaderIndex;
        }
      }
    }
  }

  }



