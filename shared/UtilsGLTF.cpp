#include "UtilsGLTF.h"

bool assignUVandSampler(
    const glTFGlobalSamplers& samplers, aiMaterial* const& mtlDescriptor, aiTextureType textureType, uint32_t& uvIndex,
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
  }
  return res;
}

namespace
{
void loadMaterialTexture(
    aiMaterial* const& mtlDescriptor, aiTextureType textureType, const char* assetFolder, lvk::Holder<lvk::TextureHandle>& textureHandle,
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
} // namespace

glTFMaterialData setupglTFMaterialData(
    const std::unique_ptr<lvk::IContext>& ctx, const glTFGlobalSamplers& samplers, aiMaterial* const& mtlDescriptor,
    const char* assetFolder, glTFDataHolder& glTFDataholder)
{
  std::unique_ptr<glTFMaterialTextures> mat = std::make_unique<glTFMaterialTextures>();

  uint32_t materialType = 0;

  static int whitePixel = 0xFFFFFFFF;

  mat->white = ctx->createTexture(
      {
          .type       = lvk::TextureType_2D,
          .format     = lvk::Format_RGBA_SRGB8,
          .dimensions = {1, 1},
          .usage      = lvk::TextureUsageBits_Sampled,
          .data       = &whitePixel,
          .debugName  = "white1x1",
  },
      "white1x1");

  aiShadingMode shademode = (aiShadingMode)-1;
  if (mtlDescriptor->Get(AI_MATKEY_SHADING_MODEL, shademode) == AI_SUCCESS) {
    materialType = shademode == aiShadingMode_Unlit ? Unlit : 0;
  }

  loadMaterialTexture(mtlDescriptor, aiTextureType_BASE_COLOR, assetFolder, mat->baseColorTexture, ctx, true);
  loadMaterialTexture(mtlDescriptor, aiTextureType_METALNESS, assetFolder, mat->surfacePropertiesTexture, ctx, false);

  materialType = MetallicRoughness;

  // if (mat->baseColorTexture.valid()) {
  //   materialType = MetallicRoughness;
  // } else {
  //   // FIXME
  //   loadMaterialTexture(mtlDescriptor, aiTextureType_DIFFUSE, assetFolder, mat->baseColorTexture, ctx, true);
  //   loadMaterialTexture(mtlDescriptor, aiTextureType_SPECULAR, assetFolder, mat->surfacePropertiesTexture, ctx, true);

  //  if (mat->baseColorTexture.valid()) {
  //    materialType = SpecularGlossiness;
  //  } else {
  //    printf("Unknown material type\n");
  //    materialType = MetallicRoughness;
  //    // exit(256);
  //  }
  //}

  // Load common textures
  loadMaterialTexture(mtlDescriptor, aiTextureType_LIGHTMAP, assetFolder, mat->occlusionTexture, ctx, false);
  loadMaterialTexture(mtlDescriptor, aiTextureType_EMISSIVE, assetFolder, mat->emissiveTexture, ctx, true);
  loadMaterialTexture(mtlDescriptor, aiTextureType_NORMALS, assetFolder, mat->normalTexture, ctx, false);

  // Sheen
  loadMaterialTexture(mtlDescriptor, aiTextureType_SHEEN, assetFolder, mat->sheenColorTexture, ctx, true, 0);
  loadMaterialTexture(mtlDescriptor, aiTextureType_SHEEN, assetFolder, mat->sheenRoughnessTexture, ctx, false, 1);

  // Clearcoat
  loadMaterialTexture(mtlDescriptor, aiTextureType_CLEARCOAT, assetFolder, mat->clearCoatTexture, ctx, true, 0);
  loadMaterialTexture(mtlDescriptor, aiTextureType_CLEARCOAT, assetFolder, mat->clearCoatRoughnessTexture, ctx, false, 1);
  loadMaterialTexture(mtlDescriptor, aiTextureType_CLEARCOAT, assetFolder, mat->clearCoatNormalTexture, ctx, false, 2);

  // Specular
  loadMaterialTexture(mtlDescriptor, aiTextureType_SPECULAR, assetFolder, mat->specularTexture, ctx, true, 0);
  loadMaterialTexture(mtlDescriptor, aiTextureType_SPECULAR, assetFolder, mat->specularColorTexture, ctx, true, 1);

  // Transmission
  loadMaterialTexture(mtlDescriptor, aiTextureType_TRANSMISSION, assetFolder, mat->transmissionTexture, ctx, true, 0);

  // Volume
  loadMaterialTexture(mtlDescriptor, aiTextureType_TRANSMISSION, assetFolder, mat->thicknessTexture, ctx, true, 1);

  // Iridescence
  // loadMaterialTexture(mtlDescriptor, aiTextureType_IRID, assetFolder, mat->specularTexture, ctx, true, 0);

  // Anistoropy

  glTFMaterialData res = {
    .baseColorFactor                  = vec4(1.0f, 1.0f, 1.0f, 1.0f),
    .metallicRoughnessNormalOcclusion = vec4(1.0f, 1.0f, 1.0f, 1.0f),
    .specularFactors                  = vec4(1.0f, 1.0f, 1.0f, 1.0f),
    .emissiveFactorAlphaCutoff        = vec4(0.0f, 0.0f, 0.0f, 0.5f),
    .occlusionTexture                 = mat->occlusionTexture.index(),
    .emissiveTexture                  = mat->emissiveTexture.valid() ? mat->emissiveTexture.index() : mat->white.index(),
    .baseColorTexture                 = mat->baseColorTexture.valid() ? mat->baseColorTexture.index() : mat->white.index(),
    .surfacePropertiesTexture         = mat->surfacePropertiesTexture.valid() ? mat->surfacePropertiesTexture.index() : mat->white.index(),
    .normalTexture                    = mat->normalTexture.valid() ? mat->normalTexture.index() : mat->white.index(),
    .sheenColorTexture                = mat->sheenColorTexture.index(),
    .sheenRoughnessTexture            = mat->sheenRoughnessTexture.index(),
    .clearCoatTexture                 = mat->clearCoatTexture.valid() ? mat->clearCoatTexture.index() : mat->white.index(),
    .clearCoatRoughnessTexture   = mat->clearCoatRoughnessTexture.valid() ? mat->clearCoatRoughnessTexture.index() : mat->white.index(),
    .clearCoatNormalTexture      = mat->clearCoatNormalTexture.valid() ? mat->clearCoatNormalTexture.index() : mat->white.index(),
    .specularTexture             = mat->specularTexture.valid() ? mat->specularTexture.index() : mat->white.index(),
    .specularColorTexture        = mat->specularColorTexture.valid() ? mat->specularColorTexture.index() : mat->white.index(),
    .transmissionTexture         = mat->transmissionTexture.index(),
    .thicknessTexture            = mat->thicknessTexture.index(),
    .iridescenceTexture          = mat->iridescenceTexture.index(),
    .iridescenceThicknessTexture = mat->iridescenceThicknessTexture.index(),
    .anisotropyTexture           = mat->anisotropyTexture.index(),
    .materialType                = materialType,
  };

  aiColor4D aiColor;
  if (mtlDescriptor->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == AI_SUCCESS) {
    res.baseColorFactor = vec4(aiColor.r, aiColor.g, aiColor.b, aiColor.a);
  }
  assignUVandSampler(samplers, mtlDescriptor, aiTextureType_DIFFUSE, res.baseColorTextureUV, res.baseColorTextureSampler);

  if (mtlDescriptor->Get(AI_MATKEY_COLOR_EMISSIVE, aiColor) == AI_SUCCESS) {
    // mat->emissiveFactor = vec3(aiColor.r, aiColor.g, aiColor.b);
    res.emissiveFactorAlphaCutoff = vec4(aiColor.r, aiColor.g, aiColor.b, 0.5f);
  }
  assignUVandSampler(samplers, mtlDescriptor, aiTextureType_EMISSIVE, res.emissiveTextureUV, res.emissiveTextureSampler);

  ai_real emissiveStrength = 1.0f;
  if (mtlDescriptor->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissiveStrength) == AI_SUCCESS) {
    // mat->emissiveFactor = vec3(aiColor.r, aiColor.g, aiColor.b);
    res.emissiveFactorAlphaCutoff *= vec4(emissiveStrength, emissiveStrength, emissiveStrength, 1.0f);
  }

