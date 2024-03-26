//

layout(push_constant) uniform PerFrameData {
  mat4 MVP;
  uvec2 bufferId;
  uint textureId;
  float time;
  uint numU;
  uint numV;
  float minU, maxU;
  float minV, maxV;
  uint P1, P2;
  uint Q1, Q2;
  float morph;
} pc;
