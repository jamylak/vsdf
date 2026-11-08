#include "online_sdf_renderer.h"
#include "test_utils.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <vector>

namespace {
// Intercept volk's entry points after setup, forwarding to the real driver.
// SUBOPTIMAL is injected only after a real acquire has signaled its semaphore.
// A pre-existing indexing mismatch waited on imageIndex after acquiring with
// frameIndex, and presented with frameIndex after signaling imageIndex. Check
// the actual semaphore handles on every frame, including after recreation.
// These GPU tests keep the swapchain image count unchanged; capacity changes
// during resize require separate coverage.
struct RecoveryProbe {
    PFN_vkAcquireNextImageKHR acquire = vkAcquireNextImageKHR;
    PFN_vkQueueSubmit submit = vkQueueSubmit;
    PFN_vkQueuePresentKHR present = vkQueuePresentKHR;
    PFN_vkCreateSwapchainKHR createSwapchain = vkCreateSwapchainKHR;
    PFN_vkResetFences resetFences = vkResetFences;
    VkResult injectedAcquire;
    VkResult injectedPresent;
    uint32_t acquisitions = 0;
    uint32_t submissions = 0;
    uint32_t presentations = 0;
    uint32_t recreations = 0;
    uint32_t fenceResets = 0;
    uint32_t acquiredImage = 0;
    VkSemaphore acquiredSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderSemaphore = VK_NULL_HANDLE;
    bool pendingImage = false;
    std::vector<char> events;
    static inline RecoveryProbe *active = nullptr;

    RecoveryProbe(VkResult acquireResult, VkResult presentResult)
        : injectedAcquire(acquireResult), injectedPresent(presentResult) {
        active = this;
        vkAcquireNextImageKHR = acquireHook;
        vkQueueSubmit = submitHook;
        vkQueuePresentKHR = presentHook;
        vkCreateSwapchainKHR = createHook;
        vkResetFences = resetHook;
    }
    ~RecoveryProbe() {
        vkAcquireNextImageKHR = acquire;
        vkQueueSubmit = submit;
        vkQueuePresentKHR = present;
        vkCreateSwapchainKHR = createSwapchain;
        vkResetFences = resetFences;
        active = nullptr;
    }

    static VKAPI_ATTR VkResult VKAPI_CALL
    acquireHook(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout,
                VkSemaphore semaphore, VkFence fence, uint32_t *image) {
        auto &p = *active;
        EXPECT_FALSE(p.pendingImage);
        ++p.acquisitions;
        if (p.acquisitions == 3 &&
            p.injectedAcquire == VK_ERROR_OUT_OF_DATE_KHR) {
            // Failed acquisition must neither signal nor return an image.
            p.events.push_back('O');
            return VK_ERROR_OUT_OF_DATE_KHR;
        }
        const auto result =
            p.acquire(device, swapchain, timeout, semaphore, fence, image);
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            ADD_FAILURE() << "Unexpected real acquire result: " << result;
            return result;
        }
        p.pendingImage = true;
        p.acquiredImage = *image;
        p.acquiredSemaphore = semaphore;
        if (p.acquisitions == 3 && p.injectedAcquire == VK_SUBOPTIMAL_KHR) {
            p.events.push_back('S');
            return VK_SUBOPTIMAL_KHR;
        }
        p.events.push_back('A');
        return result;
    }

    static VKAPI_ATTR VkResult VKAPI_CALL submitHook(VkQueue queue,
                                                     uint32_t count,
                                                     const VkSubmitInfo *infos,
                                                     VkFence fence) {
        auto &p = *active;
        EXPECT_TRUE(p.pendingImage);
        EXPECT_EQ(count, 1u);
        EXPECT_EQ(infos[0].waitSemaphoreCount, 1u);
        EXPECT_EQ(infos[0].pWaitSemaphores[0], p.acquiredSemaphore);
        EXPECT_EQ(infos[0].signalSemaphoreCount, 1u);
        p.renderSemaphore = infos[0].pSignalSemaphores[0];
        ++p.submissions;
        p.events.push_back('Q');
        return p.submit(queue, count, infos, fence);
    }

