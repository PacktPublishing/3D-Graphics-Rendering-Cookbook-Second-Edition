#include <data/shaders/gltf/common.sp>
#include <data/shaders/gltf/common_material.sp>

const int kMaxAttributes = 2;

struct InputAttributes {
  vec2 uv[kMaxAttributes];
};

struct Light {
  vec3 direction;
  float range;

  vec3 color;
  float intensity;

  vec3 position;
  float innerConeCos;

  float outerConeCos;
  int type;
  int padding[2];
};

const int LightType_Directional = 0;
const int LightType_Point = 1;
const int LightType_Spot = 2;

struct EnvironmentMapDataGPU {
  uint envMapTexture;
  uint envMapTextureSampler;
  uint envMapTextureIrradiance;
  uint envMapTextureIrradianceSampler;
  uint texBRDFLUT;
  uint texBRDFLUTSampler;
  uint envMapTextureCharlie;
  uint envMapTextureCharlieSampler;
};

layout(std430, buffer_reference) readonly buffer Materials {
  MetallicRoughnessDataGPU material[];
};

layout(std430, buffer_reference) readonly buffer Environments {
  EnvironmentMapDataGPU environment[];
};

layout(std430, buffer_reference) readonly buffer Lights {
  Light lights[];
};

MetallicRoughnessDataGPU getMaterial(uint idx) {
  return perFrame.materials.material[idx]; 
}

EnvironmentMapDataGPU getEnvironmentMap(uint idx) {
  return perFrame.environments.environment[idx]; 
}

float getMetallicFactor(MetallicRoughnessDataGPU mat) {
  return mat.metallicRoughnessNormalOcclusion.x;
}

float getRoughnessFactor(MetallicRoughnessDataGPU mat) {
  return mat.metallicRoughnessNormalOcclusion.y;
}

float getNormalScale(MetallicRoughnessDataGPU mat) {
  return mat.metallicRoughnessNormalOcclusion.z;
}

float getOcclusionFactor(MetallicRoughnessDataGPU mat) {
  return mat.metallicRoughnessNormalOcclusion.w;
}

uint getMaterialType(MetallicRoughnessDataGPU mat) {
  return mat.materialType;
}

vec3 getSpecularFactor(MetallicRoughnessDataGPU mat) {
  return mat.specularGlossiness.xyz;
}

float getGlossinessFactor(MetallicRoughnessDataGPU mat) {
  return mat.specularGlossiness.w;
}

vec4 getEmissiveFactorAlphaCutoff(MetallicRoughnessDataGPU mat) {
  return mat.emissiveFactorAlphaCutoff;
}

vec4 getSheenColorFactor(InputAttributes tc, MetallicRoughnessDataGPU mat) {
  return textureBindless2D(mat.sheenColorTexture, mat.sheenColorTextureSampler, tc.uv[mat.sheenColorTextureUV]) * vec4(mat.sheenFactors.xyz, 1.0f);
}

float getSheenRoughnessFactor(InputAttributes tc, MetallicRoughnessDataGPU mat) {
  return textureBindless2D(mat.sheenRoughnessTexture, mat.sheenRoughnessTextureSampler, tc.uv[mat.sheenRoughnessTextureUV]).a * mat.sheenFactors.a;
}

float getClearcoatFactor(InputAttributes tc, MetallicRoughnessDataGPU mat) {
  return textureBindless2D(mat.clearCoatTexture, mat.clearCoatTextureSampler, tc.uv[mat.clearCoatTextureUV]).r * mat.clearcoatTransmissionThickness.x;
}

float getClearcoatRoughnessFactor(InputAttributes tc, MetallicRoughnessDataGPU mat) {
  return textureBindless2D(mat.clearCoatRoughnessTexture, mat.clearCoatRoughnessTextureSampler, tc.uv[mat.clearCoatRoughnessTextureUV]).g * mat.clearcoatTransmissionThickness.y;
}

float getTransmissionFactor(InputAttributes tc, MetallicRoughnessDataGPU mat) {
  return textureBindless2D(mat.transmissionTexture, mat.transmissionTextureSampler, tc.uv[mat.transmissionTextureUV]).r * mat.clearcoatTransmissionThickness.z;
}

float getVolumeTickness(InputAttributes tc, MetallicRoughnessDataGPU mat) {
  return textureBindless2D(mat.thicknessTexture, mat.thicknessTextureSampler, tc.uv[mat.thicknessTextureUV]).g * mat.clearcoatTransmissionThickness.w;
}

vec4 getVolumeAttenuation(MetallicRoughnessDataGPU mat) {
  return mat.attenuation;
}

float getIOR(MetallicRoughnessDataGPU mat) {
  return mat.ior;
}

vec3 getSpecularColorFactor(InputAttributes tc, MetallicRoughnessDataGPU mat) {
  return textureBindless2D(mat.specularColorTexture, mat.specularColorTextureSampler, tc.uv[mat.specularColorTextureUV]).rgb * mat.specularFactors.rgb;
}

