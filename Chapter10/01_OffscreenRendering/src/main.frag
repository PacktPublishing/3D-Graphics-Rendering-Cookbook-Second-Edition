//

layout (location=0) in vec2 uv;

layout (location=0) out vec4 out_FragColor;

layout(push_constant) uniform PushConstants {
  mat4 mvp;
  uint texture;
};

void main() {
  out_FragColor = textureBindless2D(texture, 0, uv);
}
