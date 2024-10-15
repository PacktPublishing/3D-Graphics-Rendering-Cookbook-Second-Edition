#include "UtilsGLTF.h"

bool assignUVandSampler(
    const GLTFGlobalSamplers& samplers, const aiMaterial* mtlDescriptor, aiTextureType textureType, uint32_t& uvIndex,
    uint32_t& textureSampler, int index)
{
  aiString path;
  aiTextureMapMode mapmode[3] = { aiTextureMapMode_Clamp, aiTextureMapMode_Clamp, aiTextureMapMode_Clamp };
  bool res                    = mtlDescriptor->GetTexture(textureType, index, &path, 0, &uvIndex, 0, 0, mapmode) == AI_SUCCESS;
  switch (mapmode[0]) {
  case aiTextureMapMode_Clamp:
    textureSampler = samplers.clamp.index();
    break;
  case aiTextureMapMode_Wrap:
    textureSampler = samplers.wrap.index();
    break;
  case aiTextureMapMode_Mirror:
    textureSampler = samplers.mirror.index();
    break;
  default:
    break;
  }
  return res;
}

namespace
{
void loadMaterialTexture(
    const aiMaterial* mtlDescriptor, aiTextureType textureType, const char* assetFolder, lvk::Holder<lvk::TextureHandle>& textureHandle,
    const std::unique_ptr<lvk::IContext>& ctx, bool sRGB, int index = 0)
{
  if (mtlDescriptor->GetTextureCount(textureType) > 0) {
    aiString path;
    if (mtlDescriptor->GetTexture(textureType, index, &path) == AI_SUCCESS) {
      aiString fullPath(assetFolder);
      fullPath.Append(path.C_Str());

      textureHandle = loadTexture(ctx, fullPath.C_Str(), lvk::TextureType_2D, sRGB);
      if (textureHandle.empty()) {
        assert(0);
        exit(256);
      }
    }
  }
}

uint32_t getNextMtxId(GLTFContext& gltf, const std::string& name, uint32_t& nextEmptyId, glm::mat4 mtx)
{
  uint32_t mtxId = 0;
  auto boneId    = gltf.bonesStorage.find(name);
  if (boneId == gltf.bonesStorage.end()) {
    mtxId = nextEmptyId;
    nextEmptyId++;
  } else {
    mtxId = boneId->second.boneId;
  }

  if (gltf.matrices.size() <= mtxId) {
    gltf.matrices.resize(mtxId + 1);
  }

  gltf.matrices[mtxId] = mtx;

  return mtxId;
}

} // namespace

