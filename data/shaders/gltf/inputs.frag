#include <data/shaders/gltf/common.sp>

const int kMaxAttributes = 2;

struct InputAttributes {
	vec2 uv[kMaxAttributes];
};

struct MetallicRoughness {
	vec4 baseColorFactor;
	vec4 metallicRoughnessNormalOcclusion; // Packed metallicFactor, roughnessFactor, normalScale, occlusionStrength
	vec4 specularGlossiness; // Packed specularFactor.xyz, glossiness 
	vec4 sheenFactors;
	vec4 clearcoatTransmissionTickness;
	vec4 specularFactors;
	vec4 attenuation;
	vec4 iridescence;
	//vec4 emissiveStrengthIor;
	vec4 anisotropy;
	vec4 emissiveFactorAlphaCutoff; // vec3 emissiveFactor + float AlphaCutoff
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
	uint sheenColorTexture;
	uint sheenColorTextureSampler;
	uint sheenColorTextureUV;
	uint sheenRoughnessTexture;
	uint sheenRoughnessTextureSampler;
	uint sheenRoughnessTextureUV;
	uint clearCoatTexture;
	uint clearCoatTextureSampler;
	uint clearCoatTextureUV;
	uint clearCoatRoughnessTexture;
	uint clearCoatRoughnessTextureSampler;
	uint clearCoatRoughnessTextureUV;
	uint clearCoatNormalTexture;
	uint clearCoatNormalTextureSampler;
	uint clearCoatNormalTextureUV;
	uint specularTexture;
	uint specularTextureSampler;
	uint specularTextureUV;
	uint specularColorTexture;
	uint specularColorTextureSampler;
	uint specularColorTextureUV;
	uint transmissionTexture;
	uint transmissionTextureSampler;
	uint transmissionTextureUV;
	uint thicknessTexture;
	uint thicknessTextureSampler;
	uint thicknessTextureUV;
	uint iridescenceTexture;
	uint iridescenceTextureSampler;
	uint iridescenceTextureUV;
	uint iridescenceThicknessTexture;
	uint iridescenceThicknessTextureSampler;
	uint iridescenceThicknessTextureUV;
	uint anisotropyTexture;
	uint anisotropyTextureSampler;
	uint anisotropyTextureUV;
	uint alphaMode;
	uint materialType;
	uint padding[3];
};

struct EnvironmentMap {
	uint envMapTexture;
	uint envMapTextureSampler;
	uint envMapTextureIrradiance;
	uint envMapTextureIrradianceSampler;
	uint texBRDF_LUT;
	uint texBRDF_LUTSampler;
	uint envMapTextureCharlie;
	uint envMapTextureCharlieSampler;
};

const uint kMaxMaterials = 128;
const uint kMaxEnvironments = 4;

layout(std430, buffer_reference) readonly buffer Materials {
	MetallicRoughness material[kMaxMaterials];
};

layout(std430, buffer_reference) readonly buffer Environments {
	EnvironmentMap environment[kMaxEnvironments];
};

MetallicRoughness getMaterial(uint idx) 
{
	return perFrame.materials.material[idx]; 
}

EnvironmentMap getEnvironmentMap(uint idx) 
{
	return perFrame.environments.environment[idx]; 
}

float getMetallicFactor(uint idx)
{
	MetallicRoughness mat = getMaterial(idx);
	return mat.metallicRoughnessNormalOcclusion.x;
}

float getRoughnessFactor(uint idx)
{
	MetallicRoughness mat = getMaterial(idx);
	return mat.metallicRoughnessNormalOcclusion.y;
}

float getNormalScale(uint idx)
{
	MetallicRoughness mat = getMaterial(idx);
	return mat.metallicRoughnessNormalOcclusion.z;
}

float getOcclusionFactor(uint idx)
{
	MetallicRoughness mat = getMaterial(idx);
	return mat.metallicRoughnessNormalOcclusion.w;
}

uint getMaterialType(uint idx)
{
	MetallicRoughness mat = getMaterial(idx);
	return mat.materialType;
}

vec3 getSpecularFactor(uint idx)
{
	MetallicRoughness mat = getMaterial(idx);
	return mat.specularGlossiness.xyz;
}

float getGlossinessFactor(uint idx)
{
	MetallicRoughness mat = getMaterial(idx);
	return mat.specularGlossiness.w;
}

vec4 getEmissiveFactorAlphaCutoff(uint idx)
{
	MetallicRoughness mat = getMaterial(idx);
	return mat.emissiveFactorAlphaCutoff;
}

vec4 getSheenColorFactor(InputAttributes tc, uint idx)
{
	MetallicRoughness mat = getMaterial(idx);
	return textureBindless2D(mat.sheenColorTexture, mat.sheenColorTextureSampler, tc.uv[mat.sheenColorTextureUV]) * vec4(mat.sheenFactors.xyz, 1.0f);
}

float getSheenRoughnessFactor(InputAttributes tc, uint idx)
{
	MetallicRoughness mat = getMaterial(idx);
	return textureBindless2D(mat.sheenRoughnessTexture, mat.sheenRoughnessTextureSampler, tc.uv[mat.sheenRoughnessTextureUV]).a * mat.sheenFactors.a;
}

