#ifndef WEBGPU_THINGY_SRC_UTILS_H_
#define WEBGPU_THINGY_SRC_UTILS_H_

#include <cstdint>
#include <filesystem>
#include <vector>
#include <webgpu/webgpu.hpp>

wgpu::ShaderModule loadShaderModule(const std::filesystem::path& path, wgpu::Device device);
bool loadGeometry(const std::filesystem::path& path, std::vector<float>& pointData, std::vector<uint16_t>& indexData);

#endif //WEBGPU_THINGY_SRC_UTILS_H_
