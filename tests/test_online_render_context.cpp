#include "online_sdf_renderer.h"
#include <gtest/gtest.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

// Exercise the production allocation/cleanup methods through Volk's dispatch
// pointers. No Vulkan device, GLFW window, or GPU is needed for these tests.
class OnlineRenderContextTest : public ::testing::Test {
  protected:
    struct Driver {
        uint32_t imageCount = 3;
        uintptr_t nextHandle = 1;
        std::map<uintptr_t, std::string> live;
        uint32_t queryCount = 0;
        uint32_t idleCalls = 0;
        bool pending = false;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;

        template <typename T> T allocate(const char *kind) {
            auto value = nextHandle++;
            live.emplace(value, kind);
            return (T)value;
        }
        size_t count(const char *kind) const {
            size_t result = 0;
            for (const auto &[handle, type] : live)
                result += type == kind;
            return result;
        }
        template <typename T> void release(T handle, const char *kind) {
            EXPECT_FALSE(pending)
                << "Destroying " << kind << " with work pending";
            auto it = live.find((uintptr_t)handle);
            ASSERT_NE(it, live.end()) << "Unknown or already freed " << kind;
            EXPECT_EQ(it->second, kind);
            live.erase(it);
        }
    };
    inline static Driver *driver;
    Driver state;
    std::vector<std::function<void()>> restore;
    OnlineSDFRenderer renderer{"unused.frag"};

    template <typename T> void mock(T &entry, T replacement) {
        restore.emplace_back([&entry, old = entry] { entry = old; });
        entry = replacement;
    }

