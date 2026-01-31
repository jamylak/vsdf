#ifndef GLFWUTILS_H
#define GLFWUTILS_H

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <cstdlib>
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
 * Must be called once before creating windows.
 */
static void initGLFW() {
    // Default to X11 on Linux to avoid Wayland/libdecor ASAN leak noise.
    // See issue #68: https://github.com/jamylak/vsdf/issues/68
    // Summary: ASAN reports leaks from the Wayland decoration stack
    // (libdecor/GTK/Pango/Fontconfig via GLFW) that persist until process exit.
    // This is not related to render/present stalls; override with GLFW_PLATFORM
    // if you explicitly want Wayland.
#if defined(__linux__)
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif
    const char *platform = std::getenv("GLFW_PLATFORM");
    if (platform) {
        if (std::string(platform) == "x11" || std::string(platform) == "X11") {
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
        } else if (std::string(platform) == "wayland" || std::string(platform) == "Wayland") {
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
        } else if (std::string(platform) == "any" || std::string(platform) == "ANY") {
            glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
        } else if (std::string(platform) == "null" || std::string(platform) == "NULL") {
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_NULL);
        }
    }
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No OpenGL context
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
