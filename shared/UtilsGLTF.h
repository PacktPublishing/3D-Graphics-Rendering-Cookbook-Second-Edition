#pragma once
#include "VulkanApp.h"

#include <assimp/GltfMaterial.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>

#include <lvk/LVK.h>

constexpr uint32_t Unlit              = 0xF;
constexpr uint32_t MetallicRoughness  = 0x1;
constexpr uint32_t SpecularGlossiness = 0x2;
constexpr uint32_t Sheen              = 0x4;
constexpr uint32_t ClearCoat          = 0x8;
constexpr uint32_t Specular          = 0x10;

const uint32_t kMaxMaterials    = 128;
const uint32_t kMaxEnvironments = 4;

using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

inline glm::mat4 AiMatrix4x4ToGlm(const aiMatrix4x4* from)
{
  glm::mat4 to;

  to[0][0] = (float)from->a1;
  to[0][1] = (float)from->b1;
  to[0][2] = (float)from->c1;
  to[0][3] = (float)from->d1;
  to[1][0] = (float)from->a2;
  to[1][1] = (float)from->b2;
  to[1][2] = (float)from->c2;
  to[1][3] = (float)from->d2;
  to[2][0] = (float)from->a3;
  to[2][1] = (float)from->b3;
  to[2][2] = (float)from->c3;
  to[2][3] = (float)from->d3;
  to[3][0] = (float)from->a4;
  to[3][1] = (float)from->b4;
  to[3][2] = (float)from->c4;
  to[3][3] = (float)from->d4;

  return to;
}

struct EnvironmentMapData {
  uint32_t envMapTexture                  = 0;
  uint32_t envMapTextureSampler           = 0;
  uint32_t envMapTextureIrradiance        = 0;
  uint32_t envMapTextureIrradianceSampler = 0;

  uint32_t lutBRDFTexture              = 0;
  uint32_t lutBRDFTextureSampler       = 0;
  uint32_t envMapTextureCharlie        = 0;
  uint32_t envMapTextureCharlieSampler = 0;
};

struct glTFGlobalSamplers {
  glTFGlobalSamplers(const std::unique_ptr<lvk::IContext>& ctx)
  {
    clamp = ctx->createSampler({
        .minFilter = lvk::SamplerFilter::SamplerFilter_Linear,
        .magFilter = lvk::SamplerFilter::SamplerFilter_Linear,
        .mipMap    = lvk::SamplerMip::SamplerMip_Linear,
        .wrapU     = lvk::SamplerWrap::SamplerWrap_Clamp,
        .wrapV     = lvk::SamplerWrap::SamplerWrap_Clamp,
        .wrapW     = lvk::SamplerWrap::SamplerWrap_Clamp,
        .debugName = "Clamp Sampler",
    });

    wrap = ctx->createSampler({
        .minFilter = lvk::SamplerFilter::SamplerFilter_Linear,
        .magFilter = lvk::SamplerFilter::SamplerFilter_Linear,
        .mipMap    = lvk::SamplerMip::SamplerMip_Linear,
        .wrapU     = lvk::SamplerWrap::SamplerWrap_Repeat,
        .wrapV     = lvk::SamplerWrap::SamplerWrap_Repeat,
        .wrapW     = lvk::SamplerWrap::SamplerWrap_Repeat,
        .debugName = "Wrap Sampler",
    });

    mirror = ctx->createSampler({
        .minFilter = lvk::SamplerFilter::SamplerFilter_Linear,
        .magFilter = lvk::SamplerFilter::SamplerFilter_Linear,
        .mipMap    = lvk::SamplerMip::SamplerMip_Linear,
        .wrapU     = lvk::SamplerWrap::SamplerWrap_MirrorRepeat,
        .wrapV     = lvk::SamplerWrap::SamplerWrap_MirrorRepeat,
        .debugName = "Mirror Sampler",
    });
  }

  ~glTFGlobalSamplers()
  {
    clamp.reset();
    wrap.reset();
    mirror.reset();
  }

