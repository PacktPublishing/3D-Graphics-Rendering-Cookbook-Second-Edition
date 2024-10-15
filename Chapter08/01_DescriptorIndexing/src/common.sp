//

layout(push_constant) uniform PerFrameData {
  mat4 proj;
  uint textureId;
  float x;
  float y;
  float width;
  float height;
  float alphaScale;
} pc;
