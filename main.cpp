/*
 * Copyright (c) 2019-2024, NVIDIA CORPORATION.  All rights reserved.
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
 * SPDX-FileCopyrightText: Copyright (c) 2019-2024 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */


/* Contact ckubisch@nvidia.com (Christoph Kubisch) for feedback */

#define DEBUG_FILTER 1

#include "vk_ext_device_generated_commands.hpp"
#include <imgui/imgui_helper.h>

#include <nvvk/appwindowprofiler_vk.hpp>

#include <nvh/cameracontrol.hpp>
#include <nvh/fileoperations.hpp>
#include <nvh/geometry.hpp>

#include <algorithm>

#include "renderer.hpp"
#include "threadpool.hpp"
#include "resources_vk.hpp"
#include "glm/gtc/matrix_access.hpp"

namespace generatedcmds {
int const SAMPLE_SIZE_WIDTH(1024);
int const SAMPLE_SIZE_HEIGHT(960);

void setupVulkanContextInfo(nvvk::ContextCreateInfo& info)
{
  info.apiMajor = 1;
  info.apiMinor = 3;

  static VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjsFeatureExt = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT};
  info.addDeviceExtension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME, true, &shaderObjsFeatureExt, VK_EXT_SHADER_OBJECT_SPEC_VERSION);

#if 1
  static VkPhysicalDeviceDeviceGeneratedCommandsFeaturesNV dgcFeaturesNv = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_NV};
  info.addDeviceExtension(VK_NV_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME, true, &dgcFeaturesNv,
                          VK_NV_DEVICE_GENERATED_COMMANDS_SPEC_VERSION);

  static VkPhysicalDeviceDeviceGeneratedCommandsFeaturesEXT dgcFeaturesExt = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_EXT};
  info.addDeviceExtension(VK_EXT_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME, true, &dgcFeaturesExt,
                          VK_EXT_DEVICE_GENERATED_COMMANDS_SPEC_VERSION);

#if _DEBUG
  // extensions don't work with validation layer
#if 1
  info.removeInstanceLayer("VK_LAYER_KHRONOS_validation");
#else

  // Removing the handle wrapping to the KHRONOS validation layer
  // See: https://vulkan.lunarg.com/doc/sdk/1.3.275.0/linux/khronos_validation_layer.html
  static const char*    layer_name      = "VK_LAYER_KHRONOS_validation";
  static const VkBool32 handle_wrapping = VK_FALSE;

  static const VkLayerSettingEXT settings[] = {
      {layer_name, "handle_wrapping", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1, &handle_wrapping},
  };

  static VkLayerSettingsCreateInfoEXT layerSettingsCreateInfo = {
      .sType        = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT,
      .settingCount = static_cast<uint32_t>(std::size(settings)),
      .pSettings    = settings,
  };

  info.instanceCreateInfoExt = &layerSettingsCreateInfo;
#endif
#endif
#endif
}


class Sample : public nvvk::AppWindowProfilerVK
{

  enum GuiEnums
  {
    GUI_SHADERS,
    GUI_BINDINGS,
    GUI_RENDERER,
    GUI_STRATEGY,
    GUI_MSAA,
  };

public:
  struct Tweak
  {
    int         renderer      = 0;
    BindingMode binding       = BINDINGMODE_INDEX_VERTEXATTRIB;
    Strategy    strategy      = STRATEGY_GROUPS;
    int         msaa          = 4;
    int         copies        = 4;
    bool        unordered     = true;
    bool        interleaved   = true;
    bool        sorted        = false;
    bool        permutated    = false;
    bool        binned        = false;
    bool        animation     = false;
    bool        animationSpin = false;
    int         useShaderObjs = 0;
    uint32_t    maxShaders    = 16;
    int         cloneaxisX    = 1;
    int         cloneaxisY    = 1;
    int         cloneaxisZ    = 1;
    float       percent       = 1.01f;
    uint32_t    workingSet    = 4096;
    uint32_t    workerThreads = 4;
    bool        workerBatched = true;
  };


  bool     m_useUI              = true;
  bool     m_supportsShaderObjs = false;
  bool     m_supportsBinning    = false;
  bool     m_supportsNV         = false;
  uint32_t m_maxThreads         = 1;

