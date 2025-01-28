//

#include <Chapter05/02_VertexPulling/src/common.sp>

vec3 getPosition(int i) {
  return vec3(vtx.in_Vertices[i].x, vtx.in_Vertices[i].y, vtx.in_Vertices[i].z);
}

vec2 getTexCoord(int i) {
  return vec2(vtx.in_Vertices[i].u, vtx.in_Vertices[i].v);
}

layout (location=0) out vec2 uv;

void main() {
  gl_Position = MVP * vec4(getPosition(gl_VertexIndex), 1.0);
  uv = getTexCoord(gl_VertexIndex);
}