float getClearcoatFactor(InputAttributes tc, uint idx)
{
	MetallicRoughness mat = getMaterial(idx);
	return textureBindless2D(mat.clearCoatTexture, mat.clearCoatTextureSampler, tc.uv[mat.clearCoatTextureUV]).r * mat.clearcoatTransmissionTickness.x;
}

float getClearcoatRoughnessFactor(InputAttributes tc, uint idx)
{
	MetallicRoughness mat = getMaterial(idx);
	return textureBindless2D(mat.clearCoatRoughnessTexture, mat.clearCoatRoughnessTextureSampler, tc.uv[mat.clearCoatRoughnessTextureUV]).g * mat.clearcoatTransmissionTickness.y;
}

vec3 getSpecularColorFactor(InputAttributes tc, uint idx)
{
    MetallicRoughness mat = getMaterial(idx);
    return textureBindless2D(mat.specularColorTexture, mat.specularColorTextureSampler, tc.uv[mat.specularColorTextureUV]).rgb * mat.specularFactors.rgb;
}

float getSpecularFactor(InputAttributes tc, uint idx)
{
    MetallicRoughness mat = getMaterial(idx);
    return textureBindless2D(mat.specularTexture, mat.specularTextureSampler, tc.uv[mat.specularTextureUV]).a * mat.specularFactors.a;
}

vec2 getNormalUV(InputAttributes tc, uint idx) 
{
	MetallicRoughness mat = getMaterial(idx);
	return tc.uv[mat.normalTextureUV];
}

vec4 sampleAO(InputAttributes tc, uint idx) 
{
	MetallicRoughness mat = getMaterial(idx);
	return textureBindless2D(mat.occlusionTexture, mat.occlusionTextureSampler, tc.uv[mat.occlusionTextureUV]);// * mat.metallicRoughnessNormalOcclusion.w;
}

vec4 sampleEmissive(InputAttributes tc, uint idx) 
{
	MetallicRoughness mat = getMaterial(idx);
	return textureBindless2D(mat.emissiveTexture, mat.emissiveTextureSampler, tc.uv[mat.emissiveTextureUV]) * vec4(mat.emissiveFactorAlphaCutoff.xyz, 1.0f);
}

vec4 sampleAlbedo(InputAttributes tc, uint idx) 
{
	MetallicRoughness mat = getMaterial(idx);
	return textureBindless2D(mat.baseColorTexture, mat.baseColorTextureSampler, tc.uv[mat.baseColorTextureUV]) * mat.baseColorFactor;
}

vec4 sampleMetallicRoughness(InputAttributes tc, uint idx) 
{
	MetallicRoughness mat = getMaterial(idx);
	return textureBindless2D(mat.metallicRoughnessTexture, mat.metallicRoughnessTextureSampler, tc.uv[mat.metallicRoughnessTextureUV]);
}

vec4 sampleNormal(InputAttributes tc, uint idx) 
{
	MetallicRoughness mat = getMaterial(idx);
	return textureBindless2D(mat.normalTexture, mat.normalTextureSampler, tc.uv[mat.normalTextureUV]);// * mat.metallicRoughnessNormalOcclusion.z;
}

vec4 sampleClearcoatNormal(InputAttributes tc, uint idx)
{
	MetallicRoughness mat = getMaterial(idx);
	return textureBindless2D(mat.clearCoatNormalTexture, mat.clearCoatNormalTextureSampler, tc.uv[mat.clearCoatNormalTextureUV]);
}

vec4 sampleBRDF_LUT(vec2 tc, uint idx) 
{
	EnvironmentMap map = getEnvironmentMap(idx);
	return textureBindless2D(map.texBRDF_LUT, map.texBRDF_LUTSampler, tc);
}

vec4 sampleEnvMap(vec3 tc, uint idx) 
{
	EnvironmentMap map = getEnvironmentMap(idx);
	return textureBindlessCube(map.envMapTexture, map.envMapTextureSampler, tc);
}

vec4 sampleEnvMapLod(vec3 tc, float lod, uint idx) 
{
	EnvironmentMap map = getEnvironmentMap(idx);
	return textureBindlessCubeLod(map.envMapTexture, map.envMapTextureSampler, tc, lod);
}

vec4 sampleCharlieEnvMapLod(vec3 tc, float lod, uint idx) 
{
	EnvironmentMap map = getEnvironmentMap(idx);
	return textureBindlessCubeLod(map.envMapTextureCharlie, map.envMapTextureCharlieSampler, tc, lod);
}


vec4 sampleEnvMapIrradiance(vec3 tc, uint idx) 
{
	EnvironmentMap map = getEnvironmentMap(idx);
	return textureBindlessCube(map.envMapTextureIrradiance, map.envMapTextureIrradianceSampler, tc);
}

int sampleEnvMapQueryLevels(uint idx) 
{
	EnvironmentMap map = getEnvironmentMap(idx);
	return textureBindlessQueryLevels2D(map.envMapTexture);
}
