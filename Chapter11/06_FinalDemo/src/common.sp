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
  // ray-traced ambient occlusion with spatial hashing (Gautron et al. 2020)
  // NOTE: this struct is shared with the vertex stage (no int64 there), so the hash-map
  // device address is stored as a uvec2 and reconstructed into a HashSlot reference in AO.sp
  uint tlas;
  uint frameId;
  uvec2 hashSlot;
  uint enableAO;
  uint enableSpatialHash;
  uint enableFiltering;
  uint aoSamples;
  float aoRadius;
  float aoPower;
  float sp;            // target screen-space cell size in pixels
  float smin;          // minimal world-space cell size
  uint maxSamples;     // max accumulated samples per hash cell
  uint hashMapSize;    // number of hash slots (power of two)
  float resolutionY;
  float projScaleY;    // proj[1][1] = 1/tan(fovY/2)
  float a2cThickness;  // alpha-to-coverage edge softness (only read when A2C is enabled)
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
