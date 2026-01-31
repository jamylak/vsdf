#ifndef GLFWUTILS_H
#define GLFWUTILS_H

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <cctype>
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
#if defined(__linux__)
#if (GLFW_VERSION_MAJOR < 3) ||                                              \
    (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR < 4)
#error "GLFW 3.4+ is required on Linux to force X11/Wayland via GLFW_PLATFORM."
#endif
    const char *platformEnv = std::getenv("GLFW_PLATFORM");
    if (platformEnv && platformEnv[0] != '\0') {
        std::string platform = platformEnv;
        for (char &ch : platform) {
            ch = static_cast<char>(
                std::tolower(static_cast<unsigned char>(ch)));
        }
        int platformHint = 0;
        if (platform == "x11") {
            platformHint = GLFW_PLATFORM_X11;
        } else if (platform == "wayland") {
            platformHint = GLFW_PLATFORM_WAYLAND;
        } else if (platform == "null") {
            platformHint = GLFW_PLATFORM_NULL;
        } else {
            throw std::runtime_error(
                "Invalid GLFW_PLATFORM value on Linux: " + platform +
                " (expected x11, wayland, null)");
        }
        glfwInitHint(GLFW_PLATFORM, platformHint);
    } else {
        // Default to X11 on Linux to avoid Wayland/libdecor ASAN leak.
        // See issue #68: https://github.com/jamylak/vsdf/issues/68
        // Summary: ASAN reports leaks seemingly from the Wayland decoration stack
        // (libdecor/GTK/Pango/Fontconfig via GLFW) that persist until process exit.
        // This is not related to render/present stalls; override with GLFW_PLATFORM
        // if you explicitly want Wayland.
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    }
#endif
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
