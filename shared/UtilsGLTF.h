#pragma once

#include "VulkanApp.h"

#include <assimp/GltfMaterial.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>

#include "LineCanvas.h"
#include "UtilsAnim.h"

#include <lvk/LVK.h>

enum MaterialType : uint32_t {
  MaterialType_Invalid            = 0,
  MaterialType_Unlit              = 0x80,
  MaterialType_MetallicRoughness  = 0x1,
  MaterialType_SpecularGlossiness = 0x2,
  MaterialType_Sheen              = 0x4,
  MaterialType_ClearCoat          = 0x8,
  MaterialType_Specular           = 0x10,
  MaterialType_Transmission       = 0x20,
  MaterialType_Volume             = 0x40,
};

const uint32_t kMaxMaterials    = 128;
const uint32_t kMaxEnvironments = 4;
const uint32_t kMaxLights       = 8;

using glm::mat4;
using glm::quat;
using glm::vec2;
using glm::vec3;
using glm::vec4;

inline mat4 aiMatrix4x4ToMat4(const aiMatrix4x4& from)
{
  mat4 to;

  to[0][0] = (float)from.a1;
  to[0][1] = (float)from.b1;
  to[0][2] = (float)from.c1;
  to[0][3] = (float)from.d1;
  to[1][0] = (float)from.a2;
  to[1][1] = (float)from.b2;
  to[1][2] = (float)from.c2;
  to[1][3] = (float)from.d2;
  to[2][0] = (float)from.a3;
  to[2][1] = (float)from.b3;
  to[2][2] = (float)from.c3;
  to[2][3] = (float)from.d3;
  to[3][0] = (float)from.a4;
  to[3][1] = (float)from.b4;
  to[3][2] = (float)from.c4;
  to[3][3] = (float)from.d4;

  return to;
}

inline vec3 aiVector3DToVec3(const aiVector3D& from)
{
  return vec3(from.x, from.y, from.z);
}

inline glm::quat aiQuaternionToQuat(const aiQuaternion& from)
{
  return glm::quat(from.w, from.x, from.y, from.z);
}

enum LightType : uint32_t {
  LightType_Directional = 0,
  LightType_Point       = 1,
  LightType_Spot        = 2,
};

struct EnvironmentMapDataGPU {
  uint32_t envMapTexture                  = 0;
  uint32_t envMapTextureSampler           = 0;
  uint32_t envMapTextureIrradiance        = 0;
  uint32_t envMapTextureIrradianceSampler = 0;

  uint32_t lutBRDFTexture        = 0;
  uint32_t lutBRDFTextureSampler = 0;

  uint32_t envMapTextureCharlie        = 0;
  uint32_t envMapTextureCharlieSampler = 0;
};

struct LightDataGPU {
  vec3 direction = vec3(0, 0, 1);
  float range    = 10000.0;

  vec3 color      = vec3(1, 1, 1);
  float intensity = 1.0;

  vec3 position = vec3(0, 0, -5);

  float innerConeCos = 0.0;
  float outerConeCos = 0.78;

  LightType type = LightType_Directional;

  int nodeId  = ~0;
  int padding = 0;
};

struct GLTFGlobalSamplers {
  GLTFGlobalSamplers(const std::unique_ptr<lvk::IContext>& ctx)
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

  lvk::Holder<lvk::SamplerHandle> clamp;
  lvk::Holder<lvk::SamplerHandle> wrap;
  lvk::Holder<lvk::SamplerHandle> mirror;
};

struct EnvironmentsPerFrame {
  EnvironmentMapDataGPU environments[kMaxEnvironments];
};

struct Vertex {
  vec3 position;
  vec3 normal;
  vec4 color;
  vec2 uv0;
  vec2 uv1;
  float padding[2];
};

struct MorphTarget {
  uint32_t meshId = ~0;
  std::vector<uint32_t> offset;
};

static_assert(sizeof(Vertex) == sizeof(uint32_t) * 16);

struct GLTFMaterialTextures {
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

  bool wasLoaded = false;
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

  lvk::Holder<lvk::TextureHandle> texBRDF_LUT;
  lvk::Holder<lvk::TextureHandle> envMapTexture;
  lvk::Holder<lvk::TextureHandle> envMapTextureCharlie;
  lvk::Holder<lvk::TextureHandle> envMapTextureIrradiance;
};

bool assignUVandSampler(
    const GLTFGlobalSamplers& samplers, const aiMaterial* mtlDescriptor, aiTextureType textureType, uint32_t& uvIndex,
    uint32_t& textureSampler, int index = 0);

struct GLTFMaterialDataGPU {
  vec4 baseColorFactor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
  vec4 metallicRoughnessNormalOcclusion =
      vec4(1.0f, 1.0f, 1.0f, 1.0f); // Packed metallicFactor, roughnessFactor, normalScale, occlusionStrength
  vec4 specularGlossiness = vec4(1.0f, 1.0f, 1.0f, 1.0f);
  vec4 sheenFactors       = vec4(1.0f, 1.0f, 1.0f, 1.0f); // Sheen

