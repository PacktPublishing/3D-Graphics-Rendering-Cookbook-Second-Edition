//

#include <data/shaders/gltf/common_material.sp>
#include <Chapter11/04_OIT/src/common_oit.sp>

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

layout(std430, buffer_reference) buffer AtomicCounter {
  uint numFragments;
};

layout(std430, buffer_reference) readonly buffer LightBuffer {
  mat4 viewProjBias;
  vec4 lightDir;
  uint shadowTexture;
  uint shadowSampler;
};

layout(std430, buffer_reference) buffer OIT {
  AtomicCounter atomicCounter;
  TransparencyListsBuffer oitLists;
  uint texHeadsOIT;
  uint maxOITFragments;
};

layout(push_constant) uniform PerFrameData {
  mat4 viewProj;
  vec4 cameraPos;
  TransformBuffer transforms;
  DrawDataBuffer drawData;
  MaterialBuffer materials;
  OIT oit;
  LightBuffer light; // one directional light
  uint texSkybox;
  uint texSkyboxIrradiance;
} pc;
