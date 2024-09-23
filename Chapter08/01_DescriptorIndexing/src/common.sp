//

layout(push_constant) uniform PerFrameData {
  mat4 proj;
  uint textureId;
  float x;
  float y;
  float w;
  float h;
  float alphaScale;
} pc;