GLTFMaterialDataGPU setupglTFMaterialData(
    const std::unique_ptr<lvk::IContext>& ctx, const GLTFGlobalSamplers& samplers, const aiMaterial* mtlDescriptor, const char* assetFolder,
    GLTFDataHolder& glTFDataholder, bool& useVolumetric)
{
  GLTFMaterialTextures mat;

  uint32_t materialTypeFlags = MaterialType_Invalid;

  const uint32_t whitePixel = 0xFFFFFFFF;

  mat.white = ctx->createTexture(
      {
          .type       = lvk::TextureType_2D,
          .format     = lvk::Format_RGBA_SRGB8,
          .dimensions = {1, 1},
          .usage      = lvk::TextureUsageBits_Sampled,
          .data       = &whitePixel,
          .debugName  = "white1x1",
  },
      "white1x1");

  aiShadingMode shadingMode = aiShadingMode_NoShading;
  if (mtlDescriptor->Get(AI_MATKEY_SHADING_MODEL, shadingMode) == AI_SUCCESS) {
    if (shadingMode == aiShadingMode_Unlit) {
      materialTypeFlags = MaterialType_Unlit;
    }
  }

  loadMaterialTexture(mtlDescriptor, aiTextureType_BASE_COLOR, assetFolder, mat.baseColorTexture, ctx, true);
  loadMaterialTexture(mtlDescriptor, aiTextureType_METALNESS, assetFolder, mat.surfacePropertiesTexture, ctx, false);

  materialTypeFlags = MaterialType_MetallicRoughness;

  // Load common textures
  loadMaterialTexture(mtlDescriptor, aiTextureType_LIGHTMAP, assetFolder, mat.occlusionTexture, ctx, false);
  loadMaterialTexture(mtlDescriptor, aiTextureType_EMISSIVE, assetFolder, mat.emissiveTexture, ctx, true);
  loadMaterialTexture(mtlDescriptor, aiTextureType_NORMALS, assetFolder, mat.normalTexture, ctx, false);

  // Sheen
  loadMaterialTexture(mtlDescriptor, aiTextureType_SHEEN, assetFolder, mat.sheenColorTexture, ctx, true, 0);
  loadMaterialTexture(mtlDescriptor, aiTextureType_SHEEN, assetFolder, mat.sheenRoughnessTexture, ctx, false, 1);

  // Clearcoat
  loadMaterialTexture(mtlDescriptor, aiTextureType_CLEARCOAT, assetFolder, mat.clearCoatTexture, ctx, true, 0);
  loadMaterialTexture(mtlDescriptor, aiTextureType_CLEARCOAT, assetFolder, mat.clearCoatRoughnessTexture, ctx, false, 1);
  loadMaterialTexture(mtlDescriptor, aiTextureType_CLEARCOAT, assetFolder, mat.clearCoatNormalTexture, ctx, false, 2);

  // Specular
  loadMaterialTexture(mtlDescriptor, aiTextureType_SPECULAR, assetFolder, mat.specularTexture, ctx, true, 0);
  loadMaterialTexture(mtlDescriptor, aiTextureType_SPECULAR, assetFolder, mat.specularColorTexture, ctx, true, 1);

  // Transmission
  loadMaterialTexture(mtlDescriptor, aiTextureType_TRANSMISSION, assetFolder, mat.transmissionTexture, ctx, true, 0);

  // Volume
  loadMaterialTexture(mtlDescriptor, aiTextureType_TRANSMISSION, assetFolder, mat.thicknessTexture, ctx, true, 1);

  // Iridescence
  // loadMaterialTexture(mtlDescriptor, aiTextureType_IRID, assetFolder, mat.specularTexture, ctx, true, 0);

  // Anisotropy

  GLTFMaterialDataGPU res = {
    .baseColorFactor                  = vec4(1.0f, 1.0f, 1.0f, 1.0f),
    .metallicRoughnessNormalOcclusion = vec4(1.0f, 1.0f, 1.0f, 1.0f),
    .specularFactors                  = vec4(1.0f, 1.0f, 1.0f, 1.0f),
    .emissiveFactorAlphaCutoff        = vec4(0.0f, 0.0f, 0.0f, 0.5f),
    .occlusionTexture                 = mat.occlusionTexture.index(),
    .emissiveTexture                  = mat.emissiveTexture.valid() ? mat.emissiveTexture.index() : mat.white.index(),
    .baseColorTexture                 = mat.baseColorTexture.valid() ? mat.baseColorTexture.index() : mat.white.index(),
    .surfacePropertiesTexture         = mat.surfacePropertiesTexture.valid() ? mat.surfacePropertiesTexture.index() : mat.white.index(),
    .normalTexture                    = mat.normalTexture.valid() ? mat.normalTexture.index() : mat.white.index(),
    .sheenColorTexture                = mat.sheenColorTexture.index(),
    .sheenRoughnessTexture            = mat.sheenRoughnessTexture.index(),
    .clearCoatTexture                 = mat.clearCoatTexture.valid() ? mat.clearCoatTexture.index() : mat.white.index(),
    .clearCoatRoughnessTexture        = mat.clearCoatRoughnessTexture.valid() ? mat.clearCoatRoughnessTexture.index() : mat.white.index(),
    .clearCoatNormalTexture           = mat.clearCoatNormalTexture.valid() ? mat.clearCoatNormalTexture.index() : mat.white.index(),
    .specularTexture                  = mat.specularTexture.valid() ? mat.specularTexture.index() : mat.white.index(),
    .specularColorTexture             = mat.specularColorTexture.valid() ? mat.specularColorTexture.index() : mat.white.index(),
    .transmissionTexture              = mat.transmissionTexture.valid() ? mat.transmissionTexture.index() : mat.white.index(),
    .thicknessTexture                 = mat.thicknessTexture.valid() ? mat.thicknessTexture.index() : mat.white.index(),
    .iridescenceTexture               = mat.iridescenceTexture.index(),
    .iridescenceThicknessTexture      = mat.iridescenceThicknessTexture.index(),
    .anisotropyTexture                = mat.anisotropyTexture.index(),
    .materialTypeFlags                = materialTypeFlags,
  };

  aiColor4D aiColor;
  if (mtlDescriptor->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == AI_SUCCESS) {
    res.baseColorFactor = vec4(aiColor.r, aiColor.g, aiColor.b, aiColor.a);
  }
  assignUVandSampler(samplers, mtlDescriptor, aiTextureType_DIFFUSE, res.baseColorTextureUV, res.baseColorTextureSampler);

  if (mtlDescriptor->Get(AI_MATKEY_COLOR_EMISSIVE, aiColor) == AI_SUCCESS) {
    // mat.emissiveFactor = vec3(aiColor.r, aiColor.g, aiColor.b);
    res.emissiveFactorAlphaCutoff = vec4(aiColor.r, aiColor.g, aiColor.b, 0.5f);
  }
  assignUVandSampler(samplers, mtlDescriptor, aiTextureType_EMISSIVE, res.emissiveTextureUV, res.emissiveTextureSampler);

  ai_real emissiveStrength = 1.0f;
  if (mtlDescriptor->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissiveStrength) == AI_SUCCESS) {
    res.emissiveFactorAlphaCutoff *= vec4(emissiveStrength, emissiveStrength, emissiveStrength, 1.0f);
  }

  if (materialTypeFlags & MaterialType_MetallicRoughness) {
    ai_real metallicFactor;
    if (mtlDescriptor->Get(AI_MATKEY_METALLIC_FACTOR, metallicFactor) == AI_SUCCESS) {
      res.metallicRoughnessNormalOcclusion.x = metallicFactor;
    }
    assignUVandSampler(
        samplers, mtlDescriptor, aiTextureType_METALNESS, res.surfacePropertiesTextureUV, res.surfacePropertiesTextureSampler);

    ai_real roughnessFactor;
    if (mtlDescriptor->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughnessFactor) == AI_SUCCESS) {
      res.metallicRoughnessNormalOcclusion.y = roughnessFactor;
    }
  } else if (materialTypeFlags & MaterialType_SpecularGlossiness) {
    ai_real specularFactor[3];
    if (mtlDescriptor->Get(AI_MATKEY_SPECULAR_FACTOR, specularFactor) == AI_SUCCESS) {
      res.specularGlossiness.x = specularFactor[0];
      res.specularGlossiness.y = specularFactor[1];
      res.specularGlossiness.z = specularFactor[2];
    }
    assignUVandSampler(
        samplers, mtlDescriptor, aiTextureType_SPECULAR, res.surfacePropertiesTextureUV, res.surfacePropertiesTextureSampler);

    ai_real glossinessFactor;
    if (mtlDescriptor->Get(AI_MATKEY_GLOSSINESS_FACTOR, glossinessFactor) == AI_SUCCESS) {
      res.specularGlossiness.w = glossinessFactor;
    }
  }

  ai_real normalScale;
  if (mtlDescriptor->Get(AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_NORMALS, 0), normalScale) == AI_SUCCESS) {
    res.metallicRoughnessNormalOcclusion.z = normalScale;
  }
  assignUVandSampler(samplers, mtlDescriptor, aiTextureType_NORMALS, res.normalTextureUV, res.normalTextureSampler);

  ai_real occlusionStrength;
  if (mtlDescriptor->Get(AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_LIGHTMAP, 0), occlusionStrength) == AI_SUCCESS) {
    res.metallicRoughnessNormalOcclusion.w = occlusionStrength;
  }
  assignUVandSampler(samplers, mtlDescriptor, aiTextureType_LIGHTMAP, res.occlusionTextureUV, res.occlusionTextureSampler);

  aiString alphaMode = aiString("OPAQUE");
  if (mtlDescriptor->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS) {
    if (alphaMode == aiString("MASK")) {
      res.alphaMode = GLTFMaterialDataGPU::AlphaMode_Mask;
    } else if (alphaMode == aiString("BLEND")) {
      res.alphaMode = GLTFMaterialDataGPU::AlphaMode_Blend;
    } else {
      res.alphaMode = GLTFMaterialDataGPU::AlphaMode_Opaque;
    }
  }

  ai_real alphaCutoff;
  if (mtlDescriptor->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff) == AI_SUCCESS) {
    res.emissiveFactorAlphaCutoff.w = alphaCutoff;
  }

  // Extensions
  // Sheen
  {
    bool useSheen = !mat.sheenColorTexture.empty() || !mat.sheenRoughnessTexture.empty();
    aiColor4D sheenColorFactor;
    if (mtlDescriptor->Get(AI_MATKEY_SHEEN_COLOR_FACTOR, sheenColorFactor) == AI_SUCCESS) {
      res.sheenFactors = vec4(sheenColorFactor.r, sheenColorFactor.g, sheenColorFactor.b, sheenColorFactor.a);
      useSheen         = true;
    }
    ai_real sheenRoughnessFactor;
    if (mtlDescriptor->Get(AI_MATKEY_SHEEN_ROUGHNESS_FACTOR, sheenRoughnessFactor) == AI_SUCCESS) {
      res.sheenFactors.w = sheenRoughnessFactor;
      useSheen           = true;
    }

    if (assignUVandSampler(samplers, mtlDescriptor, aiTextureType_SHEEN, res.sheenColorTextureUV, res.sheenColorTextureSampler, 0)) {
      useSheen = true;
    }
    if (assignUVandSampler(
            samplers, mtlDescriptor, aiTextureType_SHEEN, res.sheenRoughnessTextureUV, res.sheenRoughnessTextureSampler, 1)) {
      useSheen = true;
    }

    if (useSheen) {
      res.materialTypeFlags |= MaterialType_Sheen;
    }
  }

  // Clear coat
  {
    bool useClearCoat = !mat.clearCoatTexture.empty() || !mat.clearCoatRoughnessTexture.empty() || !mat.clearCoatNormalTexture.empty();
    ai_real clearcoatFactor;
    if (mtlDescriptor->Get(AI_MATKEY_CLEARCOAT_FACTOR, clearcoatFactor) == AI_SUCCESS) {
      res.clearcoatTransmissionThickness.x = clearcoatFactor;
      useClearCoat                         = true;
    }

    ai_real clearcoatRoughnessFactor;
    if (mtlDescriptor->Get(AI_MATKEY_CLEARCOAT_ROUGHNESS_FACTOR, clearcoatRoughnessFactor) == AI_SUCCESS) {
      res.clearcoatTransmissionThickness.y = clearcoatRoughnessFactor;
      useClearCoat                         = true;
    }

    if (assignUVandSampler(
            samplers, mtlDescriptor, aiTextureType_CLEARCOAT, res.clearCoatTextureUV, res.clearCoatNormalTextureSampler, 0)) {
      useClearCoat = true;
    }

    if (assignUVandSampler(
            samplers, mtlDescriptor, aiTextureType_CLEARCOAT, res.clearCoatRoughnessTextureUV, res.clearCoatRoughnessTextureSampler, 1)) {
      useClearCoat = true;
    }

    if (assignUVandSampler(
            samplers, mtlDescriptor, aiTextureType_CLEARCOAT, res.clearCoatNormalTextureUV, res.clearCoatNormalTextureSampler, 2)) {
      useClearCoat = true;
    }

    if (useClearCoat) {
      res.materialTypeFlags |= MaterialType_ClearCoat;
    }
  }

  // Specular
  {
    bool useSpecular = !mat.specularColorTexture.empty() || !mat.specularTexture.empty();

    ai_real specularFactor;
    if (mtlDescriptor->Get(AI_MATKEY_SPECULAR_FACTOR, specularFactor) == AI_SUCCESS) {
      res.specularFactors.w = specularFactor;
      useSpecular           = true;
    }

    assignUVandSampler(samplers, mtlDescriptor, aiTextureType_SPECULAR, res.specularTextureUV, res.specularTextureSampler, 0);

    aiColor4D specularColorFactor;
    if (mtlDescriptor->Get(AI_MATKEY_COLOR_SPECULAR, specularColorFactor) == AI_SUCCESS) {
      res.specularFactors = vec4(specularColorFactor.r, specularColorFactor.g, specularColorFactor.b, res.specularFactors.w);
      useSpecular         = true;
    }

    assignUVandSampler(samplers, mtlDescriptor, aiTextureType_SPECULAR, res.specularColorTextureUV, res.specularColorTextureSampler, 1);

    if (useSpecular) {
      res.materialTypeFlags |= MaterialType_Specular;
    }
  }

  // Transmission
  {
    bool useTransmission = !mat.transmissionTexture.empty();

    ai_real transmissionFactor = 0.0f;
    if (mtlDescriptor->Get(AI_MATKEY_TRANSMISSION_FACTOR, transmissionFactor) == AI_SUCCESS) {
      res.clearcoatTransmissionThickness.z = transmissionFactor;
      useTransmission                      = true;
    }

    if (useTransmission) {
      res.materialTypeFlags |= MaterialType_Transmission;
      useVolumetric = true;
    }

    assignUVandSampler(samplers, mtlDescriptor, aiTextureType_TRANSMISSION, res.transmissionTextureUV, res.transmissionTextureSampler, 0);
  }

  {
    bool useVolume = !mat.thicknessTexture.empty();

    ai_real thicknessFactor = 0.0f;
    if (mtlDescriptor->Get(AI_MATKEY_VOLUME_THICKNESS_FACTOR, thicknessFactor) == AI_SUCCESS) {
      res.clearcoatTransmissionThickness.w = thicknessFactor;
      useVolume                            = true;
    }

    ai_real attenuationDistance = 0.0f;
    if (mtlDescriptor->Get(AI_MATKEY_VOLUME_ATTENUATION_DISTANCE, attenuationDistance) == AI_SUCCESS) {
      res.attenuation.w = attenuationDistance;
      useVolume         = true;
    }

    aiColor4D volumeAttenuationColor;
    if (mtlDescriptor->Get(AI_MATKEY_VOLUME_ATTENUATION_COLOR, volumeAttenuationColor) == AI_SUCCESS) {
      res.attenuation.x = volumeAttenuationColor.r;
      res.attenuation.y = volumeAttenuationColor.g;
      res.attenuation.z = volumeAttenuationColor.b;
      useVolume         = true;
    }

    if (useVolume) {
      res.materialTypeFlags |= MaterialType_Transmission | MaterialType_Volume;
      useVolumetric = true;
    }

    assignUVandSampler(samplers, mtlDescriptor, aiTextureType_TRANSMISSION, res.thicknessTextureUV, res.thicknessTextureSampler, 1);
  }

  // IOR
  ai_real ior;
  if (mtlDescriptor->Get(AI_MATKEY_REFRACTI, ior) == AI_SUCCESS) {
    res.ior = ior;
  }

  mat.wasLoaded = true;

  glTFDataholder.textures.push_back(std::move(mat));

  return res;
}

