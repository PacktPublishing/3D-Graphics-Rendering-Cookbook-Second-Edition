#include <stdio.h>
#include <stdint.h>
#include <vector>

#include <ktx-software/lib/vkformat_enum.h>
#include <ktx.h>
#include <ktx-software/lib/gl_format.h>

#include <stb/stb_image.h>
#include <stb/stb_image_resize2.h>

#include <bc7enc/Compress.h>

#include <lvk/LVK.h>

int main()
{
  const int numChannels = 4;
  int origW, origH;
  const uint8_t* pixels = stbi_load("data/wood.jpg", &origW, &origH, nullptr, numChannels);

  assert(pixels);

  printf("Compressing texture to BC7...\n");

  const uint32_t numMipLevels = lvk::calcNumMipLevels(origW, origH);

  // create a GLI texture with the BC7 format
  ktxTextureCreateInfo createInfo = {
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

  // Create KTX texture
  // hard coded and support only BC7 format
  ktxTexture1* texture = nullptr;
  (void)LVK_VERIFY(ktxTexture1_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture) == KTX_SUCCESS);

  int w = origW;
  int h = origH;

  for (uint32_t i = 0; i != numMipLevels; ++i) {
    std::vector<uint8_t> destPixels(w * h * numChannels);

    stbir_resize_uint8_linear((const unsigned char*)pixels, origW, origH, 0, (unsigned char*)destPixels.data(), w, h, 0, STBIR_RGBA);

    const block16_vec packedImage16 = Compress::getCompressedImage(destPixels.data(), w, h, numChannels, false);
    ktxTexture_SetImageFromMemory(
        ktxTexture(texture), i, 0, 0, reinterpret_cast<const uint8_t*>(packedImage16.data()), sizeof(block16) * packedImage16.size());

    h = h > 1 ? h >> 1 : 1;
    w = w > 1 ? w >> 1 : 1;
  }

  ktxTexture_WriteToNamedFile(ktxTexture(texture), ".cache/image.ktx");
  ktxTexture_Destroy(ktxTexture(texture));

  return 0;
}
