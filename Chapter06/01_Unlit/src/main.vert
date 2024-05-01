//
#version 460 core

layout(push_constant) uniform PerFrameData {
  mat4 MVP;
  vec4 baseColor;
  uint textureId;
};

layout (location = 0) in vec3 pos;
layout (location = 1) in vec4 color;
layout (location = 2) in vec2 uv;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec4 outVertexColor;

void main() {
  gl_Position = MVP * vec4(pos, 1.0);
  outUV = uv;
  outVertexColor = color * baseColor;
}
