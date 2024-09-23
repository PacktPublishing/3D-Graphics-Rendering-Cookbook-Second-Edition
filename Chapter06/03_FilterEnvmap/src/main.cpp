#include <lvk/LVK.h>

#include <imgui/imgui.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "shared/UtilsCubemap.h"

#include "stb_image.h"
#include "stb_image_resize2.h"

#include <shared/Utils.h>

#include <ktx.h>
#include <ktx-software/lib/vk_format.h>
#include <ktx-software/lib/gl_format.h>

#include <vector>

using glm::vec3;
using glm::vec4;

enum Distribution {
  Distribution_Lambertian = 0,
  Distribution_GGX        = 1,
  Distribution_Charlie    = 2,
};

ktxTexture1* bitmapToCube(Bitmap& bmp, bool mipmaps)
{
  if (bmp.comp_ != 3 || bmp.type_ != eBitmapType_Cube || bmp.fmt_ != eBitmapFormat_Float) {
    LLOGW("Wrong cubemap properties!\n");
    exit(255);
  }

  const int w = bmp.w_;
  const int h = bmp.h_;

  const auto mipLevels = mipmaps ? lvk::calcNumMipLevels(w, h) : 1;

  ktxTextureCreateInfo createInfo = {
    .glInternalformat = GL_RGBA32F,
    .vkFormat         = VK_FORMAT_R32G32B32A32_SFLOAT,
    .baseWidth        = static_cast<uint32_t>(w),
    .baseHeight       = static_cast<uint32_t>(h),
    .baseDepth        = 1u,
    .numDimensions    = 2u,
    .numLevels        = mipLevels,
    .numLayers        = 1u,
    .numFaces         = 6u,
    .generateMipmaps  = KTX_FALSE,
  };

  ktxTexture1* cubemap = nullptr;
  (void)LVK_VERIFY(ktxTexture1_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &cubemap) == KTX_SUCCESS);

  const int numFacePixels = w * h;

  for (size_t face = 0; face < 6; face++) {
    const vec3* src = reinterpret_cast<vec3*>(bmp.data_.data()) + face * numFacePixels;
    size_t offset   = 0;
    (void)LVK_VERIFY(ktxTexture_GetImageOffset(ktxTexture(cubemap), 0, 0, face, &offset) == KTX_SUCCESS);
    float* dst = (float*)(cubemap->pData + offset);
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        const vec4 rgba = vec4(src[x + y * w], 1.0f);
        memcpy(dst, &rgba, sizeof(rgba));
        dst += 4;
      }
    }
  }

  if (mipmaps) {
    uint32_t prevWidth  = cubemap->baseWidth;
    uint32_t prevHeight = cubemap->baseHeight;

    for (uint32_t face = 0; face < 6; face++) {
      LLOGL(".");
      for (uint32_t miplevel = 1; miplevel < cubemap->numLevels; miplevel++) {
        LLOGL(":");
        const uint32_t width  = prevWidth > 1 ? prevWidth >> 1 : 1;
        const uint32_t height = prevHeight > 1 ? prevWidth >> 1 : 1;

        size_t prevOffset = 0;
        (void)LVK_VERIFY(ktxTexture_GetImageOffset(ktxTexture(cubemap), miplevel - 1, 0, face, &prevOffset) == KTX_SUCCESS);
        size_t offset = 0;
        (void)LVK_VERIFY(ktxTexture_GetImageOffset(ktxTexture(cubemap), miplevel, 0, face, &offset) == KTX_SUCCESS);

        stbir_resize_float_linear(
            reinterpret_cast<const float*>(cubemap->pData + prevOffset), prevWidth, prevHeight, 0,
            reinterpret_cast<float*>(cubemap->pData + offset), width, height, 0, STBIR_RGBA);

        prevWidth  = width;
        prevHeight = height;
      }
      prevWidth  = cubemap->baseWidth;
      prevHeight = cubemap->baseHeight;
    }
  }

  LLOGL("\n");

  return cubemap;
}