  ImGuiH::Registry m_ui;
  double           m_uiTime = 0;

  Tweak m_tweak;
  Tweak m_lastTweak;
  bool  m_lastVsync;

  CadScene                  m_scene;
  std::vector<unsigned int> m_renderersSorted;
  std::string               m_rendererName;

  Renderer*         m_renderer = nullptr;
  ResourcesVK       m_resources;
  Resources::Global m_shared;
  Renderer::Stats   m_renderStats;

  std::string m_modelFilename;
  double      m_animBeginTime;

  double m_lastFrameTime = 0;
  double m_frames        = 0;

  double m_statsFrameTime    = 0;
  double m_statsCpuTime      = 0;
  double m_statsGpuTime      = 0;
  double m_statsGpuDrawTime  = 0;
  double m_statsGpuBuildTime = 0;

  bool initProgram();
  bool initScene(const char* filename, int clones, int cloneaxis);
  void initRenderer(int type);
  void deinitRenderer();
  void initResources();

  void setupConfigParameters();
  void setRendererFromName();

  Sample()
      : AppWindowProfilerVK(false)
  {
    m_maxThreads          = ThreadPool::sysGetNumCores();
    m_tweak.workerThreads = m_maxThreads;

    setupConfigParameters();
    setupVulkanContextInfo(m_contextInfo);
#if defined(NDEBUG)
    setVsync(false);
#endif
  }

public:
  bool validateConfig() override;

  void postBenchmarkAdvance() override { setRendererFromName(); }

  bool begin() override;
  void think(double time) override;
  void resize(int width, int height) override;

  void processUI(int width, int height, double time);

  nvh::CameraControl m_control;

  void end() override;

  // return true to prevent m_window updates
  bool mouse_pos(int x, int y) override
  {
    if(!m_useUI)
      return false;

    return ImGuiH::mouse_pos(x, y);
  }
  bool mouse_button(int button, int action) override
  {
    if(!m_useUI)
      return false;

    return ImGuiH::mouse_button(button, action);
  }
  bool mouse_wheel(int wheel) override
  {
    if(!m_useUI)
      return false;

    return ImGuiH::mouse_wheel(wheel);
  }
  bool key_char(int key) override
  {
    if(!m_useUI)
      return false;

    return ImGuiH::key_char(key);
  }
  bool key_button(int button, int action, int mods) override
  {
    if(!m_useUI)
      return false;

    return ImGuiH::key_button(button, action, mods);
  }
};


bool Sample::initProgram()
{
  return true;
}

bool Sample::initScene(const char* filename, int clones, int cloneaxis)
{
  std::string modelFilename(filename);

  if(!nvh::fileExists(filename))
  {
    modelFilename = nvh::getFileName(filename);
    std::vector<std::string> searchPaths;
    searchPaths.push_back("./");
    searchPaths.push_back(exePath() + PROJECT_RELDIRECTORY);
    searchPaths.push_back(exePath() + PROJECT_DOWNLOAD_RELDIRECTORY);
    modelFilename = nvh::findFile(modelFilename, searchPaths);
  }

  m_scene.unload();

  bool status = m_scene.loadCSF(modelFilename.c_str(), clones, cloneaxis);
  if(status)
  {
    LOGI("\nscene %s\n", filename);
    LOGI("geometries: %6d\n", uint32_t(m_scene.m_geometry.size()));
    LOGI("materials:  %6d\n", uint32_t(m_scene.m_materials.size()));
    LOGI("nodes:      %6d\n", uint32_t(m_scene.m_matrices.size()));
    LOGI("objects:    %6d\n", uint32_t(m_scene.m_objects.size()));
    LOGI("\n");
  }
  else
  {
    LOGW("\ncould not load model %s\n", modelFilename.c_str());
  }

  m_shared.animUbo.numMatrices = uint(m_scene.m_matrices.size());

  return status;
}

void Sample::deinitRenderer()
{
  if(m_renderer)
  {
    m_resources.synchronize();
    m_renderer->deinit();
    delete m_renderer;
    m_renderer = nullptr;
  }
}