    static VKAPI_ATTR VkResult VKAPI_CALL
    presentHook(VkQueue queue, const VkPresentInfoKHR *info) {
        auto &p = *active;
        EXPECT_TRUE(p.pendingImage);
        EXPECT_EQ(info->pImageIndices[0], p.acquiredImage);
        EXPECT_EQ(info->pWaitSemaphores[0], p.renderSemaphore);
        const auto result = p.present(queue, info);
        p.pendingImage = false;
        ++p.presentations;
        p.events.push_back('P');
        // Consume the real present wait before overriding its status.
        if (p.presentations == 3 && p.injectedPresent != VK_SUCCESS) {
            EXPECT_TRUE(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);
            return p.injectedPresent;
        }
        return result;
    }

    static VKAPI_ATTR VkResult VKAPI_CALL createHook(
        VkDevice device, const VkSwapchainCreateInfoKHR *info,
        const VkAllocationCallbacks *allocator, VkSwapchainKHR *swapchain) {
        auto &p = *active;
        EXPECT_FALSE(p.pendingImage)
            << "Recreation abandoned a successfully acquired image";
        EXPECT_NE(info->oldSwapchain, VK_NULL_HANDLE);
        ++p.recreations;
        p.events.push_back('R');
        return p.createSwapchain(device, info, allocator, swapchain);
    }

    static VKAPI_ATTR VkResult VKAPI_CALL resetHook(VkDevice device,
                                                    uint32_t count,
                                                    const VkFence *fences) {
        auto &p = *active;
        EXPECT_TRUE(p.pendingImage)
            << "Failed acquisition must not reset a frame fence";
        ++p.fenceResets;
        return p.resetFences(device, count, fences);
    }
};

void checkRecovery(VkResult acquireResult, VkResult presentResult) {
    if (shouldSkipSmokeTests()) {
        GTEST_SKIP() << "GPU recovery test requires VSDF_SMOKE_TESTS=1 in CI";
    }
    // Absolute shader path; no working-directory changes or application hooks.
    OnlineRenderOptions options{};
    options.headless = true;
    options.maxFrames = 8;
    OnlineSDFRenderer renderer((std::filesystem::path(VSDF_SOURCE_DIR) /
                                "shaders/debug_quadrants.frag")
                                   .string(),
                               true, options);
    renderer.setup();
    RecoveryProbe probe(acquireResult, presentResult);
    renderer.gameLoop();

    EXPECT_EQ(probe.recreations, 1u);
    EXPECT_EQ(probe.submissions, 8u);
    EXPECT_EQ(probe.presentations, 8u);
    EXPECT_EQ(probe.fenceResets, 8u);
    EXPECT_FALSE(probe.pendingImage);
    const bool failedAcquire = acquireResult == VK_ERROR_OUT_OF_DATE_KHR;
    EXPECT_EQ(probe.acquisitions, failedAcquire ? 9u : 8u);
    const std::vector<char> expected =
        failedAcquire
            ? std::vector<char>{'O', 'R', 'A', 'Q', 'P'}
            : std::vector<char>{acquireResult == VK_SUBOPTIMAL_KHR ? 'S' : 'A',
                                'Q',
                                'P',
                                'R',
                                'A',
                                'Q',
                                'P'};
    EXPECT_NE(std::search(probe.events.begin(), probe.events.end(),
                          expected.begin(), expected.end()),
              probe.events.end());
}
} // namespace

TEST(OnlineAcquireRecovery, SuboptimalAcquireCompletesBeforeRecreation) {
    checkRecovery(VK_SUBOPTIMAL_KHR, VK_SUCCESS);
}

TEST(OnlineAcquireRecovery, OutOfDateAcquireSkipsSubmission) {
    checkRecovery(VK_ERROR_OUT_OF_DATE_KHR, VK_SUCCESS);
}

TEST(OnlineAcquireRecovery, SuboptimalAcquireAndPresentRecreateOnce) {
    checkRecovery(VK_SUBOPTIMAL_KHR, VK_SUBOPTIMAL_KHR);
}

TEST(OnlineAcquireRecovery, SuboptimalAcquireAndOutOfDatePresentRecreateOnce) {
    checkRecovery(VK_SUBOPTIMAL_KHR, VK_ERROR_OUT_OF_DATE_KHR);
}
