#include <cassert>
#include <iostream>
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>
#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>
#include "utils.h"
#include "magic_enum.hpp"

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
  wgpu::Instance instance = wgpu::createInstance(wgpu::InstanceDescriptor{});
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
  wgpu::Adapter adapter = instance.requestAdapter(adapterOpts);
  assert(adapter);
  std::cout << "Got adapter: " << adapter << std::endl;

  // Adapter Capabilities
  wgpu::SupportedLimits supportedLimits;
  adapter.getLimits(&supportedLimits);

  // Device Requirements
  wgpu::RequiredLimits requiredLimits = wgpu::Default;
  requiredLimits.limits.maxVertexAttributes = 2;
  requiredLimits.limits.maxVertexBuffers = 1;
  requiredLimits.limits.maxBufferSize = 15 * 5 * sizeof(float);
  requiredLimits.limits.maxVertexBufferArrayStride = 5 * sizeof(float);
  requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment; // This must be set even if we do not use storage buffers for now
  requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment; // This must be set even if we do not use uniform buffers for now
  requiredLimits.limits.maxInterStageShaderComponents = 3;

  // Device
  std::cout << "Requesting device..." << std::endl;
  wgpu::DeviceDescriptor deviceDesc = wgpu::Default;
  deviceDesc.label = "Default device";
  deviceDesc.defaultQueue.label = "Default queue";
  deviceDesc.requiredLimits = &requiredLimits;
  wgpu::Device device = adapter.requestDevice(deviceDesc);
  device.getLimits(&supportedLimits);
  std::cout << "device.maxVertexAttributes: " << supportedLimits.limits.maxVertexAttributes << std::endl;
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
#ifdef WEBGPU_BACKEND_WGPU
  wgpu::TextureFormat swapChainFormat = surface.getPreferredFormat(adapter);
#else
  TextureFormat swapChainFormat = TextureFormat::BGRA8Unorm;
