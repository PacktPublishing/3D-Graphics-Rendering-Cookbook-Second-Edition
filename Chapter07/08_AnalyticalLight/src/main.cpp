#include "shared/VulkanApp.h"

#include <shared/UtilsGLTF.h>

int main()
{
  VulkanApp app({
      .initialCameraPos    = vec3(0.0f, 3.5f, -5.0f),
      .initialCameraTarget = vec3(0.0f, 2.0f, 0.0f),
  });

  GLTFContext gltf(app);

  loadGLTF(
      gltf, "deps/src/glTF-Sample-Assets/Models/LightsPunctualLamp/glTF/LightsPunctualLamp.gltf",
      "deps/src/glTF-Sample-Assets/Models/LightsPunctualLamp/glTF/");

  const mat4 t = glm::translate(mat4(1.0f), vec3(0.0f, 1.1f, 0.0f));

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    const mat4 m = t * glm::rotate(mat4(1.0f), glm::radians(180.0f), vec3(0, 1, 0));
    const mat4 v = app.camera_.getViewMatrix();
    const mat4 p = glm::perspective(45.0f, aspectRatio, 0.01f, 100.0f);

    renderGLTF(gltf, m, v, p);
  });

  return 0;
}