    void SetUp() override {
        driver = &state;
        renderer.window = nullptr;
        renderer.surface = VK_NULL_HANDLE;
        renderer.swapchainFormat = {VK_FORMAT_B8G8R8A8_UNORM,
                                    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        mock(
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR,
            +[](VkPhysicalDevice, VkSurfaceKHR,
                VkSurfaceCapabilitiesKHR *caps) {
                *caps = {};
                caps->minImageCount = 2;
                caps->currentExtent = {800, 600};
                caps->supportedCompositeAlpha =
                    VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
                return VK_SUCCESS;
            });
        mock(
            vkGetPhysicalDeviceSurfacePresentModesKHR,
            +[](VkPhysicalDevice, VkSurfaceKHR, uint32_t *count,
                VkPresentModeKHR *modes) {
                *count = 1;
                if (modes)
                    modes[0] = VK_PRESENT_MODE_FIFO_KHR;
                return VK_SUCCESS;
            });
        mock(
            vkCreateSwapchainKHR,
            +[](VkDevice, const VkSwapchainCreateInfoKHR *info,
                const VkAllocationCallbacks *, VkSwapchainKHR *out) {
                EXPECT_EQ(info->oldSwapchain, driver->swapchain);
                // A retired swapchain must survive until its replacement is
                // created.
                if (driver->swapchain)
                    EXPECT_EQ(driver->live.count((uintptr_t)driver->swapchain),
                              1u);
                *out = driver->allocate<VkSwapchainKHR>("swapchain");
                driver->swapchain = *out;
                return VK_SUCCESS;
            });
        mock(
            vkDestroySwapchainKHR, +[](VkDevice, VkSwapchainKHR handle,
                                       const VkAllocationCallbacks *) {
                EXPECT_EQ(driver->count("framebuffer"), 0u);
                EXPECT_EQ(driver->count("view"), 0u);
                driver->release(handle, "swapchain");
            });
        mock(
            vkGetSwapchainImagesKHR,
            +[](VkDevice, VkSwapchainKHR, uint32_t *count, VkImage *images) {
                *count = driver->imageCount;
                if (images)
                    for (uint32_t i = 0; i < *count; ++i)
                        images[i] = (VkImage)(uintptr_t)(i + 1);
                return VK_SUCCESS;
            });
        mock(
            vkCreateQueryPool,
            +[](VkDevice, const VkQueryPoolCreateInfo *info,
                const VkAllocationCallbacks *, VkQueryPool *out) {
                driver->queryCount = info->queryCount;
                *out = driver->allocate<VkQueryPool>("query");
                return VK_SUCCESS;
            });
        mock(
            vkCreateSemaphore,
            +[](VkDevice, const VkSemaphoreCreateInfo *,
                const VkAllocationCallbacks *, VkSemaphore *out) {
                *out = driver->allocate<VkSemaphore>("semaphore");
                return VK_SUCCESS;
            });
        mock(
            vkCreateFence, +[](VkDevice, const VkFenceCreateInfo *info,
                               const VkAllocationCallbacks *, VkFence *out) {
                EXPECT_NE(info->flags & VK_FENCE_CREATE_SIGNALED_BIT, 0u);
                *out = driver->allocate<VkFence>("fence");
                return VK_SUCCESS;
            });
        mock(
            vkAllocateCommandBuffers,
            +[](VkDevice, const VkCommandBufferAllocateInfo *info,
                VkCommandBuffer *out) {
                for (uint32_t i = 0; i < info->commandBufferCount; ++i)
                    out[i] = driver->allocate<VkCommandBuffer>("command");
                return VK_SUCCESS;
            });
        mock(
            vkCreateImageView,
            +[](VkDevice, const VkImageViewCreateInfo *,
                const VkAllocationCallbacks *, VkImageView *out) {
                *out = driver->allocate<VkImageView>("view");
                return VK_SUCCESS;
            });
        mock(
            vkCreateFramebuffer,
            +[](VkDevice, const VkFramebufferCreateInfo *,
                const VkAllocationCallbacks *, VkFramebuffer *out) {
                *out = driver->allocate<VkFramebuffer>("framebuffer");
                return VK_SUCCESS;
            });
        mock(
            vkDeviceWaitIdle, +[](VkDevice) {
                driver->pending = false;
                ++driver->idleCalls;
                return VK_SUCCESS;
            });
        mock(
            vkFreeCommandBuffers, +[](VkDevice, VkCommandPool, uint32_t count,
                                      const VkCommandBuffer *buffers) {
                for (uint32_t i = 0; i < count; ++i)
                    driver->release(buffers[i], "command");
            });
#define MOCK_DESTROY(Name, Type, Kind)                                         \
    mock(                                                                      \
        Name, +[](VkDevice, Type handle, const VkAllocationCallbacks *) {      \
            EXPECT_EQ(driver->count("command"), 0u);                           \
            driver->release(handle, Kind);                                     \
        })
        MOCK_DESTROY(vkDestroySemaphore, VkSemaphore, "semaphore");
        MOCK_DESTROY(vkDestroyFence, VkFence, "fence");
        MOCK_DESTROY(vkDestroyQueryPool, VkQueryPool, "query");
        MOCK_DESTROY(vkDestroyFramebuffer, VkFramebuffer, "framebuffer");
        MOCK_DESTROY(vkDestroyImageView, VkImageView, "view");
#undef MOCK_DESTROY
    }

    void TearDown() override {
        for (auto it = restore.rbegin(); it != restore.rend(); ++it)
            (*it)();
        driver = nullptr;
    }

    void expectCapacity(uint32_t count) {
        EXPECT_EQ(renderer.swapchainImages.count, count);
        EXPECT_EQ(renderer.commandBuffers.count, count);
        EXPECT_EQ(renderer.fences.count, count);
        EXPECT_EQ(renderer.imageAvailableSemaphores.count, count);
        EXPECT_EQ(renderer.renderFinishedSemaphores.count, count);
        EXPECT_EQ(renderer.swapchainImageViews.count, count);
        EXPECT_EQ(renderer.frameBuffers.count, count);
        EXPECT_EQ(state.queryCount, 2 * count);
        EXPECT_EQ(state.count("command"), count);
        EXPECT_EQ(state.count("fence"), count);
        EXPECT_EQ(state.count("semaphore"), 2 * count);
        EXPECT_EQ(state.count("query"), 1u);
        EXPECT_EQ(state.count("view"), count);
        EXPECT_EQ(state.count("framebuffer"), count);
        EXPECT_EQ(state.count("swapchain"), 1u);
        for (uint32_t i = 0; i < count; ++i) {
            EXPECT_NE(renderer.commandBuffers.commandBuffers[i],
                      VK_NULL_HANDLE);
            EXPECT_NE(renderer.fences.fences[i], VK_NULL_HANDLE);
            EXPECT_NE(renderer.imageAvailableSemaphores.semaphores[i],
                      VK_NULL_HANDLE);
            EXPECT_NE(renderer.renderFinishedSemaphores.semaphores[i],
                      VK_NULL_HANDLE);
        }
    }

    void checkRecreations(const std::vector<uint32_t> &counts) {
        for (size_t i = 0; i < counts.size(); ++i) {
            SCOPED_TRACE(::testing::Message() << "Recreation " << i << ": "
                                              << counts[i] << " images");
            if (i != 0) {
                state.pending = true;
                renderer.destroyRenderContext();
                EXPECT_EQ(renderer.commandBuffers.count, 0u);
                EXPECT_EQ(renderer.fences.count, 0u);
                EXPECT_EQ(renderer.imageAvailableSemaphores.count, 0u);
                EXPECT_EQ(renderer.renderFinishedSemaphores.count, 0u);
                EXPECT_EQ(renderer.queryPool, VK_NULL_HANDLE);
                EXPECT_EQ(state.live.size(), 1u); // old swapchain only
            }
            state.imageCount = counts[i];
            renderer.setupRenderContext();
            expectCapacity(counts[i]);
        }
        state.pending = true;
        renderer.destroyRenderContext();
        vkDestroySwapchainKHR(renderer.logicalDevice, renderer.swapchain,
                              nullptr);
        EXPECT_TRUE(state.live.empty());
        EXPECT_EQ(state.idleCalls, counts.size());
    }
};

TEST_F(OnlineRenderContextTest, ImageCountIncreases) {
    checkRecreations({3, 4});
}

TEST_F(OnlineRenderContextTest, ImageCountDecreases) {
    checkRecreations({4, 2});
}

TEST_F(OnlineRenderContextTest, RepeatedAndUnchangedImageCounts) {
    checkRecreations({3, 4, 2, 2, 3, 4, 3});
}
