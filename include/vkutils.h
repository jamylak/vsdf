#ifndef VKUTILS_H
#define VKUTILS_H
#include <cstddef>
#include <cstdint>
#include <spdlog/spdlog.h>
#include <sys/types.h>
#include <vector>
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include "fileutils.h"
#include <GLFW/glfw3.h>
#include <array>
#include <glm/glm.hpp>
#include <spdlog/fmt/fmt.h>

#define VK_CHECK(x)                                                            \
    do {                                                                       \
        VkResult err = x;                                                      \
        if (err)                                                               \
            throw std::logic_error("Got a runtime_error");                     \
    } while (0);
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

inline constexpr size_t MAX_SWAPCHAIN_IMAGES = 10;

namespace vkutils {
struct PushConstants {
    float iTime;
    uint iFrame;
    glm::vec2 iResolution;
    glm::vec2 iMouse;
};

/* Helper structs so that we can pass around swapchain images
 * or image views on the stack without having to go to the heap
 * unnesicarily. I want to sometimes avoid vector
 */
struct SwapchainImages {
    VkImage images[MAX_SWAPCHAIN_IMAGES];
    uint32_t count;
};

struct SwapchainImageViews {
    VkImageView imageViews[MAX_SWAPCHAIN_IMAGES];
    uint32_t count;
};

struct CommandBuffers {
    VkCommandBuffer commandBuffers[MAX_SWAPCHAIN_IMAGES];
    uint32_t count;
};

struct Fences {
    VkFence fences[MAX_SWAPCHAIN_IMAGES];
    uint32_t count;
};

struct Semaphores {
    VkSemaphore semaphores[MAX_SWAPCHAIN_IMAGES];
    uint32_t count;
};

struct FrameBuffers {
    VkFramebuffer framebuffers[MAX_SWAPCHAIN_IMAGES];
    uint32_t count;
};

[[nodiscard]] static VkInstance setupVulkanInstance() {
    const VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "Emerald",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Emerald Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_2,
    };
    spdlog::info("Size of push constants {}", sizeof(PushConstants));

    uint32_t extensionCount = 0;
    const char **glfwExtensions =
        glfwGetRequiredInstanceExtensions(&extensionCount);

    // Use vector for convenience here, this is only run once at startup
    std::vector<const char *> extensions(glfwExtensions,
                                         glfwExtensions + extensionCount);

#ifdef __APPLE__
    extensions.push_back("VK_KHR_portability_enumeration");
#endif

    spdlog::debug("Using the following extensions: ");
    for (const auto &extension : extensions) {
        spdlog::debug("- {}", extension);
    }

    spdlog::debug("Creating vk instance...");

    static const char *validationLayers[] = {
        "VK_LAYER_KHRONOS_validation",
        // "VK_LAYER_LUNARG_api_dump",
        // "VK_LAYER_LUNARG_parameter_validation",
        // "VK_LAYER_LUNARG_screenshot",
        // "VK_LAYER_LUNARG_core_validation",
        // "VK_LAYER_LUNARG_device_limits",
        // "VK_LAYER_LUNARG_object_tracker",
    };

    static const VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
#ifdef __APPLE__
        .flags = VkInstanceCreateFlagBits::
            VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
#else
        .flags = 0,
#endif
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = ARRAY_SIZE(validationLayers),
        .ppEnabledLayerNames = validationLayers,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    VkInstance instance;
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
    return instance;
}

[[nodiscard]] static VkPhysicalDeviceProperties
getDeviceProperties(VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    return deviceProperties;
}