  lvk::Holder<lvk::SamplerHandle> clamp;
  lvk::Holder<lvk::SamplerHandle> wrap;
  lvk::Holder<lvk::SamplerHandle> mirror;
};

struct EnvironmentsPerFrame {
  EnvironmentMapData environments[kMaxEnvironments];
};

struct vertex {
  vec3 position;
  vec3 normal;
  vec4 color;
  vec2 uv0;
  vec2 uv1;
};

enum class PrimitiveType : uint32_t {
  ePoint,
  eLine,
  eTriangle
};

struct glTFMaterialTextures {
  ~glTFMaterialTextures()
  {
    baseColorTexture.reset();
    normalTexture.reset();
    occlusionTexture.reset();
    emissiveTexture.reset();
    surfacePropertiesTexture.reset();
    sheenColorTexture.reset();
    sheenRoughnessTexture.reset();
    clearCoatTexture.reset();
    clearCoatRoughnessTexture.reset();
    clearCoatNormalTexture.reset();
    specularColorTexture.reset();
    specularTexture.reset();
    transmissionTexture.reset();
    iridescenceTexture.reset();
	 iridescenceThicknessTexture.reset();
    white.reset();
  }

  // Metallic Roughness / SpecluarGlossiness
  lvk::Holder<lvk::TextureHandle> baseColorTexture;
  lvk::Holder<lvk::TextureHandle> surfacePropertiesTexture;

  // Common properties
  lvk::Holder<lvk::TextureHandle> normalTexture;
  lvk::Holder<lvk::TextureHandle> occlusionTexture;
  lvk::Holder<lvk::TextureHandle> emissiveTexture;

  // Sheen
  lvk::Holder<lvk::TextureHandle> sheenColorTexture;
  lvk::Holder<lvk::TextureHandle> sheenRoughnessTexture;

  // Clearcoat
  lvk::Holder<lvk::TextureHandle> clearCoatTexture;
  lvk::Holder<lvk::TextureHandle> clearCoatRoughnessTexture;
  lvk::Holder<lvk::TextureHandle> clearCoatNormalTexture;

  // Specular
  lvk::Holder<lvk::TextureHandle> specularTexture;
  lvk::Holder<lvk::TextureHandle> specularColorTexture;

  // Transmission
  lvk::Holder<lvk::TextureHandle> transmissionTexture;

  // Volumen
  lvk::Holder<lvk::TextureHandle> thicknessTexture;

  // Iridescence
  lvk::Holder<lvk::TextureHandle> iridescenceTexture;
  lvk::Holder<lvk::TextureHandle> iridescenceThicknessTexture;

  // Anisotropy
  lvk::Holder<lvk::TextureHandle> anisotropyTexture;

  lvk::Holder<lvk::TextureHandle> white;
};

struct EnvironmentMapTextures {
  explicit EnvironmentMapTextures(const std::unique_ptr<lvk::IContext>& ctx)
  : EnvironmentMapTextures(
        ctx, "data/brdfLUT.ktx", "data/piazza_bologni_1k_prefilter.ktx", "data/piazza_bologni_1k_irradiance.ktx",
        "data/piazza_bologni_1k_charlie.ktx")
  {
  }

  EnvironmentMapTextures(
      const std::unique_ptr<lvk::IContext>& ctx, const char* brdfLUT, const char* prefilter, const char* irradiance,
      const char* prefilterCharlie = nullptr)
  {
    texBRDF_LUT = loadTexture(ctx, brdfLUT, lvk::TextureType_2D);
    if (texBRDF_LUT.empty()) {
      assert(0);
      exit(255);
    }

    envMapTexture = loadTexture(ctx, prefilter, lvk::TextureType_Cube);
    if (envMapTexture.empty()) {
      assert(0);
      exit(255);
    }

    envMapTextureIrradiance = loadTexture(ctx, irradiance, lvk::TextureType_Cube);
    if (envMapTextureIrradiance.empty()) {
      assert(0);
      exit(255);
    }

    if (prefilterCharlie) {
      envMapTextureCharlie = loadTexture(ctx, prefilterCharlie, lvk::TextureType_Cube);
      if (envMapTextureCharlie.empty()) {
        assert(0);
        exit(255);
      }
    }
  }

