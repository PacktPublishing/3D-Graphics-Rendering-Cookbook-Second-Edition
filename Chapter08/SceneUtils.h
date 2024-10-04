#pragma once

#include <algorithm>
#include <execution>
#include <filesystem>

#include <ktx-software/lib/gl_format.h>
#include <ktx-software/lib/vkformat_enum.h>
#include <ktx.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "stb_image.h"
#include "stb_image_resize2.h"

#include <bc7enc/Compress.h>

#include "shared/UtilsGLTF.h"
#include "Chapter08/VKMesh08.h"

// find a file in directory which "almost" coincides with the origFile (their lowercase versions coincide)
std::string findSubstitute(const std::string& origFile)
{
  namespace fs = std::filesystem;

  // Make absolute path
  auto apath = fs::absolute(fs::path(origFile));
  // Absolute lowercase filename [we compare with it]
  auto afile = lowercaseString(apath.filename().string());
  // Directory where in which the file should be
  auto dir = fs::path(origFile).remove_filename();

  // Iterate each file non-recursively and compare lowercase absolute path with 'afile'
  for (auto& p : fs::directory_iterator(dir))
    if (afile == lowercaseString(p.path().filename().string()))
      return p.path().string();

  return std::string{};
}

std::string fixTextureFile(const std::string& file)
{
  // TODO: check the findSubstitute() function
  return std::filesystem::exists(file) ? file : findSubstitute(file);
}

std::string convertTexture(
    const std::string& file, const std::string& basePath, std::unordered_map<std::string, uint32_t>& opacityMapIndices,
    const std::vector<std::string>& opacityMaps)
{
  const int maxNewWidth  = 512;
  const int maxNewHeight = 512;

  if (!std::filesystem::exists(".cache/out_textures/")) {
    std::filesystem::create_directories(".cache/out_textures/");
  }

  const std::string srcFile = replaceAll(basePath + file, "\\", "/");
  const std::string newFile = std::string(".cache/out_textures/") +
                              lowercaseString(replaceAll(replaceAll(srcFile, "..", "__"), "/", "__") + std::string("__rescaled")) +
                              std::string(".ktx");

  // load this image
  int origWidth, origHeight, texChannels;
  stbi_uc* pixels = stbi_load(fixTextureFile(srcFile).c_str(), &origWidth, &origHeight, &texChannels, STBI_rgb_alpha);
  uint8_t* src    = pixels;
  texChannels     = STBI_rgb_alpha;

  std::vector<uint8_t> tmpImage(maxNewWidth * maxNewHeight * 4);

  if (!src) {
    printf("Failed to load [%s] texture\n", srcFile.c_str());
    origWidth   = maxNewWidth;
    origHeight  = maxNewHeight;
    texChannels = STBI_rgb_alpha;
    src         = tmpImage.data();
  } else {
    printf("Loaded [%s] %dx%d texture with %d channels\n", srcFile.c_str(), origWidth, origHeight, texChannels);
  }

  if (opacityMapIndices.count(file) > 0) {
    const std::string opacityMapFile = replaceAll(basePath + opacityMaps[opacityMapIndices[file]], "\\", "/");
    int opacityWidth, opacityHeight;
    stbi_uc* opacityPixels = stbi_load(fixTextureFile(opacityMapFile).c_str(), &opacityWidth, &opacityHeight, nullptr, 1);

    if (!opacityPixels) {
      printf("Failed to load opacity mask [%s]\n", opacityMapFile.c_str());
    }

    assert(opacityPixels);
    assert(origWidth == opacityWidth);
    assert(origHeight == opacityHeight);

    // store the opacity mask in the alpha component of this image
    if (opacityPixels)
      for (int y = 0; y != opacityHeight; y++)
        for (int x = 0; x != opacityWidth; x++)
          src[(y * opacityWidth + x) * texChannels + 3] = opacityPixels[y * opacityWidth + x];

    stbi_image_free(opacityPixels);
  }

  const int newW = std::min(origWidth, maxNewWidth);
  const int newH = std::min(origHeight, maxNewHeight);

  printf("Compressing texture to BC7...\n");

  const uint32_t numMipLevels = lvk::calcNumMipLevels(newW, newH);

  // create a KTX texture to store the BC7 format
  ktxTextureCreateInfo createInfo = {
    .glInternalformat = GL_COMPRESSED_RGBA_BPTC_UNORM,
    .vkFormat         = VK_FORMAT_BC7_UNORM_BLOCK,
    .baseWidth        = (uint32_t)newW,
    .baseHeight       = (uint32_t)newH,
    .baseDepth        = 1u,
    .numDimensions    = 2u,
    .numLevels        = numMipLevels,
    .numLayers        = 1u,
    .numFaces         = 1u,
    .generateMipmaps  = KTX_FALSE,
  };

  int w = newW;
  int h = newH;

  // Create KTX texture
  // hard coded and support only BC7 format
  ktxTexture1* texture = nullptr;
  (void)LVK_VERIFY(ktxTexture1_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture) == KTX_SUCCESS);

  for (uint32_t i = 0; i != numMipLevels; ++i) {
    std::vector<uint8_t> destPixels(w * h * texChannels);

    stbir_resize_uint8_linear((const unsigned char*)src, origWidth, origHeight, 0, (unsigned char*)destPixels.data(), w, h, 0, STBIR_RGBA);

    const block16_vec packedImage16 = Compress::getCompressedImage(destPixels.data(), w, h, texChannels, false);
    ktxTexture_SetImageFromMemory(
        ktxTexture(texture), i, 0, 0, reinterpret_cast<const uint8_t*>(packedImage16.data()), sizeof(block16) * packedImage16.size());

    h = h > 1 ? h >> 1 : 1;
    w = w > 1 ? w >> 1 : 1;
  }

  ktxTexture_WriteToNamedFile(ktxTexture(texture), newFile.c_str());
  ktxTexture_Destroy(ktxTexture(texture));

  if (pixels)
    stbi_image_free(pixels);

  return newFile;
}

