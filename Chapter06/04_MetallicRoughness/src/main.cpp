#include "shared/VulkanApp.h"

#include <assimp/GltfMaterial.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <shared/Utils.h>
#include <shared/UtilsGLTF.h>

uint32_t globalClampSampler  = 0;
uint32_t globalWrapSampler   = 0;
uint32_t globalMirrorSampler = 0;

struct MetallicRoughnessDataGPU {
  vec4 baseColorFactor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
  // packed metallicFactor, roughnessFactor, normalScale, occlusionStrength
  vec4 metallicRoughnessNormalOcclusion = vec4(1.0f, 1.0f, 1.0f, 1.0f);
  // vec3 emissiveFactor + float AlphaCutoff
  vec4 emissiveFactorAlphaCutoff = vec4(0.0f, 0.0f, 0.0f, 0.5f);

  uint32_t occlusionTexture        = 0;
  uint32_t occlusionTextureSampler = 0;
  uint32_t occlusionTextureUV      = 0;

  uint32_t emissiveTexture        = 0;
  uint32_t emissiveTextureSampler = 0;
  uint32_t emissiveTextureUV      = 0;

  uint32_t baseColorTexture        = 0;
  uint32_t baseColorTextureSampler = 0;
  uint32_t baseColorTextureUV      = 0;

  uint32_t metallicRoughnessTexture        = 0;
  uint32_t metallicRoughnessTextureSampler = 0;
  uint32_t metallicRoughnessTextureUV      = 0;

  uint32_t normalTexture        = 0;
  uint32_t normalTextureSampler = 0;
  uint32_t normalTextureUV      = 0;

  uint32_t alphaMode = 0;

  enum AlphaMode : uint32_t {
    AlphaMode_Opaque = 0,
    AlphaMode_Mask   = 1,
    AlphaMode_Blend  = 2,
  };
};

struct MetallicRoughnessMaterialsPerFrame {
  MetallicRoughnessDataGPU materials[kMaxMaterials];
};

GLTFMaterialTextures loadMaterialTextures(
    const std::unique_ptr<lvk::IContext>& ctx, const char* texAOFile, const char* texEmissiveFile, const char* texAlbedoFile,
    const char* texMeRFile, const char* texNormalFile)
{
  GLTFMaterialTextures mat;

  mat.baseColorTexture = loadTexture(ctx, texAlbedoFile, lvk::TextureType_2D, true);
  if (mat.baseColorTexture.empty()) {
    return {};
  }

  mat.occlusionTexture = loadTexture(ctx, texAOFile);
  if (mat.occlusionTexture.empty()) {
    return {};
  }

  mat.normalTexture = loadTexture(ctx, texNormalFile);
  if (mat.normalTexture.empty()) {
    return {};
  }

  mat.emissiveTexture = loadTexture(ctx, texEmissiveFile, lvk::TextureType_2D, true);
  if (mat.emissiveTexture.empty()) {
    return {};
  }

  mat.surfacePropertiesTexture = loadTexture(ctx, texMeRFile);
  if (mat.surfacePropertiesTexture.empty()) {
    return {};
  }

  mat.wasLoaded = true;

  return mat;
}

