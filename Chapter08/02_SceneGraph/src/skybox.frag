//

layout(push_constant) uniform PerFrameData {
  mat4 view;
  mat4 proj;
  uint texSkybox;
} pc;

layout (location=0) in vec3 dir;

layout (location=0) out vec4 out_FragColor;

void main() {
	out_FragColor = textureBindlessCube(pc.texSkybox, 0, dir);
};
