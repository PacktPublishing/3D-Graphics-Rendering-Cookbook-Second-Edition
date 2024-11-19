//

layout (location=0) in vec2 uv;

layout (location=0) out vec4 out_FragColor;

layout(push_constant) uniform PushConstants {
  uint texColor;
  uint texSSAO;
  uint smpl;
  float scale;
  float bias;
} pc;

void main() {
  vec4  color = textureBindless2D(pc.texColor, pc.smpl, uv);
  float ssao  = clamp( textureBindless2D(pc.texSSAO,  pc.smpl, uv).x + pc.bias, 0.0, 1.0 );

  out_FragColor = vec4(
    mix(color, color * ssao, pc.scale).rgb,
    1.0
  );
}
