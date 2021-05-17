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



#ifndef RENDERER_H__
#define RENDERER_H__

#include "resources.hpp"
#include <nvh/profiler.hpp>

// disable state filtering for buffer binds
#define USE_NOFILTER          0
// print per-thread stats
#define PRINT_TIMER_STATS     1

namespace generatedcmds {

  enum Strategy {         // per-object 
    STRATEGY_GROUPS,      // sorted and combined parts by material
    STRATEGY_INDIVIDUAL,  // keep all parts individual
  };

  class Renderer {
  public:
    struct Stats {
      uint32_t drawCalls = 0;
      uint32_t drawTriangles = 0;
      uint32_t shaderBindings = 0;
      uint32_t preprocessSizeKB = 0;
      uint32_t cmdBuffers = 0;
    };

    struct Config {
      Strategy      strategy;
      BindingMode   bindingMode;
      uint32_t      objectFrom;
      uint32_t      objectNum;
      uint32_t      maxShaders = 16;
      uint32_t      workerThreads;
      bool          interleaved = false;
      bool          sorted = false;
      bool          unordered = false;
      bool          permutated = false;
    };
  
    struct DrawItem {
      bool                solid;
      int                 materialIndex;
      int                 geometryIndex;
      int                 matrixIndex;
      int                 shaderIndex;
      CadScene::DrawRange range;
    };

    static inline bool DrawItem_compare_groups(const DrawItem& a, const DrawItem& b)
    {
      int diff = 0;
      diff     = diff != 0 ? diff : (a.solid == b.solid ? 0 : (a.solid ? -1 : 1));
      diff     = diff != 0 ? diff : (a.shaderIndex - b.shaderIndex);
      diff     = diff != 0 ? diff : (a.geometryIndex - b.geometryIndex);
      diff     = diff != 0 ? diff : (a.materialIndex - b.materialIndex);
      diff     = diff != 0 ? diff : (a.matrixIndex - b.matrixIndex);

      return diff < 0;
    }

    class Type {
    public:
      Type() {
        getRegistry().push_back(this);
      }

    public:
      virtual bool         isAvailable(const nvvk::Context& context) const = 0;
      virtual const char* name() const = 0;
      virtual Renderer* create() const = 0;
      virtual unsigned int priority() const { return 0xFF; } 

      virtual Resources* resources() = 0;
    };

    typedef std::vector<Type*> Registry;

   static Registry& getRegistry()
   {
     static Registry s_registry;
     return s_registry;
   }

  public:
    virtual void init(const CadScene* NV_RESTRICT scene, Resources* resources, const Config& config, Stats& stats) {}
    virtual void deinit() {}
    virtual void draw(const Resources::Global& global, Stats& stats) {}
    virtual uint32_t supportedBindingModes() { return 0xFF;}

    virtual ~Renderer() {}
    
    void fillDrawItems( std::vector<DrawItem>& drawItems, const CadScene* NV_RESTRICT scene, const Config& config, Stats& stats);
    void fillRandomPermutation(uint32_t drawCount, uint32_t* permutation, const DrawItem* drawItems, Stats& stats);

    Config                       m_config;
    const CadScene* NV_RESTRICT  m_scene;
   
  };
}

#endif