void Sample::initResources()
{
  std::string            prepend;
  CadScene::IndexingBits bits = m_scene.getIndexingBits();
  prepend += nvh::ShaderFileManager::format("#define INDEXED_MATRIX_BITS %d\n", bits.matrices);
  prepend += nvh::ShaderFileManager::format("#define INDEXED_MATERIAL_BITS %d\n", bits.materials);

  bool valid = m_resources.init(&m_context, &m_swapChain, &m_profiler);
  valid = valid && m_resources.initFramebuffer(m_windowState.m_swapSize[0], m_windowState.m_swapSize[1], m_tweak.msaa, getVsync());
  valid               = valid && m_resources.initPrograms(exePath(), prepend);
  valid               = valid && m_resources.initScene(m_scene);
  m_resources.m_frame = 0;

  if(!valid)
  {
    LOGE("resource initialization failed\n");
    exit(-1);
  }

  m_lastVsync = getVsync();
}

void Sample::initRenderer(int typesort)
{
  int type = m_renderersSorted[typesort];

  deinitRenderer();

  {
    uint32_t    supported = Renderer::getRegistry()[type]->supportedBindingModes();
    BindingMode mode      = BINDINGMODE_DSETS;
    m_ui.enumReset(GUI_BINDINGS);
    if(supported & (1 << BINDINGMODE_DSETS))
    {
      m_ui.enumAdd(GUI_BINDINGS, BINDINGMODE_DSETS, "dsetbinding");
      mode = BINDINGMODE_DSETS;
    }
    if(supported & (1 << BINDINGMODE_PUSHADDRESS))
    {
      m_ui.enumAdd(GUI_BINDINGS, BINDINGMODE_PUSHADDRESS, "pushaddress");
      mode = BINDINGMODE_PUSHADDRESS;
    }
    if(supported & (1 << BINDINGMODE_INDEX_BASEINSTANCE) && m_scene.supportsIndexing())
    {
      m_ui.enumAdd(GUI_BINDINGS, BINDINGMODE_INDEX_BASEINSTANCE, "baseinstance index");
      mode = BINDINGMODE_INDEX_BASEINSTANCE;
    }
    if(supported & (1 << BINDINGMODE_INDEX_VERTEXATTRIB) && m_scene.supportsIndexing())
    {
      m_ui.enumAdd(GUI_BINDINGS, BINDINGMODE_INDEX_VERTEXATTRIB, "inst.vertexattrib index");
      mode = BINDINGMODE_INDEX_VERTEXATTRIB;
    }

    if(!(supported & (1 << m_tweak.binding)))
    {
      m_tweak.binding = mode;
    }
  }

  {
    bool supported     = Renderer::getRegistry()[type]->supportsShaderObjs();
    bool useShaderObjs = false;
    m_ui.enumReset(GUI_SHADERS);
    m_ui.enumAdd(GUI_SHADERS, SHADERMODE_PIPELINE, "pipeline");
    if(supported)
    {
      m_ui.enumAdd(GUI_SHADERS, SHADERMODE_OBJS, "shaderobjs");
    }

    if(!supported && m_tweak.useShaderObjs)
    {
      m_tweak.useShaderObjs = false;
    }
  }

  if(m_tweak.sorted)
  {
    m_tweak.permutated = false;
  }

  m_tweak.maxShaders = std::min(m_tweak.maxShaders, std::min(uint32_t(NUM_MATERIAL_SHADERS),
                                                             Renderer::getRegistry()[type]->supportedShaderBinds()));
  m_tweak.maxShaders = std::max(m_tweak.maxShaders, uint32_t(1));

  Renderer::Config config;
  config.objectFrom    = 0;
  config.objectNum     = uint32_t(double(m_scene.m_objects.size()) * double(m_tweak.percent));
  config.strategy      = m_tweak.strategy;
  config.bindingMode   = m_tweak.binding;
  config.sorted        = m_tweak.sorted;
  config.binned        = m_tweak.binned;
  config.interleaved   = m_tweak.interleaved;
  config.unordered     = m_tweak.unordered;
  config.permutated    = m_tweak.permutated;
  config.maxShaders    = m_tweak.maxShaders;
  config.workerThreads = m_tweak.workerThreads;
  config.shaderObjs    = m_tweak.useShaderObjs != 0;

  m_renderStats = Renderer::Stats();

  LOGI("renderer: %s\n", Renderer::getRegistry()[type]->name());
  m_renderer = Renderer::getRegistry()[type]->create();
  m_renderer->init(&m_scene, &m_resources, config, m_renderStats);

  LOGI("drawCalls:    %9d\n", m_renderStats.drawCalls);
  LOGI("drawTris:     %9d\n", m_renderStats.drawTriangles);
  LOGI("shaderBinds:  %9d\n", m_renderStats.shaderBindings);
  LOGI("prep.Buffer:  %9d KB\n\n", m_renderStats.preprocessSizeKB);
}