float getSpecularFactor(InputAttributes tc, MetallicRoughnessDataGPU mat) {
  return textureBindless2D(mat.specularTexture, mat.specularTextureSampler, tc.uv[mat.specularTextureUV]).a * mat.specularFactors.a;
}

vec2 getNormalUV(InputAttributes tc, MetallicRoughnessDataGPU mat) {
  return mat.normalTextureUV > -1 ? tc.uv[mat.normalTextureUV] : tc.uv[0];
}

vec4 sampleAO(InputAttributes tc, MetallicRoughnessDataGPU mat)  {
  return textureBindless2D(mat.occlusionTexture, mat.occlusionTextureSampler, tc.uv[mat.occlusionTextureUV]);
}

vec4 sampleEmissive(InputAttributes tc, MetallicRoughnessDataGPU mat) {
  vec2 uv = mat.emissiveTextureUV > -1 ? tc.uv[mat.emissiveTextureUV] : tc.uv[0];
  return textureBindless2D(mat.emissiveTexture, mat.emissiveTextureSampler, uv) * vec4(mat.emissiveFactorAlphaCutoff.xyz, 1.0f);
}

vec4 sampleAlbedo(InputAttributes tc, MetallicRoughnessDataGPU mat) {
  return textureBindless2D(mat.baseColorTexture, mat.baseColorTextureSampler, tc.uv[mat.baseColorTextureUV]) * mat.baseColorFactor;
}

vec4 sampleMetallicRoughness(InputAttributes tc, MetallicRoughnessDataGPU mat) {
  return textureBindless2D(mat.metallicRoughnessTexture, mat.metallicRoughnessTextureSampler, tc.uv[mat.metallicRoughnessTextureUV]);
}

vec4 sampleNormal(InputAttributes tc, MetallicRoughnessDataGPU mat) {
  return textureBindless2D(mat.normalTexture, mat.normalTextureSampler, tc.uv[mat.normalTextureUV]);// * mat.metallicRoughnessNormalOcclusion.z;
}

vec4 sampleClearcoatNormal(InputAttributes tc, MetallicRoughnessDataGPU mat) {
  return textureBindless2D(mat.clearCoatNormalTexture, mat.clearCoatNormalTextureSampler, tc.uv[mat.clearCoatNormalTextureUV]);
}

vec4 sampleBRDF_LUT(vec2 tc, EnvironmentMapDataGPU map) {
  return textureBindless2D(map.texBRDFLUT, map.texBRDFLUTSampler, tc);
}

vec4 sampleEnvMap(vec3 tc, EnvironmentMapDataGPU map) {
  return textureBindlessCube(map.envMapTexture, map.envMapTextureSampler, tc);
}

vec4 sampleEnvMapLod(vec3 tc, float lod, EnvironmentMapDataGPU map) {
  return textureBindlessCubeLod(map.envMapTexture, map.envMapTextureSampler, tc, lod);
}

vec4 sampleCharlieEnvMapLod(vec3 tc, float lod, EnvironmentMapDataGPU map) {
  return textureBindlessCubeLod(map.envMapTextureCharlie, map.envMapTextureCharlieSampler, tc, lod);
}

vec4 sampleEnvMapIrradiance(vec3 tc, EnvironmentMapDataGPU map) {
  return textureBindlessCube(map.envMapTextureIrradiance, map.envMapTextureIrradianceSampler, tc);
}

int sampleEnvMapQueryLevels(EnvironmentMapDataGPU map) {
  return textureBindlessQueryLevelsCube(map.envMapTexture);
}

vec4 sampleTransmissionFramebuffer(vec2 tc) {
  return textureBindless2D(perFrame.transmissionFramebuffer, perFrame.transmissionFramebufferSampler, tc);
}

bool isMaterialTypeSheen(MetallicRoughnessDataGPU mat) {
  return (getMaterialType(mat) & 0x4) != 0;
}

bool isMaterialTypeClearCoat(MetallicRoughnessDataGPU mat) {
  return (getMaterialType(mat) & 0x8) != 0;
}

bool isMaterialTypeSpecular(MetallicRoughnessDataGPU mat) {
  return (getMaterialType(mat) & 0x10) != 0;
}

bool isMaterialTypeTransmission(MetallicRoughnessDataGPU mat) {
  return (getMaterialType(mat) & 0x20) != 0;
}

bool isMaterialTypeVolume(MetallicRoughnessDataGPU mat) {
  return (getMaterialType(mat) & 0x40) != 0;
}

bool isMaterialTypeUnlit(MetallicRoughnessDataGPU mat) {
  return (getMaterialType(mat) & 0x80) != 0;
}


uint getLightsCount() {
  return perFrame.lightsCount;
}

Light getLight(uint i) {
  return perFrame.lights.lights[i];
}

mat4 getModel() {
  uint mtxId = perFrame.transforms.transforms[oBaseInstance].mtxId;
  return perFrame.drawable.model * perFrame.matrices.matrix[mtxId];
}

uint getMaterialId() {
  return perFrame.transforms.transforms[oBaseInstance].matId;
}