static uint32_t getNumVertices(const aiScene& scene)
{
  uint32_t num = 0;
  for (uint32_t i = 0; i != scene.mNumMeshes; i++) {
    num += scene.mMeshes[i]->mNumVertices;
  }
  return num;
}

static uint32_t getNumIndices(const aiScene& scene)
{
  uint32_t num = 0;
  for (uint32_t i = 0; i != scene.mNumMeshes; i++) {
    for (uint32_t j = 0; j != scene.mMeshes[i]->mNumFaces; j++) {
      LVK_ASSERT(scene.mMeshes[i]->mFaces[j].mNumIndices == 3);
      num += scene.mMeshes[i]->mFaces[j].mNumIndices;
    }
  }
  return num;
}

static uint32_t getNumMorphVertices(const aiScene& scene)
{
  uint32_t num = 0;
  for (uint32_t i = 0; i != scene.mNumMeshes; i++) {
    num += scene.mMeshes[i]->mNumVertices * scene.mMeshes[i]->mNumAnimMeshes;
  }
  return num;
}

static uint32_t getNodeId(GLTFContext& gltf, const std::string& name)
{
  for (uint32_t i = 0; i != gltf.nodesStorage.size(); i++) {
    if (gltf.nodesStorage[i].name == name)
      return i;
  }
  return ~0;
}