[[nodiscard]] static VkPhysicalDevice findGPU(VkInstance instance) {
    uint32_t deviceCount = 0;
    spdlog::debug("Enumerating devices...");
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));

    if (deviceCount == 0) {
        spdlog::error("No devices found!");
        throw std::runtime_error("No devices found!");
    }

    spdlog::info("Found {} devices", deviceCount);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    VK_CHECK(
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

    for (uint32_t i = 0; i < deviceCount; i++) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(devices[i], &deviceProperties);
        spdlog::debug("Device {} has Vulkan version {}", i,
                      deviceProperties.apiVersion);
        spdlog::debug("Device {} has driver version {}", i,
                      deviceProperties.driverVersion);
        spdlog::debug("Device {} has vendor ID {}", i,
                      deviceProperties.vendorID);
        spdlog::debug("Device {} has device ID {}", i,
                      deviceProperties.deviceID);
        spdlog::debug("Device {} has device type {}", i,
                      static_cast<uint32_t>(deviceProperties.deviceType));
        spdlog::debug("Device {} has device name {}", i,
                      deviceProperties.deviceName);

        // Example of selecting a discrete GPU
        if (deviceProperties.deviceType ==
            VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            spdlog::info("Selecting discrete GPU: {}",
                         deviceProperties.deviceName);
            return devices[i];
        }
    }
    // If no discrete GPU is found, fallback to the first device
    spdlog::debug("No discrete GPU found. Fallback to the first device.");
    return devices[0];
}

[[nodiscard]] static VkSurfaceKHR createVulkanSurface(VkInstance instance,
                                                      GLFWwindow *window) {
    spdlog::debug("Creating Vulkan surface...");
    VkSurfaceKHR surface;
    VkResult result =
        glfwCreateWindowSurface(instance, window, nullptr, &surface);
    if (result != VK_SUCCESS) {
        spdlog::error("Failed to create Vulkan surface");
        spdlog::error("Result: 0x{:x}", static_cast<int>(result));
        vkDestroyInstance(instance, nullptr);
        glfwTerminate();
        throw std::runtime_error("Failed to create Vulkan surface");
    }
    spdlog::debug("Created vulkan surface");
    return surface;
}

[[nodiscard]] static uint32_t
getVulkanGraphicsQueueIndex(VkPhysicalDevice physicalDevice,
                            VkSurfaceKHR surface) {
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                             nullptr);

    if (queueFamilyCount == 0)
        throw std::runtime_error("No queue families found");

    spdlog::debug("Found {} queue families", queueFamilyCount);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                             queueFamilies.data());

    // Print debug info for all queue families
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        spdlog::debug("Queue family {} has {} queues", i,
                      queueFamilies[i].queueCount);
        spdlog::debug("Queue family {} supports graphics: {} ", i,
                      queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT);
        spdlog::debug("Queue family {} supports compute: {} ", i,
                      queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT);
        spdlog::debug("Queue family {} supports transfer: {} ", i,
                      queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT);
        spdlog::debug("Queue family {} supports sparse binding: {} ", i,
                      queueFamilies[i].queueFlags &
                          VK_QUEUE_SPARSE_BINDING_BIT);
        spdlog::debug("Queue family {} supports protected: {} ", i,
                      queueFamilies[i].queueFlags & VK_QUEUE_PROTECTED_BIT);

        VkBool32 supportsPresent;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface,
                                             &supportsPresent);
        spdlog::debug("Queue family {} supports present: {} ", i,
                      supportsPresent);

        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
            supportsPresent)
            return i;
    }

    throw std::runtime_error("Failed to find graphics queue");
}

[[nodiscard]] static VkDevice
createVulkanLogicalDevice(VkPhysicalDevice physicalDevice,
                          uint32_t graphicsQueueIndex) {
    float queuePriority = 1.0f;

    spdlog::debug("Create a queue...");
    VkDeviceQueueCreateInfo queueInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = graphicsQueueIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    static const char *requiredExtensions[] = {
        "VK_KHR_swapchain",
#ifdef __APPLE__
        "VK_KHR_portability_subset",
#endif
    };

    spdlog::debug("Create a logical device...");
    VkDevice device;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures{
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .dynamicRendering = VK_TRUE,
    };

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &dynamicRenderingFeatures,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueInfo,
        .enabledExtensionCount = ARRAY_SIZE(requiredExtensions),
        .ppEnabledExtensionNames = requiredExtensions,
    };

    VK_CHECK(
        vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device));
    spdlog::debug("Created logical device");

    return device;
}

