//

layout(std430, buffer_reference) buffer Materials;
layout(std430, buffer_reference) buffer Environments;

layout(std430, buffer_reference) buffer PerDrawData {
  mat4 model;
  mat4 view;
  mat4 proj;
  vec4 cameraPos;
};

layout(push_constant) uniform PerFrameData {
  mat4 model;
  PerDrawData drawable;
  Materials materials;
  Environments environments;
  uint matId;
  uint envId;
} perFrame;


uint getMaterialId() {
  return perFrame.matId;
}

uint getEnvironmentId() {
  return perFrame.envId;
}