void updateLights(GLTFContext& gltf, lvk::IContext* ctx)
{
  for (LightDataGPU& light : gltf.lights) {
    if (light.nodeId == -1)
      continue;
    light.position  = glm::vec3(gltf.matrices[light.nodeId][3]);
    light.direction = gltf.matrices[light.nodeId] * glm::vec4(light.direction, 0.0);
  }

  LVK_ASSERT(gltf.lights.size() <= kMaxLights);

  ctx->upload(gltf.lightsBuffer, gltf.lights.data(), sizeof(LightDataGPU) * gltf.lights.size());
}

void loadGLTF(GLTFContext& gltf, const char* glTFName, const char* glTFDataPath)
{
  const aiScene* scene = aiImportFile(glTFName, aiProcess_Triangulate);
  if (!scene || !scene->HasMeshes()) {
    printf("Unable to load %s\n", glTFName);
    exit(255);
  }
  SCOPE_EXIT
  {
    aiReleaseImport(scene);
  };

  std::vector<Vertex> vertices;
  std::vector<VertexBoneData> skinningData;
  std::vector<uint32_t> indices;
  std::vector<uint32_t> startVertex;
  std::vector<uint32_t> startIndex;

  startVertex.push_back(0);
  startIndex.push_back(0);

  vertices.reserve(getNumVertices(*scene));
  indices.reserve(getNumIndices(*scene));
  skinningData.resize(getNumVertices(*scene));

  std::vector<Vertex> morphData;
  gltf.morphTargets.resize(scene->mNumMeshes);
  morphData.reserve(getNumMorphVertices(*scene));

  uint32_t numBones           = 0;
  uint32_t morphTargetsOffset = 0;
  uint32_t vertOffset         = 0;

  for (uint32_t m = 0; m < scene->mNumMeshes; ++m) {
    const aiMesh* mesh                    = scene->mMeshes[m];
    gltf.meshesRemap[mesh->mName.C_Str()] = m;

    for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
      const aiVector3D v   = mesh->mVertices[i];
      const aiVector3D n   = mesh->mNormals ? mesh->mNormals[i] : aiVector3D(0.0f, 1.0f, 0.0f);
      const aiColor4D c    = mesh->mColors[0] ? mesh->mColors[0][i] : aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
      const aiVector3D uv0 = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i] : aiVector3D(0.0f, 0.0f, 0.0f);
      const aiVector3D uv1 = mesh->mTextureCoords[1] ? mesh->mTextureCoords[1][i] : aiVector3D(0.0f, 0.0f, 0.0f);
      vertices.push_back({
          .position = vec3(v.x, v.y, v.z),
          .normal   = vec3(n.x, n.y, n.z),
          .color    = vec4(c.r, c.g, c.b, c.a),
          .uv0      = vec2(uv0.x, 1.0f - uv0.y),
          .uv1      = vec2(uv1.x, 1.0f - uv1.y),
      });

      if (mesh->mNumBones == 0) {
        auto& vertex    = skinningData[vertices.size() - 1];
        vertex.meshId   = m;
        vertex.position = vec3(v.x, v.y, v.z);
		  vertex.normal = vec3(n.x, n.y, n.z);
      }
    }

    startVertex.push_back((uint32_t)vertices.size());
    for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
      for (int j = 0; j != 3; j++) {
        indices.push_back(mesh->mFaces[i].mIndices[j]);
      }
    }
    startIndex.push_back((uint32_t)indices.size());

    // load bones
    for (int bone = 0; bone < mesh->mNumBones; ++bone) {
      uint32_t boneId(numBones);
      std::string boneName = mesh->mBones[bone]->mName.C_Str();
      if (!gltf.bonesStorage.contains(boneName)) {
        gltf.bonesStorage[boneName] = { boneId, aiMatrix4x4ToMat4(mesh->mBones[bone]->mOffsetMatrix) };
        numBones++;
      } else {
        boneId = gltf.bonesStorage[boneName].boneId;
      }

      auto weights   = mesh->mBones[bone]->mWeights;
      int numWeights = mesh->mBones[bone]->mNumWeights;

      for (int w = 0; w < numWeights; ++w) {
        int vertexId = weights[w].mVertexId;
        float weight = weights[w].mWeight;
        assert(vertexId <= vertices.size());
        auto& vertex    = skinningData[vertexId + vertOffset];
        vertex.position = vertices[vertexId + vertOffset].position;
		  vertex.normal = vertices[vertexId + vertOffset].normal;

        assert(vertex.meshId == ~0 || vertex.meshId == m);
        vertex.meshId = m;
        for (int i = 0; i < MAX_BONES_PER_VERTEX; ++i) {
          if (vertex.boneId[i] == ~0) {
            vertex.weight[i] = weight;
            vertex.boneId[i] = boneId;
            break;
          }
        }
      }
    }

    vertOffset += mesh->mNumVertices;
  }
  // load morph targets
  for (uint32_t c = 0; c < scene->mNumMeshes; ++c) {
    const aiMesh* m          = scene->mMeshes[c];
    MorphTarget& morphTarget = gltf.morphTargets[c];

    if (m->mNumAnimMeshes) {
      morphTarget.meshId = c;

      for (uint32_t a = 0; a < m->mNumAnimMeshes; a++) {
        const aiAnimMesh* mesh = m->mAnimMeshes[a];

        for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
          const aiVector3D v   = mesh->mVertices[i];
          const aiVector3D n   = mesh->mNormals ? mesh->mNormals[i] : aiVector3D(0.0f, 1.0f, 0.0f);
			 const aiVector3D srcNorm = m->mNormals ? m->mNormals[i] : aiVector3D(0.0f, 1.0f, 0.0f);
          const aiColor4D c    = mesh->mColors[0] ? mesh->mColors[0][i] : aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
          const aiVector3D uv0 = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i] : aiVector3D(0.0f, 0.0f, 0.0f);
          const aiVector3D uv1 = mesh->mTextureCoords[1] ? mesh->mTextureCoords[1][i] : aiVector3D(0.0f, 0.0f, 0.0f);
          morphData.push_back({
              .position = vec3(v.x - m->mVertices[i].x, v.y - m->mVertices[i].y, v.z - m->mVertices[i].z),
              .normal   = vec3(n.x - srcNorm.x, n.y - srcNorm.y, n.z - srcNorm.z),
              .color    = vec4(c.r, c.g, c.b, c.a),
              .uv0      = vec2(uv0.x, 1.0f - uv0.y),
              .uv1      = vec2(uv1.x, 1.0f - uv1.y),
          });
        }
        morphTarget.offset.push_back(morphTargetsOffset);
        morphTargetsOffset += mesh->mNumVertices;
      }
    }
  }

  if (!scene->mRootNode) {
    printf("Scene has no root node\n");
    exit(255);
  }

  auto& ctx = gltf.app.ctx_;

  for (unsigned int mtl = 0; mtl < scene->mNumMaterials; ++mtl) {
    const aiMaterial* mtlDescriptor = scene->mMaterials[mtl];
    gltf.matPerFrame.materials[mtl] =
        setupglTFMaterialData(ctx, gltf.samplers, mtlDescriptor, glTFDataPath, gltf.glTFDataholder, gltf.isVolumetricMaterial);
  }

  uint32_t nonBoneMtxId = numBones;

  std::string rootName = scene->mRootNode->mName.C_Str() ? scene->mRootNode->mName.C_Str() : "root";
  gltf.nodesStorage.push_back({
      .name       = rootName,
      .modelMtxId = getNextMtxId(gltf, rootName, nonBoneMtxId, aiMatrix4x4ToMat4(scene->mRootNode->mTransformation)),
      .transform  = aiMatrix4x4ToMat4(scene->mRootNode->mTransformation),
  });

  gltf.root = gltf.nodesStorage.size() - 1;

  std::function<void(const aiNode* rootNode, GLTFNodeRef gltfNode)> traverseTree = [&](const aiNode* rootNode, GLTFNodeRef gltfNode) {
    for (unsigned int m = 0; m < rootNode->mNumMeshes; ++m) {
      const uint32_t meshIdx = rootNode->mMeshes[m];
      const aiMesh* mesh     = scene->mMeshes[meshIdx];

      gltf.meshesStorage.push_back({
          .primitive    = lvk::Topology_Triangle,
          .vertexOffset = startVertex[meshIdx],
          .vertexCount  = mesh->mNumVertices,
          .indexOffset  = startIndex[meshIdx],
          .indexCount   = mesh->mNumFaces * 3,
          .matIdx       = mesh->mMaterialIndex,
          .sortingType =
              gltf.matPerFrame.materials[mesh->mMaterialIndex].alphaMode == GLTFMaterialDataGPU::AlphaMode_Blend ? SortingType_Transparent
              : gltf.matPerFrame.materials[mesh->mMaterialIndex].materialTypeFlags & MaterialType_Transmission   ? SortingType_Transmission
                                                                                                                 : SortingType_Opaque,
      });
      gltf.nodesStorage[gltfNode].meshes.push_back(gltf.meshesStorage.size() - 1);
    }
    for (GLTFNodeRef i = 0; i < rootNode->mNumChildren; i++) {
      const aiNode* node    = rootNode->mChildren[i];
      std::string childName = node->mName.C_Str() ? node->mName.C_Str() : "node";
      const GLTFNode childNode({
          .name       = childName,
          .modelMtxId = getNextMtxId(
              gltf, childName, nonBoneMtxId,
              gltf.matrices[gltf.nodesStorage[gltfNode].modelMtxId] * aiMatrix4x4ToMat4(node->mTransformation)),
          .transform = aiMatrix4x4ToMat4(node->mTransformation),
      });
      gltf.nodesStorage.push_back(childNode);
      const size_t nodeIdx = gltf.nodesStorage.size() - 1;
      gltf.nodesStorage[gltfNode].children.push_back(nodeIdx);
      traverseTree(node, nodeIdx);
    }
  };

  traverseTree(scene->mRootNode, gltf.root);

  initAnimations(gltf, scene);

  // add dummy vertices to align buffer to 16 to run compute shader
  gltf.maxVertices = (1 + (vertices.size() / 16)) * 16;
  vertices.resize(gltf.maxVertices);

  gltf.vertexBuffer = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Vertex | lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_Device,
      .size      = sizeof(Vertex) * vertices.size(),
      .data      = vertices.data(),
      .debugName = "Buffer: vertex",
  });

  size_t vssize = (1 + (skinningData.size() / 16)) * 16;
  skinningData.resize(vssize);

  assert(vssize == gltf.maxVertices);

  gltf.vertexSkinningBuffer = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Vertex | lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_Device,
      .size      = sizeof(VertexBoneData) * skinningData.size(),
      .data      = skinningData.data(),
      .debugName = "Buffer: skinning vertex data",
  });

  const bool hasMorphData = !morphData.empty();

  gltf.vertexMorphingBuffer = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Vertex | lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_Device,
      .size      = hasMorphData ? sizeof(Vertex) * morphData.size() : sizeof(Vertex), // always have dummy buffer
      .data      = hasMorphData ? morphData.data() : nullptr,
      .debugName = "Buffer: morphing vertex data",
  });

  gltf.morphsBuffer = gltf.app.ctx_->createBuffer({
      .usage     = lvk::BufferUsageBits_Vertex | lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_HostVisible,
      .size      = MAX_MORPHS * sizeof(MorphState),
      .debugName = "Morphs matrices",
  });

  gltf.indexBuffer = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Index,
      .storage   = lvk::StorageType_Device,
      .size      = sizeof(uint32_t) * indices.size(),
      .data      = indices.data(),
      .debugName = "Buffer: index",
  });

  const lvk::VertexInput vdesc = {
    .attributes    = { { .location = 0, .format = lvk::VertexFormat::Float3, .offset = 0  },
							  { .location = 1, .format = lvk::VertexFormat::Float3, .offset = 12 },
							  { .location = 2, .format = lvk::VertexFormat::Float4, .offset = 24 },
							  { .location = 3, .format = lvk::VertexFormat::Float2, .offset = 40 },
							  { .location = 4, .format = lvk::VertexFormat::Float2, .offset = 48 }, },
    .inputBindings = { { .stride = sizeof(Vertex) } },
  };

  gltf.vert      = loadShaderModule(ctx, "data/shaders/gltf/main.vert");
  gltf.frag      = loadShaderModule(ctx, "data/shaders/gltf/main.frag");
  gltf.animation = loadShaderModule(ctx, "data/shaders/gltf/animation.comp");

  gltf.pipelineSolid = ctx->createRenderPipeline({
      .vertexInput = vdesc,
      .smVert      = gltf.vert,
      .smFrag      = gltf.frag,
      .color       = { { .format = ctx->getSwapchainFormat() } },
      .depthFormat = gltf.app.getDepthFormat(),
      .cullMode    = lvk::CullMode_Back,
  });

  gltf.pipelineTransparent = ctx->createRenderPipeline({
      .vertexInput = vdesc,
      .smVert      = gltf.vert,
      .smFrag      = gltf.frag,
      .color       = { {
                .format              = ctx->getSwapchainFormat(),
                .blendEnabled        = true,
                .rgbBlendOp          = lvk::BlendOp_Subtract,
                .alphaBlendOp        = lvk::BlendOp_Subtract,
                .srcRGBBlendFactor   = lvk::BlendFactor_SrcColor,
                .srcAlphaBlendFactor = lvk::BlendFactor_SrcAlpha,
                .dstRGBBlendFactor   = lvk::BlendFactor_OneMinusDstColor,
                .dstAlphaBlendFactor = lvk::BlendFactor_OneMinusDstAlpha,
      } },
      .depthFormat = gltf.app.getDepthFormat(),
      .cullMode    = lvk::CullMode_Back,
  });

  gltf.matBuffer = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Uniform,
      .storage   = lvk::StorageType_HostVisible,
      .size      = sizeof(gltf.matPerFrame),
      .data      = &gltf.matPerFrame,
      .debugName = "PerFrame materials",
  });

  const EnvironmentsPerFrame envPerFrame = {
    .environments = { {
        .envMapTexture                  = gltf.envMapTextures.envMapTexture.index(),
        .envMapTextureSampler           = gltf.samplers.clamp.index(),
        .envMapTextureIrradiance        = gltf.envMapTextures.envMapTextureIrradiance.index(),
        .envMapTextureIrradianceSampler = gltf.samplers.clamp.index(),
        .lutBRDFTexture                 = gltf.envMapTextures.texBRDF_LUT.index(),
        .lutBRDFTextureSampler          = gltf.samplers.clamp.index(),
        .envMapTextureCharlie           = gltf.envMapTextures.envMapTextureCharlie.index(),
        .envMapTextureCharlieSampler    = gltf.samplers.clamp.index(),
    } },
  };

  gltf.envBuffer = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_HostVisible,
      .size      = sizeof(envPerFrame),
      .data      = &envPerFrame,
      .debugName = "PerFrame environments",
  });

  gltf.lightsBuffer = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_HostVisible,
      .size      = sizeof(LightDataGPU) * kMaxLights,
      .debugName = "Lights",
  });

  // Load lights
  // Atten = 1/( att0 + att1 * d + att2 * d*d)
  {
    for (size_t i = 0; i < scene->mNumLights; ++i) {
      LightDataGPU ld;
      const aiLight* light = scene->mLights[i];

      ld.color     = glm::vec3(light->mColorDiffuse[0] / 255.0f, light->mColorDiffuse[1] / 255.0f, light->mColorDiffuse[2] / 255.0f);
      ld.nodeId    = getNodeId(gltf, light->mName.C_Str());
      ld.direction = glm::vec3(light->mDirection[0], light->mDirection[1], light->mDirection[2]);
      ld.range     = 10000.0f;

      if (light->mType == aiLightSource_POINT) {
        ld.type = LightType_Point;
      } else if (light->mType == aiLightSource_SPOT) {
        ld.type         = LightType_Spot;
        ld.innerConeCos = light->mAngleInnerCone;
        ld.outerConeCos = light->mAngleOuterCone;
      } else if (light->mType == aiLightSource_DIRECTIONAL) {
        ld.type = LightType_Directional;
      }

      gltf.lights.push_back(ld);
    }

    if (gltf.lights.empty()) {
      gltf.lights.push_back(LightDataGPU());
    }
  }

  updateLights(gltf, ctx.get());

  for (uint32_t i = 0; i != LVK_ARRAY_NUM_ELEMENTS(gltf.perFrameBuffer); i++) {
    gltf.perFrameBuffer[i] = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Uniform,
        .storage   = lvk::StorageType_HostVisible,
        .size      = sizeof(GLTFFrameData),
        .data      = &gltf.frameData,
        .debugName = ("Per Frame data " + std::to_string(i)).c_str(),
    });
  }

  LVK_ASSERT(gltf.pipelineSolid.valid());

  // Cameras

  gltf.cameras.reserve(scene->mNumCameras);
  for (uint32_t c = 0; c < scene->mNumCameras; ++c) {
    aiCamera* camera = scene->mCameras[c];
    gltf.cameras.push_back({ .name       = camera->mName.C_Str(),
                             .nodeIdx    = getNodeId(gltf, camera->mName.C_Str()),
                             .pos        = aiVector3DToVec3(camera->mPosition),
                             .up         = aiVector3DToVec3(camera->mUp),
                             .lookAt     = aiVector3DToVec3(camera->mUp),
                             .hFOV       = camera->mHorizontalFOV,
                             .near       = camera->mClipPlaneNear,
                             .far        = camera->mClipPlaneFar,
                             .aspect     = camera->mAspect,
                             .orthoWidth = camera->mOrthographicWidth });
  }
}

