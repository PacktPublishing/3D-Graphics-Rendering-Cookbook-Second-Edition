#include "shared/VulkanApp.h"

#include <assimp/GltfMaterial.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <shared/UtilsGLTF.h>

struct SpecularGlossinessDataGPU {
  vec4 baseColorFactor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
  // packed metallicFactor, roughnessFactor, normalScale, occlusionStrength
  vec4 metallicRoughnessNormalOcclusion = vec4(1.0f, 1.0f, 1.0f, 1.0f);
  vec4 specularGlossiness               = vec4(1.0f, 1.0f, 1.0f, 1.0f);
  // vec3 emissiveFactor + float AlphaCutoff
  vec4 emissiveFactorAlphaCutoff = vec4(0.0f, 0.0f, 0.0f, 0.5f);

  uint32_t occlusionTexture        = 0;
  uint32_t occlusionTextureSampler = 0;
  uint32_t occlusionTextureUV      = 0;

  uint32_t emissiveTexture         = 0;
  uint32_t emissiveTextureSampler  = 0;
  uint32_t emissiveTextureUV       = 0;

  uint32_t baseColorTexture        = 0;
  uint32_t baseColorTextureSampler = 0;
  uint32_t baseColorTextureUV              = 0;

  uint32_t surfacePropertiesTexture        = 0;
  uint32_t surfacePropertiesTextureSampler = 0;
  uint32_t surfacePropertiesTextureUV      = 0;

  uint32_t normalTexture        = 0;
  uint32_t normalTextureSampler = 0;
  uint32_t normalTextureUV      = 0;

  uint32_t alphaMode            = 0;
  uint32_t materialType = 0;
  uint32_t padding[3]   = { 0, 0 };

  enum AlphaMode : uint32_t {
    AlphaMode_Opaque = 0,
    AlphaMode_Mask   = 1,
    AlphaMode_Blend  = 2,
  };
};

struct SpecularGlossinessMaterialsPerFrame {
  SpecularGlossinessDataGPU materials[kMaxMaterials];
};

void loadMaterialTexture(
    const aiMaterial* mtlDescriptor, aiTextureType textureType, const char* assetFolder, lvk::Holder<lvk::TextureHandle>& textureHandle,
    const std::unique_ptr<lvk::IContext>& ctx, bool sRGB)
{
  if (mtlDescriptor->GetTextureCount(textureType) > 0) {
    aiString path;
    mtlDescriptor->GetTexture(textureType, 0, &path);
    aiString fullPath(assetFolder);
    fullPath.Append(path.C_Str());

    textureHandle = loadTexture(ctx, fullPath.C_Str(), lvk::TextureType_2D, sRGB);
    if (textureHandle.empty()) {
      assert(0);
      exit(256);
    }
  }
}

