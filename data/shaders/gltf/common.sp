//

layout(std430, buffer_reference) buffer Materials;
layout(std430, buffer_reference) buffer Environments;

layout(std430, buffer_reference) buffer PerDrawData {
  mat4 model;
  mat4 view;
  mat4 proj;
  vec4 cameraPos;
};

struct TransformsBuffer {
  mat4 model;
  uint matId;
  uint nodeRef; // for CPU only
  uint meshRef; // for CPU only
  uint opaque;  // for CPU only
};

layout(std430, buffer_reference) readonly buffer Transforms {
  TransformsBuffer transforms[];
};

layout(push_constant) uniform PerFrameData {
  PerDrawData drawable;
  Materials materials;
  Environments environments;
  Transforms  transforms;
  uint transformId;
  uint envId;
  uint transmissionFramebuffer;
  uint transmissionFramebufferSampler;
} perFrame;

uint getMaterialId() {
  return perFrame.transforms.transforms[perFrame.transformId].matId;
}

uint getEnvironmentId() {
  return perFrame.envId;
}

mat4 getModel() {
  return perFrame.drawable.model * perFrame.transforms.transforms[perFrame.transformId].model;
}

mat4 getViewProjection() {
  return perFrame.drawable.proj * perFrame.drawable.view;
}
