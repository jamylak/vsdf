#ifndef GLFWUTILS_H
#define GLFWUTILS_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <string>

/**
 * Utility namespace for common GLFW operations, particularly focused
 * on Vulkan applications. Includes initialization and window creation
 * functionalities.
 */
namespace glfwutils {

/**
 * Initializes GLFW library. Throws runtime_error if initialization fails.
 * Ensures GLFW is only initialized once.
 */
static void initGLFW(bool headless) {
    static bool isInitialized = false;
    if (!isInitialized) {
        if (headless) {
#if defined(GLFW_PLATFORM_NULL)
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_NULL);
#endif
        }
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No OpenGL context
        isInitialized = true;
    }
}

/**
 * Creates a GLFW window for Vulkan rendering.
 *
 * @param width The width of the window.
 * @param height The height of the window.
 * @param title The title of the window.
 * @return A pointer to the created GLFWwindow.
 * @throws std::runtime_error if window creation fails.
 */
static GLFWwindow *createGLFWwindow(int width = 800, int height = 600,
                                    const std::string &title = "Vulkan") {
    GLFWwindow *window =
        glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    return window;
}

} // namespace glfwutils

#endif // GLFWUTILS_H
