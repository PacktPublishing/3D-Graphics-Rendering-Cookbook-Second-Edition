#include "shared/VulkanApp.h"
#include "shared/Tonemap.h"

#define DEMO_TEXTURE_MAX_SIZE 2048
#define DEMO_TEXTURE_CACHE_FOLDER ".cache/out_textures_11/"
#define fileNameCachedMeshes ".cache/ch11_bistro.meshes"
#define fileNameCachedMaterials ".cache/ch11_bistro.materials"
#define fileNameCachedHierarchy ".cache/ch11_bistro.scene"

#include "Chapter10/Bistro.h"
#include "Chapter10/Skybox.h"
#include "Chapter11/VKMesh11Lazy.h"

// 64M hash slots * 8 bytes = 512 MB world-space AO cache
constexpr uint32_t kAOHashMapSize = 64u * 1024u * 1024u;

bool drawMeshesOpaque      = true;
bool drawMeshesTransparent = true;
bool drawWireframe         = false;
bool drawBoxes             = false;
bool drawLightFrustum      = false;
// alpha-to-coverage (anti-aliases alpha-tested foliage; requires MSAA)
bool enableA2C     = true;
float a2cThickness = 8.0f;
// Ray-traced ambient occlusion with world-space spatial hashing (Gautron 2020)
bool aoEnable           = true;
bool aoSpatialHash      = true;
bool aoFiltering        = false;
bool aoTimeVaryingNoise = true;
int aoSamples           = 2;      // per-pixel samples when spatial hashing is off (sample default)
float aoRadius          = 1.5f;   // world-space ray length (~sample's 8.0 scaled to this scene)
float aoPower           = 0.75f;
float aoPixelSize       = 6.0f;   // target cell size in screen pixels (sp)
float aoMinCellSize     = 0.01f;  // minimal world-space cell size (smin)
int aoMaxSamples        = 192;    // max accumulated samples per hash cell
// OIT
bool oitShowHeatmap   = false;
float oitOpacityBoost = 0.0f;
// HDR
bool hdrDrawCurves       = false;
bool hdrEnableBloom      = true;
float hdrBloomStrength   = 0.01f;
int hdrNumBloomPasses    = 2;
float hdrAdaptationSpeed = 3.0f;
// Culling
enum CullingMode {
  CullingMode_None = 0,
  CullingMode_CPU  = 1,
  CullingMode_GPU  = 2,
};
mat4 cullingView       = mat4(1.0f);
int cullingMode        = CullingMode_CPU;
bool freezeCullingView = false;

struct LightParams {
  float theta          = +90.0f;
  float phi            = -26.0f;
  float depthBiasConst = 1.1f;
  float depthBiasSlope = 2.0f;

  bool operator==(const LightParams&) const = default;
} light;

