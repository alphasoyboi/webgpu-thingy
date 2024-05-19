#include <cassert>
#include <iostream>
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>
#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

int main (int, char**) {
  // Window
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  GLFWwindow* window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);

  if (!window) {
    std::cerr << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return -1;
  }

  // Instance
  wgpu::InstanceDescriptor desc = wgpu::Default;
  wgpu::Instance instance = wgpu::createInstance(desc);
  if (!instance) {
    std::cerr << "Could not initialize WebGPU!" << std::endl;
    return 1;
  }
  std::cout << "WGPU instance: " << instance << std::endl;

  // Adapter
  std::cout << "Requesting adapter..." << std::endl;
  wgpu::RequestAdapterOptions adapterOpts = wgpu::Default;
  wgpu::Surface surface = glfwGetWGPUSurface(instance, window);
  adapterOpts.compatibleSurface = surface;
  wgpu::Adapter adapter = nullptr;
  instance.requestAdapter(adapterOpts, [&adapter](wgpu::RequestAdapterStatus status, wgpu::Adapter a, char const* msg) {
    if (status == WGPURequestAdapterStatus_Success) {
      adapter = a;
    } else {
      std::cout << "Could not get WebGPU adapter: " << msg << std::endl;
    }
  });
  assert(adapter);
  std::cout << "Got adapter: " << adapter << std::endl;

  // Device
  std::cout << "Requesting device..." << std::endl;
  wgpu::DeviceDescriptor deviceDesc = wgpu::Default;
  deviceDesc.label = "Default device"; // anything works here, that's your call
  deviceDesc.defaultQueue.label = "Default queue";
  wgpu::Device device = nullptr;
  adapter.requestDevice(deviceDesc, [&device](wgpu::RequestDeviceStatus status, wgpu::Device d, char const * msg) {
    if (status == WGPURequestDeviceStatus_Success) {
      device = d;
    } else {
      std::cout << "Could not get WebGPU device: " << msg << std::endl;
    }
  });
  assert(device);
  device.setUncapturedErrorCallback([](wgpu::ErrorType type, char const* message) {
    std::cout << "Uncaptured device error: type " << type;
    if (message) std::cout << " (" << message << ")";
    std::cout << std::endl;
  });
  std::cout << "Got device: " << device << std::endl;

  // Queue
  wgpu::Queue queue = device.getQueue();
  queue.onSubmittedWorkDone([](wgpu::QueueWorkDoneStatus status) {
    std::cout << "Queued work finished with status: " << status << std::endl;
  });

  // Swapchain
  wgpu::SwapChainDescriptor swapChainDesc = wgpu::Default;
  swapChainDesc.width = 640;
  swapChainDesc.height = 480;
  swapChainDesc.format = surface.getPreferredFormat(adapter);
  swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
  swapChainDesc.presentMode = WGPUPresentMode_Fifo;
  wgpu::SwapChain swapChain = device.createSwapChain(surface, swapChainDesc);
  std::cout << "Swapchain: " << swapChain << std::endl;

  // Describe the render pass
  wgpu::RenderPassColorAttachment renderPassColorAttachment = wgpu::Default;
  //renderPassColorAttachment.view = nullptr;
  //renderPassColorAttachment.resolveTarget = nullptr;
  renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
  renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
  renderPassColorAttachment.clearValue = WGPUColor{ 0.9, 0.1, 0.2, 1.0 };

  while (!glfwWindowShouldClose(window)) {
    // Get the next texture and give it to the render pass
    wgpu::TextureView nextTexture = swapChain.getCurrentTextureView();
    if (!nextTexture) {
      std::cerr << "Cannot acquire next swap chain texture" << std::endl;
      break;
    }
    renderPassColorAttachment.view = nextTexture;

    // Command Buffer
    wgpu::CommandEncoderDescriptor encoderDesc = wgpu::Default;
    encoderDesc.label = "Render pass encoder";
    wgpu::CommandEncoder encoder = device.createCommandEncoder(encoderDesc);
    wgpu::CommandBufferDescriptor cmdBufferDescriptor = wgpu::Default;
    cmdBufferDescriptor.label = "Render pass command buffer";

    // Declare render pass descriptor and write render pass commands
    wgpu::RenderPassDescriptor renderPassDesc = wgpu::Default;
    //renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;
    //renderPassDesc.nextInChain = nullptr;
    wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
    renderPass.end();
    wgpu::CommandBuffer command = encoder.finish(cmdBufferDescriptor);

    // Finally submit the command queue and present the swap chain
    queue.submit(1, &command);
    swapChain.present();

    // Release resources
    command.release();
    renderPass.release();
    encoder.release();
    nextTexture.release();

    // Poll events and check for window close request
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
  }

  // Cleanup WebGPU resources
  swapChain.release();
  queue.release();
  device.release();
  surface.release();
  adapter.release();
  instance.release();

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}