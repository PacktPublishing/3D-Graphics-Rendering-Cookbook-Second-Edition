#pragma once

class Skybox
{
public:
  Skybox(
      const std::unique_ptr<lvk::IContext>& ctx, const char* skyboxTexture, const char* skyboxIrradiance, lvk::Format colorFormat,
      lvk::Format depthFormat, uint32_t numSamples = 1)
  {
    texSkybox           = loadTexture(ctx, skyboxTexture, lvk::TextureType_Cube);
    texSkyboxIrradiance = loadTexture(ctx, skyboxIrradiance, lvk::TextureType_Cube);

    vertSkybox     = loadShaderModule(ctx, "Chapter08/02_SceneGraph/src/skybox.vert");
    fragSkybox     = loadShaderModule(ctx, "Chapter08/02_SceneGraph/src/skybox.frag");
    pipelineSkybox = ctx->createRenderPipeline({
        .smVert       = vertSkybox,
        .smFrag       = fragSkybox,
        .color        = { { .format = colorFormat } },
        .depthFormat  = depthFormat,
        .samplesCount = numSamples,
    });
  }

  void draw(lvk::ICommandBuffer& buf, const mat4& view, const mat4& proj) const
  {
    buf.cmdPushDebugGroupLabel("Skybox", 0xff0000ff);
    buf.cmdBindRenderPipeline(pipelineSkybox);
    const struct {
      mat4 mvp;
      uint32_t texSkybox;
    } pc = {
      .mvp       = proj * mat4(mat3(view)), // discard the translation
      .texSkybox = texSkybox.index(),
    };
    buf.cmdPushConstants(pc);
    buf.cmdBindDepthState({ .isDepthWriteEnabled = false });
    buf.cmdDraw(36);
    buf.cmdPopDebugGroupLabel();
  }

  lvk::Holder<lvk::TextureHandle> texSkybox;
  lvk::Holder<lvk::TextureHandle> texSkyboxIrradiance;
  lvk::Holder<lvk::ShaderModuleHandle> vertSkybox;
  lvk::Holder<lvk::ShaderModuleHandle> fragSkybox;
  lvk::Holder<lvk::RenderPipelineHandle> pipelineSkybox;
};