[[nodiscard]] static VkSurfaceCapabilitiesKHR
getSurfaceCapabilities(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    // Surface Capabilities
    spdlog::debug("Get surface capabilities");
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                                       &surfaceCapabilities));
    return surfaceCapabilities;
}

[[nodiscard]] consteval auto getPreferredFormats() {
    return std::to_array({
        VK_FORMAT_R8G8B8_SRGB,
        VK_FORMAT_R8G8B8_UNORM,
    });
}

[[nodiscard]] static VkSurfaceFormatKHR
selectSwapchainFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    // Get surface formats
    uint32_t surfaceFormatCount;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        physicalDevice, surface, &surfaceFormatCount, nullptr));
    spdlog::debug("Surface format count: {}", surfaceFormatCount);

    if (surfaceFormatCount == 0) {
        throw std::runtime_error("Failed to find any surface formats.");
    }

    // NOTE: Would like to move to array but not sure of a good max
    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data()));

    // Handle the special case where the surface format is undefined.
    if (surfaceFormatCount == 1 &&
        surfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
        spdlog::info("Surface format is undefined, selecting "
                     "VK_FORMAT_R8G8B8A8_SRGB as default.");
        return {VK_FORMAT_R8G8B8A8_SRGB, surfaceFormats[0].colorSpace};
    }

    static constexpr auto preferredFormatArray = getPreferredFormats();
    for (const auto &candidate : surfaceFormats) {
        if (std::find(preferredFormatArray.begin(), preferredFormatArray.end(),
                      candidate.format) != preferredFormatArray.end()) {
            return candidate;
        }
    }

    // Fallback: if no preferred formats are found, use first available format.
    spdlog::debug(
        "No preferred format found, using the first available format.");
    return surfaceFormats[0];
}

[[nodiscard]] static VkExtent2D
getSwapchainSize(GLFWwindow *window,
                 const VkSurfaceCapabilitiesKHR &surfaceCapabilities) {
    VkExtent2D swapchainSize;
    if (surfaceCapabilities.currentExtent.width == 0xFFFFFFFF) {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        swapchainSize = {static_cast<uint32_t>(width),
                         static_cast<uint32_t>(height)};
    } else {
        swapchainSize = surfaceCapabilities.currentExtent;
    }

    spdlog::debug("Swapchain size: {}x{}", swapchainSize.width,
                  swapchainSize.height);
    return swapchainSize;
}

[[nodiscard]] static VkSwapchainKHR
createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device,
                VkSurfaceKHR surface,
                const VkSurfaceCapabilitiesKHR &surfaceCapabilities,
                VkExtent2D swapchainSize, VkSurfaceFormatKHR surfaceFormat,
                GLFWwindow *window, VkSwapchainKHR oldSwapchain) {
    // Determine the number of VkImage's to use in the swapchain.
    // Ideally, we desire to own 1 image at a time, the rest of the images can
    // either be rendered to and/or being queued up for display.
    uint32_t desiredSwapchainImages = surfaceCapabilities.minImageCount + 1;
    if ((surfaceCapabilities.maxImageCount > 0) &&
        (desiredSwapchainImages > surfaceCapabilities.maxImageCount)) {
        // Application must settle for fewer images than desired.
        desiredSwapchainImages = surfaceCapabilities.maxImageCount;
    }
    spdlog::debug("Desired swapchain images: {}", desiredSwapchainImages);

    // Just set identity bit transform
    VkSurfaceTransformFlagBitsKHR preTransform =
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    // Query the list of supported present modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                              &presentModeCount, nullptr);

    static constexpr uint32_t presentModeCountMax = 30; // more than enough
    std::array<VkPresentModeKHR, presentModeCountMax> presentModes;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physicalDevice, surface, &presentModeCount, presentModes.data());

    VkPresentModeKHR swapchainPresentMode =
        VK_PRESENT_MODE_FIFO_KHR; // Default mode
    for (const auto &mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break; // Highest priority
        } else if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            // Don't break to keep checking for MAILBOX
        }
    }

    VkCompositeAlphaFlagBitsKHR composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (surfaceCapabilities.supportedCompositeAlpha &
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
        composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    } else if (surfaceCapabilities.supportedCompositeAlpha &
               VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
        composite = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    } else if (surfaceCapabilities.supportedCompositeAlpha &
               VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
        composite = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    } else if (surfaceCapabilities.supportedCompositeAlpha &
               VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) {
        composite = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    }

    spdlog::debug("Composite alpha: {}", static_cast<int>(composite));

    spdlog::debug("Selected surface format");
    spdlog::info("Surface format: {}", static_cast<int>(surfaceFormat.format));
    spdlog::info("Color space: {}", static_cast<int>(surfaceFormat.colorSpace));

    // Create a swapchain
    spdlog::debug("Create a swapchain");
    VkSwapchainCreateInfoKHR swapchainCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .surface = surface,
        .minImageCount = desiredSwapchainImages,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent =
            {
                .width = swapchainSize.width,
                .height = swapchainSize.height,
            },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = preTransform,
        .compositeAlpha = composite,
        .presentMode = swapchainPresentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = oldSwapchain,
    };

    VkSwapchainKHR swapchain;
    VK_CHECK(vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr,
                                  &swapchain));

    return swapchain;
}