  if (materialType == MetallicRoughness) {
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
  } else if (materialType == SpecularGlossiness) {
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
      res.alphaMode = (uint32_t)glTFMaterialData::AlphaMode::eMask;
    } else if (alphaMode == aiString("BLEND")) {
      res.alphaMode = (uint32_t)glTFMaterialData::AlphaMode::eBlend;
    } else {
      res.alphaMode = (uint32_t)glTFMaterialData::AlphaMode::eOpaque;
    }
  }

  ai_real alphaCutoff;
  if (mtlDescriptor->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff) == AI_SUCCESS) {
    res.emissiveFactorAlphaCutoff.w = alphaCutoff;
  }

  // Extensions
  // Sheen
  {
    bool useSheen = !mat->sheenColorTexture.empty() || !mat->sheenRoughnessTexture.empty();
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
      res.materialType |= Sheen;
    }
  }

  // Clear coat
  {
    bool useClearCoat = !mat->clearCoatTexture.empty() || !mat->clearCoatRoughnessTexture.empty() || !mat->clearCoatNormalTexture.empty();
    ai_real clearcoatFactor;
    if (mtlDescriptor->Get(AI_MATKEY_CLEARCOAT_FACTOR, clearcoatFactor) == AI_SUCCESS) {
      res.clearcoatTransmissionTickness.x = clearcoatFactor;
      useClearCoat                        = true;
    }

    ai_real clearcoatRoughnessFactor;
    if (mtlDescriptor->Get(AI_MATKEY_CLEARCOAT_ROUGHNESS_FACTOR, clearcoatRoughnessFactor) == AI_SUCCESS) {
      res.clearcoatTransmissionTickness.y = clearcoatRoughnessFactor;
      useClearCoat                        = true;
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
      res.materialType |= ClearCoat;
    }
  }

  // Specular
  {
    bool useSpecular = !mat->specularColorTexture.empty() || !mat->specularTexture.empty();

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
      res.materialType |= Specular;
    }
  }

  glTFDataholder.textures.push_back(std::move(mat));
  return res;
}

