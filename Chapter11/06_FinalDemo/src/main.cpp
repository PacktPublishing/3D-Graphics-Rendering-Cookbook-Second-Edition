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

bool drawMeshesOpaque      = true;
bool drawMeshesTransparent = true;
bool drawWireframe         = false;
bool drawBoxes             = false;
bool drawLightFrustum      = false;
// SSAO
bool ssaoEnable          = true;
bool ssaoEnableBlur      = true;
int ssaoNumBlurPasses    = 1;
float ssaoDepthThreshold = 30.0f; // bilateral blur
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

  lvk::Holder<lvk::TextureHandle> texOpaqueColorWithSSAO = ctx->createTexture({
      .format     = kOffscreenFormat,
      .dimensions = sizeFb,
      .usage      = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
      .debugName  = "opaqueColorWithSSAO",
  });
  // final HDR scene color (SSAO + OIT)
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
      .swizzle      = swizzle,
      .debugName    = "texLuminance",
  }) };

  for (uint32_t v = 1; v != LVK_ARRAY_NUM_ELEMENTS(texLumViews); v++) {
    texLumViews[v] = ctx->createTextureView(texLumViews[0], { .mipLevel = v, .swizzle = swizzle }, "texLumViews[]");
  }

  const uint16_t brightPixel = glm::packHalf1x16(50.0f);

  // ping-pong textures for iterative luminance adaptation
  const lvk::TextureDesc luminanceTextureDesc{
    .format     = lvk::Format_R_F16,
    .dimensions = {1, 1},
    .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
    .swizzle    = swizzle,
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
      .swizzle    = { .r = lvk::Swizzle_R, .g = lvk::Swizzle_R, .b = lvk::Swizzle_R, .a = lvk::Swizzle_1 },
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
  };
  lvk::Holder<lvk::BufferHandle> bufferLight = ctx->createBuffer({
      .usage     = lvk::BufferUsageBits_Storage,
      .storage   = lvk::StorageType_Device,
      .size      = sizeof(LightData),
      .debugName = "Buffer: light",
  });

  lvk::Holder<lvk::TextureHandle> texSSAO                = ctx->createTexture({
                     .format     = ctx->getSwapchainFormat(),
                     .dimensions = ctx->getDimensions(ctx->getCurrentSwapchainTexture()),
                     .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
                     .debugName  = "texSSAO",
  });
  lvk::Holder<lvk::TextureHandle> texBlur[]              = {
    ctx->createTexture({
                     .format     = ctx->getSwapchainFormat(),
                     .dimensions = ctx->getDimensions(ctx->getCurrentSwapchainTexture()),
                     .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
                     .debugName  = "texBlur0",
    }),
    ctx->createTexture({
                     .format     = ctx->getSwapchainFormat(),
                     .dimensions = ctx->getDimensions(ctx->getCurrentSwapchainTexture()),
                     .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
                     .debugName  = "texBlur1",
    }),
  };

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

  lvk::Holder<lvk::ShaderModuleHandle> compSSAO        = loadShaderModule(ctx, "Chapter10/04_SSAO/src/SSAO.comp");
  lvk::Holder<lvk::ComputePipelineHandle> pipelineSSAO = ctx->createComputePipeline({
      .smComp = compSSAO,
  });

  // SSAO
  lvk::Holder<lvk::TextureHandle> texRotations = loadTexture(ctx, "data/rot_texture.bmp");

  lvk::Holder<lvk::ShaderModuleHandle> compBlur         = loadShaderModule(ctx, "data/shaders/Blur.comp");
  lvk::Holder<lvk::ComputePipelineHandle> pipelineBlurX = ctx->createComputePipeline({
      .smComp   = compBlur,
      .specInfo = {.entries = { { .constantId = 0, .size = sizeof(uint32_t) } }, .data = &kHorizontal, .dataSize = sizeof(uint32_t)},
  });
  lvk::Holder<lvk::ComputePipelineHandle> pipelineBlurY = ctx->createComputePipeline({
      .smComp   = compBlur,
      .specInfo = {.entries = { { .constantId = 0, .size = sizeof(uint32_t) } }, .data = &kVertical, .dataSize = sizeof(uint32_t)},
  });

  lvk::Holder<lvk::ShaderModuleHandle> vertCombine       = loadShaderModule(ctx, "data/shaders/QuadFlip.vert");
  lvk::Holder<lvk::ShaderModuleHandle> fragCombine       = loadShaderModule(ctx, "Chapter10/04_SSAO/src/combine.frag");
  lvk::Holder<lvk::RenderPipelineHandle> pipelineCombineSSAO = ctx->createRenderPipeline({
      .smVert = vertCombine,
      .smFrag = fragCombine,
      .color  = { { .format = kOffscreenFormat } },
  });

  lvk::Holder<lvk::ShaderModuleHandle> compCulling        = loadShaderModule(ctx, "Chapter11/02_CullingGPU/src/FrustumCulling.comp");
  lvk::Holder<lvk::ComputePipelineHandle> pipelineCulling = ctx->createComputePipeline({
      .smComp = compCulling,
  });

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
    uint32_t texDepth;
    uint32_t texRotation;
    uint32_t texOut;
    uint32_t sampler;
    float zNear;
    float zFar;
    float radius;
    float attScale;
    float distScale;
  } pcSSAO = {
    .texDepth    = texOpaqueDepth.index(),
    .texRotation = texRotations.index(),
    .texOut      = texSSAO.index(),
    .sampler     = samplerClamp.index(),
    .zNear       = 0.01f,
    .zFar        = 200.0f,
    .radius      = 0.01f,
    .attScale    = 0.95f,
    .distScale   = 1.7f,
  };

  struct {
    uint32_t texColor;
    uint32_t texSSAO;
    uint32_t sampler;
    float scale;
    float bias;
  } pcCombineSSAO = {
    .texColor = texOpaqueColor.index(),
    .texSSAO  = texSSAO.index(),
    .sampler  = samplerClamp.index(),
    .scale    = 1.1f,
    .bias     = 0.1f,
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
    mesh.processLoadedTextures();

    const mat4 view = app.camera_.getViewMatrix();
    const mat4 proj = glm::perspective(45.0f, aspectRatio, pcSSAO.zNear, pcSSAO.zFar);

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
        buf.cmdDispatchThreadGroups(
            { 1 + cullingData.numMeshesToCull / 64 }, { .buffers = { lvk::BufferHandle(meshesOpaque.bufferIndirect_) } });
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
        buf.cmdUpdateBuffer(
            bufferLight, LightData{
                             .viewProjBias  = scaleBias * lightProj * lightView,
                             .lightDir      = vec4(lightDir, 0.0f),
                             .shadowTexture = texShadowMap.index(),
                             .shadowSampler = samplerShadow.index(),
                         });
      }

      // 1. Render scene
      const lvk::Framebuffer framebufferMSAA = {
        .color        = { { .texture = msaaColor, .resolveTexture = texOpaqueColor } },
        .depthStencil = { .texture = msaaDepth, .resolveTexture = texOpaqueDepth },
      };
      buf.cmdBeginRendering(
          lvk::RenderPass{
              .color = { { .loadOp = lvk::LoadOp_Clear, .storeOp = lvk::StoreOp_MsaaResolve, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
              .depth = { .loadOp = lvk::LoadOp_Clear, .storeOp = lvk::StoreOp_MsaaResolve, .clearDepth = 1.0f }
      },
          framebufferMSAA, { .buffers = { lvk::BufferHandle(meshesOpaque.bufferIndirect_) } });
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
        mesh.draw(
            buf, pipelineOpaque, &pc, sizeof(pc), { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true }, drawWireframe,
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

      // 2. Compute SSAO
      if (ssaoEnable) {
        buf.cmdBindComputePipeline(pipelineSSAO);
        buf.cmdPushConstants(pcSSAO);
        // clang-format off
        buf.cmdDispatchThreadGroups(
            { .width  = 1 + (uint32_t)sizeFb.width  / 16,
              .height = 1 + (uint32_t)sizeFb.height / 16 },
            { .textures = { lvk::TextureHandle(texOpaqueDepth),
                            lvk::TextureHandle(texSSAO) } });
		  // clang-format on

        // 3. Blur SSAO
        if (ssaoEnableBlur) {
          const lvk::Dimensions blurDim = {
            .width  = 1 + (uint32_t)sizeFb.width / 16,
            .height = 1 + (uint32_t)sizeFb.height / 16,
          };
          struct BlurPC {
            uint32_t texDepth;
            uint32_t texIn;
            uint32_t texOut;
            float depthThreshold;
          };
          struct BlurPass {
            lvk::TextureHandle texIn;
            lvk::TextureHandle texOut;
          };
          std::vector<BlurPass> passes;
          {
            passes.reserve(2 * ssaoNumBlurPasses);
            passes.push_back({ texSSAO, texBlur[0] });
            for (int i = 0; i != ssaoNumBlurPasses - 1; i++) {
              passes.push_back({ texBlur[0], texBlur[1] });
              passes.push_back({ texBlur[1], texBlur[0] });
            }
            passes.push_back({ texBlur[0], texSSAO });
          }
          for (uint32_t i = 0; i != passes.size(); i++) {
            const BlurPass p = passes[i];
            buf.cmdBindComputePipeline(i & 1 ? pipelineBlurX : pipelineBlurY);
            buf.cmdPushConstants(BlurPC{
                .texDepth       = texOpaqueDepth.index(),
                .texIn          = p.texIn.index(),
                .texOut         = p.texOut.index(),
                .depthThreshold = pcSSAO.zFar * ssaoDepthThreshold,
            });
            // clang-format off
            buf.cmdDispatchThreadGroups(blurDim, { .textures = {p.texIn, p.texOut, lvk::TextureHandle(texOpaqueDepth)} });
				// clang-format on
          }
        }

        // combine SSAO
        // clang-format off
        buf.cmdBeginRendering(
            { .color = {{ .loadOp = lvk::LoadOp_Load, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } }} },
            { .color = { { .texture = texOpaqueColorWithSSAO } } },
            { .textures = { lvk::TextureHandle(texSSAO), lvk::TextureHandle(texOpaqueColor) } });
        // clang-format on
        buf.cmdBindRenderPipeline(pipelineCombineSSAO);
        buf.cmdPushConstants(pcCombineSSAO);
        buf.cmdBindDepthState({});
        buf.cmdDraw(3);
        buf.cmdEndRendering();
      }

      // combine OIT
      const lvk::Framebuffer framebufferOffscreen = {
        .color = { { .texture = texSceneColor } },
      };
      // clang-format off
      buf.cmdBeginRendering(
          lvk::RenderPass{ .color = {{ .loadOp = lvk::LoadOp_Load, .storeOp = lvk::StoreOp_Store }} },
          framebufferOffscreen,
          { .textures = { lvk::TextureHandle(texHeadsOIT), lvk::TextureHandle(texOpaqueColor), lvk::TextureHandle(texOpaqueColorWithSSAO) },
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
        .texColor                = (ssaoEnable ? texOpaqueColorWithSSAO : texOpaqueColor).index(),
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
		// clang-format off
      buf.cmdDispatchThreadGroups(sizeBloom.divide2D(16), { .textures = {lvk::TextureHandle(texSceneColor), lvk::TextureHandle(texLumViews[0])} });
		// clang-format on
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
          buf.cmdDispatchThreadGroups(
              sizeBloom.divide2D(16), {
                                          .textures = {p.texIn, p.texOut, lvk::TextureHandle(texBrightPass)}
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
      buf.cmdDispatchThreadGroups(
          { 1, 1, 1 },
          { .textures = {
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
      buf.cmdBeginRendering(renderPassMain, framebufferMain, { .textures = { lvk::TextureHandle(texAdaptedLum[1]) } });

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
        if (ImGui::CollapsingHeader("SSAO")) {
          ImGui::Indent(indentSize);
          ImGui::Checkbox("Enable SSAO", &ssaoEnable);
          ImGui::BeginDisabled(!ssaoEnable);
          ImGui::Checkbox("Enable blur", &ssaoEnableBlur);
          ImGui::BeginDisabled(!ssaoEnableBlur);
          ImGui::SliderFloat("Blur depth threshold", &ssaoDepthThreshold, 0.0f, 50.0f);
          ImGui::SliderInt("Blur num passes", &ssaoNumBlurPasses, 1, 5);
          ImGui::EndDisabled();
          ImGui::SliderFloat("SSAO scale", &pcCombineSSAO.scale, 0.0f, 2.0f);
          ImGui::SliderFloat("SSAO bias", &pcCombineSSAO.bias, 0.0f, 0.3f);
          ImGui::SliderFloat("SSAO radius", &pcSSAO.radius, 0.001f, 0.02f);
          ImGui::SliderFloat("SSAO attenuation scale", &pcSSAO.attScale, 0.5f, 1.5f);
          ImGui::SliderFloat("SSAO distance scale", &pcSSAO.distScale, 0.0f, 2.0f);
          if (ssaoEnable)
            ImGui::Image(texSSAO.index(), ImVec2(windowWidth, windowWidth / aspectRatio));
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
