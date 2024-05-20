#include <cassert>
#include <iostream>
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>
#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

const char* shaderSource = R"(
@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {
    var p = vec2f(0.0, 0.0);
    if (in_vertex_index == 0u) {
        p = vec2f(-0.5, -0.5);
    } else if (in_vertex_index == 1u) {
        p = vec2f(0.5, -0.5);
    } else {
        p = vec2f(0.0, 0.5);
    }
    return vec4f(p, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
    return vec4f(0.0, 0.4, 1.0, 1.0);
}
)";

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
  wgpu::TextureFormat swapChainFormat = surface.getPreferredFormat(adapter);
  swapChainDesc.format = swapChainFormat;
  swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
  swapChainDesc.presentMode = WGPUPresentMode_Fifo;
  wgpu::SwapChain swapChain = device.createSwapChain(surface, swapChainDesc);
  std::cout << "Swapchain: " << swapChain << std::endl;

  // Shader module
  wgpu::ShaderModuleDescriptor shaderDesc = wgpu::Default;
#ifdef WEBGPU_BACKEND_WGPU
  shaderDesc.hintCount = 0;
  shaderDesc.hints = nullptr;
#endif
  wgpu::ShaderModuleWGSLDescriptor shaderCodeDesc = wgpu::Default;
  // Set the chained struct's header
  shaderCodeDesc.chain.next = nullptr;
  shaderCodeDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
  shaderCodeDesc.code = shaderSource;
  // Connect the chain
  shaderDesc.nextInChain = &shaderCodeDesc.chain;
  wgpu::ShaderModule shaderModule = device.createShaderModule(shaderDesc);

  // Pipeline
  wgpu::RenderPipelineDescriptor pipelineDesc = wgpu::Default;
  pipelineDesc.vertex.bufferCount = 0;
  pipelineDesc.vertex.buffers = nullptr;
  pipelineDesc.vertex.module = shaderModule;
  pipelineDesc.vertex.entryPoint = "vs_main";
  pipelineDesc.vertex.constantCount = 0;
  pipelineDesc.vertex.constants = nullptr;
  pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList; // Each sequence of 3 vertices is considered as a triangle
  pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
  pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
  pipelineDesc.primitive.cullMode = wgpu::CullMode::None;
  // Fragment shader
  wgpu::FragmentState fragmentState = wgpu::Default;
  fragmentState.module = shaderModule;
  fragmentState.entryPoint = "fs_main";
  fragmentState.constantCount = 0;
  fragmentState.constants = nullptr;
  wgpu::BlendState blendState;
  blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
  blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
  blendState.color.operation = wgpu::BlendOperation::Add;
  wgpu::ColorTargetState colorTarget;
  colorTarget.format = swapChainFormat;
  colorTarget.blend = &blendState;
  colorTarget.writeMask = wgpu::ColorWriteMask::All;
  fragmentState.targetCount = 1;
  fragmentState.targets = &colorTarget;
  pipelineDesc.fragment = &fragmentState;
  pipelineDesc.depthStencil = nullptr;
  pipelineDesc.multisample.count = 1;  // Samples per pixel
  pipelineDesc.multisample.mask = ~0u; // Default value for the mask, meaning "all bits on"
  pipelineDesc.multisample.alphaToCoverageEnabled = false; // Default value as well (irrelevant for count = 1 anyways)
  pipelineDesc.layout = nullptr;
  wgpu::RenderPipeline pipeline = device.createRenderPipeline(pipelineDesc);

  // Describe the render pass
  wgpu::RenderPassColorAttachment renderPassColorAttachment = wgpu::Default;
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
    // Select which render pipeline to use
    renderPass.setPipeline(pipeline);
    // Draw 1 instance of a 3-vertices shape
    renderPass.draw(3, 1, 0, 0);
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