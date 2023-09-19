/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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
 * SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

// use vulkan 1.2 otherwise 1.1 with EXT_buffer_device_address
#define USE_VULKAN_1_2_BUFFER_ADDRESS   1
// use pipeline referencing N pipelines for DGC, otherwise single pipeline with N shadergroups
#define USE_PIPELINE_REFERENCES         1
// artificially create a few more shader permutations
#define NUM_MATERIAL_SHADERS            128