MetallicRoughnessDataGPU setupMetallicRoughnessData(
    const GLTFGlobalSamplers& samplers, const GLTFMaterialTextures& mat, const aiMaterial* mtlDescriptor)
{
  MetallicRoughnessDataGPU res = {
    .baseColorFactor                  = vec4(1.0f, 1.0f, 1.0f, 1.0f),
    .metallicRoughnessNormalOcclusion = vec4(1.0f, 1.0f, 1.0f, 1.0f),
    .emissiveFactorAlphaCutoff        = vec4(0.0f, 0.0f, 0.0f, 0.5f),
    .occlusionTexture                 = mat.occlusionTexture.index(),
    .emissiveTexture                  = mat.emissiveTexture.index(),
    .baseColorTexture                 = mat.baseColorTexture.index(),
    .metallicRoughnessTexture         = mat.surfacePropertiesTexture.index(),
    .normalTexture                    = mat.normalTexture.index(),
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

  ai_real metallicFactor;
  if (mtlDescriptor->Get(AI_MATKEY_METALLIC_FACTOR, metallicFactor) == AI_SUCCESS) {
    res.metallicRoughnessNormalOcclusion.x = metallicFactor;
  }
  assignUVandSampler(samplers, mtlDescriptor, aiTextureType_METALNESS, res.metallicRoughnessTextureUV, res.metallicRoughnessTextureSampler);

  ai_real roughnessFactor;
  if (mtlDescriptor->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughnessFactor) == AI_SUCCESS) {
    res.metallicRoughnessNormalOcclusion.y = roughnessFactor;
  }

  ai_real normalScale;
  if (mtlDescriptor->Get(AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_NORMALS, 0), normalScale) == AI_SUCCESS) {
    res.metallicRoughnessNormalOcclusion.z = normalScale;
  }
  assignUVandSampler(samplers, mtlDescriptor, aiTextureType_NORMALS, res.normalTextureUV, res.normalTextureSampler);

  ai_real occlusionStrength;
  if (mtlDescriptor->Get(AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_AMBIENT_OCCLUSION, 0), occlusionStrength) == AI_SUCCESS) {
    res.metallicRoughnessNormalOcclusion.w = occlusionStrength;
  }
  assignUVandSampler(samplers, mtlDescriptor, aiTextureType_LIGHTMAP, res.occlusionTextureUV, res.occlusionTextureSampler);

  aiString alphaMode = aiString("OPAQUE");
  if (mtlDescriptor->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS) {
    if (alphaMode == aiString("MASK")) {
      res.alphaMode = MetallicRoughnessDataGPU::AlphaMode_Mask;
    } else if (alphaMode == aiString("BLEND")) {
      res.alphaMode = MetallicRoughnessDataGPU::AlphaMode_Blend;
    } else {
      res.alphaMode = MetallicRoughnessDataGPU::AlphaMode_Opaque;
    }
  }

  ai_real alphaCutoff;
  if (mtlDescriptor->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff) == AI_SUCCESS) {
    res.emissiveFactorAlphaCutoff.w = alphaCutoff;
  }

  return res;
}