int main()
{
  MeshData meshData;
  Scene scene;
  loadBistro(meshData, scene);

  VulkanApp app({
      .initialCameraPos    = vec3(-18.621f, 4.621f, -6.359f),
      .initialCameraTarget = vec3(0, +5.0f, 0),
  });

  app.positioner_.maxSpeed_ = 1.5f;

  LineCanvas3D canvas3d;

  std::unique_ptr<lvk::IContext> ctx(app.ctx_.get());

  const lvk::Dimensions sizeFb = ctx->getDimensions(ctx->getCurrentSwapchainTexture());

  const uint32_t kNumSamples         = 8;
  const lvk::Format kOffscreenFormat = lvk::Format_RGBA_F16;

  // MSAA
  lvk::Holder<lvk::TextureHandle> msaaColor = ctx->createTexture({
      .format     = kOffscreenFormat,
      .dimensions = sizeFb,
      .numSamples = kNumSamples,
      .usage      = lvk::TextureUsageBits_Attachment,
      .storage    = lvk::StorageType_Memoryless,
      .debugName  = "msaaColor",
  });

  lvk::Holder<lvk::TextureHandle> msaaDepth = ctx->createTexture({
      .format     = app.getDepthFormat(),
      .dimensions = sizeFb,
      .numSamples = kNumSamples,
      .usage      = lvk::TextureUsageBits_Attachment,
      .storage    = lvk::StorageType_Memoryless,
      .debugName  = "msaaDepth",
  });

  lvk::Holder<lvk::TextureHandle> texOpaqueDepth = ctx->createTexture({
      .format     = app.getDepthFormat(),
      .dimensions = sizeFb,
      .usage      = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled,
      .debugName  = "opaqueDepth",
  });

  lvk::Holder<lvk::TextureHandle> texOpaqueColor = ctx->createTexture({
      .format     = kOffscreenFormat,
      .dimensions = sizeFb,
      .usage      = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
      .debugName  = "opaqueColor",
  });

  // final HDR scene color (AO is applied in the opaque pass; this holds opaque + OIT)
  lvk::Holder<lvk::TextureHandle> texSceneColor = ctx->createTexture({
      .format     = kOffscreenFormat,
      .dimensions = sizeFb,
      .usage      = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
      .debugName  = "sceneColor",
  });

  // HDR light adaptation
  const lvk::Dimensions sizeBloom = { 512, 512 };

  lvk::Holder<lvk::TextureHandle> texBrightPass = ctx->createTexture({
      .format     = kOffscreenFormat,
      .dimensions = sizeBloom,
      .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
      .debugName  = "texBrightPass",
  });
  lvk::Holder<lvk::TextureHandle> texBloomPass  = ctx->createTexture({
       .format     = kOffscreenFormat,
       .dimensions = sizeBloom,
       .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
       .debugName  = "texBloomPass",
  });
  // ping-pong
  lvk::Holder<lvk::TextureHandle> texBloom[] = {
    ctx->createTexture({
        .format     = kOffscreenFormat,
        .dimensions = sizeBloom,
        .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
        .debugName  = "texBloom0",
    }),
    ctx->createTexture({
        .format     = kOffscreenFormat,
        .dimensions = sizeBloom,
        .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
        .debugName  = "texBloom1",
    }),
  };

  const lvk::ComponentMapping swizzle = { .r = lvk::Swizzle_R, .g = lvk::Swizzle_R, .b = lvk::Swizzle_R, .a = lvk::Swizzle_1 };

  lvk::Holder<lvk::TextureHandle> texLumViews[10] = { ctx->createTexture({
      .format       = lvk::Format_R_F16,
      .dimensions   = sizeBloom,
      .usage        = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
      .numMipLevels = lvk::calcNumMipLevels(sizeBloom.width, sizeBloom.height),
      .components   = swizzle,
      .debugName    = "texLuminance",
  }) };

  for (uint32_t v = 1; v != LVK_ARRAY_NUM_ELEMENTS(texLumViews); v++) {
    texLumViews[v] = ctx->createTextureView(texLumViews[0], { .mipLevel = v, .components = swizzle }, "texLumViews[]");
  }

  const uint16_t brightPixel = glm::packHalf1x16(50.0f);

  // ping-pong textures for iterative luminance adaptation
  const lvk::TextureDesc luminanceTextureDesc{
    .format     = lvk::Format_R_F16,
    .dimensions = {1, 1},
    .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
    .components = swizzle,
    .data       = &brightPixel,
  };
  lvk::Holder<lvk::TextureHandle> texAdaptedLum[2] = {
    ctx->createTexture(luminanceTextureDesc, "texAdaptedLuminance0"),
    ctx->createTexture(luminanceTextureDesc, "texAdaptedLuminance1"),
  };
  // shadows
  lvk::Holder<lvk::TextureHandle> texShadowMap = ctx->createTexture({
      .type       = lvk::TextureType_2D,
      .format     = lvk::Format_Z_UN16,
      .dimensions = { 4096, 4096 },
      .usage      = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled,
      .components = { .r = lvk::Swizzle_R, .g = lvk::Swizzle_R, .b = lvk::Swizzle_R, .a = lvk::Swizzle_1 },
      .debugName  = "Shadow map",
  });

  lvk::Holder<lvk::SamplerHandle> samplerShadow = ctx->createSampler({
      .wrapU               = lvk::SamplerWrap_Clamp,
      .wrapV               = lvk::SamplerWrap_Clamp,
      .depthCompareOp      = lvk::CompareOp_LessEqual,
      .depthCompareEnabled = true,
      .debugName           = "Sampler: shadow",
  });

  struct LightData {
    mat4 viewProjBias;
    vec4 lightDir;
    uint32_t shadowTexture;
    uint32_t shadowSampler;
    // ray-traced AO with spatial hashing - layout must match LightBuffer in common.sp
    uint32_t tlas              = 0;
    uint32_t frameId           = 0;
    uint64_t hashSlot          = 0; // device address, read as uvec2 in the shader
    uint32_t enableAO          = 0;
    uint32_t enableSpatialHash = 0;
    uint32_t enableFiltering   = 0;
    uint32_t aoSamples         = 0;
    float aoRadius             = 0;
    float aoPower              = 0;
    float sp                   = 0;
    float smin                 = 0;
    uint32_t maxSamples        = 0;
    uint32_t hashMapSize       = 0;
    float resolutionY          = 0;
    float projScaleY           = 0;
    float a2cThickness         = 0;
  } lightData;
  lvk::Holder<lvk::BufferHandle> bufferLight = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_Device,
      .size      = sizeof(LightData),
      .debugName = "Buffer: light",
  });

  lvk::Holder<lvk::SamplerHandle> samplerClamp = ctx->createSampler({
      .wrapU = lvk::SamplerWrap_Clamp,
      .wrapV = lvk::SamplerWrap_Clamp,
      .wrapW = lvk::SamplerWrap_Clamp,
  });

  const Skybox skyBox(
      ctx, "data/immenstadter_horn_2k_prefilter.ktx", "data/immenstadter_horn_2k_irradiance.ktx", kOffscreenFormat, app.getDepthFormat(),
      kNumSamples);
  VKMesh11Lazy mesh(ctx, meshData, scene);
  const VKPipeline11 pipelineOpaque(
      ctx, meshData.streams, kOffscreenFormat, app.getDepthFormat(), kNumSamples,
      loadShaderModule(ctx, "Chapter11/06_FinalDemo/src/main.vert"), loadShaderModule(ctx, "Chapter11/06_FinalDemo/src/opaque.frag"));
  // alpha-to-coverage variant of the opaque pipeline (kEnableAlphaToCoverage spec constant + .alphaToCoverage)
  const VKPipeline11 pipelineOpaqueA2C(
      ctx, meshData.streams, kOffscreenFormat, app.getDepthFormat(), kNumSamples,
      loadShaderModule(ctx, "Chapter11/06_FinalDemo/src/main.vert"), loadShaderModule(ctx, "Chapter11/06_FinalDemo/src/opaque.frag"),
      true);
  const VKPipeline11 pipelineTransparent(
      ctx, meshData.streams, kOffscreenFormat, app.getDepthFormat(), kNumSamples,
      loadShaderModule(ctx, "Chapter11/06_FinalDemo/src/main.vert"), loadShaderModule(ctx, "Chapter11/06_FinalDemo/src/transparent.frag"));
  const VKPipeline11 pipelineShadow(
      ctx, meshData.streams, lvk::Format_Invalid, ctx->getFormat(texShadowMap), 1,
      loadShaderModule(ctx, "Chapter11/03_DirectionalShadows/src/shadow.vert"),
      loadShaderModule(ctx, "Chapter11/03_DirectionalShadows/src/shadow.frag"));

  lvk::Holder<lvk::ShaderModuleHandle> vertOIT       = loadShaderModule(ctx, "data/shaders/QuadFlip.vert");
  lvk::Holder<lvk::ShaderModuleHandle> fragOIT       = loadShaderModule(ctx, "Chapter11/04_OIT/src/oit.frag");
  lvk::Holder<lvk::RenderPipelineHandle> pipelineOIT = ctx->createRenderPipeline({
      .smVert = vertOIT,
      .smFrag = fragOIT,
      .color  = { { .format = kOffscreenFormat } },
  });

  lvk::Holder<lvk::ShaderModuleHandle> compBrightPass        = loadShaderModule(ctx, "Chapter10/05_HDR/src/BrightPass.comp");
  lvk::Holder<lvk::ComputePipelineHandle> pipelineBrightPass = ctx->createComputePipeline({ .smComp = compBrightPass });

  lvk::Holder<lvk::ShaderModuleHandle> compAdaptationPass        = loadShaderModule(ctx, "Chapter10/06_HDR_Adaptation/src/Adaptation.comp");
  lvk::Holder<lvk::ComputePipelineHandle> pipelineAdaptationPass = ctx->createComputePipeline({ .smComp = compAdaptationPass });

  const uint32_t kHorizontal = 1;
  const uint32_t kVertical   = 0;

  lvk::Holder<lvk::ShaderModuleHandle> compBloomPass     = loadShaderModule(ctx, "Chapter10/05_HDR/src/Bloom.comp");
  lvk::Holder<lvk::ComputePipelineHandle> pipelineBloomX = ctx->createComputePipeline({
      .smComp   = compBloomPass,
      .specInfo = {.entries = { { .constantId = 0, .size = sizeof(uint32_t) } }, .data = &kHorizontal, .dataSize = sizeof(uint32_t)},
  });
  lvk::Holder<lvk::ComputePipelineHandle> pipelineBloomY = ctx->createComputePipeline({
      .smComp   = compBloomPass,
      .specInfo = {.entries = { { .constantId = 0, .size = sizeof(uint32_t) } }, .data = &kVertical, .dataSize = sizeof(uint32_t)},
  });

  lvk::Holder<lvk::ShaderModuleHandle> vertToneMap = loadShaderModule(ctx, "data/shaders/QuadFlip.vert");
  lvk::Holder<lvk::ShaderModuleHandle> fragToneMap = loadShaderModule(ctx, "Chapter10/05_HDR/src/ToneMap.frag");

  lvk::Holder<lvk::RenderPipelineHandle> pipelineToneMap = ctx->createRenderPipeline({
      .smVert = vertToneMap,
      .smFrag = fragToneMap,
      .color  = { { .format = ctx->getSwapchainFormat() } },
  });

  lvk::Holder<lvk::ShaderModuleHandle> compCulling        = loadShaderModule(ctx, "Chapter11/02_CullingGPU/src/FrustumCulling.comp");
  lvk::Holder<lvk::ComputePipelineHandle> pipelineCulling = ctx->createComputePipeline({
      .smComp = compCulling,
  });

  // ---------------------------------------------------------------------------------------------
  // Ray-traced AO: build acceleration structures from the scene geometry.
  // We bake a world-space triangle soup that replicates exactly what the rasterizer fetches:
  // for every triangle index the GPU reads vertex (mesh.vertexOffset + indexData[...]) and the
  // vertex shader transforms it by scene.globalTransform[node]. We do the same on the CPU and
  // build a single identity-instance TLAS, so AO rays hit precisely the rasterized surfaces.
  // (NOTE: indices in MeshData are local per source file but offset-baked across merged files,
  //  so 'mesh.vertexOffset + index' is the correct absolute vertex - see mergeMeshData().)
  // ---------------------------------------------------------------------------------------------
  lvk::Holder<lvk::BufferHandle> bufferAOVertices;
  lvk::Holder<lvk::BufferHandle> bufferAOIndices;
  lvk::Holder<lvk::BufferHandle> bufferAOInstances;
  std::vector<lvk::Holder<lvk::AccelStructHandle>> aoBLAS;
  lvk::Holder<lvk::AccelStructHandle> aoTLAS;
  lvk::Holder<lvk::BufferHandle> bufferAOHash;
  {
    const uint32_t stride     = meshData.streams.getVertexSize();
    const uint32_t totalVerts = (uint32_t)(meshData.vertexData.size() / stride);
    std::vector<vec3> aoVertices; // world-space triangle soup (3 verts per triangle)
    aoVertices.reserve(meshData.indexData.size());
    for (const auto& p : scene.meshForNode) {
      const Mesh& m         = meshData.meshes[p.second];
      const Material& mtl   = meshData.materials[m.materialID];
      // Skip transparent (glass) geometry - it is not a solid occluder. (NOTE: most Bistro materials
      // carry alphaTest=0.5 from the OBJ export, so alphaTest is not usable to single out foliage.)
      if (mtl.flags & sMaterialFlags_Transparent)
        continue;
      const mat4 model        = scene.globalTransform[p.first];
      const uint32_t idxCount = m.getLODIndicesCount(0);
      for (uint32_t i = 0; i < idxCount; i++) {
        const uint32_t vIdx = m.vertexOffset + meshData.indexData[m.indexOffset + i];
        LVK_ASSERT(vIdx < totalVerts);
        const float* pos = reinterpret_cast<const float*>(meshData.vertexData.data() + size_t(vIdx) * stride);
        aoVertices.push_back(vec3(model * vec4(pos[0], pos[1], pos[2], 1.0f)));
      }
    }
    // identity index buffer (the soup is already expanded - no shared vertices)
    std::vector<uint32_t> aoIndices(aoVertices.size());
    for (uint32_t i = 0; i < (uint32_t)aoIndices.size(); i++)
      aoIndices[i] = i;
    LLOGL("[AO] world-space occluder soup: %zu verts, %zu tris\n", aoVertices.size(), aoVertices.size() / 3);

    bufferAOVertices = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Storage | lvk::BufferUsageBits_AccelStructBuildInputReadOnly,
        .storage   = lvk::StorageType_Device,
        .size      = sizeof(vec3) * aoVertices.size(),
        .data      = aoVertices.data(),
        .debugName = "Buffer: AO vertices (world space)",
    });
    bufferAOIndices = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Index | lvk::BufferUsageBits_AccelStructBuildInputReadOnly,
        .storage   = lvk::StorageType_Device,
        .size      = sizeof(uint32_t) * aoIndices.size(),
        .data      = aoIndices.data(),
        .debugName = "Buffer: AO indices",
    });

    const glm::mat3x4 identity(1.0f);
    lvk::Holder<lvk::BufferHandle> transformBuffer = ctx->createBuffer({
        .usage   = lvk::BufferUsageBits_AccelStructBuildInputReadOnly,
        .storage = lvk::StorageType_HostVisible,
        .size    = sizeof(glm::mat3x4),
        .data    = &identity,
    });

    const uint32_t totalPrimitiveCount = (uint32_t)aoIndices.size() / 3;
    lvk::AccelStructDesc blasDesc{
        .type            = lvk::AccelStructType_BLAS,
        .geometryType    = lvk::AccelStructGeomType_Triangles,
        .vertexFormat    = lvk::VertexFormat_Float3,
        .vertexBuffer    = bufferAOVertices,
        .vertexStride    = sizeof(vec3),
        .numVertices     = (uint32_t)aoVertices.size(),
        .indexFormat     = lvk::IndexFormat_UI32,
        .indexBuffer     = bufferAOIndices,
        .transformBuffer = transformBuffer,
        .buildRange      = { .primitiveCount = totalPrimitiveCount },
        .buildFlags      = lvk::AccelStructBuildFlagBits_PreferFastTrace,
        .debugName       = "BLAS: AO",
    };
    const lvk::AccelStructSizes blasSizes = ctx->getAccelStructSizes(blasDesc);
    const uint32_t maxStorageBufferSize   = ctx->getMaxStorageBufferRange();

    // a single BLAS may exceed the maximum storage buffer size - split it into several
    const uint32_t requiredBlasCount = [&blasSizes, maxStorageBufferSize]() {
      const uint32_t count1 = (uint32_t)(blasSizes.buildScratchSize / maxStorageBufferSize);
      const uint32_t count2 = (uint32_t)(blasSizes.accelerationStructureSize / maxStorageBufferSize);
      return 1 + (count1 > count2 ? count1 : count2);
    }();
    blasDesc.buildRange.primitiveCount = totalPrimitiveCount / requiredBlasCount;

    aoBLAS.reserve(requiredBlasCount);
    std::vector<lvk::AccelStructInstance> instances;
    instances.reserve(requiredBlasCount);
    const uint32_t primitiveCount = blasDesc.buildRange.primitiveCount;
    for (uint32_t i = 0; i < totalPrimitiveCount; i += primitiveCount) {
      const uint32_t rest                 = totalPrimitiveCount - i;
      blasDesc.buildRange.primitiveOffset = i * 3 * sizeof(uint32_t);
      blasDesc.buildRange.primitiveCount  = (primitiveCount < rest) ? primitiveCount : rest;
      aoBLAS.emplace_back(ctx->createAccelerationStructure(blasDesc));
      instances.emplace_back(lvk::AccelStructInstance{
          .transform                              = (const lvk::mat3x4&)identity,
          .instanceCustomIndex                    = 0,
          .mask                                   = 0xff,
          .instanceShaderBindingTableRecordOffset = 0,
          .flags                                  = lvk::AccelStructInstanceFlagBits_TriangleFacingCullDisable,
          .accelerationStructureReference         = ctx->gpuAddress(aoBLAS.back()),
      });
    }

    bufferAOInstances = ctx->createBuffer(lvk::BufferDesc{
        .usage     = lvk::BufferUsageBits_AccelStructBuildInputReadOnly,
        .storage   = lvk::StorageType_HostVisible,
        .size      = sizeof(lvk::AccelStructInstance) * instances.size(),
        .data      = instances.data(),
        .debugName = "Buffer: AO TLAS instances",
    });
    aoTLAS = ctx->createAccelerationStructure({
        .type            = lvk::AccelStructType_TLAS,
        .geometryType    = lvk::AccelStructGeomType_Instances,
        .instancesBuffer = bufferAOInstances,
        .buildRange      = { .primitiveCount = (uint32_t)instances.size() },
        .buildFlags      = lvk::AccelStructBuildFlagBits_PreferFastTrace,
    });
    printf("[AO] TLAS done (%zu instances)\n", instances.size());
  }

  // world-space AO hash map (zero-initialized)
  {
    bufferAOHash = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Storage,
        .storage   = lvk::StorageType_Device,
        .size      = sizeof(uint64_t) * kAOHashMapSize,
        .debugName = "Buffer: AO hash map",
    });
    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    buf.cmdFillBuffer(bufferAOHash, 0, lvk::LVK_WHOLE_SIZE, 0);
    ctx->submit(buf);
  }

  uint32_t aoFrameId = 0;
  bool aoResetHash   = false;

  app.addKeyCallback([](GLFWwindow* window, int key, int scancode, int action, int mods) {
    const bool pressed = action != GLFW_RELEASE;
    if (!pressed || ImGui::GetIO().WantCaptureKeyboard)
      return;
    if (key == GLFW_KEY_P)
      freezeCullingView = !freezeCullingView;
    if (key == GLFW_KEY_N)
      cullingMode = CullingMode_None;
    if (key == GLFW_KEY_C)
      cullingMode = CullingMode_CPU;
    if (key == GLFW_KEY_G)
      cullingMode = CullingMode_GPU;
  });

  // pretransform bounding boxes to world space
  std::vector<BoundingBox> reorderedBoxes;
  reorderedBoxes.resize(scene.globalTransform.size());
  for (auto& p : scene.meshForNode) {
    reorderedBoxes[p.first] = meshData.boxes[p.second].getTransformed(scene.globalTransform[p.first]);
  }

  lvk::Holder<lvk::BufferHandle> bufferAABBs = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_Device,
      .size      = reorderedBoxes.size() * sizeof(BoundingBox),
      .data      = reorderedBoxes.data(),
      .debugName = "Buffer: AABBs",
  });

  // create the scene AABB in world space
  BoundingBox bigBoxWS = reorderedBoxes.front();
  for (const auto& b : reorderedBoxes) {
    bigBoxWS.combinePoint(b.min_);
    bigBoxWS.combinePoint(b.max_);
  }

  struct CullingData {
    vec4 frustumPlanes[6];
    vec4 frustumCorners[8];
    uint32_t numMeshesToCull  = 0;
    uint32_t numVisibleMeshes = 0; // GPU
  } emptyCullingData;

  int numVisibleMeshes = 0; // CPU

  // round-robin
  const lvk::BufferDesc cullingDataDesc = {
    .usage     = lvk::BufferUsageBits_Storage,
    .storage   = lvk::StorageType_HostVisible,
    .size      = sizeof(CullingData),
    .data      = &emptyCullingData,
    .debugName = "Buffer: CullingData 0",
  };
  lvk::Holder<lvk::BufferHandle> bufferCullingData[] = {
    ctx->createBuffer(cullingDataDesc, "Buffer: CullingData 0"),
    ctx->createBuffer(cullingDataDesc, "Buffer: CullingData 1"),
  };
  lvk::SubmitHandle submitHandle[LVK_ARRAY_NUM_ELEMENTS(bufferCullingData)] = {};

  uint32_t currentBufferId = 0; // for culling stats

  struct {
    uint64_t commands;
    uint64_t drawData;
    uint64_t AABBs;
    uint64_t meshes;
  } pcCulling = {
    .commands = 0,
    .drawData = ctx->gpuAddress(mesh.bufferDrawData_),
    .AABBs    = ctx->gpuAddress(bufferAABBs),
  };

  VKIndirectBuffer11 meshesOpaque(ctx, mesh.numMeshes_, lvk::StorageType_HostVisible);
  VKIndirectBuffer11 meshesTransparent(ctx, mesh.numMeshes_, lvk::StorageType_HostVisible);

  auto isTransparent = [&meshData, &mesh](const DrawIndexedIndirectCommand& c) -> bool {
    const uint32_t mtlIndex = mesh.drawData_[c.baseInstance].materialId;
    const Material& mtl     = meshData.materials[mtlIndex];
    return (mtl.flags & sMaterialFlags_Transparent) > 0;
  };

  mesh.indirectBuffer_.selectTo(meshesOpaque, [&isTransparent](const DrawIndexedIndirectCommand& c) -> bool { return !isTransparent(c); });
  mesh.indirectBuffer_.selectTo(
      meshesTransparent, [&isTransparent](const DrawIndexedIndirectCommand& c) -> bool { return isTransparent(c); });

  struct TransparentFragment {
    uint64_t rgba; // f16vec4
    float depth;
    uint32_t next;
  };

  const uint32_t kMaxOITFragments = sizeFb.width * sizeFb.height * kNumSamples;

  lvk::Holder<lvk::BufferHandle> bufferAtomicCounter = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_Device,
      .size      = sizeof(uint32_t),
      .debugName = "Buffer: atomic counter",
  });

  lvk::Holder<lvk::BufferHandle> bufferListsOIT = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_Device,
      .size      = sizeof(TransparentFragment) * kMaxOITFragments,
      .debugName = "Buffer: transparency lists",
  });

  lvk::Holder<lvk::TextureHandle> texHeadsOIT = ctx->createTexture({
      .format     = lvk::Format_R_UI32,
      .dimensions = sizeFb,
      .usage      = lvk::TextureUsageBits_Storage,
      .debugName  = "oitHeads",
  });

  const struct OITBuffer {
    uint64_t bufferAtomicCounter;
    uint64_t bufferTransparencyLists;
    uint32_t texHeadsOIT;
    uint32_t maxOITFragments;
  } oitBufferData = {
    .bufferAtomicCounter     = ctx->gpuAddress(bufferAtomicCounter),
    .bufferTransparencyLists = ctx->gpuAddress(bufferListsOIT),
    .texHeadsOIT             = texHeadsOIT.index(),
    .maxOITFragments         = kMaxOITFragments,
  };

  lvk::Holder<lvk::BufferHandle> bufferOIT = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_Device,
      .size      = sizeof(oitBufferData),
      .data      = &oitBufferData,
      .debugName = "Buffer: OIT",
  });

  // update shadow map
  LightParams prevLight = { .depthBiasConst = 0 };

  // clang-format off
  const mat4 scaleBias = mat4(0.5, 0.0, 0.0, 0.0,
                              0.0, 0.5, 0.0, 0.0,
                              0.0, 0.0, 1.0, 0.0,
                              0.5, 0.5, 0.0, 1.0);
  // clang-format on

  auto clearTransparencyBuffers = [&bufferAtomicCounter, &texHeadsOIT, sizeFb](lvk::ICommandBuffer& buf) {
    buf.cmdClearColorImage(texHeadsOIT, { .uint32 = { 0xffffffff } });
    buf.cmdFillBuffer(bufferAtomicCounter, 0, sizeof(uint32_t), 0);
  };

  struct {
    uint32_t texColor;
    uint32_t texLuminance;
    uint32_t texBloom;
    uint32_t sampler;
    int drawMode = ToneMapping_Uchimura;

    float exposure      = 0.95f;
    float bloomStrength = 0.0f;

    // Reinhard
    float maxWhite = 1.0f;

    // Uchimura
    float P = 1.0f;  // max display brightness
    float a = 1.05f; // contrast
    float m = 0.1f;  // linear section start
    float l = 0.8f;  // linear section length
    float c = 3.0f;  // black tightness
    float b = 0.0f;  // pedestal

    // Khronos PBR
    float startCompression = 0.8f;  // highlight compression start
    float desaturation     = 0.15f; // desaturation speed
  } pcHDR = {
    .texColor     = texSceneColor.index(),
    .texLuminance = texAdaptedLum[0].index(), // 1x1
    .texBloom     = texBloomPass.index(),
    .sampler      = samplerClamp.index(),
  };

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    LVK_PROFILER_FUNCTION();

    const mat4 view = app.camera_.getViewMatrix();
    const mat4 proj = glm::perspective(45.0f, aspectRatio, 0.01f, 200.0f);

    // prepare culling data
    if (!freezeCullingView)
      cullingView = app.camera_.getViewMatrix();

    CullingData cullingData = {
      .numMeshesToCull = static_cast<uint32_t>(meshesOpaque.drawCommands_.size()),
    };

    getFrustumPlanes(proj * cullingView, cullingData.frustumPlanes);
    getFrustumCorners(proj * cullingView, cullingData.frustumCorners);

    // light
    const glm::mat4 rot1 = glm::rotate(mat4(1.f), glm::radians(light.theta), glm::vec3(0, 1, 0));
    const glm::mat4 rot2 = glm::rotate(rot1, glm::radians(light.phi), glm::vec3(1, 0, 0));
    const vec3 lightDir  = glm::normalize(vec3(rot2 * vec4(0.0f, -1.0f, 0.0f, 1.0f)));
    const mat4 lightView = glm::lookAt(glm::vec3(0.0f), lightDir, vec3(0, 0, 1));

    // transform scene AABB to light space
    const BoundingBox boxLS = bigBoxWS.getTransformed(lightView);
    const mat4 lightProj    = glm::orthoLH_ZO(boxLS.min_.x, boxLS.max_.x, boxLS.min_.y, boxLS.max_.y, boxLS.max_.z, boxLS.min_.z);

    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    {
      mesh.processLoadedTextures(buf);

      clearTransparencyBuffers(buf);

      // cull scene (we cull only opaque meshes)
      if (cullingMode == CullingMode_None) {
        numVisibleMeshes                = static_cast<uint32_t>(scene.meshForNode.size()); // all meshes
        DrawIndexedIndirectCommand* cmd = meshesOpaque.getDrawIndexedIndirectCommandPtr();
        for (auto& c : meshesOpaque.drawCommands_) {
          (cmd++)->instanceCount = 1;
        }
        ctx->flushMappedMemory(meshesOpaque.bufferIndirect_, 0, meshesOpaque.drawCommands_.size() * sizeof(DrawIndexedIndirectCommand));
      } else if (cullingMode == CullingMode_CPU) {
        numVisibleMeshes =
            static_cast<uint32_t>(meshesTransparent.drawCommands_.size()); // all transparent meshes are visible - we don't cull them

        DrawIndexedIndirectCommand* cmd = meshesOpaque.getDrawIndexedIndirectCommandPtr();
        for (size_t i = 0; i != meshesOpaque.drawCommands_.size(); i++) {
          const BoundingBox box  = reorderedBoxes[mesh.drawData_[cmd->baseInstance].transformId];
          const uint32_t count   = isBoxInFrustum(cullingData.frustumPlanes, cullingData.frustumCorners, box) ? 1 : 0;
          (cmd++)->instanceCount = count;
          numVisibleMeshes += count;
        }
        ctx->flushMappedMemory(meshesOpaque.bufferIndirect_, 0, meshesOpaque.drawCommands_.size() * sizeof(DrawIndexedIndirectCommand));
      } else if (cullingMode == CullingMode_GPU) {
        buf.cmdBindComputePipeline(pipelineCulling);
        pcCulling.meshes   = ctx->gpuAddress(bufferCullingData[currentBufferId]);
        pcCulling.commands = ctx->gpuAddress(meshesOpaque.bufferIndirect_);
        cullingData.numVisibleMeshes =
            static_cast<uint32_t>(meshesTransparent.drawCommands_.size()); // all transparent meshes are visible - we don't cull them
        buf.cmdPushConstants(pcCulling);
        buf.cmdUpdateBuffer(bufferCullingData[currentBufferId], cullingData);
        buf.cmdDispatch({ 1 + cullingData.numMeshesToCull / 64 }, { .buffers = { lvk::BufferHandle(meshesOpaque.bufferIndirect_) } });
      }

      // 0. Update shadow map
      if (prevLight != light) {
        prevLight = light;
        buf.cmdBeginRendering(
            lvk::RenderPass{
                .depth = {.loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f}
        },
            lvk::Framebuffer{ .depthStencil = { .texture = texShadowMap } });
        buf.cmdPushDebugGroupLabel("Shadow map", 0xff0000ff);
        buf.cmdSetDepthBias(light.depthBiasConst, light.depthBiasSlope);
        buf.cmdSetDepthBiasEnable(true);
        mesh.draw(buf, pipelineShadow, lightView, lightProj);
        buf.cmdSetDepthBiasEnable(false);
        buf.cmdPopDebugGroupLabel();
        buf.cmdEndRendering();
        lightData.viewProjBias  = scaleBias * lightProj * lightView;
        lightData.lightDir      = vec4(lightDir, 0.0f);
        lightData.shadowTexture = texShadowMap.index();
        lightData.shadowSampler = samplerShadow.index();
      }

      // update light + ray-traced AO parameters (uploaded every frame for frameId/live tweaks)
      lightData.tlas              = aoTLAS.index();
      lightData.frameId           = aoTimeVaryingNoise ? aoFrameId++ : 0;
      lightData.hashSlot          = ctx->gpuAddress(bufferAOHash);
      lightData.enableAO          = aoEnable ? 1u : 0u;
      lightData.enableSpatialHash = aoSpatialHash ? 1u : 0u;
      lightData.enableFiltering   = aoFiltering ? 1u : 0u;
      lightData.aoSamples         = (uint32_t)aoSamples;
      lightData.aoRadius          = aoRadius;
      lightData.aoPower           = aoPower;
      lightData.sp                = aoPixelSize;
      lightData.smin              = aoMinCellSize;
      lightData.maxSamples        = (uint32_t)aoMaxSamples;
      lightData.hashMapSize       = kAOHashMapSize;
      lightData.resolutionY       = (float)sizeFb.height;
      lightData.projScaleY        = proj[1][1];
      lightData.a2cThickness      = a2cThickness;
      buf.cmdUpdateBuffer(bufferLight, lightData);

      // 1. Render scene
      const lvk::Framebuffer framebufferMSAA = {
        .color        = { { .texture = msaaColor, .resolveTexture = texOpaqueColor } },
        .depthStencil = { .texture = msaaDepth, .resolveTexture = texOpaqueDepth },
      };
      buf.cmdBeginRendering(
          lvk::RenderPass{
              .color = { { .loadOp = lvk::LoadOp_Clear, .storeOp = lvk::StoreOp_DontCare, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
              .depth = { .loadOp = lvk::LoadOp_Clear, .storeOp = lvk::StoreOp_DontCare, .clearDepth = 1.0f }
      },
          framebufferMSAA,
          { .buffers = {
                lvk::BufferHandle(meshesOpaque.bufferIndirect_),
                lvk::BufferHandle(bufferAOHash),
                lvk::BufferHandle(bufferLight),
            } });
      skyBox.draw(buf, view, proj);
      const struct {
        mat4 viewProj;
        vec4 cameraPos;
        uint64_t bufferTransforms;
        uint64_t bufferDrawData;
        uint64_t bufferMaterials;
        uint64_t bufferOIT;
        uint64_t bufferLight;
        uint32_t texSkybox;
        uint32_t texSkyboxIrradiance;
      } pc = {
        .viewProj            = proj * view,
        .cameraPos           = vec4(app.camera_.getPosition(), 1.0f),
        .bufferTransforms    = ctx->gpuAddress(mesh.bufferTransforms_),
        .bufferDrawData      = ctx->gpuAddress(mesh.bufferDrawData_),
        .bufferMaterials     = ctx->gpuAddress(mesh.bufferMaterials_),
        .bufferOIT           = ctx->gpuAddress(bufferOIT),
        .bufferLight         = ctx->gpuAddress(bufferLight),
        .texSkybox           = skyBox.texSkybox.index(),
        .texSkyboxIrradiance = skyBox.texSkyboxIrradiance.index(),
      };
      if (drawMeshesOpaque) {
        buf.cmdPushDebugGroupLabel("Mesh opaque", 0xff0000ff);
        // alpha-to-coverage needs MSAA to produce sub-pixel coverage for the foliage
        const VKPipeline11& opaquePipeline = (enableA2C && !drawWireframe) ? pipelineOpaqueA2C : pipelineOpaque;
        mesh.draw(
            buf, opaquePipeline, &pc, sizeof(pc), { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true }, drawWireframe,
            &meshesOpaque);
        buf.cmdPopDebugGroupLabel();
      }
      if (drawMeshesTransparent) {
        buf.cmdPushDebugGroupLabel("Mesh transparent", 0xff0000ff);
        mesh.draw(
            buf, pipelineTransparent, &pc, sizeof(pc), { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = false }, drawWireframe,
            &meshesTransparent);
        buf.cmdPopDebugGroupLabel();
      }
      app.drawGrid(buf, proj, vec3(0, -1.0f, 0), kNumSamples, kOffscreenFormat);
      canvas3d.clear();
      canvas3d.setMatrix(proj * view);
      if (freezeCullingView)
        canvas3d.frustum(cullingView, proj, vec4(1, 1, 0, 1));
      if (drawLightFrustum)
        canvas3d.frustum(lightView, lightProj, vec4(1, 1, 0, 1));
      // render all bounding boxes
      if (drawBoxes) {
		  // draw transparent boxes (always visible)
        for (auto& c : meshesTransparent.drawCommands_) {
          const uint32_t transformId = mesh.drawData_[c.baseInstance].transformId;
          const uint32_t meshId      = scene.meshForNode[transformId];
          const BoundingBox box      = meshData.boxes[meshId];
          canvas3d.box(scene.globalTransform[transformId], box, vec4(0, 1, 0, 1));
        }
        // draw opaque boxes
        const DrawIndexedIndirectCommand* cmd = meshesOpaque.getDrawIndexedIndirectCommandPtr();
        for (auto& c : meshesOpaque.drawCommands_) {
          const uint32_t transformId = mesh.drawData_[c.baseInstance].transformId;
          const uint32_t meshId      = scene.meshForNode[transformId];
          const BoundingBox box      = meshData.boxes[meshId];
          canvas3d.box(scene.globalTransform[transformId], box, (cmd++)->instanceCount ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1));
        }
      }
      canvas3d.render(*ctx.get(), framebufferMSAA, buf, kNumSamples);
      buf.cmdEndRendering();

      // Note: ambient occlusion is now ray-traced inside the opaque fragment shader (AO.sp) and
      // already baked into texOpaqueColor - no separate SSAO compute/blur/combine passes here.

      // combine OIT
      const lvk::Framebuffer framebufferOffscreen = {
        .color = { { .texture = texSceneColor } },
      };
      // clang-format off
      buf.cmdBeginRendering(
          lvk::RenderPass{ .color = {{ .loadOp = lvk::LoadOp_Load, .storeOp = lvk::StoreOp_Store }} },
          framebufferOffscreen,
          { .sampledImages = { lvk::TextureHandle(texOpaqueColor) },
            .storageImages = { lvk::TextureHandle(texHeadsOIT) },
            .buffers  = { lvk::BufferHandle(bufferListsOIT) } });
		// clang-format on
      const struct {
        uint64_t bufferTransparencyLists;
        uint32_t texColor;
        uint32_t texHeadsOIT;
        float time;
        float opacityBoost;
        uint32_t showHeatmap;
      } pcOIT = {
        .bufferTransparencyLists = ctx->gpuAddress(bufferListsOIT),
        .texColor                = texOpaqueColor.index(),
        .texHeadsOIT             = texHeadsOIT.index(),
        .time                    = static_cast<float>(glfwGetTime()),
        .opacityBoost            = oitOpacityBoost,
        .showHeatmap             = oitShowHeatmap ? 1u : 0u,
      };
      buf.cmdBindRenderPipeline(pipelineOIT);
      buf.cmdPushConstants(pcOIT);
      buf.cmdBindDepthState({});
      buf.cmdDraw(3);
      buf.cmdEndRendering();

      // 2. Bright pass - extract luminance and bright areas
      const struct {
        uint32_t texColor;
        uint32_t texOut;
        uint32_t texLuminance;
        uint32_t sampler;
        float exposure;
      } pcBrightPass = {
        .texColor     = texSceneColor.index(),
        .texOut       = texBrightPass.index(),
        .texLuminance = texLumViews[0].index(),
        .sampler      = samplerClamp.index(),
        .exposure     = pcHDR.exposure,
      };
      buf.cmdBindComputePipeline(pipelineBrightPass);
      buf.cmdPushConstants(pcBrightPass);
      buf.cmdDispatch(
          sizeBloom.divide2D(16), {
                                      .sampledImages = { lvk::TextureHandle(texSceneColor) },
                                      .storageImages = { lvk::TextureHandle(texLumViews[0]) },
                                  });
      buf.cmdGenerateMipmap(texLumViews[0]);

      // 2.1. Bloom
      struct BlurPC {
        uint32_t texIn;
        uint32_t texOut;
        uint32_t sampler;
      };
      struct StreaksPC {
        uint32_t texIn;
        uint32_t texOut;
        uint32_t texRotationPattern;
        uint32_t sampler;
      };
      struct BlurPass {
        lvk::TextureHandle texIn;
        lvk::TextureHandle texOut;
      };
      std::vector<BlurPass> passes;
      {
        passes.reserve(2 * hdrNumBloomPasses);
        passes.push_back({ texBrightPass, texBloom[0] });
        for (int i = 0; i != hdrNumBloomPasses - 1; i++) {
          passes.push_back({ texBloom[0], texBloom[1] });
          passes.push_back({ texBloom[1], texBloom[0] });
        }
        passes.push_back({ texBloom[0], texBloomPass });
      }
      for (uint32_t i = 0; i != passes.size(); i++) {
        const BlurPass p = passes[i];
        buf.cmdBindComputePipeline(i & 1 ? pipelineBloomX : pipelineBloomY);
        buf.cmdPushConstants(BlurPC{
            .texIn   = p.texIn.index(),
            .texOut  = p.texOut.index(),
            .sampler = samplerClamp.index(),
        });
        if (hdrEnableBloom)
          buf.cmdDispatch(
              sizeBloom.divide2D(16), {
                                          .sampledImages = { p.texIn, lvk::TextureHandle(texBrightPass) },
                                          .storageImages = { p.texOut },
          });
      }

      // 3. Light adaptation pass
      const struct {
        uint32_t texCurrSceneLuminance;
        uint32_t texPrevAdaptedLuminance;
        uint32_t texNewAdaptedLuminance;
        float adaptationSpeed;
      } pcAdaptationPass = {
        .texCurrSceneLuminance   = texLumViews[LVK_ARRAY_NUM_ELEMENTS(texLumViews) - 1].index(), // 1x1,
        .texPrevAdaptedLuminance = texAdaptedLum[0].index(),
        .texNewAdaptedLuminance  = texAdaptedLum[1].index(),
        .adaptationSpeed         = deltaSeconds * hdrAdaptationSpeed,
      };
      buf.cmdBindComputePipeline(pipelineAdaptationPass);
      buf.cmdPushConstants(pcAdaptationPass);
      // clang-format off
      buf.cmdDispatch(
          { 1, 1, 1 },
          { .storageImages = {
                lvk::TextureHandle(texLumViews[0]), // transition the entire mip-pyramid
                lvk::TextureHandle(texAdaptedLum[0]),
                lvk::TextureHandle(texAdaptedLum[1]),
            } });
		// clang-format on

      // HDR light adaptation: render tone-mapped scene into a swapchain image
      const lvk::RenderPass renderPassMain = {
        .color = { { .loadOp = lvk::LoadOp_Load, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
      };
      const lvk::Framebuffer framebufferMain = {
        .color = { { .texture = ctx->getCurrentSwapchainTexture() } },
      };

      // transition the entire mip-pyramid
      buf.cmdBeginRendering(renderPassMain, framebufferMain, { .sampledImages = { lvk::TextureHandle(texAdaptedLum[1]) } });

      buf.cmdBindRenderPipeline(pipelineToneMap);
      buf.cmdPushConstants(pcHDR);
      buf.cmdBindDepthState({});
      buf.cmdDraw(3); // fullscreen triangle

      app.imgui_->beginFrame(framebufferMain);
      app.drawFPS();
      app.drawMemo();

      // render UI
      {
        const ImGuiViewport* v  = ImGui::GetMainViewport();
        const float windowWidth = v->WorkSize.x / 5;
        ImGui::SetNextWindowPos(ImVec2(10, 200));
        ImGui::SetNextWindowSizeConstraints(ImVec2(-1, 0), ImVec2(-1, v->WorkSize.y - 210));
        ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Checkbox("Draw wireframe", &drawWireframe);
        ImGui::Text("Draw:");
        const float indentSize = 16.0f;
        ImGui::Indent(indentSize);
        ImGui::Checkbox("Opaque meshes", &drawMeshesOpaque);
        ImGui::Checkbox("Transparent meshes", &drawMeshesTransparent);
        ImGui::Checkbox("Bounding boxes", &drawBoxes);
        ImGui::Checkbox("Light frustum", &drawLightFrustum);
        ImGui::Checkbox("Alpha-to-coverage (foliage)", &enableA2C);
        ImGui::BeginDisabled(!enableA2C);
        ImGui::SliderFloat("A2C thickness", &a2cThickness, 0.0f, 10.0f);
        ImGui::EndDisabled();
        ImGui::Unindent(indentSize);
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Frustum Culling")) {
          ImGui::Indent(indentSize);
          ImGui::RadioButton("None (N)", &cullingMode, CullingMode_None);
          ImGui::RadioButton("CPU  (C)", &cullingMode, CullingMode_CPU);
          ImGui::RadioButton("GPU  (G)", &cullingMode, CullingMode_GPU);
          ImGui::Unindent(indentSize);
          ImGui::Checkbox("Freeze culling frustum (P)", &freezeCullingView);
          ImGui::Separator();
          ImGui::Text("Visible meshes: %i", numVisibleMeshes);
          ImGui::Separator();
        }
        if (ImGui::CollapsingHeader("Order-Independent Transparency")) {
          ImGui::Indent(indentSize);
          ImGui::SliderFloat("Opacity boost", &oitOpacityBoost, -1.0f, +1.0f);
          ImGui::Checkbox("Show transparency heat map", &oitShowHeatmap);
          ImGui::Unindent(indentSize);
          ImGui::Separator();
        }
        if (ImGui::CollapsingHeader("Shadow Mapping")) {
          ImGui::Text("Depth bias factor:");
          ImGui::Indent(indentSize);
          ImGui::SliderFloat("Constant", &light.depthBiasConst, 0.0f, 5.0f);
          ImGui::SliderFloat("Slope", &light.depthBiasSlope, 0.0f, 5.0f);
          ImGui::Unindent(indentSize);
          ImGui::Separator();
          ImGui::Text("Light angles:");
          ImGui::Indent(indentSize);
          ImGui::SliderFloat("Theta", &light.theta, -180.0f, +180.0f);
          ImGui::SliderFloat("Phi", &light.phi, -85.0f, +85.0f);
          ImGui::Unindent(indentSize);
          ImGui::Image(texShadowMap.index(), ImVec2(512, 512));
        }
        if (ImGui::CollapsingHeader("Ray Traced Ambient Occlusion")) {
          ImGui::Indent(indentSize);
          ImGui::Checkbox("Enable AO", &aoEnable);
          ImGui::BeginDisabled(!aoEnable);
          ImGui::SliderFloat("AO radius", &aoRadius, 0.1f, 8.0f);
          ImGui::SliderFloat("AO power", &aoPower, 0.5f, 2.0f);
          ImGui::Checkbox("Time-varying noise", &aoTimeVaryingNoise);
          ImGui::Separator();
          ImGui::Checkbox("Spatial hashing", &aoSpatialHash);
          ImGui::Indent(indentSize);
          ImGui::BeginDisabled(!aoSpatialHash);
          ImGui::SliderFloat("Pixel size (sp)", &aoPixelSize, 1.0f, 10.0f);
          ImGui::SliderFloat("Min cell size", &aoMinCellSize, 0.005f, 0.2f);
          ImGui::SliderInt("Max samples/cell", &aoMaxSamples, 16, 250);
          ImGui::Checkbox("Trilinear filtering", &aoFiltering);
          if (ImGui::Button("Reset hash map"))
            aoResetHash = true;
          ImGui::EndDisabled();
          ImGui::Unindent(indentSize);
          ImGui::BeginDisabled(aoSpatialHash);
          ImGui::SliderInt("Per-pixel samples", &aoSamples, 1, 32);
          ImGui::EndDisabled();
          ImGui::EndDisabled();
          ImGui::Unindent(indentSize);
          ImGui::Separator();
        }
        if (ImGui::CollapsingHeader("Tone Mapping and HDR")) {
          ImGui::Indent(indentSize);
          ImGui::Checkbox("Draw tone mapping curves", &hdrDrawCurves);
          ImGui::SliderFloat("Exposure", &pcHDR.exposure, 0.1f, 2.0f);
          ImGui::SliderFloat("Adaptation speed", &hdrAdaptationSpeed, 1.0f, 10.0f);
          ImGui::Checkbox("Enable bloom", &hdrEnableBloom);
          pcHDR.bloomStrength = hdrEnableBloom ? hdrBloomStrength : 0.0f;
          ImGui::BeginDisabled(!hdrEnableBloom);
          ImGui::Indent(indentSize);
          ImGui::SliderFloat("Bloom strength", &hdrBloomStrength, 0.0f, 1.0f);
          ImGui::SliderInt("Bloom num passes", &hdrNumBloomPasses, 1, 5);
          ImGui::Unindent(indentSize);
          ImGui::EndDisabled();
          ImGui::Text("Tone mapping mode:");
          ImGui::RadioButton("None", &pcHDR.drawMode, ToneMapping_None);
          ImGui::RadioButton("Reinhard", &pcHDR.drawMode, ToneMapping_Reinhard);
          if (pcHDR.drawMode == ToneMapping_Reinhard) {
            ImGui::Indent(indentSize);
            ImGui::BeginDisabled(pcHDR.drawMode != ToneMapping_Reinhard);
            ImGui::SliderFloat("Max white", &pcHDR.maxWhite, 0.5f, 2.0f);
            ImGui::EndDisabled();
            ImGui::Unindent(indentSize);
          }
          ImGui::RadioButton("Uchimura", &pcHDR.drawMode, ToneMapping_Uchimura);
          if (pcHDR.drawMode == ToneMapping_Uchimura) {
            ImGui::Indent(indentSize);
            ImGui::BeginDisabled(pcHDR.drawMode != ToneMapping_Uchimura);
            ImGui::SliderFloat("Max brightness", &pcHDR.P, 1.0f, 2.0f);
            ImGui::SliderFloat("Contrast", &pcHDR.a, 0.0f, 5.0f);
            ImGui::SliderFloat("Linear section start", &pcHDR.m, 0.0f, 1.0f);
            ImGui::SliderFloat("Linear section length", &pcHDR.l, 0.0f, 1.0f);
            ImGui::SliderFloat("Black tightness", &pcHDR.c, 1.0f, 3.0f);
            ImGui::SliderFloat("Pedestal", &pcHDR.b, 0.0f, 1.0f);
            ImGui::EndDisabled();
            ImGui::Unindent(indentSize);
          }
          ImGui::RadioButton("Khronos PBR Neutral", &pcHDR.drawMode, ToneMapping_KhronosPBR);
          if (pcHDR.drawMode == ToneMapping_KhronosPBR) {
            ImGui::Indent(indentSize);
            ImGui::SliderFloat("Highlight compression start", &pcHDR.startCompression, 0.0f, 1.0f);
            ImGui::SliderFloat("Desaturation speed", &pcHDR.desaturation, 0.0f, 1.0f);
            ImGui::Unindent(indentSize);
          }
          ImGui::Separator();

          ImGui::Text("Average luminance 1x1:");
          ImGui::Image(pcHDR.texLuminance, ImVec2(128, 128));
          ImGui::Separator();
          ImGui::Text("Bright pass:");
          ImGui::Image(texBrightPass.index(), ImVec2(windowWidth, windowWidth / aspectRatio));
          ImGui::Text("Bloom pass:");
          ImGui::Image(texBloomPass.index(), ImVec2(windowWidth, windowWidth / aspectRatio));
          ImGui::Separator();
          ImGui::Text("Luminance pyramid 512x512");
          for (uint32_t l = 0; l != LVK_ARRAY_NUM_ELEMENTS(texLumViews); l++) {
            ImGui::Image(texLumViews[l].index(), ImVec2((int)windowWidth >> l, ((int)windowWidth >> l)));
          }
          ImGui::Unindent(indentSize);
          ImGui::Separator();
        }
        ImGui::End();

        if (hdrDrawCurves) {
          const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
          ImGui::SetNextWindowBgAlpha(0.8f);
          ImGui::SetNextWindowPos({ width * 0.6f, height * 0.7f }, ImGuiCond_Appearing);
          ImGui::SetNextWindowSize({ width * 0.4f, height * 0.3f });
          ImGui::Begin("Tone mapping curve", nullptr, flags);
          const int kNumGraphPoints = 1001;
          float xs[kNumGraphPoints];
          float ysUchimura[kNumGraphPoints];
          float ysReinhard2[kNumGraphPoints];
          float ysKhronosPBR[kNumGraphPoints];
          for (int i = 0; i != kNumGraphPoints; i++) {
            xs[i]           = float(i) / kNumGraphPoints;
            ysUchimura[i]   = uchimura(xs[i], pcHDR.P, pcHDR.a, pcHDR.m, pcHDR.l, pcHDR.c, pcHDR.b);
            ysReinhard2[i]  = reinhard2(xs[i], pcHDR.maxWhite);
            ysKhronosPBR[i] = PBRNeutralToneMapping(xs[i], pcHDR.startCompression, pcHDR.desaturation);
          }
          if (ImPlot::BeginPlot("Tone mapping curves", { width * 0.4f, height * 0.3f }, ImPlotFlags_NoInputs)) {
            ImPlot::SetupAxes("Input", "Output");
            ImPlot::PlotLine("Uchimura", xs, ysUchimura, kNumGraphPoints);
            ImPlot::PlotLine("Reinhard", xs, ysReinhard2, kNumGraphPoints);
            ImPlot::PlotLine("Khronos PBR", xs, ysKhronosPBR, kNumGraphPoints);
            ImPlot::EndPlot();
          }
          ImGui::End();
        }
      }

      app.imgui_->endFrame(buf);

      buf.cmdEndRendering();

      // clear the world-space AO cache on request (e.g. after changing cell-size parameters)
      if (aoResetHash) {
        buf.cmdFillBuffer(bufferAOHash, 0, lvk::LVK_WHOLE_SIZE, 0);
        aoResetHash = false;
      }
    }
    submitHandle[currentBufferId] = ctx->submit(buf, ctx->getCurrentSwapchainTexture());

    // retrieve culling results
    currentBufferId = (currentBufferId + 1) % LVK_ARRAY_NUM_ELEMENTS(bufferCullingData);

    if (cullingMode == CullingMode_GPU && app.fpsCounter_.numFrames_ > 1) {
      ctx->wait(submitHandle[currentBufferId]);
      ctx->download(bufferCullingData[currentBufferId], &numVisibleMeshes, sizeof(uint32_t), offsetof(CullingData, numVisibleMeshes));
    }

    // swap ping-pong textures
    std::swap(texAdaptedLum[0], texAdaptedLum[1]);
  });

  ctx.release();

  return 0;
}