  ~EnvironmentMapTextures()
  {
    texBRDF_LUT.reset();
    envMapTexture.reset();
    envMapTextureCharlie.reset();
    envMapTextureIrradiance.reset();
  }

  lvk::Holder<lvk::TextureHandle> texBRDF_LUT;
  lvk::Holder<lvk::TextureHandle> envMapTexture;
  lvk::Holder<lvk::TextureHandle> envMapTextureCharlie;
  lvk::Holder<lvk::TextureHandle> envMapTextureIrradiance;
};

bool assignUVandSampler(
    const glTFGlobalSamplers& samplers, aiMaterial* const& mtlDescriptor, aiTextureType textureType, uint32_t& uvIndex,
    uint32_t& textureSampler, int index = 0);

struct glTFMaterialData {
  vec4 baseColorFactor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
  vec4 metallicRoughnessNormalOcclusion = vec4(1.0f, 1.0f, 1.0f, 1.0f); // Packed metallicFactor, roughnessFactor, normalScale, occlusionStrength
  vec4 specularGlossiness               = vec4(1.0f, 1.0f, 1.0f, 1.0f);
  vec4 sheenFactors                     = vec4(1.0f, 1.0f, 1.0f, 1.0f); // Sheen

  vec4 clearcoatTransmissionTickness = vec4(1.0f, 1.0f, 1.0f, 1.0f); // Clearcoat
  vec4 specularFactors = vec4(1.0f, 1.0f, 1.0f, 1.0f);
  vec4 attenuation     = vec4(1.0f, 1.0f, 1.0f, 1.0f);
  vec4 iridescence = vec4(1.0f, 1.0f, 1.0f, 1.0f);

  // Emissive Strength
  //vec4 emissiveStrengthIor = vec4(1.0, 1.0, 1.0, 1.0);
  // IOR
  // float ior;
  // Anisotropy
  vec4 anisotropy = vec4(1.0, 1.0, 1.0, 1.0);
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
  uint32_t sheenColorTexture    = 0;

  uint32_t sheenColorTextureSampler     = 0;
  uint32_t sheenColorTextureUV          = 0;
  uint32_t sheenRoughnessTexture        = 0;
  uint32_t sheenRoughnessTextureSampler = 0;

  uint32_t sheenRoughnessTextureUV = 0;
  uint32_t clearCoatTexture       = 0;
  uint32_t clearCoatTextureSampler = 0;
  uint32_t clearCoatTextureUV      = 0;

  uint32_t clearCoatRoughnessTexture = 0;
  uint32_t clearCoatRoughnessTextureSampler = 0;
  uint32_t clearCoatRoughnessTextureUV      = 0;
  uint32_t clearCoatNormalTexture  = 0;

  uint32_t clearCoatNormalTextureSampler = 0;
  uint32_t clearCoatNormalTextureUV      = 0;
  uint32_t specularTexture             = 0;
  uint32_t specularTextureSampler        = 0;

  uint32_t specularTextureUV        = 0;
  uint32_t specularColorTexture     = 0;
  uint32_t specularColorTextureSampler = 0;
  uint32_t specularColorTextureUV      = 0;

  uint32_t transmissionTexture        = 0;
  uint32_t transmissionTextureSampler = 0;
  uint32_t transmissionTextureUV      = 0;
  uint32_t thicknessTexture           = 0;

  uint32_t thicknessTextureSampler = 0;
  uint32_t thicknessTextureUV      = 0;
  uint32_t iridescenceTexture      = 0;
  uint32_t iridescenceTextureSampler = 0;

  uint32_t iridescenceTextureUV = 0;
  uint32_t iridescenceThicknessTexture = 0;
  uint32_t iridescenceThicknessTextureSampler = 0;
  uint32_t iridescenceThicknessTextureUV      = 0;