void loadglTF(glTFContext& gltf, const char* glTFName, const char* glTFDataPath)
{
  const aiScene* scene = aiImportFile(glTFName, aiProcess_Triangulate);
  if (!scene || !scene->HasMeshes()) {
    printf("Unable to load %s\n", glTFName);
    exit(255);
  }

  const vec4 white = vec4(1.0f, 1.0f, 1.0f, 1.0f);

  std::vector<vertex> vertices;
  std::vector<uint32_t> indices;

  std::vector<uint32_t> startVertex;
  std::vector<uint32_t> startIndex;

  startVertex.push_back(0);
  startIndex.push_back(0);

  for (uint32_t m = 0; m < scene->mNumMeshes; ++m) {
    const aiMesh* mesh = scene->mMeshes[m];

    for (unsigned int i = 0; i != mesh->mNumVertices; i++) {
      const aiVector3D v = mesh->mVertices[i];
      vec3 n             = mesh->mNormals ? vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z) : vec3(0.0f, 1.0f, 0.0f);
      vec4 color =
          mesh->mColors[0] ? vec4(mesh->mColors[0][i].r, mesh->mColors[0][i].g, mesh->mColors[0][i].b, mesh->mColors[0][i].a) : white;
      vec2 uv0 = mesh->mTextureCoords[0] ? vec2(mesh->mTextureCoords[0][i].x, 1.0f - mesh->mTextureCoords[0][i].y) : vec2(0.0f, 0.0f);
      vec2 uv1 = mesh->mTextureCoords[1] ? vec2(mesh->mTextureCoords[1][i].x, 1.0f - mesh->mTextureCoords[1][i].y) : vec2(0.0f, 0.0f);

      vertices.push_back({ vec3(v.x, v.y, v.z), n, color, uv0, uv1 });
    }

    startVertex.push_back((uint32_t)vertices.size());
    for (unsigned int i = 0; i != mesh->mNumFaces; i++) {
      for (int j = 0; j != 3; j++) {
        indices.push_back(mesh->mFaces[i].mIndices[j]);
      }
    }
    startIndex.push_back((uint32_t)indices.size());
  }

  if (!scene->mRootNode) {
    printf("Scene has no root node\n");
    exit(255);
  }

  auto& ctx = gltf.app.ctx_;

  for (auto mtl = 0; mtl < scene->mNumMaterials; ++mtl) {
    const auto mtlDescriptor        = scene->mMaterials[mtl];
    gltf.matPerFrame.materials[mtl] = setupglTFMaterialData(ctx, gltf.samplers, mtlDescriptor, glTFDataPath, gltf.glTFDataholder);
  }


  gltf.nodesStorage.push_back({ .name      = scene->mRootNode->mName.C_Str() ? scene->mRootNode->mName.C_Str() : "root",
                                .transform = AiMatrix4x4ToGlm(&scene->mRootNode->mTransformation) });

  gltf.root = gltf.nodesStorage.size() - 1;


  std::function<void(const aiNode* rootNode, glTFNodeRef gltfNode)> traverseTree = [&](const aiNode* rootNode, glTFNodeRef gltfNode) {
    for (auto m = 0; m < rootNode->mNumMeshes; ++m) {
      const uint32_t meshIdx = rootNode->mMeshes[m];
      const aiMesh* mesh     = scene->mMeshes[meshIdx];

      gltf.meshesStorage.push_back({ .primitive    = PrimitiveType::eTriangle,
                                     .vertexOffset = startVertex[meshIdx],
                                     .vertexCount  = mesh->mNumVertices,
                                     .indexOffset  = startIndex[meshIdx],
                                     .indexCount   = mesh->mNumFaces * 3,
                                     .matIdx       = mesh->mMaterialIndex,
                                     // FIXME
            .opaque = gltf.matPerFrame.materials[mesh->mMaterialIndex].alphaMode != (uint32_t)glTFMaterialData::AlphaMode::eBlend });
      gltf.nodesStorage[gltfNode].meshes.push_back(gltf.meshesStorage.size() - 1);
    }
    for (glTFNodeRef i = 0; i < rootNode->mNumChildren; i++) {
      const aiNode* node = rootNode->mChildren[i];
      glTFNode childNode({ .name      = node->mName.C_Str() ? node->mName.C_Str() : "node",
                           .transform = gltf.nodesStorage[gltfNode].transform * AiMatrix4x4ToGlm(&node->mTransformation) });
      gltf.nodesStorage.push_back(childNode);
      auto nodeIdx = gltf.nodesStorage.size() - 1;
      gltf.nodesStorage[gltfNode].children.push_back(nodeIdx);
      traverseTree(node, nodeIdx);
    }
  };

  traverseTree(scene->mRootNode, gltf.root);

  gltf.vertexBuffer = ctx->createBuffer(
      { .usage     = lvk::BufferUsageBits_Vertex,
        .storage   = lvk::StorageType_Device,
        .size      = sizeof(vertex) * vertices.size(),
        .data      = vertices.data(),
        .debugName = "Buffer: vertex" },
      nullptr);
  gltf.indexBuffer = ctx->createBuffer(
      { .usage     = lvk::BufferUsageBits_Index,
        .storage   = lvk::StorageType_Device,
        .size      = sizeof(uint32_t) * indices.size(),
        .data      = indices.data(),
        .debugName = "Buffer: index" },
      nullptr);

  const lvk::VertexInput vdesc = {
    .attributes    = { { .location = 0, .format = lvk::VertexFormat::Float3, .offset = 0  },
							  { .location = 1, .format = lvk::VertexFormat::Float3, .offset = 12 },
                       { .location = 2, .format = lvk::VertexFormat::Float4, .offset = 24 },
							  { .location = 3, .format = lvk::VertexFormat::Float2, .offset = 40 },
							  { .location = 4, .format = lvk::VertexFormat::Float2, .offset = 48 }, },
    .inputBindings = { { .stride = sizeof(vertex) } },
  };

  gltf.vert = loadShaderModule(ctx, "data/shaders/gltf/main.vert");
  gltf.frag = loadShaderModule(ctx, "data/shaders/gltf/main.frag");

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

  EnvironmentsPerFrame envPerFrame;
  envPerFrame.environments[0] = {
    .envMapTexture                  = gltf.envMapTextures.envMapTexture.index(),
    .envMapTextureSampler           = gltf.samplers.clamp.index(),
    .envMapTextureIrradiance        = gltf.envMapTextures.envMapTextureIrradiance.index(),
    .envMapTextureIrradianceSampler = gltf.samplers.clamp.index(),
    .lutBRDFTexture                 = gltf.envMapTextures.texBRDF_LUT.index(),
    .lutBRDFTextureSampler          = gltf.samplers.clamp.index(),
    .envMapTextureCharlie           = gltf.envMapTextures.envMapTextureCharlie.index(),
    .envMapTextureCharlieSampler    = gltf.samplers.clamp.index(),
  };

  gltf.envBuffer = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Uniform,
      .storage   = lvk::StorageType_HostVisible,
      .size      = sizeof(envPerFrame),
      .data      = &envPerFrame,
      .debugName = "PerFrame environments",
  });

  gltf.perFrameBuffer = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Uniform,
      .storage   = lvk::StorageType_HostVisible,
      .size      = sizeof(glTFFrameData),
      .data      = &gltf.frameData,
      .debugName = "Per Frame data",
  });

  LVK_ASSERT(gltf.pipelineSolid.valid());

  aiReleaseImport(scene);
}