[[nodiscard]] static SwapchainImages
getSwapchainImages(VkDevice device, VkSwapchainKHR swapchain) {
    SwapchainImages swapchainImages;
    uint32_t swapchainImageCount;
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount,
                                     nullptr));
    spdlog::debug("Swapchain image count: {}", swapchainImageCount);
    swapchainImages.count = swapchainImageCount;

    if (swapchainImageCount > MAX_SWAPCHAIN_IMAGES) {
        throw std::runtime_error(fmt::format("Swapchain image count {} exceeds "
                                             "maximum images {}",
                                             swapchainImageCount,
                                             MAX_SWAPCHAIN_IMAGES));
    }

    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount,
                                     swapchainImages.images));
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        spdlog::debug("Swapchain image {}", i);
    }
    return swapchainImages;
}

[[nodiscard]] static SwapchainImageViews
createSwapchainImageViews(VkDevice device, VkSurfaceFormatKHR surfaceFormat,
                          const SwapchainImages &swapchainImages) {
    // Create image views
    SwapchainImageViews swapchainImageViews;
    swapchainImageViews.count = swapchainImages.count;
    for (uint32_t i = 0; i < swapchainImages.count; i++) {
        VkImageViewCreateInfo imageViewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .flags = 0,
            .image = swapchainImages.images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surfaceFormat.format,
            .components =
                {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };
        imageViewCreateInfo.pNext = nullptr;

        VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr,
                                   &swapchainImageViews.imageViews[i]));
    }
    return swapchainImageViews;
}

[[nodiscard]] static VkCommandPool
createCommandPool(VkDevice device, uint32_t graphicsQueueIndex) {
    spdlog::debug("Create command pool");
    VkCommandPoolCreateInfo commandPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsQueueIndex,
    };
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr,
                                 &commandPool));
    return commandPool;
}

[[nodiscard]] static VkDescriptorSetLayout
createDescriptorSetLayout(VkDevice device) {
    VkDescriptorSetLayoutBinding layoutBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &layoutBinding,
    };

    VkDescriptorSetLayout descriptorSetLayout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                         &descriptorSetLayout));
    return descriptorSetLayout;
}

[[nodiscard]] static VkDescriptorPool createDescriptorPool(VkDevice &device) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    VkDescriptorPool descriptorPool;
    VK_CHECK(
        vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));
    return descriptorPool;
}

[[nodiscard]] static VkDescriptorSet
allocateDescriptorSet(VkDescriptorPool &descriptorPool, VkDevice &device,
                      VkDescriptorSetLayout &descriptorSetLayout) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorSet descriptorSet;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
    return descriptorSet;
}

