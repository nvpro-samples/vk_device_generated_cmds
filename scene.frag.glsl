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
    SceneData       scene;
  };
  layout(set=DRAW_UBO_MATERIAL, binding=0, scalar) uniform materialBuffer {
    MaterialData    material;
  };
  
#elif UNIFORMS_TECHNIQUE == UNIFORMS_PUSHCONSTANTS_ADDRESS

  layout(set=0, binding=DRAW_UBO_SCENE, scalar) uniform sceneBuffer {
    SceneData   scene;
  };
  layout(buffer_reference, buffer_reference_align=16, scalar) readonly buffer MaterialBuffer {
    MaterialData  material;
  };
  
  layout(push_constant, scalar) uniform pushConstants {
    layout(offset=8)
    MaterialBuffer v;
  };
  #define material v.material
  
#endif


layout(location=0) in Interpolants {
  vec3 wPos;
  vec3 wNormal;
#if SHADER_PERMUTATION
  vec3 oNormal;
#endif
} IN;

layout(location=0,index=0) out vec4 out_Color;

void main()
{
  MaterialSide side = material.sides[gl_FrontFacing ? 1 : 0];

  vec4 color = side.ambient + side.emissive;
#if SHADER_PERMUTATION
  ivec2 pixel = ivec2(gl_FragCoord.xy);
  pixel /= (SHADER_PERMUTATION % 8) + 1;
  pixel %= (SHADER_PERMUTATION % 2) + 1;
  pixel = ivec2(1) - pixel;
  
  color = mix(color, vec4(IN.oNormal*0.5+0.5, 1), vec4(0.5) * float(pixel.x * pixel.y));
  color += 0.001 * float(SHADER_PERMUTATION);
#endif

  vec3 eyePos = vec3(scene.viewMatrixIT[0].w,scene.viewMatrixIT[1].w,scene.viewMatrixIT[2].w);

  vec3 lightDir = normalize( scene.wLightPos.xyz - IN.wPos);
  vec3 viewDir  = normalize( eyePos - IN.wPos);
  vec3 halfDir  = normalize(lightDir + viewDir);
  vec3 normal   = normalize(IN.wNormal) * (gl_FrontFacing ? 1 : -1);

  float ldot = dot(normal,lightDir);
  normal *= sign(ldot);
  ldot   *= sign(ldot);

  color += side.diffuse * ldot;
  color += side.specular * pow(max(0,dot(normal,halfDir)),16);
  
  out_Color = color;
}
