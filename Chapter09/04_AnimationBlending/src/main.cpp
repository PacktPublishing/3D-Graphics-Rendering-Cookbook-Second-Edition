#include "shared/VulkanApp.h"

#include <shared/UtilsGLTF.h>

int main()
{
  VulkanApp app({
      .initialCameraPos  = vec3(2.13f, 2.44f, -3.5f),
      .initialCameraTarget = vec3(0, 0, 2),
      .showGLTFInspector = true,
  });

  GLTFContext gltf(app);

  loadGLTF(gltf, "deps/src/glTF-Sample-Assets/Models/Fox/glTF/Fox.gltf", "deps/src/glTF-Sample-Assets/Models/Fox/glTF/");

  const mat4 t = glm::translate(mat4(1.0f), vec3(0.0f, 1.1f, 0.0f));
  const mat4 s = glm::scale(mat4(1.0f), vec3(0.01f, 0.01f, 0.01f));

  AnimationState anim1 = { .animId = 1, .currentTime = 0.0f, .playOnce = false, .active = true };
  AnimationState anim2 = { .animId = 2, .currentTime = 0.0f, .playOnce = false, .active = true };

  gltf.skinning  = true;
  gltf.inspector = {
    .activeAnim         = {1, 2},
    .blend              = 0.5f,
    .showAnimations     = true,
    .showAnimationBlend = true,
  };

  app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    const mat4 m = t * glm::rotate(mat4(1.0f), glm::radians(90.0f), vec3(0, 1, 0)) * s;
    const mat4 v = app.camera_.getViewMatrix();
    const mat4 p = glm::perspective(45.0f, aspectRatio, 0.01f, 100.0f);

    animateBlendingGLTF(gltf, anim1, anim2, gltf.inspector.blend, deltaSeconds);
    renderGLTF(gltf, m, v, p);

    if (gltf.inspector.activeAnim[0] != anim1.animId || gltf.inspector.activeAnim[1] != anim2.animId) {
      anim1 = {
        .animId      = gltf.inspector.activeAnim[0],
        .currentTime = 0.0f,
        .playOnce    = false,
        .active      = gltf.inspector.activeAnim[0] != ~0u,
      };
      anim2 = {
        .animId      = gltf.inspector.activeAnim[1],
        .currentTime = 0.0f,
        .playOnce    = false,
        .active      = gltf.inspector.activeAnim[1] != ~0u,
      };
    }
  });

  return 0;
}
