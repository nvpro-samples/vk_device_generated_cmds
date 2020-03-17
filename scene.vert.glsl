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
