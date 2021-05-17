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



#ifndef CSFTHREADED_COMMON_H
#define CSFTHREADED_COMMON_H

#define VERTEX_POS_OCTNORMAL      0

// changing these orders may break a lot of things ;)
#define DRAW_UBO_SCENE     0
#define DRAW_UBO_MATRIX    1
#define DRAW_UBO_MATERIAL  2

#define ANIM_UBO              0
#define ANIM_SSBO_MATRIXOUT   1
#define ANIM_SSBO_MATRIXORIG  2

#define ANIMATION_WORKGROUPSIZE 256

#ifndef SHADER_PERMUTATION
#define SHADER_PERMUTATION 1
#endif

//////////////////////////////////////////////////////////////////////////

// see resources_vk.hpp

#ifndef UNIFORMS_MULTISETSDYNAMIC
#define UNIFORMS_MULTISETSDYNAMIC 0
#endif
#ifndef UNIFORMS_PUSHCONSTANTS_ADDRESS
#define UNIFORMS_PUSHCONSTANTS_ADDRESS 1
#endif
#ifndef UNIFORMS_TECHNIQUE
#define UNIFORMS_TECHNIQUE UNIFORMS_PUSHCONSTANTS_ADDRESS
#endif

//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
namespace generatedcmds {
  using namespace nvmath;
#endif

struct SceneData {
  mat4  viewProjMatrix;
  mat4  viewMatrix;
  mat4  viewMatrixIT;

  vec4  viewPos;
  vec4  viewDir;
  
  vec4  wLightPos;
  
  ivec2 viewport;
  ivec2 _pad;
};

// must match cadscene
struct MatrixData {
  mat4 worldMatrix;
  mat4 worldMatrixIT;
  mat4 objectMatrix;
  mat4 objectMatrixIT;
};

// must match cadscene
struct MaterialSide {
  vec4 ambient;
  vec4 diffuse;
  vec4 specular;
  vec4 emissive;
};

struct MaterialData {
  MaterialSide sides[2];
};

struct AnimationData {
  uint    numMatrices;
  float   time;
  vec2   _pad0;

  vec3    sceneCenter;
  float   sceneDimension;
};

#ifdef __cplusplus
}
#endif


#endif
