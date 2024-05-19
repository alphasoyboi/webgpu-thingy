#include <cassert>
#include <iostream>
#include <webgpu/webgpu.h>
#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

WGPUAdapter requestAdapter(WGPUInstance instance, WGPURequestAdapterOptions const * options);
WGPUDevice requestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor);

int main (int, char**) {
  // Window
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  GLFWwindow* window = glfwCreateWindow(640, 480, "Learn WebGPU", NULL, NULL);

  if (!window) {
    std::cerr << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return -1;
  }

  // Instance
  WGPUInstanceDescriptor desc = {};
  desc.nextInChain = nullptr;
  WGPUInstance instance = wgpuCreateInstance(&desc);
  if (!instance) {
    std::cerr << "Could not initialize WebGPU!" << std::endl;
    return 1;
  }
  std::cout << "WGPU instance: " << instance << std::endl;

  // Adapter
  std::cout << "Requesting adapter..." << std::endl;
  WGPURequestAdapterOptions adapterOpts = {};
  WGPUSurface surface = glfwGetWGPUSurface(instance, window);
  adapterOpts.nextInChain = nullptr;
  adapterOpts.compatibleSurface = surface;
  WGPUAdapter adapter = requestAdapter(instance, &adapterOpts);
  std::cout << "Got adapter: " << adapter << std::endl;

  // Device
  std::cout << "Requesting device..." << std::endl;
  WGPUDeviceDescriptor deviceDesc = {};
  deviceDesc.nextInChain = nullptr;
  deviceDesc.label = "Default Device"; // anything works here, that's your call
  deviceDesc.requiredFeaturesCount = 0; // we do not require any specific feature
  deviceDesc.requiredLimits = nullptr; // we do not require any specific limit
  deviceDesc.defaultQueue.nextInChain = nullptr;
  deviceDesc.defaultQueue.label = "Default Queue";
  WGPUDevice device = requestDevice(adapter, &deviceDesc);
  auto onDeviceError = [](WGPUErrorType type, char const* message, void*) {
    std::cout << "Uncaptured device error: type " << type;
    if (message) std::cout << " (" << message << ")";
    std::cout << std::endl;
  };
  wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, nullptr);
  std::cout << "Got device: " << device << std::endl;

  // Queue
  WGPUQueue queue = wgpuDeviceGetQueue(device);
  auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void* /* pUserData */) {
    std::cout << "Queued work finished with status: " << status << std::endl;
  };
  wgpuQueueOnSubmittedWorkDone(queue, onQueueWorkDone, nullptr /* pUserData */);

  // Swapchain
  WGPUSwapChainDescriptor swapChainDesc = {};
  swapChainDesc.nextInChain = nullptr;
  swapChainDesc.width = 640;
  swapChainDesc.height = 480;
  WGPUTextureFormat swapChainFormat = wgpuSurfaceGetPreferredFormat(surface, adapter);
  swapChainDesc.format = swapChainFormat;
  swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
  swapChainDesc.presentMode = WGPUPresentMode_Fifo;
  WGPUSwapChain swapChain = wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc);
  std::cout << "Swapchain: " << swapChain << std::endl;

  // Describe the render pass
  WGPURenderPassColorAttachment renderPassColorAttachment = {};
  renderPassColorAttachment.view = nullptr;
  renderPassColorAttachment.resolveTarget = nullptr;
  renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
  renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
  renderPassColorAttachment.clearValue = WGPUColor{ 0.9, 0.1, 0.2, 1.0 };

  while (!glfwWindowShouldClose(window)) {
    // Get the next texture and give it to the render pass
    WGPUTextureView nextTexture = wgpuSwapChainGetCurrentTextureView(swapChain);
    if (!nextTexture) {
      std::cerr << "Cannot acquire next swap chain texture" << std::endl;
      break;
    }
    renderPassColorAttachment.view = nextTexture;

    // Command Buffer
    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.nextInChain = nullptr;
    encoderDesc.label = "Render pass encoder";
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);
    WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
    cmdBufferDescriptor.nextInChain = nullptr;
    cmdBufferDescriptor.label = "Render pass command buffer";

    // Declare render pass descriptor and write render pass commands
    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;
    renderPassDesc.nextInChain = nullptr;
    WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
    wgpuCommandEncoderRelease(encoder);

    // Finally submit the command queue
    wgpuQueueSubmit(queue, 1, &command);
    wgpuCommandBufferRelease(command);

    // Release the texture view and present the swap chain
    wgpuTextureViewRelease(nextTexture);
    wgpuSwapChainPresent(swapChain);

    // Poll events and check for window close request
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
  }

  // 5. We clean up the WebGPU instance
  wgpuSwapChainRelease(swapChain);
  wgpuQueueRelease(queue);
  wgpuDeviceRelease(device);
  wgpuSurfaceRelease(surface);
  wgpuAdapterRelease(adapter);
  wgpuInstanceRelease(instance);


  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

