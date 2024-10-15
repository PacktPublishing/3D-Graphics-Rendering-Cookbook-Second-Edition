#include <data/shaders/gltf/common.sp>

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 color;
layout (location = 3) in vec2 uv0;
layout (location = 4) in vec2 uv1;

vec3 getPosition() {
  return pos;
}

vec3 getNormal() {
  return normal;
}

vec4 getColor() {
  return color;
}

vec2 getTexCoord(uint i) {
  return i == 0 ? uv0 : uv1;
}

mat4 getModel() {
  uint mtxId = perFrame.transforms.transforms[gl_BaseInstance].mtxId;
  return perFrame.drawable.model * perFrame.matrices.matrix[mtxId];
}