void prefilterCubemap(
    const std::unique_ptr<lvk::IContext>& ctx, ktxTexture1* cube, const char* envPrefilteredCubemap, lvk::TextureHandle envMapCube,
    Distribution distribution, uint32_t sampler, uint32_t sampleCount)
{
  lvk::Holder<lvk::TextureHandle> prefilteredMapCube = ctx->createTexture(
      {
          .type         = lvk::TextureType_Cube,
          .format       = lvk::Format_RGBA_F32,
          .dimensions   = {cube->baseWidth, cube->baseHeight, 1},
          .usage        = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Attachment,
          .numMipLevels = (uint32_t)cube->numLevels,
          .debugName    = envPrefilteredCubemap,
  },
      envPrefilteredCubemap);

  lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(ctx, "Chapter06/03_FilterEnvmap/src/main.vert");
  lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(ctx, "Chapter06/03_FilterEnvmap/src/main.frag");

  lvk::Holder<lvk::RenderPipelineHandle> pipelineSolid = ctx->createRenderPipeline({
      .smVert   = vert,
      .smFrag   = frag,
      .color    = { { .format = ctx->getFormat(prefilteredMapCube) } },
      .cullMode = lvk::CullMode_Back,
  });

  lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();

  for (uint32_t mip = 0; mip < cube->numLevels; mip++) {
    for (uint32_t face = 0; face < 6; face++) {
      buf.cmdBeginRendering(
          { .color = { {
                .loadOp     = lvk::LoadOp_Clear,
                .layer      = (uint8_t)face,
                .level      = (uint8_t)mip,
                .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f },
            } } },
          { .color = { { .texture = prefilteredMapCube } } });
      buf.cmdPushDebugGroupLabel("PrefilterMap", 0xff0000ff);
      buf.cmdBindRenderPipeline(pipelineSolid);
      buf.cmdBindDepthState({});
      struct PerFrameData {
        uint32_t face;
        float roughness;
        uint32_t sampleCount;
        uint32_t width;
        uint32_t height;
        uint32_t envMap;
        uint32_t distribution;
        uint32_t sampler;
      } perFrameData = {
        .face         = face,
        .roughness    = (float)(mip) / (float)(cube->numLevels - 1),
        .sampleCount  = sampleCount,
        .width        = cube->baseWidth,
        .height       = cube->baseHeight,
        .envMap       = envMapCube.index(),
        .distribution = uint32_t(distribution),
        .sampler      = sampler,
      };
      buf.cmdPushConstants(perFrameData);
      buf.cmdDraw(3);
      buf.cmdPopDebugGroupLabel();
      buf.cmdEndRendering();
    }
  }

  ctx->submit(buf);

  uint32_t prevWidth  = cube->baseWidth;
  uint32_t prevHeight = cube->baseHeight;

  for (uint32_t mip = 0; mip < cube->numLevels; mip++) {
    const uint32_t width  = mip == 0 ? prevWidth : (prevWidth > 1) ? prevWidth >> 1 : 1;
    const uint32_t height = mip == 0 ? prevHeight : prevHeight > 1 ? prevWidth >> 1 : 1;

    for (uint32_t face = 0; face < 6; ++face) {
      size_t offset = 0;
      ktxTexture_GetImageOffset(ktxTexture(cube), mip, 0, face, &offset);
      ctx->download(
          prefilteredMapCube,
          {
              .dimensions   = {(uint32_t)width, (uint32_t)height, 1},
              .layer        = face,
              .numLayers    = 1, //  6,
              .mipLevel     = mip,
              .numMipLevels = 1,
      },
          cube->pData + offset);
    }

    prevWidth  = width;
    prevHeight = height;
  }

  ktxTexture_WriteToNamedFile(ktxTexture(cube), envPrefilteredCubemap);

  prefilteredMapCube.reset();
}

