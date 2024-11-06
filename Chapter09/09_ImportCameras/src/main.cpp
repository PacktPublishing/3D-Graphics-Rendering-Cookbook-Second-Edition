#include "shared/VulkanApp.h"

#include <shared/UtilsGLTF.h>

int main()
{
  VulkanApp app({
      .initialCameraPos    = vec3(4.937f, 2.812f, -7.295f),
      .initialCameraTarget = vec3(0.0f, 2.0f, 13.0f),
		.showGLTFInspector = true,
  });

  GLTFContext gltf(app);

  loadGLTF(gltf, "deps/src/glTF-Sample-Assets/Models/Cameras/glTF/Cameras.gltf", "deps/src/glTF-Sample-Assets/Models/Cameras/glTF/");
  //loadGLTF(gltf, "deps/src/glTF-Sample-Assets/Models/Duck/glTF/Duck.gltf", "deps/src/glTF-Sample-Assets/Models/Duck/glTF/");

  const mat4 t = glm::translate(mat4(1.0f), vec3(0.0f, 1.1f, 0.0f));

  // by default, use the VulkanApp camera
  gltf.inspector.activeCamera = gltf.cameras.size();
  gltf.inspector.showCameras = true;

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    mat4 m = t * glm::rotate(mat4(1.0f), glm::radians(180.0f), vec3(0, 1, 0));
    mat4 v = app.camera_.getViewMatrix();
    mat4 p = glm::perspective(45.0f, aspectRatio, 0.01f, 100.0f);
	 updateCamera(gltf, m, v, p, aspectRatio);
    renderGLTF(gltf, m, v, p);
  });

  return 0;
}