  uint32_t anisotropyTexture                   = 0;
  uint32_t anisotropyTextureSampler      = 0;
  uint32_t anisotropyTextureUV                 = 0;
  uint32_t alphaMode               = 0;

  uint32_t materialType            = 0;
  uint32_t padding[3]    = { 0,0,0 };

  enum class AlphaMode {
    eOpaque = 0,
    eMask   = 1,
    eBlend  = 2,
  };
};

struct glTFDataHolder {
  std::vector<std::unique_ptr<glTFMaterialTextures>> textures;
};

struct MaterialsPerFrame {
  glTFMaterialData materials[kMaxMaterials];
};

using glTFNodeRef = uint32_t;
using glTFMeshRef = uint32_t;

struct glTFMesh {
  PrimitiveType primitive;
  uint32_t vertexOffset;
  uint32_t vertexCount;
  uint32_t indexOffset;
  uint32_t indexCount;
  uint32_t matIdx;
  uint32_t opaque;
};


struct glTFNode {
  std::string name;
  glm::mat4 transform;
  std::vector<glTFNodeRef> children;
  std::vector<glTFMeshRef> meshes;
};

struct glTFFrameData {
  mat4 view;
  mat4 proj;
  vec4 cameraPos;
};

struct glTFTransforms {
  mat4 model;
  uint32_t matId;
  glTFNodeRef nodeRef;  // for cpu only
  glTFMeshRef meshRef; // for cpu only
  uint32_t opaque;
};


glTFMaterialData setupglTFMaterialData(
    const std::unique_ptr<lvk::IContext>& ctx, const glTFGlobalSamplers& samplers, aiMaterial* const& mtlDescriptor,
    const char* assetFolder, glTFDataHolder& glTFDataholder);

struct glTFContext {
  glTFContext(const VulkanApp& app_)
  : app(app_)
  , samplers(app_.ctx_)
  , envMapTextures(app_.ctx_)

  {
  }
  ~glTFContext()
  {
    for (auto& tex : glTFDataholder.textures) {
      tex.reset();
    }
    envBuffer.reset();
    perFrameBuffer.reset();
    transformBuffer.reset();
    pipelineSolid.reset();
    pipelineTransparent.reset();
    vert.reset();
    frag.reset();
    vertexBuffer.reset();
    indexBuffer.reset();
    matBuffer.reset();
  }

  glTFDataHolder glTFDataholder;
  MaterialsPerFrame matPerFrame;
  glTFGlobalSamplers samplers;
  EnvironmentMapTextures envMapTextures;
  glTFFrameData frameData;
  std::vector<glTFTransforms> transforms;

  std::vector<glTFNode> nodesStorage;
  std::vector<glTFMesh> meshesStorage;

  std::vector<uint32_t> opaqueNodes;
  std::vector<uint32_t> transparentNodes;

  lvk::Holder<lvk::BufferHandle> envBuffer;
  lvk::Holder<lvk::BufferHandle> perFrameBuffer;
  lvk::Holder<lvk::BufferHandle> transformBuffer;
  lvk::Holder<lvk::RenderPipelineHandle> pipelineSolid;
  lvk::Holder<lvk::RenderPipelineHandle> pipelineTransparent;
  lvk::Holder<lvk::ShaderModuleHandle> vert;
  lvk::Holder<lvk::ShaderModuleHandle> frag;
  lvk::Holder<lvk::BufferHandle> vertexBuffer;
  lvk::Holder<lvk::BufferHandle> indexBuffer;
  lvk::Holder<lvk::BufferHandle> matBuffer;

  const lvk::DepthState dState = { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true };

  glTFNodeRef root;
  const VulkanApp& app;
};

void loadglTF(glTFContext& context, const char* gltfName, const char* glTFDataPath);
void renderglTF(glTFContext& context, const mat4& m, const mat4& v, const mat4& p, bool rebuildRenderList = false);
