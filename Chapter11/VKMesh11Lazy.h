#pragma once

#include "Chapter11/VKMesh11.h"

#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/for_each.hpp>

struct LoadedTextureData {
  uint32_t index      = 0;
  ktxTexture1* ktxTex = nullptr;
  lvk::TextureDesc desc;
};

LoadedTextureData loadTextureData(const char* fileName)
{
  const bool isKTX = endsWith(fileName, ".ktx") || endsWith(fileName, ".KTX");

  if (!isKTX) {
    printf("Unable to load not-KTX file %s\n", fileName);
    return {};
  }

  ktxTexture1* ktxTex = nullptr;

  if (!LVK_VERIFY(ktxTexture1_CreateFromNamedFile(fileName, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTex) == KTX_SUCCESS)) {
    LLOGW("Failed to load %s\n", fileName);
    assert(0);
    return {};
  }

  const lvk::Format format = [](uint32_t glInternalFormat) {
    switch (glInternalFormat) {
    case GL_COMPRESSED_RGBA_BPTC_UNORM:
      return lvk::Format_BC7_RGBA;
    case GL_RGBA8:
      return lvk::Format_RGBA_UN8;
    case GL_RG16F:
      return lvk::Format_RG_F16;
    case GL_RGBA16F:
      return lvk::Format_RGBA_F16;
    case GL_RGBA32F:
      return lvk::Format_RGBA_F32;
    default:
      LLOGW("Unsupported pixel format (%u)\n", glInternalFormat);
      assert(0);
    }
    return lvk::Format_Invalid;
  }(ktxTex->glInternalformat);

  return LoadedTextureData{
    .ktxTex = ktxTex,
    .desc   = {.type             = lvk::TextureType_2D,
               .format           = format,
               .dimensions       = { ktxTex->baseWidth, ktxTex->baseHeight, 1 },
               .usage            = lvk::TextureUsageBits_Sampled,
               .numMipLevels     = ktxTex->numLevels,
               .data             = ktxTex->pData,
               .dataNumMipLevels = ktxTex->numLevels,
               .debugName        = fileName}
  };
}

GLTFMaterialDataGPU convertToGPUMaterialLazy(
    const std::unique_ptr<lvk::IContext>& ctx, const Material& mat, const TextureFiles& files, TextureCache& cache,
    std::vector<LoadedTextureData>& loadedTextureData, std::mutex& loadingMutex)
{
  LVK_PROFILER_FUNCTION();

  GLTFMaterialDataGPU result = {
    .baseColorFactor                  = mat.baseColorFactor,
    .metallicRoughnessNormalOcclusion = vec4(mat.metallicFactor, mat.roughness, 1.0f, 1.0f),
    .clearcoatTransmissionThickness   = vec4(1.0f, 1.0f, mat.transparencyFactor, 1.0f),
    .emissiveFactorAlphaCutoff        = vec4(vec3(mat.emissiveFactor), mat.alphaTest),
  };

  auto startLoadingTexture = [&cache, &ctx, &files, &loadedTextureData, &loadingMutex](int textureId) {
    if (textureId == -1)
      return;

    if (cache.size() <= textureId) {
      cache.resize(textureId + 1);
    }
    // not in the cache and not in the queue
    const bool notInCache = cache[textureId].empty();
    const bool notInQueue = std::find_if(loadedTextureData.cbegin(), loadedTextureData.cend(), [textureId](const LoadedTextureData& d) {
                              return d.index == textureId;
                            }) == loadedTextureData.end();
    if (notInCache && notInQueue) {
      loadedTextureData.push_back(loadTextureData(files[textureId].c_str()));
      loadedTextureData.back().index = textureId;
    }
  };

  std::lock_guard lock(loadingMutex);

  startLoadingTexture(mat.baseColorTexture);
  startLoadingTexture(mat.emissiveTexture);
  startLoadingTexture(mat.normalTexture);
  startLoadingTexture(mat.opacityTexture);

  return result;
}

class VKMesh11Lazy final : public VKMesh11
{
public:
  VKMesh11Lazy(
      const std::unique_ptr<lvk::IContext>& ctx, const MeshData& meshData, const Scene& scene,
      lvk::StorageType indirectBufferStorage = lvk::StorageType_Device)
  : VKMesh11(ctx, meshData, scene, indirectBufferStorage, false)
  {
    materialsGPU_.resize(materialsCPU_.size());

    // construct Taskflow
    taskflow_.for_each_index(0u, static_cast<uint32_t>(materialsCPU_.size()), 1u, [&](int i) {
      materialsGPU_[i] = convertToGPUMaterialLazy(ctx, materialsCPU_[i], textureFiles_, textureCache_, loadedTextureData_, loadingMutex_);
    });

    // start loading
    executor_.run(taskflow_);
  }

  bool processLoadedTextures()
  {
    LVK_PROFILER_FUNCTION();

    // process only 1 texture to prevent stuttering
    LoadedTextureData tex;

    {
      std::lock_guard lock(loadingMutex_);

      if (loadedTextureData_.empty()) {
        return false;
      };

      tex = loadedTextureData_.back();
      loadedTextureData_.pop_back();
    }

    lvk::Holder<lvk::TextureHandle> texture = ctx->createTexture(tex.desc);

    ktxTexture_Destroy(ktxTexture(tex.ktxTex));

    {
      std::lock_guard lock(loadingMutex_);

      textureCache_[tex.index] = std::move(texture);

      auto getTextureFromCache = [this](int textureId) -> uint32_t {
        return textureCache_.size() > textureId ? textureCache_[textureId].index() : 0;
      };

      LVK_ASSERT(materialsCPU_.size() == materialsGPU_.size());

      // go through the texture cache and update materials
      for (size_t i = 0; i != materialsCPU_.size(); i++) {
        const Material& mtl = materialsCPU_[i];

        GLTFMaterialDataGPU& m = materialsGPU_[i];

        m.baseColorTexture    = getTextureFromCache(mtl.baseColorTexture);
        m.emissiveTexture     = getTextureFromCache(mtl.emissiveTexture);
        m.normalTexture       = getTextureFromCache(mtl.normalTexture);
        m.transmissionTexture = getTextureFromCache(mtl.opacityTexture);
      }
    }

    ctx->upload(bufferMaterials_, materialsGPU_.data(), materialsGPU_.size() * sizeof(decltype(materialsGPU_)::value_type));

    return true;
  }

public:
  // multithreading
  std::mutex loadingMutex_;
  std::vector<LoadedTextureData> loadedTextureData_;

  tf::Taskflow taskflow_;
  tf::Executor executor_{ size_t(2) };
};
