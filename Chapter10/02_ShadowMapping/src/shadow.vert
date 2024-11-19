//

#include <Chapter10/02_ShadowMapping/src/common.sp>

layout (location = 0) in vec3 pos;

void main() {
  gl_Position = pc.perFrame.proj * pc.perFrame.view * pc.model * vec4(pos, 1.0);
}