  vec4 clearcoatTransmissionThickness = vec4(1.0f, 1.0f, 1.0f, 1.0f); // Clearcoat
  vec4 specularFactors                = vec4(1.0f, 1.0f, 1.0f, 1.0f);
  vec4 attenuation                    = vec4(1.0f, 1.0f, 1.0f, 1.0f);

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

  uint32_t surfacePropertiesTexture        = 0;
  uint32_t surfacePropertiesTextureSampler = 0;
  uint32_t surfacePropertiesTextureUV      = 0;

  uint32_t normalTexture        = 0;
  uint32_t normalTextureSampler = 0;
  uint32_t normalTextureUV      = 0;

  uint32_t sheenColorTexture            = 0;
  uint32_t sheenColorTextureSampler     = 0;
  uint32_t sheenColorTextureUV          = 0;
  uint32_t sheenRoughnessTexture        = 0;
  uint32_t sheenRoughnessTextureSampler = 0;

  uint32_t sheenRoughnessTextureUV = 0;
  uint32_t clearCoatTexture        = 0;
  uint32_t clearCoatTextureSampler = 0;
  uint32_t clearCoatTextureUV      = 0;

  uint32_t clearCoatRoughnessTexture        = 0;
  uint32_t clearCoatRoughnessTextureSampler = 0;
  uint32_t clearCoatRoughnessTextureUV      = 0;
  uint32_t clearCoatNormalTexture           = 0;

  uint32_t clearCoatNormalTextureSampler = 0;
  uint32_t clearCoatNormalTextureUV      = 0;
  uint32_t specularTexture               = 0;
  uint32_t specularTextureSampler        = 0;

  uint32_t specularTextureUV           = 0;
  uint32_t specularColorTexture        = 0;
  uint32_t specularColorTextureSampler = 0;
  uint32_t specularColorTextureUV      = 0;

  uint32_t transmissionTexture        = 0;
  uint32_t transmissionTextureSampler = 0;
  uint32_t transmissionTextureUV      = 0;
  uint32_t thicknessTexture           = 0;

  uint32_t thicknessTextureSampler   = 0;
  uint32_t thicknessTextureUV        = 0;
  uint32_t iridescenceTexture        = 0;
  uint32_t iridescenceTextureSampler = 0;

  uint32_t iridescenceTextureUV               = 0;
  uint32_t iridescenceThicknessTexture        = 0;
  uint32_t iridescenceThicknessTextureSampler = 0;
  uint32_t iridescenceThicknessTextureUV      = 0;

  uint32_t anisotropyTexture        = 0;
  uint32_t anisotropyTextureSampler = 0;
  uint32_t anisotropyTextureUV      = 0;
  uint32_t alphaMode                = 0;

  uint32_t materialTypeFlags = 0;
  float ior                  = 1.5f;
  uint32_t padding[2]        = { 0, 0 };

  enum AlphaMode : uint32_t {
    AlphaMode_Opaque = 0,
    AlphaMode_Mask   = 1,
    AlphaMode_Blend  = 2,
  };
};

static_assert(sizeof(GLTFMaterialDataGPU) % 16 == 0);

struct GLTFDataHolder {
  std::vector<GLTFMaterialTextures> textures;
};

struct MaterialsPerFrame {
  GLTFMaterialDataGPU materials[kMaxMaterials];
};

using GLTFNodeRef = uint32_t;
using GLTFMeshRef = uint32_t;

enum SortingType : uint32_t {
  SortingType_Opaque       = 0,
  SortingType_Transmission = 1,
  SortingType_Transparent  = 2,
};

struct GLTFMesh {
  lvk::Topology primitive;
  uint32_t vertexOffset;
  uint32_t vertexCount;
  uint32_t indexOffset;
  uint32_t indexCount;
  uint32_t matIdx;
  SortingType sortingType;
};

struct GLTFNode {
  std::string name;
  uint32_t modelMtxId;
  mat4 transform = mat4(1);
  std::vector<GLTFNodeRef> children;
  std::vector<GLTFMeshRef> meshes;
};

struct GLTFFrameData {
  mat4 model;
  mat4 view;
  mat4 proj;
  vec4 cameraPos;
};

struct GLTFCamera {
  std::string name;
  uint32_t nodeIdx = ~0;
  vec3 pos;
  vec3 up;
  vec3 lookAt;
  float hFOV;
  float near;
  float far;
  float aspect = 1.0f;
  float orthoWidth;

  mat4 getProjection(float windowAspect = 1.0f) const
  {
    return orthoWidth != 0.0f
               ? glm::ortho(-windowAspect / orthoWidth, windowAspect / orthoWidth, -1.0f / orthoWidth, 1.0f / orthoWidth, far, near)
               : glm::perspective(hFOV, windowAspect == 1.0f ? aspect : windowAspect, near, far);
  }
};

