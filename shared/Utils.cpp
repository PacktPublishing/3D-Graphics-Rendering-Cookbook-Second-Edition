#include <string.h>
#include <string>
#if !defined(__APPLE__)
#include <malloc.h>
#endif

#include <stb/stb_image.h>
#include <ktx.h>
#include <ktx-software/lib/gl_format.h>

#include "Utils.h"

#include <unordered_map>

// lvk::ShaderModuleHandle -> GLSL source code
std::unordered_map<void*, std::string> debugGLSLSourceCode;

bool endsWith(const char* s, const char* part)
{
  const size_t sLength    = strlen(s);
  const size_t partLength = strlen(part);
  return sLength < partLength ? false : strcmp(s + sLength - partLength, part) == 0;
}

std::string readShaderFile(const char* fileName)
{
  FILE* file = fopen(fileName, "r");

  if (!file) {
    LLOGW("I/O error. Cannot open shader file '%s'\n", fileName);
    return std::string();
  }

  fseek(file, 0L, SEEK_END);
  const auto bytesinfile = ftell(file);
  fseek(file, 0L, SEEK_SET);

  char* buffer           = (char*)alloca(bytesinfile + 1);
  const size_t bytesread = fread(buffer, 1, bytesinfile, file);
  fclose(file);

  buffer[bytesread] = 0;

  static constexpr unsigned char BOM[] = { 0xEF, 0xBB, 0xBF };

  if (bytesread > 3) {
    if (!memcmp(buffer, BOM, 3))
      memset(buffer, ' ', 3);
  }

  std::string code(buffer);

  while (code.find("#include ") != code.npos) {
    const auto pos = code.find("#include ");
    const auto p1  = code.find('<', pos);
    const auto p2  = code.find('>', pos);
    if (p1 == code.npos || p2 == code.npos || p2 <= p1) {
      LLOGW("Error while loading shader program: %s\n", code.c_str());
      return std::string();
    }
    const std::string name    = code.substr(p1 + 1, p2 - p1 - 1);
    const std::string include = readShaderFile(name.c_str());
    code.replace(pos, p2 - pos + 1, include.c_str());
  }

  return code;
}

glslang_stage_t glslangShaderStageFromFileName(const char* fileName)
{
  if (endsWith(fileName, ".vert"))
    return GLSLANG_STAGE_VERTEX;

  if (endsWith(fileName, ".frag"))
    return GLSLANG_STAGE_FRAGMENT;

  if (endsWith(fileName, ".geom"))
    return GLSLANG_STAGE_GEOMETRY;

  if (endsWith(fileName, ".comp"))
    return GLSLANG_STAGE_COMPUTE;

  if (endsWith(fileName, ".tesc"))
    return GLSLANG_STAGE_TESSCONTROL;

  if (endsWith(fileName, ".tese"))
    return GLSLANG_STAGE_TESSEVALUATION;

  return GLSLANG_STAGE_VERTEX;
}

lvk::ShaderStage lvkShaderStageFromFileName(const char* fileName)
{
  if (endsWith(fileName, ".vert"))
    return lvk::Stage_Vert;

  if (endsWith(fileName, ".frag"))
    return lvk::Stage_Frag;

  if (endsWith(fileName, ".geom"))
    return lvk::Stage_Geom;

  if (endsWith(fileName, ".comp"))
    return lvk::Stage_Comp;

  if (endsWith(fileName, ".tesc"))
    return lvk::Stage_Tesc;

  if (endsWith(fileName, ".tese"))
    return lvk::Stage_Tese;

  return lvk::Stage_Vert;
}

lvk::Holder<lvk::ShaderModuleHandle> loadShaderModule(const std::unique_ptr<lvk::IContext>& ctx, const char* fileName) {
  const std::string code = readShaderFile(fileName);
  const lvk::ShaderStage stage = lvkShaderStageFromFileName(fileName);

  if (code.empty()) {
    return {};
  }

  lvk::Result res;

  lvk::Holder<lvk::ShaderModuleHandle> handle =
      ctx->createShaderModule({ code.c_str(), stage, (std::string("Shader Module: ") + fileName).c_str() }, &res);

  if (!res.isOk()) {
    return {};
  }

  debugGLSLSourceCode[handle.indexAsVoid()] = code;

  return handle;
}

lvk::Holder<lvk::TextureHandle> loadTexture(
    const std::unique_ptr<lvk::IContext>& ctx, const char* fileName, lvk::TextureType textureType, bool sRGB)
{
  const bool isKTX = endsWith(fileName, ".ktx") || endsWith(fileName, ".KTX");

  lvk::Result result;

  lvk::Holder<lvk::TextureHandle> texture;

  if (isKTX) {
    ktxTexture1* ktxTex = nullptr;

    if (!LVK_VERIFY(ktxTexture1_CreateFromNamedFile(fileName, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTex) == KTX_SUCCESS)) {
      LLOGW("Failed to load %s\n", fileName);
      assert(0);
      return {};
    }
    SCOPE_EXIT
    {
      ktxTexture_Destroy(ktxTexture(ktxTex));
    };

    lvk::Result result;

    const lvk::Format format = [](uint32_t glInternalFormat) {
      switch (glInternalFormat) {
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

    texture = ctx->createTexture(
        {
            .type             = textureType,
            .format           = format,
            .dimensions       = {ktxTex->baseWidth, ktxTex->baseHeight, 1},
            .usage            = lvk::TextureUsageBits_Sampled,
            .numMipLevels     = ktxTex->numLevels,
            .data             = ktxTex->pData,
            .dataNumMipLevels = ktxTex->numLevels,
            .debugName        = fileName,
    },
        fileName, &result);
  } else {
    LVK_ASSERT(textureType == lvk::TextureType_2D);

    int w, h, comp;
    const uint8_t* img = stbi_load(fileName, &w, &h, &comp, 4);

    SCOPE_EXIT
    {
      if (img)
        stbi_image_free((void*)img);
    };

    if (!img) {
      printf("Unable to load %s. File not found.\n", fileName);
      return {};
    };

    texture = ctx->createTexture(
        {
            .type       = lvk::TextureType_2D,
            .format     = sRGB ? lvk::Format_RGBA_SRGB8 : lvk::Format_RGBA_UN8,
            .dimensions = {(uint32_t)w, (uint32_t)h},
            .usage      = lvk::TextureUsageBits_Sampled,
            .data       = img,
            .debugName  = fileName,
    },
        fileName, &result);
  }

  if (!result.isOk()) {
    printf("Unable to load %s. Reason: %s\n", fileName, result.message);
  }

  return texture;
}