void updateCamera(GLTFContext& gltf, const mat4& model, mat4& view, mat4& proj, float aspectRatio)
{
  if (gltf.activeCamera == ~0 || gltf.activeCamera >= gltf.cameras.size())
    return;

  const GLTFCamera& cam = gltf.cameras[gltf.activeCamera];
  if (cam.nodeIdx == ~0)
    return;

  view = glm::inverse(model * gltf.matrices[cam.nodeIdx]);
  proj = cam.getProjection(aspectRatio);
}

void buildTransformsList(GLTFContext& gltf)
{
  gltf.transforms.clear();
  // gltf.matrices.clear();
  gltf.opaqueNodes.clear();
  gltf.transmissionNodes.clear();
  gltf.transparentNodes.clear();

  std::function<void(GLTFNodeRef gltfNode)> traverseTree = [&](GLTFNodeRef nodeRef) {
    GLTFNode& node = gltf.nodesStorage[nodeRef];
    for (GLTFNodeRef meshId : node.meshes) {
      const GLTFMesh& mesh = gltf.meshesStorage[meshId];
      gltf.transforms.push_back({
          .modelMtxId  = node.modelMtxId,
          .matId       = mesh.matIdx,
          .nodeRef     = nodeRef,
          .meshRef     = meshId,
          .sortingType = mesh.sortingType,
      });
      if (mesh.sortingType == SortingType_Transparent) {
        gltf.transparentNodes.push_back(gltf.transforms.size() - 1);
      } else if (mesh.sortingType == SortingType_Transmission) {
        gltf.transmissionNodes.push_back(gltf.transforms.size() - 1);
      } else {
        gltf.opaqueNodes.push_back(gltf.transforms.size() - 1);
      }
    }
    for (GLTFNodeRef child : node.children) {
      traverseTree(child);
    }
  };

  traverseTree(gltf.root);

  gltf.transformBuffer = gltf.app.ctx_->createBuffer({
      .usage     = lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_HostVisible,
      .size      = gltf.transforms.size() * sizeof(GLTFTransforms),
      .data      = &gltf.transforms[0],
      .debugName = "Per Frame data",
  });

  gltf.matricesBuffer = gltf.app.ctx_->createBuffer({
      .usage     = lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_HostVisible,
      .size      = gltf.matrices.size() * sizeof(glm::mat4),
      .data      = &gltf.matrices[0],
      .debugName = "Node matrices",
  });
}

