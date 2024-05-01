//

layout(std430, buffer_reference) buffer Materials;
layout(std430, buffer_reference) buffer Environments;

layout(std430, buffer_reference) buffer PerDrawData {
  mat4 model;
  mat4 view;
  mat4 proj;
  vec4 cameraPos;
  uint matId;
  uint envId;
};

layout(push_constant) uniform PerFrameData {
  PerDrawData drawable;
  Materials materials;
  Environments environments;
} perFrame;

uint getMaterialId() {
  return perFrame.drawable.matId;
}

uint getEnvironmentId() {
  return perFrame.drawable.envId;
}
