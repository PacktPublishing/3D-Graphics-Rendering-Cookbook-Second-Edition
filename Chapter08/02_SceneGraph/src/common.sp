//

#include <data/shaders/gltf/common_material.sp>

struct DrawData {
  uint transformId;
  uint materialId;
};

layout(std430, buffer_reference) readonly buffer TransformBuffer {
  mat4 model[];
};

layout(std430, buffer_reference) readonly buffer DrawDataBuffer {
  DrawData dd[];
};

layout(std430, buffer_reference) readonly buffer MaterialBuffer {
  MetallicRoughnessDataGPU material[];
};

layout(push_constant) uniform PerFrameData {
  mat4 viewProj;
  TransformBuffer transforms;
  DrawDataBuffer drawData;
  MaterialBuffer materials;
  uint texSkyboxIrradiance;
} pc;