#endif
  swapChainDesc.format = swapChainFormat;
  swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
  swapChainDesc.presentMode = wgpu::PresentMode::Fifo;
  wgpu::SwapChain swapChain = device.createSwapChain(surface, swapChainDesc);
  std::cout << "Swapchain: " << swapChain << std::endl;
  std::cout << "Swapchain format: " << magic_enum::enum_name<WGPUTextureFormat>(swapChainFormat) << std::endl;

  // Shader module
  std::cout << "Creating shader module..." << std::endl;
  wgpu::ShaderModule shaderModule = loadShaderModule(RESOURCE_DIR "/shader.wgsl", device);
  std::cout << "Shader module: " << shaderModule << std::endl;

  // Pipeline
  wgpu::RenderPipelineDescriptor pipelineDesc = wgpu::Default;
  // Vertex Position Attribute
  std::vector<wgpu::VertexAttribute> vertexAttribs(2);
  vertexAttribs[0].shaderLocation = 0;
  vertexAttribs[0].format = wgpu::VertexFormat::Float32x2;
  vertexAttribs[0].offset = 0;
  // Vertex Color Attribute
  vertexAttribs[1].shaderLocation = 1;
  vertexAttribs[1].format = wgpu::VertexFormat::Float32x3;
  vertexAttribs[1].offset = 2 * sizeof(float);
  // Vertex Buffer Layout
  wgpu::VertexBufferLayout vertexBufferLayout;
  vertexBufferLayout.attributeCount = static_cast<uint32_t>(vertexAttribs.size());
  vertexBufferLayout.attributes = vertexAttribs.data();
  vertexBufferLayout.arrayStride = 5 * sizeof(float);
  vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;

  // Vertex State
  pipelineDesc.vertex.bufferCount = 1;
  pipelineDesc.vertex.buffers = &vertexBufferLayout;

  pipelineDesc.vertex.module = shaderModule;
  pipelineDesc.vertex.entryPoint = "vs_main";
  pipelineDesc.vertex.constantCount = 0;
  pipelineDesc.vertex.constants = nullptr;
  // Primitive State
  pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList; // Each sequence of 3 vertices is considered as a triangle
  pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
  pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
  pipelineDesc.primitive.cullMode = wgpu::CullMode::None;
  // Fragment Shader
  wgpu::FragmentState fragmentState = wgpu::Default;
  fragmentState.module = shaderModule;
  fragmentState.entryPoint = "fs_main";
  fragmentState.constantCount = 0;
  fragmentState.constants = nullptr;
  pipelineDesc.fragment = &fragmentState;
  // Blend State
  wgpu::BlendState blendState;
  blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
  blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
  blendState.color.operation = wgpu::BlendOperation::Add;
  blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
  blendState.alpha.dstFactor = wgpu::BlendFactor::One;
  blendState.alpha.operation = wgpu::BlendOperation::Add;
  // Color Target
  wgpu::ColorTargetState colorTarget;
  colorTarget.format = swapChainFormat;
  colorTarget.blend = &blendState;
  colorTarget.writeMask = wgpu::ColorWriteMask::All;
  fragmentState.targetCount = 1;
  fragmentState.targets = &colorTarget;
  pipelineDesc.depthStencil = nullptr;
  pipelineDesc.multisample.count = 1;  // Samples per pixel
  pipelineDesc.multisample.mask = ~0u; // Default value for the mask, meaning "all bits on"
  pipelineDesc.multisample.alphaToCoverageEnabled = false; // Default value as well (irrelevant for count = 1 anyways)
  // Layout
  wgpu::PipelineLayoutDescriptor layoutDesc;
  layoutDesc.bindGroupLayoutCount = 0;
  layoutDesc.bindGroupLayouts = nullptr;
  wgpu::PipelineLayout layout = device.createPipelineLayout(layoutDesc);
  pipelineDesc.layout = layout;

  wgpu::RenderPipeline pipeline = device.createRenderPipeline(pipelineDesc);
  std::cout << "Render pipeline: " << pipeline << std::endl;

  std::vector<float> pointData;
  std::vector<uint16_t> indexData;

  bool success = loadGeometry(RESOURCE_DIR "/webgpu.txt", pointData, indexData);
  if (!success) {
    std::cerr << "Could not load geometry!" << std::endl;
    return 1;
  }

  // Create vertex buffer
  wgpu::BufferDescriptor bufferDesc;
  bufferDesc.size = pointData.size() * sizeof(float);
  bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
  bufferDesc.mappedAtCreation = false;
  wgpu::Buffer vertexBuffer = device.createBuffer(bufferDesc);
  queue.writeBuffer(vertexBuffer, 0, pointData.data(), bufferDesc.size);

  // Index Buffer alignment
  int indexCount = static_cast<int>(indexData.size());

  // Create index buffer
  // (we reuse the bufferDesc initialized for the vertexBuffer)
  bufferDesc.size = indexData.size() * sizeof(float);
  bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index;
  bufferDesc.mappedAtCreation = false;
  wgpu::Buffer indexBuffer = device.createBuffer(bufferDesc);
  queue.writeBuffer(indexBuffer, 0, indexData.data(), bufferDesc.size);

  while (!glfwWindowShouldClose(window)) {
    // Get the next texture and give it to the render pass
    wgpu::TextureView nextTexture = swapChain.getCurrentTextureView();
    if (!nextTexture) {
      std::cerr << "Cannot acquire next swap chain texture" << std::endl;
      break;
    }

    // Command Buffer
    wgpu::CommandEncoderDescriptor encoderDesc = wgpu::Default;
    encoderDesc.label = "Command encoder";
    wgpu::CommandEncoder encoder = device.createCommandEncoder(encoderDesc);

    // Render Pass
    wgpu::RenderPassDescriptor renderPassDesc = wgpu::Default;
    // Describe the render pass
    wgpu::RenderPassColorAttachment renderPassColorAttachment = wgpu::Default;
    renderPassColorAttachment.view = nextTexture;
    renderPassColorAttachment.resolveTarget = nullptr;
    renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
    renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
    renderPassColorAttachment.clearValue = wgpu::Color{ 0.05, 0.05, 0.05, 1.0 };
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;

    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.timestampWriteCount = 0;
    renderPassDesc.timestampWrites = nullptr;
    wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
    // Select which render pipeline to use
    renderPass.setPipeline(pipeline);
    // Set vertex buffers while encoding the render pass
    renderPass.setVertexBuffer(0, vertexBuffer, 0, pointData.size() * sizeof(float));
    renderPass.setIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint16, 0, indexData.size() * sizeof(uint16_t));
    // Replace `draw()` with `drawIndexed()` and `vertexCount` with `indexCount`
    // The extra argument is an offset within the index buffer.
    renderPass.drawIndexed(indexCount, 1, 0, 0, 0);
    renderPass.end();
    renderPass.release();
    nextTexture.release();

    // Done encoding commands, end the command buffer
    wgpu::CommandBufferDescriptor cmdBufferDescriptor = wgpu::Default;
    cmdBufferDescriptor.label = "Command buffer";
    wgpu::CommandBuffer command = encoder.finish(cmdBufferDescriptor);
    encoder.release();

    // Finally submit the command queue and present the swap chain
    queue.submit(command);
    command.release();
    swapChain.present();
#ifdef WEBGPU_BACKEND_DAWN
    // Check for pending error callbacks
    device.tick();
#endif

    // Poll events and check for window close request
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
  }

  // Cleanup WebGPU resources
  vertexBuffer.destroy();
  indexBuffer.destroy();
  vertexBuffer.release();
  indexBuffer.release();
  pipeline.release();
  shaderModule.release();
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