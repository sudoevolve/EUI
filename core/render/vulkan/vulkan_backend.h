#pragma once

#include "core/render/render_backend.h"
#include "core/render/render_types.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace core::render::vulkan {

class VulkanRenderBackend final : public RenderBackend {
public:
    explicit VulkanRenderBackend(core::window::Handle window, RenderBackend* shareBackend = nullptr);
    ~VulkanRenderBackend() override;

    VulkanRenderBackend(const VulkanRenderBackend&) = delete;
    VulkanRenderBackend& operator=(const VulkanRenderBackend&) = delete;

    bool initialize() override;
    bool valid() const override;
    void makeCurrent() override;
    void beginFrame(const RenderSurface& surface) override;
    void present() override;
    bool ensureRenderCache(int width, int height) override;
    bool renderCacheWasRecreated() const override;
    void releaseRenderCache() override;
    void beginRenderCacheFrame(int width, int height) override;
    void endRenderCacheFrame() override;
    void blitRenderCache(int width, int height) override;
    void clear(const core::Color& color) override;
    void setScissor(bool enabled, const core::Rect& rect, int framebufferHeight) override;
    void fillRect(const core::Rect& rect, const core::Color& color, int windowWidth, int windowHeight);

private:
    bool createInstance();
    bool createSurface();
    bool pickDevice();
    bool createDevice();
    bool recreateSwapchain(const RenderSurface& surface);
    void destroySwapchain();
    void destroy();
    void recordClearPass(const core::Color& color);

    core::window::Handle window_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    std::uint32_t graphicsFamily_ = 0;
    std::uint32_t presentFamily_ = 0;
    bool initialized_ = false;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;
    VkSemaphore imageAvailable_ = VK_NULL_HANDLE;
    VkSemaphore renderFinished_ = VK_NULL_HANDLE;
    VkFence inFlight_ = VK_NULL_HANDLE;
    std::uint32_t currentImage_ = 0;
    bool frameActive_ = false;
    bool frameRecorded_ = false;
    bool renderPassActive_ = false;
    bool scissorEnabled_ = false;
    core::Rect scissorRect_{};
    core::Color clearColor_{0.0f, 0.0f, 0.0f, 1.0f};
};

} // namespace core::render::vulkan