void sortTransparentNodes(GLTFContext& gltf, const vec3& cameraPos)
{
  // glTF spec expects to sort based on pivot positions (not sure correct way though)
  std::sort(gltf.transparentNodes.begin(), gltf.transparentNodes.end(), [&](uint32_t a, uint32_t b) {
    float sqrDistA = glm::length2(cameraPos - vec3(gltf.matrices[gltf.transforms[a].modelMtxId][3]));
    float sqrDistB = glm::length2(cameraPos - vec3(gltf.matrices[gltf.transforms[b].modelMtxId][3]));
    return sqrDistA < sqrDistB;
  });
}

void renderGLTF(GLTFContext& gltf, const mat4& model, const mat4& view, const mat4& proj, bool rebuildRenderList)
{
  auto& ctx = gltf.app.ctx_;

  const vec4 camPos = glm::inverse(view)[3];

  static auto cameras = camerasGLTF(gltf);
  static auto animations = animationsGLTF(gltf);

  if (rebuildRenderList || gltf.transforms.empty()) {
    buildTransformsList(gltf);
  }

  sortTransparentNodes(gltf, camPos);

  gltf.frameData = {
    .model     = model,
    .view      = view,
    .proj      = proj,
    .cameraPos = camPos,
  };

  struct PushConstants {
    uint64_t draw;
    uint64_t materials;
    uint64_t environments;
    uint64_t lights;
    uint64_t transforms;
    uint64_t matrices;
    uint32_t envId;
    uint32_t transmissionFramebuffer;
    uint32_t transmissionFramebufferSampler;
    uint32_t lightsCount;
  } pushConstants = {
    .draw                           = ctx->gpuAddress(gltf.perFrameBuffer[gltf.currentOffscreenTex]),
    .materials                      = ctx->gpuAddress(gltf.matBuffer),
    .environments                   = ctx->gpuAddress(gltf.envBuffer),
    .lights                         = ctx->gpuAddress(gltf.lightsBuffer),
    .transforms                     = ctx->gpuAddress(gltf.transformBuffer),
    .matrices                       = ctx->gpuAddress(gltf.matricesBuffer),
    .envId                          = 0,
    .transmissionFramebuffer        = 0,
    .transmissionFramebufferSampler = gltf.samplers.clamp.index(),
    .lightsCount                    = (uint32_t)gltf.lights.size(),
  };

  ctx->upload(gltf.perFrameBuffer[gltf.currentOffscreenTex], &gltf.frameData, sizeof(GLTFFrameData));

  const bool isSizeChanged = ctx->getDimensions(ctx->getCurrentSwapchainTexture()) != ctx->getDimensions(gltf.offscreenTex[0]);

  if (gltf.offscreenTex[0].empty() || isSizeChanged) {
    const lvk::Dimensions res = ctx->getDimensions(ctx->getCurrentSwapchainTexture());

    for (lvk::Holder<lvk::TextureHandle>& holder : gltf.offscreenTex) {
      holder = ctx->createTexture({
          .type         = lvk::TextureType_2D,
          .format       = ctx->getSwapchainFormat(),
          .dimensions   = {res.width, res.height},
          .usage        = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled,
          .numMipLevels = lvk::calcNumMipLevels(res.width, res.height),
          .debugName    = "offscreenTex",
      });
    }
  }

  auto drawUI = [&](lvk::ICommandBuffer& buf, const lvk::Framebuffer& framebuffer) {
    if (gltf.activeCamera == ~0)
      gltf.app.drawGrid(buf, proj, vec3(0, -1.0f, 0));
    else
      gltf.app.drawGrid(buf, proj * view, vec3(0, -1.0f, 0), camPos);

    gltf.app.imgui_->beginFrame(framebuffer);
    gltf.app.drawFPS();
    gltf.app.drawMemo();
    gltf.app.drawCameras(cameras, gltf.activeCamera);
	 gltf.app.drawAnimations(animations, &gltf.activeAnimations, gltf.activeAnimations.size() > 1 ? &gltf.animationBlend  : nullptr);

    if (gltf.activeCamera >= gltf.cameras.size() && !gltf.cameras.empty()) {
      gltf.canvas3d.clear();
      gltf.canvas3d.setMatrix(proj * view);
      const float windowAspect = ctx->getAspectRatio(framebuffer.color[0].texture);
      for (const auto& c : gltf.cameras) {
        if (c.nodeIdx == ~0u)
          continue;
        const mat4 camView = glm::inverse(model * gltf.matrices[c.nodeIdx]);
        const mat4 camProj = c.getProjection(windowAspect);
        gltf.canvas3d.frustum(camView, camProj, vec4(1, 0, 0, 1));
        gltf.canvas3d.render(*ctx.get(), framebuffer, buf);
      }
    }
    gltf.app.imgui_->endFrame(buf);
  };

  lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();

  if (gltf.animated) {
    ctx->upload(gltf.matricesBuffer, &gltf.matrices[0], gltf.matrices.size() * sizeof(glm::mat4));
    updateLights(gltf, ctx.get());

    if (gltf.morphing)
      ctx->upload(gltf.morphsBuffer, &gltf.morphStates[0], gltf.morphStates.size() * sizeof(MorphState));

    if (gltf.skinning || gltf.morphing) {
      // Run compute shader to do skinning and morphing

      struct ComputeSetup {
        uint64_t matrices;
        uint64_t morphs;
        uint64_t morphVertexBuffer;
        uint64_t inBuffer;
        uint64_t outBuffer;
        uint32_t numMorphs;
      } pc = {
        .matrices          = ctx->gpuAddress(gltf.matricesBuffer),
        .morphs            = ctx->gpuAddress(gltf.morphsBuffer),
        .morphVertexBuffer = ctx->gpuAddress(gltf.vertexMorphingBuffer),
        .inBuffer          = ctx->gpuAddress(gltf.vertexSkinningBuffer),
        .outBuffer         = ctx->gpuAddress(gltf.vertexBuffer),
        .numMorphs         = static_cast<uint32_t>(gltf.morphStates.size()),
      };
      buf.cmdBindComputePipeline(gltf.pipelineComputeAnimations);
      buf.cmdPushConstants(pc);
      buf.cmdDispatchThreadGroups(
          {
              .width = gltf.maxVertices / 16,
      },
          { .buffers = { { lvk::BufferHandle(gltf.morphsBuffer) },
                         { lvk::BufferHandle(gltf.matricesBuffer) },
                         { lvk::BufferHandle(gltf.vertexSkinningBuffer) } } });
    }
  }

  const bool screenCopy = gltf.isScreenCopyRequired();

  {
    // 1st pass
    pushConstants.transmissionFramebuffer = 0;

    const lvk::RenderPass renderPass = {
      .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
      .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f },
    };

    const lvk::Framebuffer framebuffer = {
      .color        = { { .texture = screenCopy ? gltf.offscreenTex[gltf.currentOffscreenTex] : ctx->getCurrentSwapchainTexture() } },
      .depthStencil = { .texture = gltf.app.getDepthTexture() },
    };

    {
      buf.cmdBeginRendering(renderPass, framebuffer, { .buffers = { { lvk::BufferHandle(gltf.vertexBuffer) } } });
      buf.cmdBindVertexBuffer(0, gltf.vertexBuffer, 0);
      buf.cmdBindIndexBuffer(gltf.indexBuffer, lvk::IndexFormat_UI32);

      buf.cmdBindDepthState({ .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true });

      buf.cmdBindRenderPipeline(gltf.pipelineSolid);
      buf.cmdPushConstants(pushConstants);
      for (uint32_t transformId : gltf.opaqueNodes) {
        const GLTFTransforms transform = gltf.transforms[transformId];

        buf.cmdPushDebugGroupLabel(gltf.nodesStorage[transform.nodeRef].name.c_str(), 0xff0000ff);
        const GLTFMesh submesh = gltf.meshesStorage[transform.meshRef];
        buf.cmdDrawIndexed(submesh.indexCount, 1, submesh.indexOffset, submesh.vertexOffset, transformId);
        buf.cmdPopDebugGroupLabel();
      }
      if (!screenCopy) {
        drawUI(buf, framebuffer);
      }
      buf.cmdEndRendering();
    }
  }

  if (!gltf.transmissionNodes.empty() || !gltf.transparentNodes.empty()) {
    // 2nd pass
    const lvk::RenderPass renderPass = {
      .color = { { .loadOp = lvk::LoadOp_Load } },
      .depth = { .loadOp = lvk::LoadOp_Load },
    };

    const lvk::Framebuffer framebuffer = {
      .color        = { { .texture = ctx->getCurrentSwapchainTexture() } },
      .depthStencil = { .texture = gltf.app.getDepthTexture() },
    };

    if (screenCopy) {
      buf.cmdCopyImage(
          gltf.offscreenTex[gltf.currentOffscreenTex], ctx->getCurrentSwapchainTexture(),
          ctx->getDimensions(ctx->getCurrentSwapchainTexture()));
      buf.cmdGenerateMipmap(gltf.offscreenTex[gltf.currentOffscreenTex]);

      pushConstants.transmissionFramebuffer = gltf.offscreenTex[gltf.currentOffscreenTex].index();
      buf.cmdPushConstants(pushConstants);
    }

    buf.cmdBeginRendering(renderPass, framebuffer, { .textures = { lvk::TextureHandle(gltf.offscreenTex[gltf.currentOffscreenTex]) } });
    buf.cmdBindVertexBuffer(0, gltf.vertexBuffer, 0);
    buf.cmdBindIndexBuffer(gltf.indexBuffer, lvk::IndexFormat_UI32);

    buf.cmdBindDepthState({ .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true });

    // Volumetric opaque
    buf.cmdBindRenderPipeline(gltf.pipelineSolid);
    for (uint32_t transformId : gltf.transmissionNodes) {
      const GLTFTransforms transform = gltf.transforms[transformId];
      buf.cmdPushDebugGroupLabel(gltf.nodesStorage[transform.nodeRef].name.c_str(), 0x00FF00ff);
      const GLTFMesh submesh = gltf.meshesStorage[transform.meshRef];
      buf.cmdDrawIndexed(submesh.indexCount, 1, submesh.indexOffset, submesh.vertexOffset, transformId);
      buf.cmdPopDebugGroupLabel();
    }

    //
    buf.cmdBindRenderPipeline(gltf.pipelineTransparent);
    for (uint32_t transformId : gltf.transparentNodes) {
      const GLTFTransforms transform = gltf.transforms[transformId];
      buf.cmdPushDebugGroupLabel(gltf.nodesStorage[transform.nodeRef].name.c_str(), 0x00FF00ff);
      const GLTFMesh submesh = gltf.meshesStorage[transform.meshRef];
      buf.cmdDrawIndexed(submesh.indexCount, 1, submesh.indexOffset, submesh.vertexOffset, transformId);
      buf.cmdPopDebugGroupLabel();
    }

    drawUI(buf, framebuffer);

    buf.cmdEndRendering();
  }

  ctx->wait(ctx->submit(buf, ctx->getCurrentSwapchainTexture()));

  gltf.currentOffscreenTex = (gltf.currentOffscreenTex + 1) % LVK_ARRAY_NUM_ELEMENTS(gltf.offscreenTex);
}