void buildTransformsList(glTFContext& gltf)
{
  gltf.transforms.clear();
  gltf.opaqueNodes.clear();
  gltf.transparentNodes.clear();

  std::function<void(glTFNodeRef & gltfNode)> traverseTree = [&](glTFNodeRef& nodeRef) {
    auto& node = gltf.nodesStorage[nodeRef];
    for (auto meshId : node.meshes) {
      auto mesh = gltf.meshesStorage[meshId];
      gltf.transforms.push_back(
          { .model = node.transform, .matId = mesh.matIdx, .nodeRef = nodeRef, .meshRef = meshId, .opaque = mesh.opaque });
      if (mesh.opaque) {
        gltf.opaqueNodes.push_back(gltf.transforms.size() - 1);
      } else {
        gltf.transparentNodes.push_back(gltf.transforms.size() - 1);
      }
    }
    for (auto child : node.children) {
      traverseTree(child);
    }
  };

  traverseTree(gltf.root);

  auto& ctx            = gltf.app.ctx_;

  gltf.transformBuffer.reset();

  gltf.transformBuffer = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Uniform,
      .storage   = lvk::StorageType_HostVisible,
      .size      = gltf.transforms.size() * sizeof(glTFTransforms),
      .data      = &gltf.transforms[0],
      .debugName = "Per Frame data",
  });

  //ctx->upload(gltf.transformBuffer, &gltf.transforms, gltf.transforms.size() * sizeof(glTFTransforms));
}