SpecularGlossinessDataGPU setupSpecularGlossinessData(
    const std::unique_ptr<lvk::IContext>& ctx, const GLTFGlobalSamplers& samplers, const aiMaterial* mtlDescriptor, const char* assetFolder,
    GLTFDataHolder& glTFDataholder)
{
  GLTFMaterialTextures mat;

  const MaterialType materialType = detectMaterialType(mtlDescriptor);

  loadMaterialTexture(mtlDescriptor, aiTextureType_BASE_COLOR, assetFolder, mat.baseColorTexture, ctx, true);
  loadMaterialTexture(mtlDescriptor, aiTextureType_METALNESS, assetFolder, mat.surfacePropertiesTexture, ctx, false);

  if (materialType == MaterialType_SpecularGlossiness) {
    loadMaterialTexture(mtlDescriptor, aiTextureType_DIFFUSE, assetFolder, mat.baseColorTexture, ctx, true);
    loadMaterialTexture(mtlDescriptor, aiTextureType_SPECULAR, assetFolder, mat.surfacePropertiesTexture, ctx, true);
  }

  // Load common textures
  loadMaterialTexture(mtlDescriptor, aiTextureType_LIGHTMAP, assetFolder, mat.occlusionTexture, ctx, false);
  loadMaterialTexture(mtlDescriptor, aiTextureType_EMISSIVE, assetFolder, mat.emissiveTexture, ctx, true);
  loadMaterialTexture(mtlDescriptor, aiTextureType_NORMALS, assetFolder, mat.normalTexture, ctx, false);

  SpecularGlossinessDataGPU res = {
    .baseColorFactor                  = vec4(1.0f, 1.0f, 1.0f, 1.0f),
    .metallicRoughnessNormalOcclusion = vec4(1.0f, 1.0f, 1.0f, 1.0f),
    .emissiveFactorAlphaCutoff        = vec4(0.0f, 0.0f, 0.0f, 0.5f),
    .occlusionTexture                 = mat.occlusionTexture.index(),
    .emissiveTexture                  = mat.emissiveTexture.index(),
    .baseColorTexture                 = mat.baseColorTexture.index(),
    .surfacePropertiesTexture         = mat.surfacePropertiesTexture.index(),
    .normalTexture                    = mat.normalTexture.index(),
    .materialType                     = materialType,
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

  if (materialType == MaterialType_MetallicRoughness) {
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
  } else if (materialType == MaterialType_SpecularGlossiness) {
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

  glTFDataholder.textures.push_back(std::move(mat));
  return res;
}

int main()
{
  VulkanApp app({
      .initialCameraPos = vec3(0.0f, 0.0f, -0.5f),
  });

  app.positioner_.maxSpeed_ = 2.0f;

  auto& ctx = app.ctx_;

  {
    const aiScene* scene =
        aiImportFile("deps/src/glTF-Sample-Assets/Models/SpecGlossVsMetalRough/glTF/SpecGlossVsMetalRough.gltf", aiProcess_Triangulate);

    if (!scene || !scene->HasMeshes()) {
      printf("Unable to load SpecGlossVsMetalRough.gltf\n");
      exit(255);
    }

    const vec4 white = vec4(1.0f, 1.0f, 1.0f, 1.0f);

    std::vector<Vertex> vertices;
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

    struct Node {
      std::string name;
      glm::mat4 transform;
      std::vector<Node> children;
      std::vector<GLTFMesh> meshes;
    };

    Node root({ .name      = scene->mRootNode->mName.C_Str() ? scene->mRootNode->mName.C_Str() : "root",
                .transform = aiMatrix4x4ToMat4(scene->mRootNode->mTransformation) });

    std::function<void(const aiNode* rootNode, Node& gltfNode)> traverseTree = [&](const aiNode* rootNode, Node& gltfNode) {
      for (auto m = 0; m < rootNode->mNumMeshes; ++m) {
        const uint32_t meshIdx = rootNode->mMeshes[m];
        const aiMesh* mesh     = scene->mMeshes[meshIdx];

        gltfNode.meshes.push_back({
            .primitive    = lvk::Topology_Triangle,
            .vertexOffset = startVertex[meshIdx],
            .vertexCount  = mesh->mNumVertices,
            .indexOffset  = startIndex[meshIdx],
            .indexCount   = mesh->mNumFaces * 3,
            .matIdx       = mesh->mMaterialIndex,
        });
      }
      for (unsigned int i = 0; i < rootNode->mNumChildren; i++) {
        const aiNode* node = rootNode->mChildren[i];
        Node childNode({
            .name      = node->mName.C_Str() ? node->mName.C_Str() : "node",
            .transform = aiMatrix4x4ToMat4(node->mTransformation),
        });
        traverseTree(node, childNode);
        gltfNode.children.push_back(childNode);
      }
    };

    traverseTree(scene->mRootNode, root);

    GLTFGlobalSamplers samplers(ctx);
    EnvironmentMapTextures envMapTextures(ctx);

    lvk::Holder<lvk::BufferHandle> vertexBuffer = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Vertex,
          .storage   = lvk::StorageType_Device,
          .size      = sizeof(Vertex) * vertices.size(),
          .data      = vertices.data(),
          .debugName = "Buffer: vertex" },
        nullptr);
    lvk::Holder<lvk::BufferHandle> indexBuffer = ctx->createBuffer(
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
    .inputBindings = { { .stride = sizeof(Vertex) } },
  };

    lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(ctx, "Chapter06/05_SpecularGlossiness/src/main.vert");
    lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(ctx, "Chapter06/05_SpecularGlossiness/src/main.frag");

    lvk::Holder<lvk::RenderPipelineHandle> pipelineSolid = ctx->createRenderPipeline({
        .vertexInput = vdesc,
        .smVert      = vert,
        .smFrag      = frag,
        .color       = { { .format = ctx->getSwapchainFormat() } },
        .depthFormat = app.getDepthFormat(),
        .cullMode    = lvk::CullMode_Back,
    });

    SpecularGlossinessMaterialsPerFrame matPerFrame;
    GLTFDataHolder glTFDataholder;

    for (unsigned int mtl = 0; mtl < scene->mNumMaterials; ++mtl) {
      const aiMaterial* mtlDescriptor = scene->mMaterials[mtl];
      matPerFrame.materials[mtl]      = setupSpecularGlossinessData(
          ctx, samplers, mtlDescriptor, "deps/src/glTF-Sample-Assets/Models/SpecGlossVsMetalRough/glTF/", glTFDataholder);
    }

    aiReleaseImport(scene);

    const EnvironmentsPerFrame envPerFrame = {
      .environments = { {
          .envMapTexture                  = envMapTextures.envMapTexture.index(),
          .envMapTextureSampler           = samplers.clamp.index(),
          .envMapTextureIrradiance        = envMapTextures.envMapTextureIrradiance.index(),
          .envMapTextureIrradianceSampler = samplers.clamp.index(),
          .lutBRDFTexture                 = envMapTextures.texBRDF_LUT.index(),
          .lutBRDFTextureSampler          = samplers.clamp.index(),
      } },
    };

    lvk::Holder<lvk::BufferHandle> matBuffer = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Uniform,
        .storage   = lvk::StorageType_HostVisible,
        .size      = sizeof(matPerFrame),
        .data      = &matPerFrame,
        .debugName = "PerFrame materials",
    });

    lvk::Holder<lvk::BufferHandle> envBuffer = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Uniform,
        .storage   = lvk::StorageType_HostVisible,
        .size      = sizeof(envPerFrame),
        .data      = &envPerFrame,
        .debugName = "PerFrame environments",
    });

    struct FrameData {
      mat4 model;
      mat4 view;
      mat4 proj;
      vec4 cameraPos;
    };

    lvk::Holder<lvk::BufferHandle> perFrameBuffer = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Uniform,
        .storage   = lvk::StorageType_HostVisible,
        .size      = sizeof(FrameData),
        .debugName = "Per Frame data",
    });

    LVK_ASSERT(pipelineSolid.valid());

    const bool rotateModel = true;

    app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
      const mat4 p = glm::perspective(45.0f, aspectRatio, 0.01f, 100.0f);

      const FrameData frameData = {
        .model     = glm::rotate(mat4(1.0f), rotateModel ? (float)glfwGetTime() : 0.0f, vec3(0.0f, 1.0f, 0.0f)),
        .view      = app.camera_.getViewMatrix(),
        .proj      = p,
        .cameraPos = vec4(app.camera_.getPosition(), 1.0f),
      };
      ctx->upload(perFrameBuffer, &frameData, sizeof(FrameData));

      struct PushConstants {
        mat4 model;
        uint64_t draw;
        uint64_t materials;
        uint64_t environments;
        uint32_t matId;
        uint32_t envId;
      } pushConstants = {
        .model        = glm::mat4(1.0f),
        .draw         = ctx->gpuAddress(perFrameBuffer),
        .materials    = ctx->gpuAddress(matBuffer),
        .environments = ctx->gpuAddress(envBuffer),
        .matId        = 0,
        .envId        = 0,
      };

      const lvk::RenderPass renderPass = {
        .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
        .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
      };

      const lvk::Framebuffer framebuffer = {
        .color        = { { .texture = ctx->getCurrentSwapchainTexture() } },
        .depthStencil = { .texture = app.getDepthTexture() },
      };

      lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
      {
        buf.cmdBeginRendering(renderPass, framebuffer);
        buf.cmdBindVertexBuffer(0, vertexBuffer, 0);
        buf.cmdBindIndexBuffer(indexBuffer, lvk::IndexFormat_UI32);
        buf.cmdBindRenderPipeline(pipelineSolid);
        buf.cmdBindDepthState({ .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true });
        {
          std::function<void(Node & gltfNode)> renderTree = [&](Node& node) {
            pushConstants.model = node.transform;
            for (auto submesh : node.meshes) {
              pushConstants.matId = submesh.matIdx;

              buf.cmdPushDebugGroupLabel(node.name.c_str(), 0xff0000ff);
              {
                buf.cmdPushConstants(pushConstants);
                buf.cmdDrawIndexed(submesh.indexCount, 1, submesh.indexOffset, submesh.vertexOffset);
              }
              buf.cmdPopDebugGroupLabel();
            }
            for (auto child : node.children) {
              renderTree(child);
            }
          };

          renderTree(root);

          app.drawGrid(buf, p, vec3(0, -1.0f, 0));
          app.imgui_->beginFrame(framebuffer);
          app.drawFPS();
          app.drawMemo();
          app.imgui_->endFrame(buf);

          buf.cmdEndRendering();
        }
      }
      ctx->submit(buf, ctx->getCurrentSwapchainTexture());
    });
  }

  return 0;
}