void convertAndDownscaleAllTextures(
    const std::vector<Material>& materials, const std::string& basePath, std::vector<std::string>& files,
    std::vector<std::string>& opacityMaps)
{
  std::unordered_map<std::string, uint32_t> opacityMapIndices(files.size());

  for (const auto& m : materials)
    if (m.opacityTexture != -1 && m.baseColorTexture != -1)
      opacityMapIndices[files[m.baseColorTexture]] = (uint32_t)m.opacityTexture;

  auto converter = [&](const std::string& s) -> std::string {
    return convertTexture(s, basePath, opacityMapIndices, opacityMaps);
  };

  std::transform(std::execution::par, std::begin(files), std::end(files), std::begin(files), converter);
}

void traverse(const aiScene* sourceScene, Scene& scene, aiNode* N, int parent, int depth)
{
  int newNode = addNode(scene, parent, depth);

  if (N->mName.C_Str()) {
    printPrefix(depth);
    printf("Node[%d].name = %s\n", newNode, N->mName.C_Str());

    const uint32_t stringID = (uint32_t)scene.names.size();
    scene.names.push_back(std::string(N->mName.C_Str()));
    scene.nameForNode[newNode] = stringID;
  }

  for (size_t i = 0; i < N->mNumMeshes; i++) {
    const int newSubNode = addNode(scene, newNode, depth + 1);

    const uint32_t stringID = (uint32_t)scene.names.size();
    scene.names.push_back(std::string(N->mName.C_Str()) + "_Mesh_" + std::to_string(i));
    scene.nameForNode[newSubNode] = stringID;

    const int mesh                    = (int)N->mMeshes[i];
    scene.meshForNode[newSubNode]     = mesh;
    scene.materialForNode[newSubNode] = sourceScene->mMeshes[mesh]->mMaterialIndex;

    printPrefix(depth);
    printf("Node[%d].SubNode[%d].mesh     = %d\n", newNode, newSubNode, (int)mesh);
    printPrefix(depth);
    printf("Node[%d].SubNode[%d].material = %d\n", newNode, newSubNode, sourceScene->mMeshes[mesh]->mMaterialIndex);

    scene.globalTransform[newSubNode] = glm::mat4(1.0f);
    scene.localTransform[newSubNode]  = glm::mat4(1.0f);
  }

  scene.globalTransform[newNode] = glm::mat4(1.0f);
  scene.localTransform[newNode]  = aiMatrix4x4ToMat4(N->mTransformation);

  if (N->mParent != nullptr) {
    printPrefix(depth);
    printf("\tNode[%d].parent         = %s\n", newNode, N->mParent->mName.C_Str());
    printPrefix(depth);
    printf("\tNode[%d].localTransform = ", newNode);
    printMat4(N->mTransformation);
    printf("\n");
  }

  for (unsigned int n = 0; n < N->mNumChildren; n++)
    traverse(sourceScene, scene, N->mChildren[n], newNode, depth + 1);
}

void loadMeshFile(const char* fileName, MeshData& meshData, Scene& ourScene, bool generateLODs)
{
  printf("Loading '%s'...\n", fileName);

  const unsigned int flags = 0 | aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                             aiProcess_LimitBoneWeights | aiProcess_SplitLargeMeshes | aiProcess_ImproveCacheLocality |
                             aiProcess_RemoveRedundantMaterials | aiProcess_FindDegenerates | aiProcess_FindInvalidData |
                             aiProcess_GenUVCoords;

  const aiScene* scene = aiImportFile(fileName, flags);

  if (!scene || !scene->HasMeshes()) {
    printf("Unable to load '%s'\n", fileName);
    exit(255);
  }

  meshData.meshes.reserve(scene->mNumMeshes);
  meshData.boxes.reserve(scene->mNumMeshes);

  uint32_t indexOffset  = 0;
  uint32_t vertexOffset = 0;

  for (unsigned int i = 0; i != scene->mNumMeshes; i++) {
    printf("\rConverting meshes %u/%u...", i + 1, scene->mNumMeshes);
    fflush(stdout);
    meshData.meshes.push_back(convertAIMesh(scene->mMeshes[i], meshData, indexOffset, vertexOffset, generateLODs));
  }
  printf("\n");

  // extract base model path
  const std::size_t pathSeparator = std::string(fileName).find_last_of("/\\");
  const std::string basePath = (pathSeparator != std::string::npos) ? std::string(fileName).substr(0, pathSeparator + 1) : std::string();

  std::vector<std::string> opacityMaps;

  for (unsigned int i = 0; i != scene->mNumMaterials; i++) {
    printf("\rConverting materials %u/%u...", i + 1, scene->mNumMaterials);
    const aiMaterial* m = scene->mMaterials[i];
    ourScene.materialNames.push_back(m->GetName().C_Str());
    meshData.materials.push_back(convertAIMaterial(m, meshData.textureFiles, opacityMaps));
  }
  printf("\n");

  // texture processing, rescaling and packing
  convertAndDownscaleAllTextures(meshData.materials, basePath, meshData.textureFiles, opacityMaps);

  recalculateBoundingBoxes(meshData);

  // scene hierarchy conversion
  traverse(scene, ourScene, scene->mRootNode, -1, 0);
}
