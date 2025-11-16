#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "shared/Utils.h"

#include <glslang/Include/glslang_c_interface.h>
#include "glslang/Public/resource_limits_c.h"

#include <lvk/LVK.h>
#include <lvk/vulkan/VulkanUtils.h>
#include <ldrutils/lutils/ScopeExit.h>
#include <minilog/minilog.h>

lvk::Result lvk::compileShaderGlslang(
    lvk::ShaderStage stage, const char* code, std::vector<uint8_t>* outSPIRV, const glslang_resource_t* glslLangResource);

void saveSPIRVBinaryFile(const char* filename, const uint8_t* code, size_t size)
{
	FILE* f = fopen(filename, "wb");

	if (!f)
		return;

	fwrite(code, sizeof(uint8_t), size, f);
	fclose(f);
}

void testShaderCompilation(const char* sourceFilename, const char* destFilename)
{
  std::string shaderSource = readShaderFile(sourceFilename);

  assert(!shaderSource.empty());

  std::vector<uint8_t> spirv;
  lvk::Result res =
      lvk::compileShaderGlslang(lvkShaderStageFromFileName(sourceFilename), shaderSource.c_str(), &spirv, glslang_default_resource());

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
