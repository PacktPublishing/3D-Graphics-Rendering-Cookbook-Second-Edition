#pragma once

#include <stdio.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// clang-format off
#ifdef _WIN32
#  define GLFW_EXPOSE_NATIVE_WIN32
#  define GLFW_EXPOSE_NATIVE_WGL
#elif __APPLE__
#  define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
#  define GLFW_EXPOSE_NATIVE_X11
#else
#  error Unsupported OS
#endif
// clang-format on

#include <GLFW/glfw3native.h>

GLFWwindow* initWindow(const char* windowTitle, uint32_t& outWidth, uint32_t& outHeight);