struct GLTFTransforms {
  uint32_t modelMtxId;
  uint32_t matId;
  GLTFNodeRef nodeRef; // for CPU only
  GLTFMeshRef meshRef; // for CPU only
  uint32_t sortingType;
};

// Skeleton, animation, morphing
#define MAX_BONES_PER_VERTEX 8

struct VertexBoneData {
  vec4 position;
  vec4 normal;
  uint32_t boneId[MAX_BONES_PER_VERTEX] = { ~0u, ~0u, ~0u, ~0u, ~0u, ~0u, ~0u, ~0u };
  float weight[MAX_BONES_PER_VERTEX]    = {};
  uint32_t meshId                       = ~0u;
};

static_assert(sizeof(VertexBoneData) == sizeof(uint32_t) * 25);

struct GLTFBone {
  uint32_t boneId = ~0u;
  mat4 transform  = mat4(1);
};

GLTFMaterialDataGPU setupglTFMaterialData(
    const std::unique_ptr<lvk::IContext>& ctx, const GLTFGlobalSamplers& samplers, aiMaterial* const& mtlDescriptor,
    const char* assetFolder, GLTFDataHolder& glTFDataholder, bool& useVolumetric);

struct GLTFContext {
  explicit GLTFContext(VulkanApp& app_)
  : app(app_)
  , samplers(app_.ctx_)
  , envMapTextures(app_.ctx_)
  {
  }

  GLTFDataHolder glTFDataholder;
  MaterialsPerFrame matPerFrame;
  GLTFGlobalSamplers samplers;
  EnvironmentMapTextures envMapTextures;
  GLTFFrameData frameData;
  std::vector<GLTFTransforms> transforms;
  std::vector<mat4> matrices;

  std::vector<GLTFNode> nodesStorage;
  std::vector<GLTFMesh> meshesStorage;
  std::unordered_map<std::string, GLTFBone> bonesByName;

  std::vector<MorphTarget> morphTargets;
  std::unordered_map<std::string, uint32_t> meshesRemap;

  std::vector<Animation> animations;

  std::vector<uint32_t> opaqueNodes;
  std::vector<uint32_t> transmissionNodes;
  std::vector<uint32_t> transparentNodes;

  lvk::Holder<lvk::BufferHandle> envBuffer;
  lvk::Holder<lvk::BufferHandle> lightsBuffer;
  lvk::Holder<lvk::BufferHandle> perFrameBuffer;
  lvk::Holder<lvk::BufferHandle> transformBuffer;
  lvk::Holder<lvk::BufferHandle> matricesBuffer;
  lvk::Holder<lvk::BufferHandle> morphStatesBuffer;

  lvk::Holder<lvk::RenderPipelineHandle> pipelineSolid;
  lvk::Holder<lvk::RenderPipelineHandle> pipelineTransparent;
  lvk::Holder<lvk::ComputePipelineHandle> pipelineComputeAnimations;

  lvk::Holder<lvk::ShaderModuleHandle> vert;
  lvk::Holder<lvk::ShaderModuleHandle> frag;
  lvk::Holder<lvk::ShaderModuleHandle> animation;
  lvk::Holder<lvk::BufferHandle> vertexBuffer;
  lvk::Holder<lvk::BufferHandle> vertexSkinningBuffer;
  lvk::Holder<lvk::BufferHandle> vertexMorphingBuffer;
  lvk::Holder<lvk::BufferHandle> indexBuffer;
  lvk::Holder<lvk::BufferHandle> matBuffer;

  lvk::Holder<lvk::TextureHandle> offscreenTex[3] = {};

  uint32_t currentOffscreenTex = 0;
  uint32_t maxVertices         = 0;

  std::vector<MorphState> morphStates;
  std::vector<LightDataGPU> lights;
  std::vector<GLTFCamera> cameras;

  GLTFIntrospective inspector;

  GLTFNodeRef root;
  VulkanApp& app;
  LineCanvas3D canvas3d;

  bool hasBones             = false;
  bool isVolumetricMaterial = false;
  bool animated             = false;
  bool skinning             = false;
  bool morphing             = false;
  bool doublesided          = false;
  bool enableMorphing       = true;

  bool isScreenCopyRequired() const { return isVolumetricMaterial; }
};

void loadGLTF(GLTFContext& context, const char* gltfName, const char* glTFDataPath);
void renderGLTF(GLTFContext& context, const mat4& model, const mat4& view, const mat4& proj, bool rebuildRenderList = false);
void animateGLTF(GLTFContext& gltf, AnimationState& anim, float dt);
void animateBlendingGLTF(GLTFContext& gltf, AnimationState& anim1, AnimationState& anim2, float weight, float dt);
MaterialType detectMaterialType(const aiMaterial* mtl);

void printPrefix(int ofs);
void printMat4(const aiMatrix4x4& m);

std::vector<std::string> camerasGLTF(GLTFContext& context);
void updateCamera(GLTFContext& gltf, const mat4& model, mat4& view, mat4& proj, float aspectRatio);

std::vector<std::string> animationsGLTF(GLTFContext& gltf);