[[nodiscard]] static CommandBuffers
createCommandBuffers(VkDevice device, VkCommandPool commandPool,
                     uint32_t commandBufferCount) {
    CommandBuffers commandBuffers;
    commandBuffers.count = commandBufferCount;
    spdlog::info("Create command buffers");
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = commandBufferCount,
    };
    VK_CHECK(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo,
                                      commandBuffers.commandBuffers));
    return commandBuffers;
}

[[nodiscard]] static Fences createFences(VkDevice device, uint32_t count) {
    spdlog::info("Create fences");
    Fences fences;
    fences.count = count;
    for (uint32_t i = 0; i < count; i++) {
        VkFenceCreateInfo fenceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr,
                               &fences.fences[i]));
    }
    return fences;
}

[[nodiscard]] static VkSemaphore createSemaphore(VkDevice device) {
    VkSemaphoreCreateInfo semaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkSemaphore semaphore;
    VK_CHECK(
        vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore));
    return semaphore;
}

[[nodiscard]] static Semaphores createSemaphores(VkDevice device,
                                                 uint32_t count) {
    Semaphores semaphores;
    semaphores.count = count;
    for (uint32_t i = 0; i < count; i++) {
        semaphores.semaphores[i] = createSemaphore(device);
    }
    return semaphores;
}

[[nodiscard]] static VkRenderPass createRenderPass(VkDevice device,
                                                   VkFormat format) {
    spdlog::debug("Create render pass");
    VkAttachmentDescription colorAttachment{
        .format = format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
    };

    VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };

    VkRenderPass renderPass;
    VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));

    return renderPass;
}

[[nodiscard]] static FrameBuffers
createFrameBuffers(VkDevice device, VkRenderPass renderPass, VkExtent2D extent,
                   const SwapchainImageViews &swapchainImageViews) {
    spdlog::info("Create framebuffers");
    FrameBuffers frameBuffers;
    frameBuffers.count = swapchainImageViews.count;
    for (uint32_t i = 0; i < swapchainImageViews.count; i++) {
        VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderPass,
            .attachmentCount = 1,
            .pAttachments = &swapchainImageViews.imageViews[i],
            .width = extent.width,
            .height = extent.height,
            .layers = 1,
        };

        VK_CHECK(vkCreateFramebuffer(device, &framebufferInfo, nullptr,
                                     &frameBuffers.framebuffers[i]));
    }
    return frameBuffers;
}

[[nodiscard]] static VkPipelineLayout createPipelineLayout(VkDevice device) {
    spdlog::info("Create pipeline layout");
    VkPushConstantRange pushConstantRange{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(vkutils::PushConstants),
    };
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    };

    VkPipelineLayout pipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                                    &pipelineLayout));
    return pipelineLayout;
}

[[nodiscard]] static VkShaderModule
createShaderModule(VkDevice device, const std::string &filename) {
    spdlog::info("Create shader module");
    VkShaderModule shaderModule;
    auto code = loadBinaryFile(filename);
    VkShaderModuleCreateInfo createinfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t *>(code.data()),
    };

    VK_CHECK(vkCreateShaderModule(device, &createinfo, nullptr, &shaderModule));
    return shaderModule;
}

[[nodiscard]] static VkPipeline
createGraphicsPipeline(VkDevice device, VkRenderPass renderPass,
                       VkPipelineLayout pipelineLayout, VkExtent2D extent,
                       VkShaderModule vertShaderModule,
                       VkShaderModule fragShaderModule) {
    spdlog::info("Create graphics pipeline");
    VkPipeline pipeline;

    VkPipelineShaderStageCreateInfo shaderStages[2] = {
        // Note: This one would never change
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule,
            .pName = "main",
        },
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .vertexAttributeDescriptionCount = 0,
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor{
        .offset = {0, 0},
        .extent = extent,
    };

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                      VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamicStates,
    };

    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f, // It doesn't matter really we are doing SDF
    };

    VkPipelineMultisampleStateCreateInfo multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo colorBlending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
    };

    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicStateInfo,
        .layout = pipelineLayout,
        .renderPass = renderPass,
        .subpass = 0,
    };
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                       nullptr, &pipeline));
    spdlog::info("Created graphics pipeline");
    return pipeline;
}

