#pragma once

#include <string.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#if !defined(__APPLE__)
#include <malloc.h>
#endif

#include <lvk/LVK.h>
#include <ldrutils/lutils/ScopeExit.h>
#include <glslang/Include/glslang_c_interface.h>

bool endsWith(const char* s, const char* part);

std::string readShaderFile(const char* fileName);

glslang_stage_t glslangShaderStageFromFileName(const char* fileName);

lvk::Holder<lvk::ShaderModuleHandle> loadShaderModule(const std::unique_ptr<lvk::IContext>& ctx, const char* fileName);
lvk::Holder<lvk::TextureHandle> loadTexture(
    const std::unique_ptr<lvk::IContext>& ctx, const char* fileName, lvk::TextureType textureType = lvk::TextureType_2D, bool sRGB = false);
