//

layout(std430, buffer_reference) readonly buffer PerFrameData {
  mat4 view;
  mat4 proj;
  mat4 light;
  vec4 lightAngles;
  vec4 lightPos;
  uint shadowTexture;
  uint shadowSampler;
  float depthBias;
};

layout(push_constant) uniform PushConstants {
  mat4 model;
  PerFrameData perFrame;
  uint texture;
} pc;

struct PerVertex {
  vec2 uv;
  vec3 worldNormal;
  vec3 worldPos;
  vec4 shadowCoords;
};
