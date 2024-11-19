//
layout (location=0) in vec2 uv;
layout (location=0) out vec4 out_FragColor;

const int ToneMappingMode_None = 0;
const int ToneMappingMode_Reinhard = 1;
const int ToneMappingMode_Uchimura = 2;
const int ToneMappingMode_KhronosPBR = 3;

layout(push_constant) uniform PushConstants {
  uint texColor;
  uint texLuminance;
  uint texBloom;
  uint smpl;
  int drawMode;

  float exposure;
  float bloomStrength;

  // Reinhard
  float maxWhite;

  // Uchimura
  float P;  // max display brightness
  float a;  // contrast
  float m;  // linear section start
  float l;  // linear section length
  float c;  // black tightness
  float b;  // pedestal

  // Khronos PBR
  float startCompression;  // highlight compression start
  float desaturation;      // desaturation speed
} pc;

// Uchimura 2017, "HDR theory and practice"
// http://cdn2.gran-turismo.com/data/www/pdi_publications/PracticalHDRandWCGinGTS_20181222.pdf
// Math: https://www.desmos.com/calculator/gslcdxvipg
// Source: https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp
vec3 uchimura(vec3 x, float P, float a, float m, float l, float c, float b) {
  float l0 = ((P - m) * l) / a;
  float L0 = m - m / a;
  float L1 = m + (1.0 - m) / a;
  float S0 = m + l0;
  float S1 = m + a * l0;
  float C2 = (a * P) / (P - S1);
  float CP = -C2 / P;

  vec3 w0 = vec3(1.0 - smoothstep(0.0, m, x));
  vec3 w2 = vec3(step(m + l0, x));
  vec3 w1 = vec3(1.0 - w0 - w2);

  vec3 T = vec3(m * pow(x / m, vec3(c)) + b);
  vec3 S = vec3(P - (P - S1) * exp(CP * (x - S0)));
  vec3 L = vec3(m + a * (x - m));

  return T * w0 + L * w1 + S * w2;
}

float luminance(vec3 v) {
  return dot(v, vec3(0.2126, 0.7152, 0.0722));
}

// "Tone Mapping" by Matt Taylor: https://64.github.io/tonemapping/
vec3 reinhard2(vec3 v, float maxWhite) {
  float l_old = luminance(v);
  float l_new = l_old * (1.0 + (l_old / (maxWhite * maxWhite))) / (1.0 + l_old);
  return v * (l_new / l_old);
}

// Khronos PBR Neutral Tone Mapper:
// https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/README.md#pbr-neutral-specification
// https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/pbrNeutral.glsl
vec3 PBRNeutralToneMapping(vec3 color, float startCompression, float desaturation) {
  startCompression -= 0.04;

  float x = min(color.r, min(color.g, color.b));
  float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
  color -= offset;

  float peak = max(color.r, max(color.g, color.b));
  if (peak < startCompression) return color;

  const float d = 1. - startCompression;
  float newPeak = 1. - d * d / (peak + d - startCompression);
  color *= newPeak / peak;

  float g = 1. - 1. / (desaturation * (peak - newPeak) + 1.);
  return mix(color, newPeak * vec3(1, 1, 1), g);
}

void main() {
  vec3 color = textureBindless2D(pc.texColor, pc.smpl, uv).rgb;
  vec3 bloom = textureBindless2D(pc.texBloom, pc.smpl, uv).rgb;
  float avgLuminance = textureBindless2D(pc.texLuminance, pc.smpl, vec2(0.5)).r;

  if (pc.drawMode != ToneMappingMode_None) {
    float midGray = 0.5;
    color *= pc.exposure * midGray / (avgLuminance + 0.001);
  }

  if (pc.drawMode == ToneMappingMode_Reinhard) {
    color = reinhard2(pc.exposure * color, pc.maxWhite);
  }
  if (pc.drawMode == ToneMappingMode_Uchimura) {
    color = uchimura(pc.exposure * color, pc.P, pc.a, pc.m, pc.l, pc.c, pc.b);
  }
  if (pc.drawMode == ToneMappingMode_KhronosPBR) {
    color = PBRNeutralToneMapping(pc.exposure * color, pc.startCompression, pc.desaturation);
  }

  out_FragColor = vec4(color + pc.bloomStrength * bloom, 1.0);
}