void Sample::end()
{
  deinitRenderer();
  m_resources.deinit();
  ResourcesVK::deinitImGui(m_context);
}


bool Sample::begin()
{
#if !PRINT_TIMER_STATS
  m_profilerPrint = false;
  m_timeInTitle   = true;
#else
  m_profilerPrint = true;
  m_timeInTitle   = true;
#endif


  ImGuiH::Init(m_windowState.m_winSize[0], m_windowState.m_winSize[1], this);

  if(m_context.hasDeviceExtension(VK_EXT_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME))
  {
    bool loaded = load_VK_EXT_device_generated_commands(m_context.m_instance, m_context.m_device);
    if(!loaded)
    {
      LOGE("Failed to load functions for VK_EXT_DEVICE_GENERATED_COMMANDS_EXTENSION\n");
      return false;
    }

    VkPhysicalDeviceDeviceGeneratedCommandsPropertiesEXT props = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_EXT};
    VkPhysicalDeviceProperties2 props2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    props2.pNext                       = &props;
    vkGetPhysicalDeviceProperties2(m_context.m_physicalDevice, &props2);

    if(props.deviceGeneratedCommandsMultiDrawIndirectCount)
    {
      m_supportsBinning = true;
    }
  }
  m_supportsNV         = m_context.hasDeviceExtension(VK_NV_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME);
  m_supportsShaderObjs = m_context.hasDeviceExtension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME);

  bool validated(true);
  validated = validated && initProgram();
  validated = validated
              && initScene(m_modelFilename.c_str(), m_tweak.copies - 1,
                           (m_tweak.cloneaxisX << 0) | (m_tweak.cloneaxisY << 1) | (m_tweak.cloneaxisZ << 2));

  if(!validated)
  {
    LOGE("resources failed\n");
    return false;
  }

  ResourcesVK::initImGui(m_context);

  const Renderer::Registry registry = Renderer::getRegistry();
  for(size_t i = 0; i < registry.size(); i++)
  {
    if(registry[i]->isAvailable(m_context))
    {
      uint sortkey = uint(i);
      sortkey |= registry[i]->priority() << 16;
      m_renderersSorted.push_back(sortkey);
    }
  }

  if(m_renderersSorted.empty())
  {
    LOGE("No renderers available\n");
    return false;
  }

  std::sort(m_renderersSorted.begin(), m_renderersSorted.end());

  for(size_t i = 0; i < m_renderersSorted.size(); i++)
  {
    m_renderersSorted[i] &= 0xFFFF;
  }

  for(size_t i = 0; i < m_renderersSorted.size(); i++)
  {
    LOGI("renderers found: %d %s\n", uint32_t(i), registry[m_renderersSorted[i]]->name());
  }

  setRendererFromName();

  if(m_useUI)
  {
    auto& imgui_io       = ImGui::GetIO();
    imgui_io.IniFilename = nullptr;

    for(size_t i = 0; i < m_renderersSorted.size(); i++)
    {
      m_ui.enumAdd(GUI_RENDERER, int(i), registry[m_renderersSorted[i]]->name());
    }

    m_ui.enumAdd(GUI_STRATEGY, STRATEGY_GROUPS, "object material groups");
    m_ui.enumAdd(GUI_STRATEGY, STRATEGY_INDIVIDUAL, "object individual surfaces");
    m_ui.enumAdd(GUI_STRATEGY, STRATEGY_SINGLE, "object as single mesh");

    m_ui.enumAdd(GUI_MSAA, 0, "none");
    m_ui.enumAdd(GUI_MSAA, 2, "2x");
    m_ui.enumAdd(GUI_MSAA, 4, "4x");
    m_ui.enumAdd(GUI_MSAA, 8, "8x");
  }

  m_control.m_sceneOrbit     = glm::vec3(m_scene.m_bbox.max + m_scene.m_bbox.min) * 0.5f;
  m_control.m_sceneDimension = glm::length((m_scene.m_bbox.max - m_scene.m_bbox.min));
  m_control.m_viewMatrix = glm::lookAt(m_control.m_sceneOrbit - (-vec3(1, 1, 1) * m_control.m_sceneDimension * 0.5f),
                                       m_control.m_sceneOrbit, vec3(0, 1, 0));

  m_shared.animUbo.sceneCenter    = m_control.m_sceneOrbit;
  m_shared.animUbo.sceneDimension = m_control.m_sceneDimension * 0.2f;
  m_shared.animUbo.numMatrices    = uint(m_scene.m_matrices.size());
  m_shared.sceneUbo.wLightPos     = (m_scene.m_bbox.max + m_scene.m_bbox.min) * 0.5f + m_control.m_sceneDimension;
  m_shared.sceneUbo.wLightPos.w   = 1.0;

  initResources();
  initRenderer(m_tweak.renderer);

  m_lastTweak = m_tweak;

  return validated;
}