[[nodiscard]] static VkQueryPool createQueryPool(VkDevice device,
                                                 uint32_t numSwapchainImages) {
    // Query pool used for calculating frame processing duration
    VkQueryPoolCreateInfo queryPooolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 2 * numSwapchainImages, // 2 per frame, start and end
    };

    VkQueryPool queryPool;
    VK_CHECK(
        vkCreateQueryPool(device, &queryPooolCreateInfo, nullptr, &queryPool));
    return queryPool;
}

static void recordCommandBuffer(VkDevice device, VkCommandPool commandPool,
                                VkQueryPool queryPool, VkRenderPass renderPass,
                                VkExtent2D extent, VkPipeline pipeline,
                                VkPipelineLayout pipelineLayout,
                                VkCommandBuffer commandBuffer,
                                VkFramebuffer framebuffer,
                                const PushConstants &pushConstants,
                                uint32_t imageIndex) {
    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
    };

    VkRenderPassBeginInfo renderPassBeginInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {{0, 0}, extent},
        .clearValueCount = 0,
    };
    spdlog::debug("Record command buffer");
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    vkCmdResetQueryPool(commandBuffer, queryPool, imageIndex * 2, 2);
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        queryPool, imageIndex * 2);
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo,
                         VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vkCmdPushConstants(commandBuffer, pipelineLayout,
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants),
                       &pushConstants);
    VkRect2D scissor{
        .offset = {0, 0},
        .extent = {extent.width, extent.height},
    };

    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdDraw(commandBuffer, 6, 1, 0, 0);
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        queryPool, imageIndex * 2 + 1);
    vkCmdEndRenderPass(commandBuffer);
    spdlog::debug("End command buffer");
    VK_CHECK(vkEndCommandBuffer(commandBuffer));
    spdlog::debug("Ended command buffer");
}

static void submitCommandBuffer(VkQueue queue, VkCommandBuffer commandBuffer,
                                VkSemaphore imageAvailableSemaphore,
                                VkSemaphore renderFinishedSemaphore,
                                VkFence fence) {
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &imageAvailableSemaphore,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderFinishedSemaphore,
    };
    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));
} // namespace vkutils

static void presentImage(VkQueue queue, VkSwapchainKHR swapchain,
                         VkSemaphore renderFinishedSemaphore,
                         uint32_t imageIndex) {
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex,
    };
    VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));
}

static void
destroySwapchainImageViews(VkDevice device,
                           SwapchainImageViews &swapchainImageViews) noexcept {
    for (uint32_t i = 0; i < swapchainImageViews.count; ++i) {
        vkDestroyImageView(device, swapchainImageViews.imageViews[i], nullptr);
        swapchainImageViews.imageViews[i] = VK_NULL_HANDLE;
    }
}

static void destroyFrameBuffers(VkDevice device,
                                FrameBuffers &frameBuffers) noexcept {
    for (uint32_t i = 0; i < frameBuffers.count; ++i) {
        vkDestroyFramebuffer(device, frameBuffers.framebuffers[i], nullptr);
        frameBuffers.framebuffers[i] = VK_NULL_HANDLE;
    }
}

static void destroyFences(VkDevice device, Fences &fences) noexcept {
    for (uint32_t i = 0; i < fences.count; ++i) {
        vkDestroyFence(device, fences.fences[i], nullptr);
        fences.fences[i] = VK_NULL_HANDLE;
    }
}

static void destroySemaphores(VkDevice device,
                              Semaphores &semaphores) noexcept {
    for (uint32_t i = 0; i < semaphores.count; ++i) {
        vkDestroySemaphore(device, semaphores.semaphores[i], nullptr);
        semaphores.semaphores[i] = VK_NULL_HANDLE;
    }
}

} // namespace vkutils

#endif
