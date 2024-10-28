# Vulkan Device Generated Commands Sample

The "device generated cmds" sample demonstrates the use of the [VK_NV_device_generated_commands](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_NV_device_generated_commands.html) and [VK_EXT_device_generated_commands](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_device_generated_commands.html) (**DGC**) extensions.

We recommend to have a look at this [blog post](https://devblogs.nvidia.com/new-vulkan-device-generated-commands) about the extension first.

The EXT_device_generated_commands works slightly different in detail, but is conceptually very similar.

|  | NV | EXT |
|--|----|-----|
| input stream            | flexible mix or interleaved or separated | single interleaved |
| indirect shader binding | `VkPipeline` can be extended with a fixed set of immutable `VkGraphicsShaderGroupCreateInfoNV` that are indexed at runtime | `VkIndirectExecutionSet` is a table that is indexed at runtime and does allow dynamic updates of slots with `VkPipeline` or `VkShaderEXT` |
| draw calls              | one draw per indirect command sequence | one draw, or passing an indirect buffer address and count (allows binning draw calls by state) |

The sample furthermore allows to compare the impact of a few alternative ways to do the same work:
* Generate the draw calls by different means (**renderer** in UI):
  * `re-used cmds`: The entire scene is encoded in a single big command-buffer, and re-used every frame.
  * `threaded cmds`: Each thread has FRAMES many CommandBufferPools, which are cycled through. At the beginning the pool is reset and command-buffers are generated from in chunks. Using another pool every frame avoids the use of additional fences.
  Secondary commandbuffers are generated on the worker threads and passed for enqueing into a primary commandbuffer that is later submitted on the main thread.
  * `generated cmds nv/ext`: Makes use of the DGC extension to generate the command buffer and render it (more details later).
  * `preprocess,generated cmds nv/ext`: Uses the separate preprocess step of the DGC extension and then renders the command buffer (`VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT/NV`). This allows us to measure the performance of the preprocessing operation in isolation. A separate preprocess may be useful to prepare work on an async compute queue.

* Switch shader parameter bindings through (**bindings** in UI):
  * `dsetbinding`: Traditional descriptorset bindings that use `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC` for the per-draw material and matrix data (only works on CPU-driven renderers)
  * `pushaddress`: Push constants that pass GPU pointers via [GLSL_EXT_buffer_reference](https://github.com/KhronosGroup/GLSL/blob/main/extensions/ext/GLSL_EXT_buffer_reference.txt) (core feature in Vulkan 1.2)
  * `baseinstance index`: Encoded material and matrix indices that are stored in the bits of `gl_BaseInstance` and accessed in the vertex shader.
  * `inst.vertexattrib index`: Encoded material and matrix indices that are stored in an instanced vertex attribute (`VK_VERTEX_INPUT_RATE_INSTANCE`) which itself is indexed via `firstInstance` and passed to the vertex shader. Fastest for very small draw calls, though a bit clumsy to use.

* Bind shaders differently (**shaders** in UI):
  * `pipeline`: Uses traditional `VkPipeline` 
  * `shaderobjs`: Uses vertex & fragment shader pairs of `VkShaderEXT` as provided by [VK_EXT_shader_object](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_shader_object.html)
  In the [scene.vert.glsl](scene.vert.glsl) and [scene.frag.glsl](scene.frag.glsl)  shaders check the `UNIFORMS_TECHNIQUE` define for differences.


The content being rendered in the sample is a CAD model which is made of many parts that have few triangles. Having such low complexity per draw-call can very often result into being CPU bound and also serves as stress test for the GPU's command processor.

The sample was a derived of the [threaded cadscene sample](https://github.com/nvpro-samples/gl_threaded_cadscene).
Please refer to its readme, as it explains the scene's principle setup.

For simplicity the draw call information for DGC is also created on CPU and uploaded once. In real-world applications we would use compute shaders for this and implement something like scene traversal and occlusion culling on the device.

![sample screenshot](doc/sample.png)

## Options

Beyond the key comparisons listed above there is a few more options provided.

### Scene Complexity

* **strategy** influences the amount of draw calls and as result the triangles per draw call
  * `object individual surfaces`: Each CAD surface of an object has its own draw call (stress test, very few triangles per draw). An object like a box would have six side surfaces and a cylinder has three (top, bottom palnes and curved surface). For this model it means an average of ony ~ 10 triangles per draw call. Such low complexity draw calls would better be handled by a task shader emitting mesh shaders.
  * `object material groups`: Draw calls are combined within an object based on their materials and whether their index buffer regions can be merged together.
  * `object as single mesh`: One draw call per object, we take the material from the first surface, hence the rendered image will be slightly different.
* **copies**: How many models are replicated in the scene (geometry buffers are re-used to save memory, but no hardware instancing is used for the draw calls).
* **pct visible**: Percentage of visible drawcalls.
* **max shadergroups**: Sets the subset of active shaders that are used in the scene. Each shader group is a unique vertex/fragment shader pair. `shaderIndex = materialIndex % maxShaderGroups`

To reduce the amount of commands we do some level of redundant state filtering when generating the commands for each draw call.
Because this scene has such few triangles per draw call, this is highly recommended.

A particular new feature that the DGC extension provides is the ability to switch shaders on the device. For that purpose
the sample generates up to 128 artificial vertex/fragment shader combinations, which yield different polygon stippling patterns.

When **max shadergroups** is set to `1` then indirect shader binds will be disabled in the DGC renderers. As the support is optional in the DGC extensions, the value will get automatically set to one on absence of the feature.

### Renderer Settings

* **sorted once (minimized state changes)**: Global sorting of drawcalls depending on state.
* **permutated (random state changes, gen nv: use seqindex)**: Compute a random permutation of drawcalls (stresses state changes).
The "generated nv" renderers make use of the `VK_INDIRECT_COMMANDS_LAYOUT_USAGE_INDEXED_SEQUENCES_BIT_NV / sequencesIndexBuffer`.
* **gen: unordered (non-coherent)**: The "generate" renderers use the `VK_INDIRECT_COMMANDS_LAYOUT_USAGE_UNORDERED_SEQUENCES_BIT_EXT/NV`.
This allows the hardware to ignore the original drawcall ordering, which is recommended and a lot faster. However, it can introduce a bit more z-flickering due to re-ordering of drawcalls.
* **gen ext: binned via draw_indexed_count**: In this mode we combine draw calls of the same state using a separate indirect command buffer and leverage the `VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_COUNT_EXT` to launch the draws. This is best combined with **sorted once** for best performance and lowest memory use.
* **gen nv: interleaved inputs**: The inputs for the command generation are provided as single interleaved buffer (AoS). Otherwise each input has its own buffer section (SoA). Only affects NV_dgc
* **threaded: worker threads**: How many threads are used to generate the command buffers.
* **threaded: drawcalls per cmdbuffer**: How many drawcalls per command buffer.
* **threaded: batched submission**: Each thread collects all secondary command buffers and passes them once to the main thread.
* **animation**: Animates the matrices.

## Device Generated Commands

For an overview on this extension, we recommend to have a look at this [article](https://devblogs.nvidia.com/new-vulkan-device-generated-commands).

There is a few principle steps:

1. Define a sequence of commands you want to generate as `IndirectCommandsLayoutEXT/NV`
2. If you want the ability to change shaders, create your graphics pipelines with `VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT` / `VK_PIPELINE_CREATE_INDIRECT_BINDABLE_BIT_NV` or your shader objects with `VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT`.
  * EXT: Create a `VkIndirectExecutionSet` and fill it via `vkUpdateIndirectExecutionSetPipelineEXT` or `vkUpdateIndirectExecutionSetShaderEXT`
  * NV: Create an aggregate graphice pipeline that imports those pipelines as graphics shader groups using `VkGraphicsPipelineShaderGroupsCreateInfoNV`
    which extends `VkGraphicsPipelineCreateInfo`.
3. Create a preprocess buffer based on sizing information acquired by `vkGetGeneratedCommandsMemoryRequirementsEXT/NV`.
4. Fill your input buffer(s) for the generation step and setup `VkGeneratedCommandsInfoEXT/NV` accordingly.
5. Optionally use a separate preprocess step via `vkCmdPreprocessGeneratedCommandsEXT/NV`.
6. Run the execution via `vkCmdExecuteGeneratedCommandsEXT/NV`.

### Highlighted Files

* [resources_vk.cpp](resources_vk.cpp): contains most of the scene data, shaders / pipelines that is the same for all renderers.
  * `ResourcesVK::initPipelinesOrShaders`: is called by the renderers depending on special purpose flags.
* [renderer_vkgen_ext.cpp](renderer_vkgen_ext.cpp): contains the new renderers for EXT.
  * `RendererVKGenEXT::initIndirectExecutionSet` creates the `VkIndirectExecutionSetEXT`
  * `RendererVKGenEXT::initIndirectCommandsLayout` creates the `VkIndirectCommandsLayoutEXT`
  * `RendererVKGenEXT::setupInputInterleaved` or `RendererVKGenEXT::setupInputBinned` show how the input buffers are filled
  * `RendererVKGenEXT::setupPreprocess` handles the sizing and setup of the preprocess buffer
  * `RendererVKGenEXT::getGeneratedCommandsInfo`, `RendererVKGenEXT::cmdExecute` and `RendererVKGenEXT::cmdPreprocess` are the functions relevant for generating the commands.
  * `RendererVKGenEXT::cmdStates` sets up the common rendering state prior launch of the generated commands.
* [renderer_vkgen_nv.cpp](renderer_vkgen_nv.cpp): contains the new renderers for NV.
  * `RendererVKGenNV::initShaderGroupsPipeline` creates the `VkPipeline` via `VkGraphicsPipelineShaderGroupsCreateInfoNV` 
  * `RendererVKGenNV::initIndirectCommandsLayout` creates the `VkIndirectCommandsLayoutEXT`
  * `RendererVKGenNV::setupInputInterleaved` or `RendererVKGenEXT::setupInputSeparate` show how the input buffers are filled
  * `RendererVKGenNV::setupPreprocess` handles the sizing and setup of the preprocess buffer
  * `RendererVKGenNV::getGeneratedCommandsInfo`, `RendererVKGenEXT::cmdExecute` and `RendererVKGenEXT::cmdPreprocess` are the functions relevant for generating the commands.
  * `RendererVKGenNV::cmdStates` sets up the common rendering state prior launch of the generated commands.


### Indirect Shader Binding

#### EXT_device_generated_commands

Similar to descriptor sets, there exists `VkIndirectExecutionSetEXT` which serves as a binding table with a fixed upper count.

Type | Stored Objects | Function to register | Pipeline / Shader Flag |
-----|----------------|----------------------|------------------------|
`VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT` | `VkPipeline` | `vkUpdateIndirectExecutionSetPipelineEXT` | `VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT` |
`VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT` | `VkShaderEXT` | `vkUpdateIndirectExecutionSetShaderEXT` | `VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT` |

The `VK_INDIRECT_COMMANDS_TOKEN_TYPE_EXECUTION_SET_EXT` requires either a single `uint32_t` input for pipelines, or shader-stage-many `uint32_t` for shader objects.

``` cpp
typedef struct VkIndirectExecutionSetCreateInfoEXT
{
  VkStructureType                   sType;
  void const*                       pNext;
  VkIndirectExecutionSetInfoTypeEXT type;
  VkIndirectExecutionSetInfoEXT     info;
  // either pointer to VkIndirectExecutionSetPipelineInfoEXT or 
  //                   VkIndirectExecutionSetShaderInfoEXT

} VkIndirectExecutionSetCreateInfoEXT;

// for pipelines 

typedef struct VkIndirectExecutionSetPipelineInfoEXT
{
  VkStructureType sType;
  void const*     pNext;
  VkPipeline      initialPipeline;
  uint32_t        maxPipelineCount;
} VkIndirectExecutionSetPipelineInfoEXT;

// for shader objects

typedef struct VkIndirectExecutionSetShaderLayoutInfoEXT
{
  VkStructureType              sType;
  void const*                  pNext;
  uint32_t                     setLayoutCount;
  VkDescriptorSetLayout const* pSetLayouts;
} VkIndirectExecutionSetShaderLayoutInfoEXT;

typedef struct VkIndirectExecutionSetShaderInfoEXT
{
  VkStructureType                                  sType;
  void const*                                      pNext;

  uint32_t                                         shaderCount;
  VkShaderEXT const*                               pInitialShaders;
  VkIndirectExecutionSetShaderLayoutInfoEXT const* pSetLayoutInfos;

  // the size of the table
  uint32_t                                         maxShaderCount;

  uint32_t                                         pushConstantRangeCount;
  VkPushConstantRange const*                       pPushConstantRanges;
} VkIndirectExecutionSetShaderInfoEXT;

// Updating is similar to descriptor set writes.
// Developers must ensure that the `index` is not currently
// in use by the device.

typedef struct VkWriteIndirectExecutionSetPipelineEXT
{
  VkStructureType sType;
  void const*     pNext;
  uint32_t        index;
  VkPipeline      pipeline;
} VkWriteIndirectExecutionSetPipelineEXT;

typedef struct VkWriteIndirectExecutionSetShaderEXT
{
  VkStructureType sType;
  void const*     pNext;
  uint32_t        index;
  VkShaderEXT     shader;
} VkWriteIndirectExecutionSetShaderEXT;
```

#### NV_device_generated_commands

The ray tracing extension introduced the notion of "ShaderGroups" that are stored within a pipeline object.
This extension makes use of the same principle to store multiple shader groups within a graphics pipeline object.

Each shader group can override a subset of the pipeline's state:

``` cpp
typedef struct VkGraphicsShaderGroupCreateInfoNV
{
  // A shadergroup, is a set of unique shader combinations (VS,FS,...) etc.
  // that all are stored within a single graphics pipeline that share
  // most of the state.
  // Must not mix mesh with traditional pipeline.
  VkStructureType sType;
  const void*     pNext;

  // overrides createInfo from original graphicsPipeline
  uint32_t                                          stageCount;
  const VkPipelineShaderStageCreateInfo*            pStages;
  const VkPipelineVertexInputStateCreateInfo*       pVertexInputState;
  const VkPipelineTessellationStateCreateInfo*      pTessellationState;
} VkGraphicsShaderGroupCreateInfoNV;


typedef struct VkGraphicsPipelineShaderGroupsCreateInfoNV
{
  // extends regular VkGraphicsPipelineCreateInfo
  // If bound via vkCmdBindPipeline will behave as if pGroups[0] is active,
  // otherwise bind using vkCmdBindPipelineShaderGroup with proper index
  VkStructureType                          sType;
  const void*                              pNext;

  // first shader group must match the pipeline's traditional shader stages
  uint32_t                                 groupCount;
  const VkGraphicsShaderGroupCreateInfoNV* pGroups;
  
  // we recommend importing shader groups from regular pipelines that were created with
  // `VK_PIPELINE_CREATE_INDIRECT_BINDABLE_BIT_NV` and are compatible in state
  uint32_t                                 pipelineCount;
  const VkPipeline*                        pPipelines; 
} VkGraphicsPipelineShaderGroupsCreateInfoNV;
```

You can bind a shader group using `vkCmdBindPipelineShaderGroupNV(.... groupIndex)`.
However, the primary use-case is to bind them indirectly on the device via `VK_INDIRECT_COMMANDS_TOKEN_TYPE_SHADER_GROUP_NV` which is a single `uint32_t` index into the array of shader groups.

> Important Note: To make any graphics pipeline bindable by the device set the `VK_PIPELINE_CREATE_INDIRECT_BINDABLE_BIT_NV` flag.
> This is also true for imported pipelines.

To speed up creation of a pipeline that contains many pipelines, you can pass existing
pipelines to be referenced via `pPipelines`. You must ensure that those referenced
pipelines are alive as long as the referencing pipeline is alive.
The referenced pipelines must match in all state, except for what can be overridden
per shader group. The shader groups from such imported pipelines are virtually appended in order of 
the array.

With this mechanism you can easily collect existing pipelines (thought don't forget the bindable flag and the state compatibility),
which should ease the integration of this technology.


### IndirectCommandsLayoutEXT/NV

The DGC extension allows you to generate some common graphics commands on the device based on a pre-defined sequence of command tokens.
This sequence is encoded in the `IndirectCommandsLayoutEXT/NV` object.

The following pseudo code illustrates the kind of state changes you can make.
You will see that there is no ability to change the descriptor set bindings, which is why
this sample showcases the passing of bindings via push constants.
This is somewhat similar to ray tracing as well, where you manage all resources globally
as well.

``` cpp
void cmdProcessSequence(cmd, pipeline, indirectCommandsLayout, pIndirectCommandsStreams, uint32_t s)
{
  for (uint32_t t = 0; t < indirectCommandsLayout.tokenCount; t++){
    token = indirectCommandsLayout.pTokens[t];

#if NV_device_generated_commands
    uint streamIndex  = token.stream;
#else
    // EXT_dgc has single interleaved stream
    uint streamIndex  = 0;
#endif

    uint32_t stride   = indirectCommandsLayout.pStreamStrides[token.stream];
    stream            = pIndirectCommandsStreams[token.stream];
    uint32_t offset   = stream.offset + stride * s + token.offset;
    const void* input = stream.buffer.pointer( offset )

    switch(input.type){
#if NV_device_generated_commands
    VK_INDIRECT_COMMANDS_TOKEN_TYPE_SHADER_GROUP_NV:
      VkBindShaderGroupIndirectCommandNV* bind = input;

      // the pipeline must have been created with
      // VK_PIPELINE_CREATE_INDIRECT_BINDABLE_BIT_NV
      // and VkGraphicsPipelineShaderGroupsCreateInfoNV

      vkCmdBindPipelineShaderGroupNV(cmd, indirectCommandsLayout.pipelineBindPoint,
        pipeline, bind->groupIndex);
    break;

    VK_INDIRECT_COMMANDS_TOKEN_TYPE_STATE_FLAGS_NV:
      VkSetStateFlagsIndirectCommandNV* state = input;

      if (token.indirectStateFlags & VK_INDIRECT_STATE_FLAG_FRONTFACE_BIT_NV){
        if (state.data & (1 << 0)){
          setState(VK_FRONT_FACE_CLOCKWISE);
        } else {
          setState(VK_FRONT_FACE_COUNTER_CLOCKWISE);
        }
      }
    break;
#endif

#if EXT_device_generated_commands
    VK_INDIRECT_COMMANDS_TOKEN_TYPE_EXECUTION_SET_EXT:
      uint32_t* data = input;

      if (token.pExecutionSet->type == VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT) {
        // the pipeline must have been created with
        // VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT (not the same as NV!)
        // and registered via vkUpdateIndirectExecutionSetPipelineEXT

        uint32_t pipelineIndex = *data;
        vkCmdBindPipeline(cmd, activePipelineBindPoint, indirectExecutionSet.pipelines[pipelineIndex]);
      }
      else if (token.pExecutionSet->type == VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT) {

        // the shaders must have been created with
        // VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT
        // and registered via vkUpdateIndirectExecutionSetShaderEXT

        // iterate in lowest to highest bit order
        for (shaderStageBit : iterateSetBits(token.pExecutionSet->shaderStages))
        {
          uint32_t shaderIndex = *data;
          vkCmdBindShadersEXT(cmd, 1, &shaderStageBit, &indirectExecutionSet.shaders[shaderIndex])
          data++;
        }
      }

    break;

    VK_INDIRECT_COMMANDS_TOKEN_TYPE_SEQUENCE_INDEX_EXT:
      uint32_t sequenceIndex = s;

      vkCmdPushConstants(cmd,
        activePipelineLayout,
        token.pPushConstant->updateRange.stageFlags,
        token.pPushConstant->updateRange.offset,
        token.pPushConstant->updateRange.size, &sequenceIndex);
    break;
#endif

    // we focus on the EXT flavors here
    // NV variants are pretty similar

    VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_EXT:
      uint32_t* data = input;

      vkCmdPushConstants(cmd,
        activePipelineLayout,
        token.pPushConstant->updateRange.stageFlags,
        token.pPushConstant->updateRange.offset,
        token.pPushConstant->updateRange.size, data);
    break;

    VK_INDIRECT_COMMANDS_TOKEN_TYPE_INDEX_BUFFER_EXT:
      VkBindIndexBufferIndirectCommandEXT* data = input;

      // NV: the indexType may optionally be remapped
      // from a custom uint32_t value, via
      // VkIndirectCommandsLayoutTokenNV::pIndexTypeValues
      
      // EXT: the input mode VkIndirectCommandsIndexBufferTokenEXT::mode
      // can be set to DXGI
      
      vkCmdBindIndexBuffer2KHR(cmd,
        deriveBuffer(data->bufferAddress),
        deriveOffset(data->bufferAddress),
        data->size,
        data->indexType);
    break;

    VK_INDIRECT_COMMANDS_TOKEN_TYPE_VERTEX_BUFFER_EXT:
      VkBindVertexBufferIndirectCommandEXT* data = input;

      vkCmdBindVertexBuffers2(cmd,
        token.pVertexBuffer->vertexBindingUnit, 1,
        &deriveBuffer(data->bufferAddress),
        &deriveOffset(data->bufferAddress),
        &data.size,
        &data.stride);
    break;

    // regular draws use an inlined draw indirect struct
    // directly from the input stream

    VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_EXT:
      vkCmdDrawIndexedIndirect(cmd,
        stream.buffer, offset, 1, 0);
    break;

    VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_EXT:
      vkCmdDrawIndirect(cmd,
        stream.buffer,
        offset, 1, 0);
    break;

    VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_EXT:
      vkCmdDrawMeshTasksIndirectEXT(cmd,
        stream.buffer, offset, 1, 0);
    break;

#if EXT_device_generated_commands

    // NEW for EXT_dgc is that we can use gpu-sourced indirect buffer
    // and count. The indirect draw calls can be stored anywhere.

    VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_COUNT_EXT:
      VkDrawIndirectCountIndirectCommandEXT* data = input;

      vkCmdDrawIndexedIndirect(cmd,
        deriveBuffer(data->bufferAddress), deriveOffset(data->bufferAddress), data->count, data->stride);
    break;

    VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_COUNT_EXT:
      VkDrawIndirectCountIndirectCommandEXT* data = input;

      vkCmdDrawIndirect(cmd,
        deriveBuffer(data->bufferAddress), deriveOffset(data->bufferAddress), data->count, data->stride);
    break;

    VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_COUNT_EXT:
      VkDrawIndirectCountIndirectCommandEXT* data = input;

      vkCmdDrawMeshTasksIndirectEXT(cmd,
        deriveBuffer(data->bufferAddress), deriveOffset(data->bufferAddress), data->count, data->stride);
    break;
#endif
    }
  }
}
```

The sequence generation itself is also influenced by a few usage flags, as follows:

``` cpp
cmdProcessAllSequences(
    cmd, pipeline, indirectCommandsLayout, pIndirectCommandsTokens,
    uint32_t maxSequencesCount,
    sequencesCountBuffer, uint32_t sequencesCount, 
    sequencesIndexBuffer, uint64_t sequencesIndexOffset)
{
  uint32_t sequencesCount = sequencesCountBuffer ?
    min(maxSequencesCount, sequencesCountBuffer.load_uint32(sequencesCountOffset) :
    maxSequencesCount;


  for (uint32_t s = 0; s < sequencesCount; s++)
  {
    uint32_t sUsed = s;

#if NV_device_generated_commands
    if (indirectCommandsLayout.flags & VK_INDIRECT_COMMANDS_LAYOUT_USAGE_INDEXED_SEQUENCES_BIT_NV) {
      sUsed = sequencesIndexBuffer.load_uint32( sUsed * sizeof(uint32_t) + sequencesIndexOffset);
    }
#endif

    if (indirectCommandsLayout.flags & VK_INDIRECT_COMMANDS_LAYOUT_USAGE_UNORDERED_SEQUENCES_BIT_EXT) {
      sUsed = incoherent_implementation_dependent_permutation[ sUsed ];
    }

    cmdProcessSequence(cmd, pipeline, indirectCommandsLayout, pIndirectCommandsTokens, sUsed);
  }
}
```

### Preprocess Buffer

The NVIDIA implementation of the DGC extension needs some device space to generate the commands prior their execution.
The preprocess buffer provides this space, and is sized via `vkGetGeneratedCommandsMemoryRequirementsEXT/NV`.

As you can see below, it depends on the `pipeline` (NV) or `indirectExecutionSet` (EXT), as well as the `indirectCommandsLayout` and the number of maximum
sequences and or draws you may generate. Most sizing information is based on the token type alone, however there can be some variable costs.
Especially the complexity of the shader group changes influences the number of bytes a lot. The more similar the shader groups
are, the less memory will be required.

``` cpp
typedef struct VkGeneratedCommandsMemoryRequirementsInfoEXT
{
  VkStructureType             sType;
  void*                       pNext;

  VkIndirectExecutionSetEXT   indirectExecutionSet;

  VkIndirectCommandsLayoutEXT indirectCommandsLayout;

  uint32_t                    maxSequenceCount;
  uint32_t                    maxDrawCount;     // upper limit for the DRAW.._COUNT tokens
} VkGeneratedCommandsMemoryRequirementsInfoEXT;

typedef struct VkGeneratedCommandsMemoryRequirementsInfoNV
{
  VkStructureType                     sType;
  const void*                         pNext;

  VkPipelineBindPoint                 pipelineBindPoint;
  VkPipeline                          pipeline;
  
  VkIndirectCommandsLayoutNV          indirectCommandsLayout;

  uint32_t                            maxSequencesCount;
} VkGeneratedCommandsMemoryRequirementsInfoNV;
```

The sample shows the size of the buffer in the UI as **"preprocessBuffer ... KB"**.
As of writing, the size may be substantial for a very large number of drawcalls.
If you need to stay within a memory budget, you can split your execution into multiple passes
and re-use the preprocess memory.

If you make use of the dedicated preprocess step through `vkCmdPreprocessGeneratedCommandsEXT/NV`, then you must
ensure all inputs (all buffer content etc.) are the same at execution time. An implementation is allowed to split
the workload required for execution into these two functions.

You can see the time it takes to preprocess in the UI as **"Preproc. GPU [ms]"** if you selected the **preprocess,generate cmds** renderer.

Note that the NVIDIA implementation uses an internal compute dispatch to build the preprocess buffer, therefore it is recommended to 
batch multiple explicit preprocessing steps prior the execution calls. Otherwise the barriers between implicit preprocessing and execution can slow things down significantly.

### Performance

Preliminary results from **NVIDIA RTX 6000 Ada Generation, AMD Ryzen 9 7950X 16-Core Processor, Windows 11 64-bit**.

At the time of writing the EXT_device_generated_commands implementation was new, future improvements to performance and preprocess memory may happen.

The settings are an extreme stress-test of very tiny draw calls. Such draw calls with less than a few hundred triangles
should either be avoided by design or be handled by a task shader emitting meshlets:

* 8 copies
* ~ 547 K draw calls
* ~ 5.2 M triangles (**only ~ 10 triangles** per draw call!)
* ~ 70 K serial shader binds (alternating between 16 shadergroups)
* `object individual surfaces` strategy
* `pipeline` shaders
* `unordered` active
* traditional renderers use `inst.vertexattrib index`  bindings
* threaded command buffer generation results in 134 command buffers across 16 threads
  We account the time it takes to generate the commands on the CPU as "preprocess" time.
* EXT generated commands uses `inst.vertexattrib index` bindings
  4 tokens: EXECUTION_SET, INDEX_BUFFER, VERTEX_BUFFER, DRAW_INDEXED or DRAW_INDEXED_COUNT
* NV generated commands uses `pushaddress` bindings
  6 tokens: SHADER_GROUP, INDEX_BUFFER, VERTEX_BUFFER, PUSH VERTEX, PUSH FRAGMENT, DRAW_INDEXED

#### **sorted once** OFF

renderer                                | shaders         | preprocess [ms] | draw (GPU) [ms] | dgc execution size [MB] | sequences |
----------------------------------------| --------------- | --------------- | --------------- |-------------------------|---------- |
re-used cmds                            | `pipeline`      |                 |   6.9           |                         |           |
re-used cmds                            | `shaderobjs`    |                 |   5.9           |                         |           |
threaded cmds (16 threads)              | `pipeline`      |  2.4  (CPU)     |   7.1           |                         |           |
threaded cmds (16 threads)              | `shaderobjs`    |  2.2  (CPU)     |   6.1           |                         |           |
preprocess,generated ext                | `pipeline`      |  0.4  (GPU)     |   7.0           |  436 preprocess         |  547 K    |
preprocess,generated ext                | `shaderobjs`    |  0.2  (GPU)     |   6.3           |  180 preprocess         |  547 K    |
preprocess,generated ext (**binned**)   | `pipeline`      |  0.3  (GPU)     |  24.4 !         |   54 preprocess<br>10 drawindirect |  70 K  |
preprocess,generated ext (**binned**)   | `shaderobjs`    |  0.1  (GPU)     |  21.0 !         |   21 preprocess<br>10 drawindirect |  70 K  |
preprocess,generated nv                 | `pipeline`      |  0.2  (GPU)     |   4.7 *         |  145 preprocess         |  547 K    |

We can see that with so many shader changes, shader objects do better across the renderers that support them.
They particularly help EXT_dgc to reduce its preprocess buffer.

NV_dgc is currently fastest due to the static nature of the pipeline table containing the shader groups, allowing more optimizations.

The renderers that **binned** (DRAW.._COUNT tokens) do particularly bad here, as without state sorting, there is not a lot of binning going on.
This still results in very high number of sequences (70 K) and it creates extra latency when each sequence launches a multi-draw-indirect with little work (approximately 7-8 draw calls).
With so many sequences it's faster to inline the draw call data, at the cost of higher memory.
However, we recommend to avoid such a design in the first place and always do a bit of state sorting / binning.

#### **sorted once** ON

When we do state sorting, the `pushaddress` bindings method is the fastest for most renderers, except **binned** which keeps `inst.vertexattrib index`.
This sometimes increases tokens for EXT_dgc to 6 as well.

renderer (**sorted once**)              | shaders      |preprocess [ms] | draw (GPU) [ms] | dgc execution size [MB] | sequences |
--------------------------------------- | ------------ | -------------- | --------------- |------------------------ | --------- |
re-used cmds                            | `pipeline`   |                |   1.5           |                         |           |
re-used cmds                            | `shaderobjs` |                |   1.5           |                         |           |
threaded cmds (16 threads)              | `pipeline`   |   1.2  (CPU)   |   1.8           |                         |           |
threaded cmds (16 threads)              | `shaderobjs` |   1.2  (CPU)   |   1.8           |                         |           |
preprocess,generated ext                | `pipeline`   |   0.2  (GPU)   |   1.4           |  436   preprocess       |  547 K    |
preprocess,generated ext                | `shaderobjs` |   0.2  (GPU)   |   1.3           |  196   preprocess       |  547 K    |
preprocess,generated ext (**binned**)   | `pipeline`   | < 0.1  (GPU)   |   1.5           |    0.2 preprocess<br>10 drawindirect |  16  |
preprocess,generated ext (**binned**)   | `shaderobjs` | < 0.1  (GPU)   |   1.5           |    0.1 preprocess<br>10 drawindirect |  16  |
preprocess,generated nv                 | `pipeline`   |   0.2  (GPU)   |   1.3           |  145   preprocess       |  547 K    |

Sorting by state speeds things up significantly for this scene.

This especially shows the benefit of using EXT_dgc with draw indirect calls **binned** into few sequences as it reduces memory cost substantially.
In the test we used up to 16 shaders, hence we get 16 state buckets, equal to 16 sequences here. While it's not the very fastest technique here,
in reality it should do best, because normally the individual draw calls will have more triangles.

> However, we want to stress again, that the main goal of this
> extension is not generating so much work or simply moving things on the GPU,
> but leveraging it to reduce the actual work needed.
>
> For example by using [occlusion culling](https://github.com/nvpro-samples/gl_occlusion_culling),
> or other techniques that allow you to reduce the required workload
> on the device.


### Recommendations

* Use EXT_device_generated_commands going forward
* If possible use state bucketing to lower the amount of sequences, and then distribute draw calls in a separate indirect buffer and leverage `VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_..._COUNT`. This greatly reduces the preprocess buffer size and preprocessing workload.
* For optimization potential across shader-stages we recommend binding via `VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT` and state bucketing.
* When not doing state bucketing, then avoid the use of the `VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_..._COUNT` as it may not create enough work to hide the latency of the indirect launch.
* Use explicit preprocessing (`VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT` and `vkCmdPreprocessGeneratedCommandsEXT`) and batch preprocessing prior execution.
* Do not use lots of interleaved preprocessing and executing, which is also the result when calling `vkCmdExecuteGeneratedCommandsEXT` with `isPreprocessed == false` many times.

## Acknowledgements

Special thanks to [Mike Blumenkrantz](https://www.supergoodcode.com/device-generated-commands/) for the long push to make `VK_EXT_device_generated_commands` a reality, as well as Patrick Doane for the initial kickstart.

## Building
Make sure to have installed the [Vulkan-SDK](http://lunarg.com/vulkan-sdk/). Always use 64-bit build configurations.

Ideally, clone this and other interesting [nvpro-samples](https://github.com/nvpro-samples) repositories into a common subdirectory. You will always need [nvpro_core](https://github.com/nvpro-samples/nvpro_core). The nvpro_core is searched either as a subdirectory of the sample, or in a common parent directory.

If you are interested in multiple samples, you can use [build_all](https://github.com/nvpro-samples/build_all) CMAKE as entry point, it will also give you options to enable/disable individual samples when creating the solutions.
