//

#include <Chapter11/06_FinalDemo/src/common.sp>

layout (location=0) in vec3 in_pos;
layout (location=1) in vec2 in_tc;
layout (location=2) in vec3 in_normal;

layout (location=0) out vec2 uv;
layout (location=1) out vec3 normal;
layout (location=2) out vec3 worldPos;
layout (location=3) out flat uint materialId;
layout (location=4) out vec4 shadowCoords;

void main() {
  mat4 model = pc.transforms.model[pc.drawData.dd[gl_BaseInstance].transformId];
  gl_Position = pc.viewProj * model * vec4(in_pos, 1.0);
  uv = vec2(in_tc.x, 1.0-in_tc.y);
  // cofactor matrix = det(M) * transpose(inverse(M)); unlike inverse() it stays finite for the
  // singular node transforms in Bistro, and the det() scale is normalized away in the fragment shader
  mat3 m = mat3(model);
  normal = mat3(cross(m[1], m[2]), cross(m[2], m[0]), cross(m[0], m[1])) * in_normal;
  vec4 posClip = model * vec4(in_pos, 1.0);
  worldPos = posClip.xyz/posClip.w;
  materialId = pc.drawData.dd[gl_BaseInstance].materialId;

  shadowCoords = pc.light.viewProjBias * posClip;
}
