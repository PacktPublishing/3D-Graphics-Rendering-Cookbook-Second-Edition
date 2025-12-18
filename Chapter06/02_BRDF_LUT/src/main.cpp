#include <lvk/LVK.h>

#include <GLFW/glfw3.h>

#include <shared/Utils.h>

#include <ktx.h>
#include <ktx-software/lib/src/gl_format.h>
#include <ktx-software/lib/src/vkformat_enum.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include <stdio.h>
#include <stdlib.h>

#include <vector>

const uint32_t kBrdfW      = 256;
const uint32_t kBrdfH      = 256;
const uint32_t kNumSamples = 1024;
const uint32_t kBufferSize = 4u * sizeof(uint16_t) * kBrdfW * kBrdfH; // store RGBA float16 in the buffer

void calculateLUT(const std::unique_ptr<lvk::IContext>& ctx, void* output, uint32_t size)
{
  lvk::Holder<lvk::ShaderModuleHandle> comp = loadShaderModule(ctx, "Chapter06/02_BRDF_LUT/src/main.comp");

  lvk::Holder<lvk::ComputePipelineHandle> computePipelineHandle = ctx->createComputePipeline({
      .smComp = comp,
      .specInfo = {.entries  = { { .constantId = 0,
		                             .size = sizeof(kNumSamples) },},
                   .data     = &kNumSamples,
                   .dataSize = sizeof(kNumSamples),},
  });

  lvk::Holder<lvk::BufferHandle> dstBuffer = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_HostVisible,
      .size      = size,
      .debugName = "Compute: BRDF LUT",
  });

  lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
  {
    buf.cmdBindComputePipeline(computePipelineHandle);
    buf.cmdPushDebugGroupLabel("Compute BRDF", 0xff0000ff);
    struct {
      uint32_t w = kBrdfW;
      uint32_t h = kBrdfH;
      uint64_t addr;
    } pc{
      .addr = ctx->gpuAddress(dstBuffer),
    };
    buf.cmdPushConstants(pc);
    buf.cmdDispatchThreadGroups({ kBrdfW / 16, kBrdfH / 16, 1 });
    buf.cmdPopDebugGroupLabel();
  }
  ctx->wait(ctx->submit(buf));

  memcpy(output, ctx->getMappedPtr(dstBuffer), kBufferSize);
}

int main()
{
  minilog::initialize(nullptr, { .threadNames = false });

  std::unique_ptr<lvk::IContext> ctx = lvk::createVulkanContextWithSwapchain(nullptr, 0, 0, {});

  ktxTextureCreateInfo createInfo = {
    .glInternalformat = GL_RGBA16F,
    .vkFormat         = VK_FORMAT_R16G16B16A16_SFLOAT,
    .baseWidth        = kBrdfW,
    .baseHeight       = kBrdfH,
    .baseDepth        = 1,
    .numDimensions    = 2,
    .numLevels        = 1,
    .numLayers        = 1,
    .numFaces         = 1,
    .generateMipmaps  = KTX_FALSE,
  };

  ktxTexture1* lutTexture = nullptr;
    (void)LVK_VERIFY(ktxTexture1_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &lutTexture) == KTX_SUCCESS);

  printf("Calculating LUT texture...\n");
  calculateLUT(ctx, lutTexture->pData, kBufferSize);

  printf("Saving LUT texture...\n");
  // use Pico Pixel to view https://pixelandpolygon.com/
  ktxTexture_WriteToNamedFile(ktxTexture(lutTexture), "data/brdfLUT.ktx");
  ktxTexture_Destroy(ktxTexture(lutTexture));

  return 0;
}
