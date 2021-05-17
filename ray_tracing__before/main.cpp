/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
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
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */


// ImGui - standalone example application for Glfw + Vulkan, using programmable
// pipeline If you are new to ImGui, see examples/README.txt and documentation
// at the top of imgui.cpp.

#include <array>
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include "backends/imgui_impl_glfw.h"
#include "imgui.h"

#include "hello_vulkan.h"
#include "nvh/cameramanipulator.hpp"
#include "nvh/fileoperations.hpp"
#include "nvpsystem.hpp"
#include "nvvk/appbase_vkpp.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/context_vk.hpp"


//////////////////////////////////////////////////////////////////////////
#define UNUSED(x) (void)(x)
//////////////////////////////////////////////////////////////////////////

// Default search path for shaders
std::vector<std::string> defaultSearchPaths;


// GLFW Callback functions
static void onErrorCallback(int error, const char* description)
{
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Extra UI
void renderUI(HelloVulkan& helloVk)
{
  static int item = 1;
  if(ImGui::Combo("Up Vector", &item, "X\0Y\0Z\0\0"))
  {
    nvmath::vec3f pos, eye, up;
    CameraManip.getLookat(pos, eye, up);
    up = nvmath::vec3f(item == 0, item == 1, item == 2);
    CameraManip.setLookat(pos, eye, up);
  }
  ImGui::SliderFloat3("Light Position", &helloVk.m_pushConstant.lightPosition.x, -20.f, 20.f);
  ImGui::SliderFloat("Light Intensity", &helloVk.m_pushConstant.lightIntensity, 0.f, 100.f);
  ImGui::RadioButton("Point", &helloVk.m_pushConstant.lightType, 0);
  ImGui::SameLine();
  ImGui::RadioButton("Infinite", &helloVk.m_pushConstant.lightType, 1);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
static int const SAMPLE_WIDTH  = 1280;
static int const SAMPLE_HEIGHT = 720;


//--------------------------------------------------------------------------------------------------
// Application Entry
//
int main(int argc, char** argv)
{
  UNUSED(argc);

  // Setup GLFW window
  glfwSetErrorCallback(onErrorCallback);
  if(!glfwInit())
  {
    return 1;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window =
      glfwCreateWindow(SAMPLE_WIDTH, SAMPLE_HEIGHT, PROJECT_NAME, nullptr, nullptr);


  // Setup camera
  CameraManip.setWindowSize(SAMPLE_WIDTH, SAMPLE_HEIGHT);
  CameraManip.setLookat(nvmath::vec3f(2.0f, 2.0f, 2.0f), nvmath::vec3f(0, 0, 0),
                        nvmath::vec3f(0, 1, 0));

  // Setup Vulkan
  if(!glfwVulkanSupported())
  {
    printf("GLFW: Vulkan Not Supported\n");
    return 1;
  }

  // setup some basic things for the sample, logging file for example
  NVPSystem system(PROJECT_NAME);

  // Search path for shaders and other media
  defaultSearchPaths = {
      NVPSystem::exePath() + PROJECT_RELDIRECTORY,        // shaders
      NVPSystem::exePath() + PROJECT_RELDIRECTORY "../",  // media
      PROJECT_NAME,                                       // installed: shaders + media
      NVPSystem::exePath() + std::string(PROJECT_NAME),
  };

  // Enabling the extension feature
  vk::PhysicalDeviceDescriptorIndexingFeaturesEXT indexFeature;
  vk::PhysicalDeviceScalarBlockLayoutFeaturesEXT  scalarFeature;

  // Requesting Vulkan extensions and layers
  nvvk::ContextCreateInfo contextInfo;
  contextInfo.addInstanceLayer("VK_LAYER_LUNARG_monitor", true);
  contextInfo.addInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef _WIN32
  contextInfo.addInstanceExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
  contextInfo.addInstanceExtension(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
  contextInfo.addInstanceExtension(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
  contextInfo.addInstanceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, false, &indexFeature);
  contextInfo.addDeviceExtension(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME, false, &scalarFeature);

  // Creating Vulkan base application
  nvvk::Context vkctx{};
  vkctx.initInstance(contextInfo);
  // Find all compatible devices
  auto compatibleDevices = vkctx.getCompatibleDevices(contextInfo);
  assert(!compatibleDevices.empty());
  // Use a compatible device
  vkctx.initDevice(compatibleDevices[0], contextInfo);

  // Create example
  HelloVulkan helloVk;

  // Window need to be opened to get the surface on which to draw
  const vk::SurfaceKHR surface = helloVk.getVkSurface(vkctx.m_instance, window);
  vkctx.setGCTQueueWithPresent(surface);

  helloVk.setup(vkctx.m_instance, vkctx.m_device, vkctx.m_physicalDevice,
                vkctx.m_queueGCT.familyIndex);
  helloVk.createSwapchain(surface, SAMPLE_WIDTH, SAMPLE_HEIGHT);
  helloVk.createDepthBuffer();
  helloVk.createRenderPass();
  helloVk.createFrameBuffers();

  // Setup Imgui
  helloVk.initGUI(0);  // Using sub-pass 0

  // Creation of the example
  helloVk.loadModel(nvh::findFile("media/scenes/cube_multi.obj", defaultSearchPaths));

  helloVk.createOffscreenRender();
  helloVk.createDescriptorSetLayout();
  helloVk.createGraphicsPipeline();
  helloVk.createUniformBuffer();
  helloVk.createSceneDescriptionBuffer();
  helloVk.updateDescriptorSet();

  helloVk.createPostDescriptor();
  helloVk.createPostPipeline();
  helloVk.updatePostDescriptorSet();
  nvmath::vec4f clearColor = nvmath::vec4f(1, 1, 1, 1.00f);


  helloVk.setupGlfwCallbacks(window);
  ImGui_ImplGlfw_InitForVulkan(window, true);

  // Main loop
  while(!glfwWindowShouldClose(window))
  {
    glfwPollEvents();
    if(helloVk.isMinimized())
      continue;

    // Start the Dear ImGui frame
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Updating camera buffer
    helloVk.updateUniformBuffer();

    // Show UI window.
    if(1 == 1)
    {
      ImGui::ColorEdit3("Clear color", reinterpret_cast<float*>(&clearColor));
      renderUI(helloVk);
      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                  1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      ImGui::Render();
    }

    // Start rendering the scene
    helloVk.prepareFrame();

    // Start command buffer of this frame
    auto                     curFrame = helloVk.getCurFrame();
    const vk::CommandBuffer& cmdBuff  = helloVk.getCommandBuffers()[curFrame];

    cmdBuff.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // Clearing screen
    vk::ClearValue clearValues[2];
    clearValues[0].setColor(
        std::array<float, 4>({clearColor[0], clearColor[1], clearColor[2], clearColor[3]}));
    clearValues[1].setDepthStencil({1.0f, 0});

    // Offscreen render pass
    {
      vk::RenderPassBeginInfo offscreenRenderPassBeginInfo;
      offscreenRenderPassBeginInfo.setClearValueCount(2);
      offscreenRenderPassBeginInfo.setPClearValues(clearValues);
      offscreenRenderPassBeginInfo.setRenderPass(helloVk.m_offscreenRenderPass);
      offscreenRenderPassBeginInfo.setFramebuffer(helloVk.m_offscreenFramebuffer);
      offscreenRenderPassBeginInfo.setRenderArea({{}, helloVk.getSize()});

      // Rendering Scene
      cmdBuff.beginRenderPass(offscreenRenderPassBeginInfo, vk::SubpassContents::eInline);
      helloVk.rasterize(cmdBuff);
      cmdBuff.endRenderPass();
    }


    // 2nd rendering pass: tone mapper, UI
    {
      vk::RenderPassBeginInfo postRenderPassBeginInfo;
      postRenderPassBeginInfo.setClearValueCount(2);
      postRenderPassBeginInfo.setPClearValues(clearValues);
      postRenderPassBeginInfo.setRenderPass(helloVk.getRenderPass());
      postRenderPassBeginInfo.setFramebuffer(helloVk.getFramebuffers()[curFrame]);
      postRenderPassBeginInfo.setRenderArea({{}, helloVk.getSize()});

      cmdBuff.beginRenderPass(postRenderPassBeginInfo, vk::SubpassContents::eInline);
      // Rendering tonemapper
      helloVk.drawPost(cmdBuff);
      // Rendering UI
      ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuff);
      cmdBuff.endRenderPass();
    }

    // Submit for display
    cmdBuff.end();
    helloVk.submitFrame();
  }

  // Cleanup
  helloVk.getDevice().waitIdle();
  helloVk.destroyResources();
  helloVk.destroy();

  vkctx.deinit();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
