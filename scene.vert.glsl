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


#version 440 core
/**/

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable

#include "common.h"

#if UNIFORMS_TECHNIQUE == UNIFORMS_MULTISETSDYNAMIC

  layout(set=DRAW_UBO_SCENE, binding=0, scalar) uniform sceneBuffer {
    SceneData   scene;
  };
  layout(set=DRAW_UBO_MATRIX, binding=0, scalar) uniform objectBuffer {
    MatrixData  matrix;
  };
  
#elif UNIFORMS_TECHNIQUE == UNIFORMS_PUSHCONSTANTS_ADDRESS

  layout(buffer_reference, buffer_reference_align=16, scalar) readonly buffer MatrixBuffer {
    MatrixData  matrix;
  };

  layout(push_constant, scalar) uniform pushConstants {
    MatrixBuffer v;
  };
  #define matrix v.matrix
  
  layout(set=0, binding=DRAW_UBO_SCENE, scalar) uniform sceneBuffer {
    SceneData   scene;
  };

  
#endif


in layout(location=VERTEX_POS_OCTNORMAL)      vec4 inPosNormal;

layout(location=0) out Interpolants {
  vec3 wPos;
  vec3 wNormal;
#if SHADER_PERMUTATION
  vec3 oNormal;
#endif
} OUT;

// oct functions from http://jcgt.org/published/0003/02/01/paper.pdf
vec2 oct_signNotZero(vec2 v) {
  return vec2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
}
vec3 oct_to_float32x3(vec2 e) {
  vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
  if (v.z < 0) v.xy = (1.0 - abs(v.yx)) * oct_signNotZero(v.xy);
  return normalize(v);
}
vec2 float32x3_to_oct(in vec3 v) {
  // Project the sphere onto the octahedron, and then onto the xy plane
  vec2 p = v.xy * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
  // Reflect the folds of the lower hemisphere over the diagonals
  return (v.z <= 0.0) ? ((1.0 - abs(p.yx)) * oct_signNotZero(p)) : p;
}

void main()
{
  vec3 inNormal = oct_to_float32x3(unpackSnorm2x16(floatBitsToUint(inPosNormal.w)));

  vec3 wPos     = (matrix.worldMatrix   * vec4(inPosNormal.xyz,1)).xyz;
  vec3 wNormal  = mat3(matrix.worldMatrixIT) * inNormal;

  gl_Position   = scene.viewProjMatrix * vec4(wPos,1);
  
#if SHADER_PERMUTATION
  OUT.oNormal   = inNormal;
#endif
  
  
  OUT.wPos = wPos;
  OUT.wNormal = wNormal;
}
