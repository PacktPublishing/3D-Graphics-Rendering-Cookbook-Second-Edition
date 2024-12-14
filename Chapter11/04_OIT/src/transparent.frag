//

#include <Chapter11/04_OIT/src/common.sp>
#include <data/shaders/UtilsPBR.sp>

layout (early_fragment_tests) in;

layout (set = 0, binding = 2, r32ui) uniform uimage2D kTextures2DInOut[];

layout (location=0) in vec2 uv;
layout (location=1) in vec3 normal;
layout (location=2) in vec3 worldPos;
layout (location=3) in flat uint materialId;

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
  return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
  MetallicRoughnessDataGPU mat = pc.materials.material[materialId];

  vec4 emissiveColor = vec4(mat.emissiveFactorAlphaCutoff.rgb, 0) * textureBindless2D(mat.emissiveTexture, 0, uv);
  vec4 baseColor = mat.baseColorFactor * (mat.baseColorTexture > 0 ? textureBindless2D(mat.baseColorTexture, 0, uv) : vec4(1.0));

  // world-space normal
  vec3 n = normalize(normal);

  // normal mapping: skip missing normal maps
  vec3 normalSample = textureBindless2D(mat.normalTexture, 0, uv).xyz;
  if (length(normalSample) > 0.5)
    n = perturbNormal(n, worldPos, normalSample, uv);

  // two hardcoded directional lights
  float NdotL1 = clamp(dot(n, normalize(vec3(-1, 1,+0.5))), 0.1, 1.0);
  float NdotL2 = clamp(dot(n, normalize(vec3(+1, 1,-0.5))), 0.1, 1.0);
  float NdotL = 0.2 * (NdotL1+NdotL2);

  // IBL diffuse - not trying to be PBR-correct here, just make it simple & shiny
  const vec4 f0 = vec4(0.04);
  vec3 sky = vec3(-n.x, n.y, -n.z); // rotate skybox
  vec4 diffuse = (textureBindlessCube(pc.texSkyboxIrradiance, 0, sky) + vec4(NdotL)) * baseColor * (vec4(1.0) - f0);
  // some ad hoc environment reflections for transparent objects
  vec3 v = normalize(pc.cameraPos.xyz - worldPos);
  vec3 reflection = reflect(v, n);
  reflection = vec3(reflection.x, -reflection.y, reflection.z); // rotate reflection
  vec3 colorRefl = textureBindlessCube(pc.texSkybox, 0, reflection).rgb;
  vec3 kS = fresnelSchlickRoughness(clamp(dot(n, v), 0.0, 1.0), vec3(f0), 0.1);
  vec3 color = emissiveColor.rgb + diffuse.rgb + colorRefl * kS;

  // Order-Independent Transparency: https://fr.slideshare.net/hgruen/oit-and-indirect-illumination-using-dx11-linked-lists
  float alpha = clamp(baseColor.a * mat.clearcoatTransmissionThickness.z, 0.0, 1.0);
  bool isTransparent = (alpha > 0.01) && (alpha < 0.99);
  if (isTransparent && !gl_HelperInvocation && gl_SampleMaskIn[0] == (1 << gl_SampleID)) {
    uint index = atomicAdd(pc.oit.atomicCounter.numFragments, 1);
    if (index < pc.oit.maxOITFragments) {
      uint prevIndex = imageAtomicExchange(kTextures2DInOut[pc.oit.texHeadsOIT], ivec2(gl_FragCoord.xy), index);
      TransparentFragment frag;
      frag.color = f16vec4(color, alpha);
      frag.depth = gl_FragCoord.z;
      frag.next  = prevIndex;
      pc.oit.oitLists.frags[index] = frag;
    }
  }
}
