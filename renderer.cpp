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