void process_cubemap(
    const std::unique_ptr<lvk::IContext>& ctx, const char* envPanorama, const char* envPrefilteredCubemap, const char* envIrradianceCubemap,
    const char* envCharlieCubemap)
{
  int srcW, srcH;
  float* pxs = stbi_loadf(envPanorama, &srcW, &srcH, nullptr, 3);
  if (!pxs) {
    printf("Unable to load %s\n", envPanorama);
    exit(255);
  }

  Bitmap bmp                = convertEquirectangularMapToCubeMapFaces(Bitmap(srcW, srcH, 3, eBitmapFormat_Float, pxs));
  ktxTexture1* specularCube = bitmapToCube(bmp, true);

  lvk::Result result;

  auto envMapCube = ctx->createTexture(
      {
          .type             = lvk::TextureType_Cube,
          .format           = lvk::Format_RGBA_F32,
          .dimensions       = {specularCube->baseWidth, specularCube->baseHeight, 1},
          .usage            = lvk::TextureUsageBits_Sampled,
          .numMipLevels     = specularCube->numLevels,
          .data             = specularCube->pData,
          .dataNumMipLevels = specularCube->numLevels,
          .debugName        = envPanorama,
  },
      envPanorama, &result);

  lvk::Holder<lvk::SamplerHandle> clamp = ctx->createSampler({
      .minFilter = lvk::SamplerFilter::SamplerFilter_Linear,
      .magFilter = lvk::SamplerFilter::SamplerFilter_Linear,
      .mipMap    = lvk::SamplerMip::SamplerMip_Linear,
      .wrapU     = lvk::SamplerWrap::SamplerWrap_Repeat,
      .wrapV     = lvk::SamplerWrap::SamplerWrap_Repeat,
      .wrapW     = lvk::SamplerWrap::SamplerWrap_Repeat,
      .debugName = "Clamp Sampler",
  });

  LLOGL("Prefiltering GGX...\n");

  prefilterCubemap(ctx, specularCube, envPrefilteredCubemap, envMapCube, Distribution_GGX, clamp.index(), 1024);

  LLOGL("Prefiltering Charlie...\n");

  prefilterCubemap(ctx, specularCube, envCharlieCubemap, envMapCube, Distribution_Charlie, clamp.index(), 1024);

  ktxTextureCreateInfo createInfo = {
    .glInternalformat = GL_RGBA32F,
    .vkFormat         = VK_FORMAT_R32G32B32A32_SFLOAT,
    .baseWidth        = 64,
    .baseHeight       = 64,
    .baseDepth        = 1u,
    .numDimensions    = 2u,
    .numLevels        = 1u,
    .numLayers        = 1u,
    .numFaces         = 6u,
    .generateMipmaps  = KTX_FALSE,
  };

  ktxTexture1* irradianceCube = nullptr;
  (void)LVK_VERIFY(ktxTexture1_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &irradianceCube) == KTX_SUCCESS);

  LLOGL("Prefiltering Lambertian...\n");

  prefilterCubemap(ctx, irradianceCube, envIrradianceCubemap, envMapCube, Distribution_Lambertian, clamp.index(), 2048);

  envMapCube.reset();
  clamp.reset();

  ktxTexture_Destroy(ktxTexture(specularCube));
  ktxTexture_Destroy(ktxTexture(irradianceCube));

  stbi_image_free(pxs);
}

int main()
{
  minilog::initialize(nullptr, { .threadNames = false });

  std::unique_ptr<lvk::IContext> ctx = lvk::createVulkanContextWithSwapchain(nullptr, 0, 0, {});

  process_cubemap(
      ctx, "data/piazza_bologni_1k.hdr", "data/piazza_bologni_1k_prefilter.ktx", "data/piazza_bologni_1k_irradiance.ktx",
      "data/piazza_bologni_1k_charlie.ktx");
  process_cubemap(
      ctx, "data/immenstadter_horn_2k.hdr", "data/immenstadter_horn_2k_prefilter.ktx", "data/immenstadter_horn_2k_irradiance.ktx",
      "data/immenstadter_horn_2k_charlie.ktx");

  ctx.reset();
  return 0;
}
