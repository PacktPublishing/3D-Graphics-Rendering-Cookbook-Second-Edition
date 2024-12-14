//

#include <Chapter11/03_DirectionalShadows/src/common.sp>

layout (location=0) in vec3 in_pos;
layout (location=1) in vec2 in_tc;
layout (location=2) in vec3 in_normal;

layout (location=0) out vec2 uv;
layout (location=1) out flat uint materialId;

void main() {
  mat4 model = pc.transforms.model[pc.drawData.dd[gl_BaseInstance].transformId];
  gl_Position = pc.viewProj * model * vec4(in_pos, 1.0);
  uv = vec2(in_tc.x, 1.0-in_tc.y);
  materialId = pc.drawData.dd[gl_BaseInstance].materialId;
}
