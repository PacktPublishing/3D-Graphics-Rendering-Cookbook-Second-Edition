#include "shared/VulkanApp.h"

#include <shared/UtilsGLTF.h>

int main()
{
  VulkanApp app({
      .initialCameraPos    = vec3(0.0f, -0.5f, -1.0f),
      .initialCameraTarget = vec3(0.0f, -1.0f, 0.0f),
		.showGLTFInspector = true,
  });

  app.positioner_.maxSpeed_ = 2.0f;

  GLTFContext gltf(app);

  loadGLTF(
      gltf, "deps/src/glTF-Sample-Assets/Models/SpecularSilkPouf/glTF/SpecularSilkPouf.gltf", "deps/src/glTF-Sample-Assets/Models/SpecularSilkPouf/glTF/");

  const bool rotateModel = true;

  const mat4 t = glm::translate(mat4(1.0f), vec3(0, -1, 0));

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    const mat4 m = t * glm::rotate(mat4(1.0f), rotateModel ? (float)glfwGetTime() : 0.0f, vec3(0.0f, 1.0f, 0.0f));
    const mat4 v = app.camera_.getViewMatrix();
    const mat4 p = glm::perspective(45.0f, aspectRatio, 0.01f, 100.0f);

    renderGLTF(gltf, m, v, p);
  });

  return 0;
}
