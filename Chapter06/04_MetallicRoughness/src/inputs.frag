#include <Chapter06/04_MetallicRoughness/src/common.sp>

const int kMaxAttributes = 2;

struct InputAttributes {
  vec2 uv[kMaxAttributes];
};

// corresponds to MetallicRoughnessDataGPU from Chapter06/04_MetallicRoughness/src/main.cpp
struct MetallicRoughnessDataGPU {
  vec4 baseColorFactor;
  vec4 metallicRoughnessNormalOcclusion; // packed metallicFactor, roughnessFactor, normalScale, occlusionStrength
  vec4 emissiveFactorAlphaCutoff;        // packed vec3 emissiveFactor + float AlphaCutoff
  uint occlusionTexture;
  uint occlusionTextureSampler;
  uint occlusionTextureUV;
  uint emissiveTexture;
  uint emissiveTextureSampler;
  uint emissiveTextureUV;
  uint baseColorTexture;
  uint baseColorTextureSampler;
  uint baseColorTextureUV;
  uint metallicRoughnessTexture;
  uint metallicRoughnessTextureSampler;
  uint metallicRoughnessTextureUV;
  uint normalTexture;
  uint normalTextureSampler;
  uint normalTextureUV;
  uint alphaMode;
};

// corresponds to EnvironmentMapDataGPU from shared/UtilsGLTF.h 
struct EnvironmentMapDataGPU {
  uint envMapTexture;
  uint envMapTextureSampler;
  uint envMapTextureIrradiance;
  uint envMapTextureIrradianceSampler;
  uint texBRDF_LUT;
  uint texBRDF_LUTSampler;
  uint unused0;
  uint unused1;
};

layout(std430, buffer_reference) readonly buffer Materials {
  MetallicRoughnessDataGPU material[];
};

layout(std430, buffer_reference) readonly buffer Environments {
  EnvironmentMapDataGPU environment[];
};

MetallicRoughnessDataGPU getMaterial(uint idx) {
  return perFrame.materials.material[idx]; 
}

EnvironmentMapDataGPU getEnvironmentMap(uint idx) {
  return perFrame.environments.environment[idx]; 
}

float getMetallicFactor(uint idx) {
  MetallicRoughnessDataGPU mat = getMaterial(idx);
  return mat.metallicRoughnessNormalOcclusion.x;
}

float getRoughnessFactor(uint idx) {
  MetallicRoughnessDataGPU mat = getMaterial(idx);
  return mat.metallicRoughnessNormalOcclusion.y;
}

float getNormalScale(uint idx) {
  MetallicRoughnessDataGPU mat = getMaterial(idx);
  return mat.metallicRoughnessNormalOcclusion.z;
}

float getOcclusionFactor(uint idx) {
  MetallicRoughnessDataGPU mat = getMaterial(idx);
  return mat.metallicRoughnessNormalOcclusion.w;
}

vec2 getNormalUV(InputAttributes tc, uint idx) {
  MetallicRoughnessDataGPU mat = getMaterial(idx);
  return tc.uv[mat.normalTextureUV];
}

vec4 sampleAO(InputAttributes tc, uint idx) {
  MetallicRoughnessDataGPU mat = getMaterial(idx);
  return textureBindless2D(mat.occlusionTexture, mat.occlusionTextureSampler, tc.uv[mat.occlusionTextureUV]);
}

vec4 sampleEmissive(InputAttributes tc, uint idx) {
  MetallicRoughnessDataGPU mat = getMaterial(idx);
  return textureBindless2D(mat.emissiveTexture, mat.emissiveTextureSampler, tc.uv[mat.emissiveTextureUV]) * vec4(mat.emissiveFactorAlphaCutoff.xyz, 1.0f);
}

vec4 sampleAlbedo(InputAttributes tc, uint idx) {
  MetallicRoughnessDataGPU mat = getMaterial(idx);
  return textureBindless2D(mat.baseColorTexture, mat.baseColorTextureSampler, tc.uv[mat.baseColorTextureUV]) * mat.baseColorFactor;
}

vec4 sampleMetallicRoughness(InputAttributes tc, uint idx) {
  MetallicRoughnessDataGPU mat = getMaterial(idx);
  return textureBindless2D(mat.metallicRoughnessTexture, mat.metallicRoughnessTextureSampler, tc.uv[mat.metallicRoughnessTextureUV]);
}

vec4 sampleNormal(InputAttributes tc, uint idx) {
  MetallicRoughnessDataGPU mat = getMaterial(idx);
  return textureBindless2D(mat.normalTexture, mat.normalTextureSampler, tc.uv[mat.normalTextureUV]);
}

vec4 sampleBRDF_LUT(vec2 tc, uint idx) {
  EnvironmentMapDataGPU map = getEnvironmentMap(idx);
  return textureBindless2D(map.texBRDF_LUT, map.texBRDF_LUTSampler, tc);
}

vec4 sampleEnvMap(vec3 tc, uint idx) {
  EnvironmentMapDataGPU map = getEnvironmentMap(idx);
  return textureBindlessCube(map.envMapTexture, map.envMapTextureSampler, tc);
}

vec4 sampleEnvMapLod(vec3 tc, float lod, uint idx) {
  EnvironmentMapDataGPU map = getEnvironmentMap(idx);
  return textureBindlessCubeLod(map.envMapTexture, map.envMapTextureSampler, tc, lod);
}

vec4 sampleEnvMapIrradiance(vec3 tc, uint idx) {
  EnvironmentMapDataGPU map = getEnvironmentMap(idx);
  return textureBindlessCube(map.envMapTextureIrradiance, map.envMapTextureIrradianceSampler, tc);
}

int sampleEnvMapQueryLevels(uint idx) {
  EnvironmentMapDataGPU map = getEnvironmentMap(idx);
  return textureBindlessQueryLevels2D(map.envMapTexture);
}