/**
 * Utility function to get a WebGPU adapter, so that
 *     WGPUAdapter adapter = requestAdapter(options);
 * is roughly equivalent to
 *     const adapter = await navigator.gpu.requestAdapter(options);
 */
WGPUAdapter requestAdapter(WGPUInstance instance, WGPURequestAdapterOptions const * options) {
  // A simple structure holding the local information shared with the
  // onAdapterRequestEnded callback.
  struct UserData {
    WGPUAdapter adapter = nullptr;
    bool requestEnded = false;
  };
  UserData userData;

  // Callback called by wgpuInstanceRequestAdapter when the request returns
  // This is a C++ lambda function, but could be any function defined in the
  // global scope. It must be non-capturing (the brackets [] are empty) so
  // that it behaves like a regular C function pointer, which is what
  // wgpuInstanceRequestAdapter expects (WebGPU being a C API). The workaround
  // is to convey what we want to capture through the pUserData pointer,
  // provided as the last argument of wgpuInstanceRequestAdapter and received
  // by the callback as its last argument.
  auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const * message, void * pUserData) {
    UserData& userData = *reinterpret_cast<UserData*>(pUserData);
    if (status == WGPURequestAdapterStatus_Success) {
      userData.adapter = adapter;
    } else {
      std::cout << "Could not get WebGPU adapter: " << message << std::endl;
    }
    userData.requestEnded = true;
  };

  // Call to the WebGPU request adapter procedure
  wgpuInstanceRequestAdapter(
      instance, // equivalent of navigator.gpu
      options,
      onAdapterRequestEnded,
      (void*)&userData
  );

  // In theory we should wait until onAdapterReady has been called, which
  // could take some time (what the 'await' keyword does in the JavaScript
  // code). In practice, we know that when the wgpuInstanceRequestAdapter()
  // function returns its callback has been called.
  assert(userData.requestEnded);

  return userData.adapter;
}


/**
 * Utility function to get a WebGPU device, so that
 *     WGPUAdapter device = requestDevice(adapter, options);
 * is roughly equivalent to
 *     const device = await adapter.requestDevice(descriptor);
 * It is very similar to requestAdapter
 */
WGPUDevice requestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor) {
  struct UserData {
    WGPUDevice device = nullptr;
    bool requestEnded = false;
  };
  UserData userData;

  auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, char const * message, void * pUserData) {
    UserData& userData = *reinterpret_cast<UserData*>(pUserData);
    if (status == WGPURequestDeviceStatus_Success) {
      userData.device = device;
    } else {
      std::cout << "Could not get WebGPU device: " << message << std::endl;
    }
    userData.requestEnded = true;
  };

  wgpuAdapterRequestDevice(
      adapter,
      descriptor,
      onDeviceRequestEnded,
      (void*)&userData
  );

  assert(userData.requestEnded);

  return userData.device;
}
