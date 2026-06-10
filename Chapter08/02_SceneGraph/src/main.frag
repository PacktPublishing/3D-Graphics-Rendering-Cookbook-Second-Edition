//

#include <Chapter08/02_SceneGraph/src/common.sp>
#include <data/shaders/AlphaTest.sp>
#include <data/shaders/UtilsPBR.sp>

// when enabled, foliage is anti-aliased via hardware alpha-to-coverage instead of a dithered alpha test
layout (constant_id = 0) const bool kEnableAlphaToCoverage = false;

layout (location=0) in vec2 uv;
layout (location=1) in vec3 normal;
layout (location=2) in vec3 worldPos;
layout (location=3) in flat uint materialId;

layout (location=0) out vec4 out_FragColor;

void main() {
  MetallicRoughnessDataGPU mat = pc.materials.material[materialId];

  vec4 emissiveColor = vec4(mat.emissiveFactorAlphaCutoff.rgb, 0) * textureBindless2D(mat.emissiveTexture, 0, uv);
  vec4 baseColor     = mat.baseColorFactor * (mat.baseColorTexture > 0 ? textureBindless2D(mat.baseColorTexture, 0, uv) : vec4(1.0));

  const float alphaCutoff = mat.emissiveFactorAlphaCutoff.w;

  // alpha-to-coverage only applies to opaque alpha-masked foliage; semi-transparent surfaces (glass, windows)
  // keep their dithered screen-door transparency, as their flat alpha has no edges for fwidth() to sharpen
  const bool isTransparent = mat.clearcoatTransmissionThickness.z > 0.0;
  const bool useA2C        = kEnableAlphaToCoverage && !isTransparent;

  // full coverage by default: opaque geometry and (in the A2C pipeline) semi-transparent screen-door surfaces
  float coverage = 1.0;

  if (useA2C) {
    // sharpen the alpha so the hardware can resolve smooth, anti-aliased foliage edges from the coverage mask
    // a larger 'thickness' widens the transition band, giving softer/thicker foliage silhouettes
    // https://bgolus.medium.com/anti-aliased-alpha-test-the-esoteric-alpha-to-coverage-8b177335ae4f
    if (alphaCutoff > 0.0)
      coverage = clamp((baseColor.a - alphaCutoff) / max(pc.alphaToCoverageThickness * fwidth(baseColor.a), 0.0001) + 0.5, 0.0, 1.0);
  } else {
    // scale alpha-cutoff by fwidth() to prevent alpha-tested foliage geometry from vanishing at large distances
    runAlphaTest(baseColor.a, alphaCutoff / max(32.0 * fwidth(uv.x), 1.0));
  }

  // world-space normal
  vec3 n = normalize(normal);

  // normal mapping: skip missing normal maps
  vec3 normalSample = textureBindless2D(mat.normalTexture, 0, uv).xyz;
  if (length(normalSample) > 0.5)
    n = perturbNormal(n, worldPos, normalSample, uv);

  const bool hasSkybox = pc.texSkyboxIrradiance > 0;

  // two hardcoded directional lights
  float NdotL1 = clamp(dot(n, normalize(vec3(-1, 1,+0.5))), 0.1, 1.0);
  float NdotL2 = clamp(dot(n, normalize(vec3(+1, 1,-0.5))), 0.1, 1.0);
  float NdotL = (hasSkybox ? 0.2 : 1.0) * (NdotL1+NdotL2);

  // IBL diffuse - not trying to be PBR-correct here, just make it simple & shiny
  const vec4 f0 = vec4(0.04);
  vec3 sky = vec3(-n.x, n.y, -n.z); // rotate skybox
  vec4 diffuse = hasSkybox ?
    (textureBindlessCube(pc.texSkyboxIrradiance, 0, sky) + vec4(NdotL)) * baseColor * (vec4(1.0) - f0) :
    NdotL * baseColor;

  out_FragColor = emissiveColor + diffuse;

  // the alpha-to-coverage pipeline turns the output alpha into a coverage mask; non-foliage keeps full
  // coverage (coverage == 1.0) so dithered screen-door transparency stays intact
  if (kEnableAlphaToCoverage)
    out_FragColor.a = coverage;
}