MaterialType detectMaterialType(const aiMaterial* mtl)
{
  aiShadingMode shadingMode = aiShadingMode_NoShading;

  if (mtl->Get(AI_MATKEY_SHADING_MODEL, shadingMode) == AI_SUCCESS) {
    if (shadingMode == aiShadingMode_Unlit) {
      return MaterialType_Unlit;
    }
  }

  if (shadingMode == aiShadingMode_PBR_BRDF) {
    ai_real factor = 0;
    if (mtl->Get(AI_MATKEY_GLOSSINESS_FACTOR, factor) == AI_SUCCESS) {
      return MaterialType_SpecularGlossiness;
    } else if (mtl->Get(AI_MATKEY_METALLIC_FACTOR, factor) == AI_SUCCESS) {
      return MaterialType_MetallicRoughness;
    }
  }

  LLOGW("Unknown material type\n");

  return MaterialType_Invalid;
}

void printPrefix(int ofs)
{
  for (int i = 0; i < ofs; i++)
    printf("\t");
}

void printMat4(const aiMatrix4x4& m)
{
  if (!m.IsIdentity()) {
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < 4; j++)
        printf("%f ;", m[i][j]);
  } else {
    printf(" Identity");
  }
}

void animateGLTF(GLTFContext& gltf, AnimationState& anim, float dt)
{
  if (gltf.transforms.empty())
    return;

  if (gltf.pipelineComputeAnimations.empty()) {
    gltf.pipelineComputeAnimations = gltf.app.ctx_->createComputePipeline({
        .smComp = gltf.animation,
    });
  }

  // we support only one single animation at this time
  anim.active = anim.animId != ~0;
  gltf.animated = anim.active;
  if (anim.active) {
    updateAnimation(gltf, anim, dt);
  }
}

