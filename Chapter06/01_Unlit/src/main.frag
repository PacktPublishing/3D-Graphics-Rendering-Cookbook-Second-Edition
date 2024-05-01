//
layout(push_constant) uniform PerFrameData {
  mat4 MVP;
  vec4 baseColor;
  uint textureId;
};

layout (location = 0) in vec2 uv;
layout (location = 1) in vec4 vertexColor;

layout (location=0) out vec4 out_FragColor;

void main() {
  vec4 baseColorTexture = textureBindless2D(textureId, 0, uv);
  out_FragColor = textureBindless2D(textureId, 0, uv) * vertexColor;
}
