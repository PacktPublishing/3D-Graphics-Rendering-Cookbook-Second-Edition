#include <string.h>
#include <string>
#if !defined(__APPLE__)
#include <malloc.h>
#endif

#include "Utils.h"

#include <stb/stb_image.h>
#include <ktx.h>
#include <ktx-software/lib/gl_format.h>

#include <unordered_map>

// lvk::ShaderModuleHandle -> GLSL source code
std::unordered_map<uint32_t, std::string> debugGLSLSourceCode;

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
  const size_t bytesinfile = ftell(file);
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

VkShaderStageFlagBits vkShaderStageFromFileName(const char* fileName)
{
  if (endsWith(fileName, ".vert"))
    return VK_SHADER_STAGE_VERTEX_BIT;

  if (endsWith(fileName, ".frag"))
    return VK_SHADER_STAGE_FRAGMENT_BIT;

  if (endsWith(fileName, ".geom"))
    return VK_SHADER_STAGE_GEOMETRY_BIT;

  if (endsWith(fileName, ".comp"))
    return VK_SHADER_STAGE_COMPUTE_BIT;

  if (endsWith(fileName, ".tesc"))
    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

  if (endsWith(fileName, ".tese"))
    return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

  return VK_SHADER_STAGE_VERTEX_BIT;
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

  debugGLSLSourceCode[handle.index()] = code;

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

void saveStringList(FILE* f, const std::vector<std::string>& lines)
{
  uint32_t sz = (uint32_t)lines.size();
  fwrite(&sz, sizeof(uint32_t), 1, f);
  for (const std::string& s : lines) {
    sz = (uint32_t)s.length();
    fwrite(&sz, sizeof(uint32_t), 1, f);
    fwrite(s.c_str(), sz + 1, 1, f);
  }
}

void loadStringList(FILE* f, std::vector<std::string>& lines)
{
  {
    uint32_t sz = 0;
    fread(&sz, sizeof(uint32_t), 1, f);
    lines.resize(sz);
  }
  std::vector<char> inBytes;
  for (std::string& s : lines) {
    uint32_t sz = 0;
    fread(&sz, sizeof(uint32_t), 1, f);
    inBytes.resize(sz + 1);
    fread(inBytes.data(), sz + 1, 1, f);
    s = std::string(inBytes.data());
  }
}

int addUnique(std::vector<std::string>& files, const std::string& file)
{
  if (file.empty())
    return -1;

  const auto i = std::find(std::begin(files), std::end(files), file);

  if (i != files.end())
    return (int)std::distance(files.begin(), i);

  files.push_back(file);
  return (int)files.size() - 1;
}

std::string replaceAll(const std::string& str, const std::string& oldSubStr, const std::string& newSubStr)
{
  std::string result = str;

  for (size_t p = result.find(oldSubStr); p != std::string::npos; p = result.find(oldSubStr))
    result.replace(p, oldSubStr.length(), newSubStr);

  return result;
}

// Convert 8-bit ASCII string to upper case
std::string lowercaseString(const std::string& s)
{
  std::string out(s.length(), ' ');
  std::transform(s.begin(), s.end(), out.begin(), tolower);
  return out;
}
