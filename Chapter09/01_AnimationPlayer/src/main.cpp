#include "shared/VulkanApp.h"

#include <shared/UtilsGLTF.h>

int main()
{
  VulkanApp app({
      .initialCameraPos    = vec3(7.0f, 6.8f, -13.6f),
      .initialCameraTarget = vec3(1.7f, -1.0f, 0.0f),
      .showGLTFInspector   = true,
  });

  GLTFContext gltf(app);

  // Khronos animated cube:
  //   loadGLTF(
  //     gltf, "deps/src/glTF-Sample-Assets/Models/AnimatedCube/glTF/AnimatedCube.gltf",
  //     "deps/src/glTF-Sample-Assets/Models/AnimatedCube/glTF/");

  // This work is based on "Medieval Fantasy Book" (https://sketchfab.com/3d-models/medieval-fantasy-book-06d5a80a04fc4c5ab552759e9a97d91a)
  // by Pixel (https://sketchfab.com/stefan.lengyel1) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
  loadGLTF(gltf, "data/meshes/medieval_fantasy_book/scene.gltf", "data/meshes/medieval_fantasy_book/");

  gltf.enableMorphing = false;

  const mat4 t = glm::translate(mat4(1.0f), vec3(0.0f, 2.1f, 0.0f)) * glm::scale(mat4(1.0f), vec3(0.2f));

  AnimationState anim = {
    .animId      = 0,
    .currentTime = 0.0f,
    .playOnce    = false,
    .active      = true,
  };

  gltf.inspector = {
    .activeAnim     = { 0 },
    .showAnimations = true,
  };

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    const mat4 m = t * glm::rotate(mat4(1.0f), glm::radians(180.0f), vec3(0, 1, 0));
    const mat4 v = app.camera_.getViewMatrix();
    const mat4 p = glm::perspective(45.0f, aspectRatio, 0.01f, 100.0f);
    animateGLTF(gltf, anim, deltaSeconds);
    renderGLTF(gltf, m, v, p);
    if (gltf.inspector.activeAnim[0] != anim.animId) {
      anim = {
        .animId      = gltf.inspector.activeAnim[0],
        .currentTime = 0.0f,
        .playOnce    = false,
        .active      = true,
      };
    }
  });

  return 0;
}
