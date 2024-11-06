#include "shared/VulkanApp.h"

#include <shared/UtilsGLTF.h>

int main()
{
  VulkanApp app({
      .initialCameraPos = vec3(0.0f, 1.0f, -4.0f),
		.showGLTFInspector = true,
  });

  GLTFContext gltf(app);

  loadGLTF(
      gltf, "deps/src/glTF-Sample-Assets/Models/DragonAttenuation/glTF/DragonAttenuation.gltf", "deps/src/glTF-Sample-Assets/Models/DragonAttenuation/glTF/");

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    const mat4 m = glm::rotate(mat4(1.0f), glm::radians(180.0f), vec3(0, 1, 0));
    const mat4 v = app.camera_.getViewMatrix();
    const mat4 p = glm::perspective(45.0f, aspectRatio, 0.1f, 1000.0f);

    renderGLTF(gltf, m, v, p);
  });

  return 0;
}
