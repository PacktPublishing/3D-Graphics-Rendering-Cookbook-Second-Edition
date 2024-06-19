#include <Chapter06/05_SpecularGlossiness/src/common.sp>

const uint MaterialType_MetallicRoughness  = 0x1;
const uint MaterialType_SpecularGlossiness = 0x2;

const uint kMaxAttributes = 2;

struct InputAttributes {
  vec2 uv[kMaxAttributes];
};

struct SpecularGlossinessDataGPU {
  vec4 baseColorFactor;
  vec4 metallicRoughnessNormalOcclusion; // packed metallicFactor, roughnessFactor, normalScale, occlusionStrength
  vec4 specularGlossiness;               // packed specularFactor.xyz, glossiness 
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
  uint materialType;
  uint padding[3];
};

struct EnvironmentMapDataGPU {
  uint envMapTexture;
  uint envMapTextureSampler;
  uint envMapTextureIrradiance;
  uint envMapTextureIrradianceSampler;
  uint texBRDF_LUT;
  uint texBRDF_LUTSampler;
};

layout(std430, buffer_reference) readonly buffer Materials {
  SpecularGlossinessDataGPU material[];
};

layout(std430, buffer_reference) readonly buffer Environments {
  EnvironmentMapDataGPU environment[];
};

SpecularGlossinessDataGPU getMaterial(uint idx) {
  return perFrame.materials.material[idx]; 
}

EnvironmentMapDataGPU getEnvironmentMap(uint idx) {
  return perFrame.environments.environment[idx]; 
}

float getMetallicFactor(SpecularGlossinessDataGPU mat) {
  return mat.metallicRoughnessNormalOcclusion.x;
}

float getRoughnessFactor(SpecularGlossinessDataGPU mat) {
  return mat.metallicRoughnessNormalOcclusion.y;
}

float getNormalScale(SpecularGlossinessDataGPU mat) {
  return mat.metallicRoughnessNormalOcclusion.z;
}

float getOcclusionFactor(SpecularGlossinessDataGPU mat) {
  return mat.metallicRoughnessNormalOcclusion.w;
}

uint getMaterialType(SpecularGlossinessDataGPU mat) {
  return mat.materialType;
}

vec3 getSpecularFactor(SpecularGlossinessDataGPU mat) {
  return mat.specularGlossiness.xyz;
}

float getGlossinessFactor(SpecularGlossinessDataGPU mat) {
  return mat.specularGlossiness.w;
}


vec2 getNormalUV(InputAttributes tc, SpecularGlossinessDataGPU mat) {
  return tc.uv[mat.normalTextureUV];
}

vec4 sampleAO(InputAttributes tc, SpecularGlossinessDataGPU mat) {
  return textureBindless2D(mat.occlusionTexture, mat.occlusionTextureSampler, tc.uv[mat.occlusionTextureUV]);// * mat.metallicRoughnessNormalOcclusion.w;
}

vec4 sampleEmissive(InputAttributes tc, SpecularGlossinessDataGPU mat) {
  return textureBindless2D(mat.emissiveTexture, mat.emissiveTextureSampler, tc.uv[mat.emissiveTextureUV]) * vec4(mat.emissiveFactorAlphaCutoff.xyz, 1.0f);
}

vec4 sampleAlbedo(InputAttributes tc, SpecularGlossinessDataGPU mat) {
  return textureBindless2D(mat.baseColorTexture, mat.baseColorTextureSampler, tc.uv[mat.baseColorTextureUV]) * mat.baseColorFactor;
}

vec4 sampleMetallicRoughness(InputAttributes tc, SpecularGlossinessDataGPU mat) {
  return textureBindless2D(mat.metallicRoughnessTexture, mat.metallicRoughnessTextureSampler, tc.uv[mat.metallicRoughnessTextureUV]);
}

vec4 sampleNormal(InputAttributes tc, SpecularGlossinessDataGPU mat) {
  return textureBindless2D(mat.normalTexture, mat.normalTextureSampler, tc.uv[mat.normalTextureUV]);// * mat.metallicRoughnessNormalOcclusion.z;
}

vec4 sampleBRDF_LUT(vec2 tc, EnvironmentMapDataGPU map) {
  return textureBindless2D(map.texBRDF_LUT, map.texBRDF_LUTSampler, tc);
}

vec4 sampleEnvMap(vec3 tc, EnvironmentMapDataGPU map) {
  return textureBindlessCube(map.envMapTexture, map.envMapTextureSampler, tc);
}

vec4 sampleEnvMapLod(vec3 tc, float lod, EnvironmentMapDataGPU map) {
  return textureBindlessCubeLod(map.envMapTexture, map.envMapTextureSampler, tc, lod);
}

vec4 sampleEnvMapIrradiance(vec3 tc, EnvironmentMapDataGPU map) {
  return textureBindlessCube(map.envMapTextureIrradiance, map.envMapTextureIrradianceSampler, tc);
}

int sampleEnvMapQueryLevels(EnvironmentMapDataGPU map) {
  return textureBindlessQueryLevelsCube(map.envMapTexture);
}
