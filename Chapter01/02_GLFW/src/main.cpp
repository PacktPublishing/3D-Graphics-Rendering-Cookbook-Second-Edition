#include <shared/HelpersGLFW.h>

int main(void)
{
  uint32_t w = 1280;
  uint32_t h = 800;

  GLFWwindow* window = initWindow("GLFW example", w, h);

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
