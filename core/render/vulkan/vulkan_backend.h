#pragma once

#include "core/render/render_backend.h"
#include "core/render/render_types.h"

#include <vulkan/vulkan.h>

#include <cstddef>
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
    void prepareBackdropBlur(const core::Rect& bounds, float blur, int windowWidth, int windowHeight) override;
    void drawRoundedRect(const RoundedRectDrawCommand& command, int windowWidth, int windowHeight) override;
    void drawText(const TextDrawCommand& command, int windowWidth, int windowHeight) override;
    TextureHandle createTexture(const unsigned char* pixels, int width, int height) override;
    bool updateTexture(TextureHandle handle, const unsigned char* pixels, int width, int height) override;
    void destroyTexture(TextureHandle handle) override;
    void drawTexture(TextureHandle handle,
                     const float* vertices,
                     std::size_t vertexFloatCount,
                     const core::Color& tint,
                     const core::Rect& rect,
                     float radius,
                     int windowWidth,
                     int windowHeight) override;

private:
    struct TextureResource {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        int width = 0;
        int height = 0;
        int channels = 0;
        std::uint64_t generation = 0;
    };

    bool createInstance();
    bool createSurface();
    bool pickDevice();
    bool createDevice();
    bool recreateSwapchain(const RenderSurface& surface);
    void destroySwapchain();
    void destroy();
    void recordClearPass(const core::Color& color);
    void beginLoadPass();
    void destroyRenderCacheResources();
    void transitionRenderCacheImage(VkImageLayout newLayout);
    VkExtent2D currentRenderExtent() const;
    VkFramebuffer currentFramebuffer() const;
    bool ensureRoundedRectPipeline();
    bool ensureBackdropResources();
    bool ensureBackdropDescriptor();
    void initializeBackdropImageIfNeeded();
    bool ensurePrimitiveVertexBuffer(std::size_t vertexCount);
    bool ensureTextPipeline();
    bool textAtlasNeedsUpload(const TextAtlasPageData& page) const;
    bool ensureTextAtlas(const TextAtlasPageData& page);
    bool ensureTextDescriptor();
    bool ensureTextVertexBuffer(std::size_t floatCount);
    bool ensureImagePipeline();
    bool ensureImageDescriptor(TextureResource& texture);
    bool ensureImageVertexBuffer();
    void destroyRoundedRectPipeline();
    void destroyBackdropResources();
    void destroyBackdropDescriptorPool();
    void destroyPrimitiveVertexBuffer();
    void destroyTextPipeline();
    void destroyTextResources();
    void destroyImagePipeline();
    void destroyImageResources();
    void destroyTextureResource(TextureResource& texture);
    void releasePendingTextureDeletes();
    void releasePendingUploads();
    void transitionSwapchainImage(VkImageLayout newLayout);
    std::uint32_t findMemoryType(std::uint32_t filter, VkMemoryPropertyFlags properties) const;

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
    std::vector<VkImageLayout> swapchainImageLayouts_;
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
    bool swapchainTransferSrcSupported_ = false;
    bool swapchainTransferDstSupported_ = false;
    bool backdropReady_ = false;
    core::Rect scissorRect_{};
    core::Color clearColor_{0.0f, 0.0f, 0.0f, 1.0f};

    VkDescriptorSetLayout roundedRectDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool roundedRectDescriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet roundedRectDescriptorSet_ = VK_NULL_HANDLE;
    VkPipelineLayout roundedRectPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline roundedRectPipeline_ = VK_NULL_HANDLE;
    VkImage backdropImage_ = VK_NULL_HANDLE;
    VkDeviceMemory backdropImageMemory_ = VK_NULL_HANDLE;
    VkImageView backdropImageView_ = VK_NULL_HANDLE;
    VkSampler backdropSampler_ = VK_NULL_HANDLE;
    VkImageLayout backdropImageLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    VkExtent2D backdropExtent_{};
    VkBuffer primitiveVertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory primitiveVertexMemory_ = VK_NULL_HANDLE;
    void* primitiveVertexMapped_ = nullptr;
    std::size_t primitiveVertexCapacity_ = 0;
    std::size_t primitiveVertexUsed_ = 0;

    VkImage renderCacheImage_ = VK_NULL_HANDLE;
    VkDeviceMemory renderCacheMemory_ = VK_NULL_HANDLE;
    VkImageView renderCacheView_ = VK_NULL_HANDLE;
    VkFramebuffer renderCacheFramebuffer_ = VK_NULL_HANDLE;
    VkImageLayout renderCacheLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    VkExtent2D renderCacheExtent_{};
    bool renderCacheRecreated_ = false;
    bool renderingToCache_ = false;

    VkDescriptorSetLayout textDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool textDescriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet textDescriptorSet_ = VK_NULL_HANDLE;
    VkPipelineLayout textPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline textPipeline_ = VK_NULL_HANDLE;
    TextureResource textGrayAtlas_;
    TextureResource textColorAtlas_;
    bool textDescriptorDirty_ = true;
    VkBuffer textVertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory textVertexMemory_ = VK_NULL_HANDLE;
    void* textVertexMapped_ = nullptr;
    std::size_t textVertexCapacity_ = 0;
    std::size_t textVertexUsed_ = 0;
    VkDescriptorSetLayout imageDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool imageDescriptorPool_ = VK_NULL_HANDLE;
    VkPipelineLayout imagePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline imagePipeline_ = VK_NULL_HANDLE;
    VkBuffer imageVertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory imageVertexMemory_ = VK_NULL_HANDLE;
    void* imageVertexMapped_ = nullptr;
    std::size_t imageVertexCapacity_ = 0;
    std::size_t imageVertexUsed_ = 0;
    std::vector<VkBuffer> pendingUploadBuffers_;
    std::vector<VkDeviceMemory> pendingUploadMemories_;
    std::vector<TextureResource*> pendingTextureDeletes_;
};

} // namespace core::render::vulkan