void animateBlendingGLTF(GLTFContext& gltf, AnimationState& anim1, AnimationState& anim2, float weight, float dt)
{
  if (gltf.transforms.empty())
    return;

  if (gltf.pipelineComputeAnimations.empty()) {
    gltf.pipelineComputeAnimations = gltf.app.ctx_->createComputePipeline({
        .smComp = gltf.animation,
    });
  }

  anim1.active = anim1.animId != ~0;
  anim2.active = anim2.animId != ~0;
  gltf.animated = anim1.active || anim2.active;
  if (anim1.active && anim2.active) {
    updateAnimationBlending(gltf, anim1, anim2, weight, dt);
  } else if (anim1.active) {
	  updateAnimation(gltf, anim1, dt);
  } else if (anim2.active) {
	  updateAnimation(gltf, anim2, dt);
  }

}

std::vector<std::string> camerasGLTF(GLTFContext& gltf)
{
  std::vector<std::string> names;
  names.reserve(gltf.cameras.size() + 1);

  for (auto c : gltf.cameras) {
    names.push_back(c.name);
  }
  names.push_back("Application cam");

  return names;
}

std::vector<std::string> animationsGLTF(GLTFContext& gltf)
{
	std::vector<std::string> names;
	names.reserve(gltf.animations.size());

	for (auto c : gltf.animations) {
		names.push_back(c.name);
	}

	return names;
}
