//

struct MetallicRoughnessDataGPU {
  vec4 baseColorFactor;
  vec4 metallicRoughnessNormalOcclusion; // Packed metallicFactor, roughnessFactor, normalScale, occlusionStrength
  vec4 specularGlossiness; // Packed specularFactor.xyz, glossiness 
  vec4 sheenFactors;
  vec4 clearcoatTransmissionThickness;
  vec4 specularFactors;
  vec4 attenuation;
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
  float ior;
  uint padding[2];
};
