#include "shared/VulkanApp.h"

#include <shared/UtilsGLTF.h>

int main()
{
  VulkanApp app({
      .initialCameraPos = vec3(0.0f, 0.0f, -1.0f),
  });

  app.positioner_.maxSpeed_ = 1.5f;

  GLTFContext gltf(app);

  loadGLTF(
      gltf, "deps/src/glTF-Sample-Assets/Models/TransmissionRoughnessTest/glTF/TransmissionRoughnessTest.gltf",
      "deps/src/glTF-Sample-Assets/Models/TransmissionRoughnessTest/glTF/");

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    const mat4 m = glm::rotate(mat4(1.0f), glm::radians(180.0f), vec3(0, 1, 0));
    const mat4 v = app.camera_.getViewMatrix();
    const mat4 p = glm::perspective(45.0f, aspectRatio, 0.01f, 100.0f);

    renderGLTF(gltf, m, v, p);
  });

  return 0;
}