void Sample::processUI(int width, int height, double time)
{
  // Update imgui configuration
  auto& imgui_io       = ImGui::GetIO();
  imgui_io.DeltaTime   = static_cast<float>(time - m_uiTime);
  imgui_io.DisplaySize = ImVec2(width, height);

  m_uiTime = time;

  ImGui::NewFrame();
  ImGui::SetNextWindowSize(ImGuiH::dpiScaled(380, 0), ImGuiCond_FirstUseEver);
  if(ImGui::Begin("NVIDIA " PROJECT_NAME, nullptr))
  {
    m_ui.enumCombobox(GUI_RENDERER, "renderer", &m_tweak.renderer);
    m_ui.enumCombobox(GUI_SHADERS, "shaders", &m_tweak.useShaderObjs);
    m_ui.enumCombobox(GUI_BINDINGS, "binding", &m_tweak.binding);
    m_ui.enumCombobox(GUI_STRATEGY, "strategy", &m_tweak.strategy);

    ImGui::PushItemWidth(ImGuiH::dpiScaled(100));

    //guiRegistry.enumCombobox(GUI_SUPERSAMPLE, "supersample", &tweak.supersample);
    ImGuiH::InputIntClamped("max shadergroups", &m_tweak.maxShaders, 1, NUM_MATERIAL_SHADERS, 1, 1, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGuiH::InputIntClamped("copies", &m_tweak.copies, 1, 16, 1, 1, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SliderFloat("pct visible", &m_tweak.percent, 0.0f, 1.001f);
    ImGui::Checkbox("sorted once (minimized state changes)", &m_tweak.sorted);
    ImGui::Checkbox("permutated (random state changes,\ngen nv: use seqindex)", &m_tweak.permutated);
    ImGui::Checkbox("gen: unordered (non-coherent)", &m_tweak.unordered);
    if(m_supportsBinning)
    {
      ImGui::Checkbox("gen ext: binned via draw_indexed_count", &m_tweak.binned);
    }
    if(m_supportsNV)
    {
      ImGui::Checkbox("gen nv: interleaved inputs", &m_tweak.interleaved);
    }

    ImGuiH::InputIntClamped("threaded: worker threads", &m_tweak.workerThreads, 1, m_maxThreads, 1, 1,
                            ImGuiInputTextFlags_EnterReturnsTrue);
    ImGuiH::InputIntClamped("threaded: drawcalls per cmdbuffer", &m_tweak.workingSet, 512, 1 << 20, 512, 1024,
                            ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::Checkbox("threaded: batched submission", &m_tweak.workerBatched);
    ImGui::Checkbox("animation", &m_tweak.animation);
    ImGui::PopItemWidth();
    ImGui::Separator();

    {
      int avg = 50;

      if(m_lastFrameTime == 0)
      {
        m_lastFrameTime = time;
        m_frames        = -1;
      }

      if(m_frames > 4)
      {
        double curavg = (time - m_lastFrameTime) / m_frames;
        if(curavg > 1.0 / 30.0)
        {
          avg = 10;
        }
      }

      if(m_profiler.getTotalFrames() % avg == avg - 1)
      {
        nvh::Profiler::TimerInfo info;
        m_profiler.getTimerInfo("Render", info);
        m_statsCpuTime      = info.cpu.average;
        m_statsGpuTime      = info.gpu.average;
        m_statsGpuBuildTime = 0;
        bool hasPres        = m_profiler.getTimerInfo("Pre", info);
        m_statsGpuBuildTime = hasPres ? info.gpu.average : 0;
        m_profiler.getTimerInfo("Draw", info);
        m_statsGpuDrawTime = info.gpu.average;
        m_statsFrameTime   = (time - m_lastFrameTime) / m_frames;
        m_lastFrameTime    = time;
        m_frames           = -1;
      }

      m_frames++;

      float gpuTimeF = float(m_statsGpuTime);
      float cpuTimeF = float(m_statsCpuTime);
      float bldTimef = float(m_statsGpuBuildTime);
      float drwTimef = float(m_statsGpuDrawTime);
      float maxTimeF = std::max(std::max(cpuTimeF, gpuTimeF), 0.0001f);

      //ImGui::Text("Frame          [ms]: %2.1f", m_statsFrameTime*1000.0f);
      ImGui::Text("Render     CPU [ms]: %2.3f", cpuTimeF / 1000.0f);
      ImGui::Text("Render     GPU [ms]: %2.3f", gpuTimeF / 1000.0f);
      //ImGui::ProgressBar(gpuTimeF/maxTimeF, ImVec2(0.0f, 0.0f));
      ImGui::Text("- Preproc. GPU [ms]: %2.3f", bldTimef / 1000.0f);
      ImGui::ProgressBar(bldTimef / maxTimeF, ImVec2(0.0f, 0.0f));
      ImGui::Text("- Draw     GPU [ms]: %2.3f", drwTimef / 1000.0f);
      ImGui::ProgressBar(drwTimef / maxTimeF, ImVec2(0.0f, 0.0f));

      //ImGui::ProgressBar(cpuTimeF / maxTimeF, ImVec2(0.0f, 0.0f));
      ImGui::Separator();
      ImGui::Text(" cmdBuffers:           %9d\n", m_renderStats.cmdBuffers);
      ImGui::Text(" drawCalls:            %9d\n", m_renderStats.drawCalls);
      ImGui::Text(" drawTris:             %9d\n", m_renderStats.drawTriangles);
      ImGui::Text(" serial shaderBinds:   %9d\n", m_renderStats.shaderBindings);
      ImGui::Text(" dgc sequences:        %9d\n", m_renderStats.sequences);
      ImGui::Text(" dgc preprocessBuffer: %9d KB\n", m_renderStats.preprocessSizeKB);
      ImGui::Text(" dgc indirectBuffer:   %9d KB\n\n", m_renderStats.indirectSizeKB);
    }
  }
  ImGui::End();
}

void Sample::think(double time)
{
  int width  = m_windowState.m_swapSize[0];
  int height = m_windowState.m_swapSize[1];

  if(m_useUI)
  {
    processUI(width, height, time);
  }

  m_control.processActions({m_windowState.m_winSize[0], m_windowState.m_winSize[1]},
                           glm::vec2(m_windowState.m_mouseCurrent[0], m_windowState.m_mouseCurrent[1]),
                           m_windowState.m_mouseButtonFlags, m_windowState.m_mouseWheel);

  if(m_tweak.msaa != m_lastTweak.msaa || getVsync() != m_lastVsync)
  {
    m_lastVsync = getVsync();
    m_resources.initFramebuffer(width, height, m_tweak.msaa, getVsync());
  }

  bool sceneChanged = false;
  if(m_tweak.copies != m_lastTweak.copies || m_tweak.cloneaxisX != m_lastTweak.cloneaxisX
     || m_tweak.cloneaxisY != m_lastTweak.cloneaxisY || m_tweak.cloneaxisZ != m_lastTweak.cloneaxisZ)
  {
    sceneChanged = true;
    m_resources.synchronize();
    deinitRenderer();
    m_resources.deinitScene();
    initScene(m_modelFilename.c_str(), m_tweak.copies - 1,
              (m_tweak.cloneaxisX << 0) | (m_tweak.cloneaxisY << 1) | (m_tweak.cloneaxisZ << 2));
    m_resources.initScene(m_scene);
  }

  bool rendererChanged = false;
  if(m_windowState.onPress(KEY_R) || m_tweak.copies != m_lastTweak.copies)
  {
    m_resources.synchronize();
    std::string            prepend;
    CadScene::IndexingBits bits = m_scene.getIndexingBits();
    prepend += nvh::ShaderFileManager::format("#define INDEXED_MATRIX_BITS %d\n", bits.matrices);
    prepend += nvh::ShaderFileManager::format("#define INDEXED_MATERIAL_BITS %d\n", bits.materials);
    m_resources.reloadPrograms(prepend);
    rendererChanged = true;
  }

  if(sceneChanged || rendererChanged || m_tweak.renderer != m_lastTweak.renderer
     || m_tweak.binding != m_lastTweak.binding || m_tweak.strategy != m_lastTweak.strategy
     || m_tweak.sorted != m_lastTweak.sorted || m_tweak.percent != m_lastTweak.percent
     || m_tweak.workerThreads != m_lastTweak.workerThreads || m_tweak.workerBatched != m_lastTweak.workerBatched
     || m_tweak.maxShaders != m_lastTweak.maxShaders || m_tweak.interleaved != m_lastTweak.interleaved
     || m_tweak.permutated != m_lastTweak.permutated || m_tweak.unordered != m_lastTweak.unordered
     || m_tweak.binned != m_lastTweak.binned || m_tweak.useShaderObjs != m_lastTweak.useShaderObjs)
  {
    m_resources.synchronize();
    initRenderer(m_tweak.renderer);
  }

  m_resources.beginFrame();

  if(m_tweak.animation != m_lastTweak.animation)
  {
    m_resources.synchronize();
    m_resources.animationReset();

    m_animBeginTime = time;
  }

  {
    m_shared.winWidth      = width;
    m_shared.winHeight     = height;
    m_shared.workingSet    = m_tweak.workingSet;
    m_shared.workerBatched = m_tweak.workerBatched;

    SceneData& sceneUbo = m_shared.sceneUbo;

    sceneUbo.viewport = ivec2(width, height);

    glm::mat4 projection = glm::perspectiveRH_ZO(glm::radians(45.f), float(width) / float(height),
                                                 m_control.m_sceneDimension * 0.001f, m_control.m_sceneDimension * 10.0f);
    projection[1][1] *= -1;
    glm::mat4 view = m_control.m_viewMatrix;

    if(m_tweak.animation && m_tweak.animationSpin)
    {
      double animTime = (time - m_animBeginTime) * 0.3 + glm::pi<float>() * 0.2;
      vec3   dir      = vec3(cos(animTime), 1, sin(animTime));
      view = glm::lookAt(m_control.m_sceneOrbit - (-dir * m_control.m_sceneDimension * 0.5f), m_control.m_sceneOrbit,
                         vec3(0, 1, 0));
    }

    sceneUbo.viewProjMatrix = projection * view;
    sceneUbo.viewMatrix     = view;
    sceneUbo.viewMatrixIT   = glm::transpose(glm::inverse(view));

    sceneUbo.viewPos = glm::row(sceneUbo.viewMatrixIT, 3);
    ;
    sceneUbo.viewDir = -glm::row(view, 2);

    sceneUbo.wLightPos   = glm::row(sceneUbo.viewMatrixIT, 3);
    sceneUbo.wLightPos.w = 1.0;
  }

  if(m_tweak.animation)
  {
    AnimationData& animUbo = m_shared.animUbo;
    animUbo.time           = float(time - m_animBeginTime);

    m_resources.animation(m_shared);
  }

  {
    m_renderer->draw(m_shared, m_renderStats);
  }

  {
    if(m_useUI)
    {
      ImGui::Render();
      m_shared.imguiDrawData = ImGui::GetDrawData();
    }
    else
    {
      m_shared.imguiDrawData = nullptr;
    }

    m_resources.blitFrame(m_shared);
  }

  m_resources.endFrame();
  m_resources.m_frame++;

  if(m_useUI)
  {
    ImGui::EndFrame();
  }

  m_lastTweak = m_tweak;
}

void Sample::resize(int width, int height)
{
  m_resources.initFramebuffer(width, height, m_tweak.msaa, getVsync());
}

void Sample::setRendererFromName()
{
  if(!m_rendererName.empty())
  {
    const Renderer::Registry registry = Renderer::getRegistry();
    for(size_t i = 0; i < m_renderersSorted.size(); i++)
    {
      if(strcmp(m_rendererName.c_str(), registry[m_renderersSorted[i]]->name()) == 0)
      {
        m_tweak.renderer = int(i);
      }
    }
  }
}

void Sample::setupConfigParameters()
{
  m_parameterList.addFilename(".csf", &m_modelFilename);
  m_parameterList.addFilename(".csf.gz", &m_modelFilename);
  m_parameterList.addFilename(".gltf", &m_modelFilename);

  m_parameterList.add("vkdevice", &m_contextInfo.compatibleDeviceIndex);

  m_parameterList.add("noui", &m_useUI, false);

  m_parameterList.add("unordered", &m_tweak.unordered);
  m_parameterList.add("interleaved", &m_tweak.interleaved);
  m_parameterList.add("binned", &m_tweak.binned);
  m_parameterList.add("permutated", &m_tweak.permutated);
  m_parameterList.add("sorted", &m_tweak.sorted);
  m_parameterList.add("percent", &m_tweak.percent);
  m_parameterList.add("renderer", (uint32_t*)&m_tweak.renderer);
  m_parameterList.add("renderernamed", &m_rendererName);
  m_parameterList.add("strategy", (uint32_t*)&m_tweak.strategy);
  m_parameterList.add("bindingmode", (uint32_t*)&m_tweak.binding);
  m_parameterList.add("shadermode", (uint32_t*)&m_tweak.useShaderObjs);
  m_parameterList.add("msaa", &m_tweak.msaa);
  m_parameterList.add("copies", &m_tweak.copies);
  m_parameterList.add("animation", &m_tweak.animation);
  m_parameterList.add("animationspin", &m_tweak.animationSpin);
  m_parameterList.add("minstatechanges", &m_tweak.sorted);
  m_parameterList.add("maxshaders", &m_tweak.maxShaders);
  m_parameterList.add("workerbatched", &m_tweak.workerBatched);
  m_parameterList.add("workerthreads", &m_tweak.workerThreads);
  m_parameterList.add("workingset", &m_tweak.workingSet);
  m_parameterList.add("animation", &m_tweak.animation);
  m_parameterList.add("animationspin", &m_tweak.animationSpin);
}

bool Sample::validateConfig()
{
  if(m_modelFilename.empty())
  {
    LOGI("no .csf model file specified\n");
    LOGI("exe <filename.csf/cfg> parameters...\n");
    m_parameterList.print();
    return false;
  }
  return true;
}

}  // namespace generatedcmds

using namespace generatedcmds;

int main(int argc, const char** argv)
{
  NVPSystem system(PROJECT_NAME);

#if defined(_WIN32) && defined(NDEBUG)
  //SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif

  Sample sample;
  {
    std::vector<std::string> directories;
    directories.push_back(NVPSystem::exePath());
    directories.push_back(NVPSystem::exePath() + "/media");
    directories.push_back(NVPSystem::exePath() + std::string(PROJECT_DOWNLOAD_RELDIRECTORY));
    sample.m_modelFilename = nvh::findFile(std::string("geforce.csf.gz"), directories);
  }

  return sample.run(PROJECT_NAME, argc, argv, SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT);
}
