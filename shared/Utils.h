#pragma once

#include <algorithm>
#include <memory>
#include <string.h>
#include <string>
#include <vector>
#if !defined(__APPLE__)
#include <malloc.h>
#endif

#include <glslang/Include/glslang_c_interface.h>
#include <ldrutils/lutils/ScopeExit.h>
#include <lvk/LVK.h>
#include <lvk/vulkan/VulkanUtils.h>

bool endsWith(const char* s, const char* part);

std::string readShaderFile(const char* fileName);

VkShaderStageFlagBits vkShaderStageFromFileName(const char* fileName);

lvk::Holder<lvk::ShaderModuleHandle> loadShaderModule(const std::unique_ptr<lvk::IContext>& ctx, const char* fileName);
lvk::Holder<lvk::TextureHandle> loadTexture(
    const std::unique_ptr<lvk::IContext>& ctx, const char* fileName, lvk::TextureType textureType = lvk::TextureType_2D, bool sRGB = false);

template <typename T> inline void mergeVectors(std::vector<T>& v1, const std::vector<T>& v2)
{
  v1.insert(v1.end(), v2.begin(), v2.end());
}

// From https://stackoverflow.com/a/64152990/1182653
// Delete a list of items from std::vector with indices in 'selection'
// e.g., eraseSelected({1, 2, 3, 4, 5}, {1, 3})  ->   {1, 3, 5}
//                         ^     ^    2 and 4 get deleted
template <typename T, typename Index = int> inline void eraseSelected(std::vector<T>& v, const std::vector<Index>& selection)
{
  // cut off the elements moved to the end of the vector by std::stable_partition
  v.resize(std::distance(
      v.begin(),
      // std::stable_partition moves any element whose index is in 'selection' to the end
      std::stable_partition(v.begin(), v.end(), [&selection, &v](const T& item) {
        return !std::binary_search(
            selection.begin(), selection.end(),
            // std::distance(std::find(v.begin(), v.end(), item), v.begin()) - if you don't like the pointer arithmetic below
            static_cast<Index>(static_cast<const T*>(&item) - &v[0]));
      })));
}

void saveStringList(FILE* f, const std::vector<std::string>& lines);
void loadStringList(FILE* f, std::vector<std::string>& lines);
int addUnique(std::vector<std::string>& files, const std::string& file);
std::string replaceAll(const std::string& str, const std::string& oldSubStr, const std::string& newSubStr);
std::string lowercaseString(const std::string& s); // convert 8-bit ASCII string to upper case