void sortTransparentNodes(glTFContext& gltf, const vec3& cameraPos)
{
  // glTF spec expects to sort based on pivot positions (not sure correct way though)
  std::sort(gltf.transparentNodes.begin(), gltf.transparentNodes.end(), [&](uint32_t a, uint32_t b) {
    float distA = glm::length(cameraPos - vec3(gltf.transforms[a].model[3]));
    float distB = glm::length(cameraPos - vec3(gltf.transforms[b].model[3]));
    return distA < distB;
  });
}

void renderglTF(glTFContext& gltf, const mat4& m, const mat4& v, const mat4& p, bool rebuildRenderList)
{
  auto& ctx = gltf.app.ctx_;

  auto camInv = glm::inverse(v) * vec4(0.0f, 0.0f, 1.0f, 1.0f);

  if (rebuildRenderList || gltf.transforms.empty()) {
    buildTransformsList(gltf);
  }

  sortTransparentNodes(gltf, camInv);

  gltf.frameData = {
    .view      = v,
    .proj      = p,
    .cameraPos = camInv,
  };

  struct PushConstants {
    uint64_t draw;
    uint64_t materials;
    uint64_t environments;
    uint64_t transforms;
    uint32_t transformId;
    uint32_t envId;
  } pushConstants = {
    .draw         = ctx->gpuAddress(gltf.perFrameBuffer),
    .materials    = ctx->gpuAddress(gltf.matBuffer),
    .environments = ctx->gpuAddress(gltf.envBuffer),
    .transforms   = ctx->gpuAddress(gltf.transformBuffer),
    .transformId  = 0,
    .envId        = 0,
  };

  ctx->upload(gltf.perFrameBuffer, &gltf.frameData, sizeof(glTFFrameData));

  const lvk::RenderPass renderPass = {
    .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
    .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
  };

  const lvk::Framebuffer framebuffer = {
    .color        = { { .texture = ctx->getCurrentSwapchainTexture() } },
    .depthStencil = { .texture = gltf.app.getDepthTexture() },
  };

  lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
  {
    buf.cmdBeginRendering(renderPass, framebuffer);
    buf.cmdBindVertexBuffer(0, gltf.vertexBuffer, 0);
    buf.cmdBindIndexBuffer(gltf.indexBuffer, lvk::IndexFormat_UI32);

    buf.cmdBindDepthState(gltf.dState);
    {
      buf.cmdBindRenderPipeline(gltf.pipelineSolid);
      for (auto transformId : gltf.opaqueNodes) {
        auto transform = gltf.transforms[transformId];

        pushConstants.transformId = transformId;
        buf.cmdPushDebugGroupLabel(gltf.nodesStorage[transform.nodeRef].name.c_str(), 0xff0000ff);
        {
          buf.cmdPushConstants(pushConstants);
          auto submesh = gltf.meshesStorage[transform.meshRef];
          buf.cmdDrawIndexed(submesh.indexCount, 1, submesh.indexOffset, submesh.vertexOffset);
        }
        buf.cmdPopDebugGroupLabel();
      }

    buf.cmdBindRenderPipeline(gltf.pipelineTransparent);
      for (auto transformId : gltf.transparentNodes) {
        auto transform = gltf.transforms[transformId];

        pushConstants.transformId = transformId;
        buf.cmdPushDebugGroupLabel(gltf.nodesStorage[transform.nodeRef].name.c_str(), 0x00FF00ff);
        {
          buf.cmdPushConstants(pushConstants);
          auto submesh = gltf.meshesStorage[transform.meshRef];
          buf.cmdDrawIndexed(submesh.indexCount, 1, submesh.indexOffset, submesh.vertexOffset);
        }
        buf.cmdPopDebugGroupLabel();
      }
    }

    buf.cmdEndRendering();
  }
  ctx->submit(buf, ctx->getCurrentSwapchainTexture());
}
