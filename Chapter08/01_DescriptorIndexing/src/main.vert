//
#version 460

#include <Chapter08/01_DescriptorIndexing/src/common.sp>

layout (location=0) out vec2 uv;

const vec2 pos[4] = vec2[4](
  vec2( 0.5, -0.5),
  vec2( 0.5,  0.5),
  vec2(-0.5, -0.5),
  vec2(-0.5,  0.5)
);

void main() {
  uv = pos[gl_VertexIndex] + vec2(0.5);

  vec2 p = pos[gl_VertexIndex] * vec2(pc.width, pc.height) + vec2(pc.x, pc.y);

  gl_Position = pc.proj * vec4(p, 0.0, 1.0);
}
