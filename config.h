/*
 * Copyright (c) 2021-2024, NVIDIA CORPORATION.  All rights reserved.
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
 * SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

// artificially create a few more shader permutations, pairs of vertex/fragment shaders
#define NUM_MATERIAL_SHADERS 128

// favor using drawcalls firstIndex / firstVertex rather than
// setting index / vertex buffers as much
#define USE_DRAW_OFFSETS 0

// use VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE and vkCmdBindVertexBuffers2
#define USE_DYNAMIC_VERTEX_STRIDE 0

// enforces single buffers for vbo/ibo
#define USE_SINGLE_GEOMETRY_ALLOCATION 0