int main()
{
  VulkanApp app;

  auto& ctx = app.ctx_;

  {
    const aiScene* scene = aiImportFile("deps/src/glTF-Sample-Assets/Models/DamagedHelmet/glTF/DamagedHelmet.gltf", aiProcess_Triangulate);

    if (!scene || !scene->HasMeshes()) {
      printf("Unable to load data/rubber_duck/scene.gltf\n");
      exit(255);
    }

    const aiMesh* mesh = scene->mMeshes[0];

    std::vector<Vertex> vertices;
    for (uint32_t i = 0; i != mesh->mNumVertices; i++) {
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
    }

    std::vector<uint32_t> indices;
    for (unsigned int i = 0; i != mesh->mNumFaces; i++) {
      for (int j = 0; j != 3; j++) {
        indices.push_back(mesh->mFaces[i].mIndices[j]);
      }
    }

    lvk::Holder<lvk::BufferHandle> vertexBuffer = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Vertex,
          .storage   = lvk::StorageType_Device,
          .size      = sizeof(Vertex) * vertices.size(),
          .data      = vertices.data(),
          .debugName = "Buffer: vertex" });
    lvk::Holder<lvk::BufferHandle> indexBuffer = ctx->createBuffer(
        { .usage     = lvk::BufferUsageBits_Index,
          .storage   = lvk::StorageType_Device,
          .size      = sizeof(uint32_t) * indices.size(),
          .data      = indices.data(),
          .debugName = "Buffer: index" });

    GLTFMaterialTextures mat = loadMaterialTextures(
        ctx, "deps/src/glTF-Sample-Assets/Models/DamagedHelmet/glTF/Default_AO.jpg",
        "deps/src/glTF-Sample-Assets/Models/DamagedHelmet/glTF/Default_emissive.jpg",
        "deps/src/glTF-Sample-Assets/Models/DamagedHelmet/glTF/Default_albedo.jpg",
        "deps/src/glTF-Sample-Assets/Models/DamagedHelmet/glTF/Default_metalRoughness.jpg",
        "deps/src/glTF-Sample-Assets/Models/DamagedHelmet/glTF/Default_normal.jpg");
    if (!mat.wasLoaded) {
      exit(255);
    }

    GLTFGlobalSamplers samplers(ctx);
    EnvironmentMapTextures envMapTextures(ctx);

    const lvk::VertexInput vdesc = {
    .attributes    = { { .location = 0, .format = lvk::VertexFormat::Float3, .offset = 0  },
							  { .location = 1, .format = lvk::VertexFormat::Float3, .offset = 12 },
                       { .location = 2, .format = lvk::VertexFormat::Float4, .offset = 24 },
							  { .location = 3, .format = lvk::VertexFormat::Float2, .offset = 40 },
							  { .location = 4, .format = lvk::VertexFormat::Float2, .offset = 48 }, },
    .inputBindings = { { .stride = sizeof(Vertex) } },
    };

    lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(ctx, "Chapter06/04_MetallicRoughness/src/main.vert");
    lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(ctx, "Chapter06/04_MetallicRoughness/src/main.frag");

    lvk::Holder<lvk::RenderPipelineHandle> pipelineSolid = ctx->createRenderPipeline({
        .vertexInput = vdesc,
        .smVert      = vert,
        .smFrag      = frag,
        .color       = { { .format = ctx->getSwapchainFormat() } },
        .depthFormat = app.getDepthFormat(),
        .cullMode    = lvk::CullMode_Back,
    });

    const aiMaterial* mtlDescriptor = scene->mMaterials[mesh->mMaterialIndex];

    const MetallicRoughnessMaterialsPerFrame matPerFrame = {
      .materials = { setupMetallicRoughnessData(samplers, mat, mtlDescriptor) },
    };

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
        .debugName = "PerFrame materials",
    });

    struct PerDrawData {
      mat4 model;
      mat4 view;
      mat4 proj;
      vec4 cameraPos;
      uint32_t matId;
      uint32_t envId;
    };

    lvk::Holder<lvk::BufferHandle> drawableBuffers[2] = {
      ctx->createBuffer({
          .usage     = lvk::BufferUsageBits_Uniform,
          .storage   = lvk::StorageType_HostVisible,
          .size      = sizeof(PerDrawData),
          .debugName = "PerDraw 1",
      }),
      ctx->createBuffer({
          .usage     = lvk::BufferUsageBits_Uniform,
          .storage   = lvk::StorageType_HostVisible,
          .size      = sizeof(PerDrawData),
          .debugName = "PerDraw 2",
      }),
    };

    LVK_ASSERT(pipelineSolid.valid());

    aiReleaseImport(scene);

    const bool rotateModel = true;
    uint32_t currentBuffer = 0;

    app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
      const mat4 m1 = glm::rotate(mat4(1.0f), glm::radians(+90.0f), vec3(1, 0, 0));
      const mat4 m2 = glm::rotate(mat4(1.0f), rotateModel ? (float)glfwGetTime() : 0.0f, vec3(0.0f, 1.0f, 0.0f));
      const mat4 p  = glm::perspective(45.0f, aspectRatio, 0.1f, 1000.0f);

      const PerDrawData perDrawData = {
        .model     = m2 * m1,
        .view      = app.camera_.getViewMatrix(),
        .proj      = p,
        .cameraPos = vec4(app.camera_.getPosition(), 1.0f),
        .matId     = 0,
        .envId     = 0,
      };

      ctx->upload(drawableBuffers[currentBuffer], &perDrawData, sizeof(PerDrawData));
      currentBuffer = (currentBuffer + 1) % LVK_ARRAY_NUM_ELEMENTS(drawableBuffers);

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
        {
          buf.cmdPushDebugGroupLabel("Mesh", 0xff0000ff);
          {
            buf.cmdBindVertexBuffer(0, vertexBuffer, 0);
            buf.cmdBindIndexBuffer(indexBuffer, lvk::IndexFormat_UI32);
            buf.cmdBindRenderPipeline(pipelineSolid);
            buf.cmdBindDepthState({ .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true });
            struct PerFrameData {
              uint64_t draw;
              uint64_t materials;
              uint64_t environments;
            } perFrameData = {
              .draw         = ctx->gpuAddress(drawableBuffers[currentBuffer]),
              .materials    = ctx->gpuAddress(matBuffer),
              .environments = ctx->gpuAddress(envBuffer),
            };
            buf.cmdPushConstants(perFrameData);
            buf.cmdDrawIndexed(indices.size());
          }
          buf.cmdPopDebugGroupLabel();
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
