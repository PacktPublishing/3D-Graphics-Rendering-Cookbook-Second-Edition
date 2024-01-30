#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "shared/Utils.h"

#include <glslang/Include/glslang_c_interface.h>
#include "glslang/Public/resource_limits_c.h"

#include <lvk/LVK.h>
#include <ldrutils/lutils/ScopeExit.h>
#include <minilog/minilog.h>

std::vector<unsigned int> compileShaderToSPIRV(glslang_stage_t stage, const char* code, const glslang_resource_t* glslLangResource)
{
  const glslang_input_t input = {
    .language                          = GLSLANG_SOURCE_GLSL,
    .stage                             = stage,
    .client                            = GLSLANG_CLIENT_VULKAN,
    .client_version                    = GLSLANG_TARGET_VULKAN_1_3,
    .target_language                   = GLSLANG_TARGET_SPV,
    .target_language_version           = GLSLANG_TARGET_SPV_1_6,
    .code                              = code,
    .default_version                   = 100,
    .default_profile                   = GLSLANG_NO_PROFILE,
    .force_default_version_and_profile = false,
    .forward_compatible                = false,
    .messages                          = GLSLANG_MSG_DEFAULT_BIT,
    .resource                          = glslLangResource,
  };

  glslang_shader_t* shader = glslang_shader_create(&input);
  SCOPE_EXIT
  {
    glslang_shader_delete(shader);
  };

  if (!glslang_shader_preprocess(shader, &input)) {
    LLOGW("Shader preprocessing failed:\n");
    LLOGW("  %s\n", glslang_shader_get_info_log(shader));
    LLOGW("  %s\n", glslang_shader_get_info_debug_log(shader));
    lvk::logShaderSource(code);
    assert(false);
    return {};
  }

  if (!glslang_shader_parse(shader, &input)) {
    LLOGW("Shader parsing failed:\n");
    LLOGW("  %s\n", glslang_shader_get_info_log(shader));
    LLOGW("  %s\n", glslang_shader_get_info_debug_log(shader));
    lvk::logShaderSource(glslang_shader_get_preprocessed_code(shader));
    assert(false);
    return {};
  }

  glslang_program_t* program = glslang_program_create();
  glslang_program_add_shader(program, shader);

  SCOPE_EXIT
  {
    glslang_program_delete(program);
  };

  if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
    LLOGW("Shader linking failed:\n");
    LLOGW("  %s\n", glslang_program_get_info_log(program));
    LLOGW("  %s\n", glslang_program_get_info_debug_log(program));
    assert(false);
    return {};
  }

  glslang_spv_options_t options = {
    .generate_debug_info                  = true,
    .strip_debug_info                     = false,
    .disable_optimizer                    = false,
    .optimize_size                        = true,
    .disassemble                          = false,
    .validate                             = true,
    .emit_nonsemantic_shader_debug_info   = false,
    .emit_nonsemantic_shader_debug_source = false,
  };

  glslang_program_SPIRV_generate_with_options(program, input.stage, &options);

  if (glslang_program_SPIRV_get_messages(program)) {
    LLOGW("%s\n", glslang_program_SPIRV_get_messages(program));
  }

  const unsigned int* spirv = glslang_program_SPIRV_get_ptr(program);

  return std::vector<unsigned int>(spirv, spirv + glslang_program_SPIRV_get_size(program));
}

void saveSPIRVBinaryFile(const char* filename, unsigned int* code, size_t size)
{
	FILE* f = fopen(filename, "wb");

	if (!f)
		return;

	fwrite(code, sizeof(uint32_t), size, f);
	fclose(f);
}

void testShaderCompilation(const char* sourceFilename, const char* destFilename)
{
  std::string shaderSource = readShaderFile(sourceFilename);

  assert(!shaderSource.empty());

  std::vector<unsigned int> spirv =
      compileShaderToSPIRV(glslangShaderStageFromFileName(sourceFilename), shaderSource.c_str(), glslang_default_resource());

  assert(!spirv.empty());

  saveSPIRVBinaryFile(destFilename, spirv.data(), spirv.size());
}

/*
This program should give the same result as the following commands:

   glslangValidator -g -Os --target-env vulkan1.3 main.vert -o 04_GLSLang.vert.bin
   glslangValidator -g -Os --target-env vulkan1.3 main.frag -o 04_GLSLang.frag.bin
*/
int main()
{
	glslang_initialize_process();

	testShaderCompilation("Chapter01/04_GLSLang/src/main.vert", ".cache/04_GLSLang.vert.bin");
	testShaderCompilation("Chapter01/04_GLSLang/src/main.frag", ".cache/04_GLSLang.frag.bin");

	glslang_finalize_process();

	return 0;
}
