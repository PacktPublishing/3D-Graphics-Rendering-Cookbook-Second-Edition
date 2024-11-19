//

layout (location = 0) in  vec3 pos;
layout (location = 0) out vec2 uv;

layout(push_constant) uniform PushConstants {
  mat4 mvp;
};

#define PI 3.1415926

float atan2(float y, float x) {
  return x == 0.0 ? sign(y) * PI/2 : atan(y, x);
}

void main() {
  gl_Position = mvp * vec4(pos, 1.0);

  float theta = atan2(pos.y, pos.x) / PI + 0.5;

  uv = vec2(theta, pos.z);
}
