#include <stdio.h>
#include <stdint.h>
#include <vector>

#include <ktx.h>
#include <ktx-software/lib/src/gl_format.h>
#include <ktx-software/lib/src/vkformat_enum.h>

#include <stb/stb_image.h>
#include <stb/stb_image_resize2.h>

#include <lvk/LVK.h>

int main()
{
  const int numChannels = 4;
  int origW, origH;
  uint8_t* pixels = stbi_load("data/wood.jpg", &origW, &origH, nullptr, numChannels);

  assert(pixels);

  printf("Compressing texture to BC7...\n");

  const uint32_t numMipLevels = lvk::calcNumMipLevels(origW, origH);

  // create a KTX2 texture for RGBA data
  ktxTextureCreateInfo createInfoKTX2 = {
    .glInternalformat = GL_RGBA8,
    .vkFormat         = VK_FORMAT_R8G8B8A8_UNORM,
    .baseWidth        = (uint32_t)origW,
    .baseHeight       = (uint32_t)origH,
    .baseDepth        = 1u,
    .numDimensions    = 2u,
    .numLevels        = numMipLevels,
    .numLayers        = 1u,
    .numFaces         = 1u,
    .generateMipmaps  = KTX_FALSE,
  };
  ktxTexture2* textureKTX2 = nullptr;
  (void)LVK_VERIFY(ktxTexture2_Create(&createInfoKTX2, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &textureKTX2) == KTX_SUCCESS);

  int w = origW;
  int h = origH;

  // generate custom mip-pyramid
  for (uint32_t i = 0; i != numMipLevels; ++i) {
    size_t offset = 0;
    ktxTexture_GetImageOffset(ktxTexture(textureKTX2), i, 0, 0, &offset);

    stbir_resize_uint8_linear(
        (const unsigned char*)pixels, origW, origH, 0, ktxTexture_GetData(ktxTexture(textureKTX2)) + offset, w, h, 0, STBIR_RGBA);

    h = h > 1 ? h >> 1 : 1;
    w = w > 1 ? w >> 1 : 1;
  }

  // compress to Basis and transcode to BC7
  (void)LVK_VERIFY(ktxTexture2_CompressBasis(textureKTX2, 255) == KTX_SUCCESS);
  (void)LVK_VERIFY(ktxTexture2_TranscodeBasis(textureKTX2, KTX_TTF_BC7_RGBA, 0) == KTX_SUCCESS);

  // convert to KTX1
  ktxTextureCreateInfo createInfoKTX1 = {
    .glInternalformat = GL_COMPRESSED_RGBA_BPTC_UNORM,
    .vkFormat         = VK_FORMAT_BC7_UNORM_BLOCK,
    .baseWidth        = (uint32_t)origW,
    .baseHeight       = (uint32_t)origH,
    .baseDepth        = 1u,
    .numDimensions    = 2u,
    .numLevels        = numMipLevels,
    .numLayers        = 1u,
    .numFaces         = 1u,
    .generateMipmaps  = KTX_FALSE,
  };
  ktxTexture1* textureKTX1 = nullptr;
  (void)LVK_VERIFY(ktxTexture1_Create(&createInfoKTX1, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &textureKTX1) == KTX_SUCCESS);

  for (uint32_t i = 0; i != numMipLevels; ++i) {
    size_t offset1 = 0;
    (void)LVK_VERIFY(ktxTexture_GetImageOffset(ktxTexture(textureKTX1), i, 0, 0, &offset1) == KTX_SUCCESS);
    size_t offset2 = 0;
    (void)LVK_VERIFY(ktxTexture_GetImageOffset(ktxTexture(textureKTX2), i, 0, 0, &offset2) == KTX_SUCCESS);
    memcpy(
        ktxTexture_GetData(ktxTexture(textureKTX1)) + offset1, ktxTexture_GetData(ktxTexture(textureKTX2)) + offset2,
        ktxTexture_GetImageSize(ktxTexture(textureKTX1), i));
  }

  ktxTexture_WriteToNamedFile(ktxTexture(textureKTX1), ".cache/image.ktx");
  ktxTexture_Destroy(ktxTexture(textureKTX1));
  ktxTexture_Destroy(ktxTexture(textureKTX2));

  if (pixels)
    stbi_image_free(pixels);

  return 0;
}
