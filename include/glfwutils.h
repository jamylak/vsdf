#ifndef GLFWUTILS_H
#define GLFWUTILS_H

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <volk.h>
#include <stdexcept>
#include <string>

/**
 * Utility namespace for common GLFW operations, particularly focused
 * on Vulkan applications. Includes initialization and window creation
 * functionalities.
 */
namespace glfwutils {

/**
 * Initializes GLFW library and volk. Throws runtime_error if initialization fails.
 * Ensures GLFW and volk are only initialized once.
 */
static void initGLFW() {
    static bool isInitialized = false;
    if (!isInitialized) {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No OpenGL context
        
        // Initialize volk after GLFW
        VkResult result = volkInitialize();
        if (result != VK_SUCCESS) {
            glfwTerminate();
            throw std::runtime_error("Failed to initialize volk (Vulkan loader not found)");
        }
        
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
