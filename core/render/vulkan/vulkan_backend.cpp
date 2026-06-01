#include "core/render/vulkan/vulkan_backend.h"

#include "core/render/render_types.h"
#include "core/render/vulkan/vulkan_rounded_rect_shaders.h"
#include "core/render/vulkan/vulkan_image_shaders.h"
#include "core/render/vulkan/vulkan_text_shaders.h"

#if defined(EUI_WINDOW_BACKEND_SDL2)
#include <SDL.h>
#include <SDL_vulkan.h>
#else
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>

namespace core::render::vulkan {

namespace {

constexpr std::size_t kPrimitiveVertexCapacity = 65536;
constexpr std::size_t kTextVertexFloatCapacity = 262144;
constexpr std::size_t kImageVertexFloatCapacity = 65536;

bool hasExtension(const std::vector<VkExtensionProperties>& extensions, const char* name) {
    return std::any_of(extensions.begin(), extensions.end(), [&](const VkExtensionProperties& extension) {
        return std::strcmp(extension.extensionName, name) == 0;
    });
}

void addUniqueExtension(std::vector<const char*>& extensions, const char* name) {
    const auto exists = std::any_of(extensions.begin(), extensions.end(), [&](const char* extension) {
        return std::strcmp(extension, name) == 0;
    });
    if (!exists) {
        extensions.push_back(name);
    }
}

std::vector<VkExtensionProperties> instanceExtensions() {
    std::uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> extensions(count);
    if (count > 0) {
        vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());
    }
    return extensions;
}

std::vector<VkExtensionProperties> deviceExtensions(VkPhysicalDevice device) {
    std::uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> extensions(count);
    if (count > 0) {
        vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
    }
    return extensions;
}

VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const VkSurfaceFormatKHR& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR} : formats.front();
}

VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
    for (VkPresentModeKHR mode : modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

struct RoundedRectPushConstants {
    float windowAndShape[4] = {};
    float fillColor[4] = {};
    float gradientStart[4] = {};
    float gradientEnd[4] = {};
    float borderColor[4] = {};
    float rect[4] = {};
    float flags[4] = {};
    float flags2[4] = {};
};

static_assert(sizeof(RoundedRectPushConstants) == 128, "Rounded rect push constants must fit Vulkan 1.0 minimum size.");

struct TextPushConstants {
    float windowSize[4] = {};
    float color[4] = {};
};

struct ImagePushConstants {
    float windowSize[4] = {};
    float tint[4] = {};
    float rect[4] = {};
    float flags[4] = {};
};

void writeColor(float (&target)[4], const core::Color& color) {
    target[0] = color.r;
    target[1] = color.g;
    target[2] = color.b;
    target[3] = color.a;
}

VkShaderModule createShaderModule(VkDevice device, const std::uint32_t* code, std::size_t codeSize) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = codeSize;
    createInfo.pCode = code;

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return module;
}

VkAccessFlags accessMaskForLayout(VkImageLayout layout) {
    switch (layout) {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_ACCESS_SHADER_READ_BIT;
        default:
            return 0;
    }
}

VkPipelineStageFlags sourceStageForLayout(VkImageLayout layout) {
    switch (layout) {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        default:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

VkPipelineStageFlags destinationStageForLayout(VkImageLayout layout) {
    if (layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    return sourceStageForLayout(layout);
}

void transitionImageLayout(VkCommandBuffer commandBuffer,
                           VkImage image,
                           VkImageLayout oldLayout,
                           VkImageLayout newLayout) {
    if (image == VK_NULL_HANDLE || oldLayout == newLayout) {
        return;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = accessMaskForLayout(oldLayout);
    barrier.dstAccessMask = accessMaskForLayout(newLayout);

    vkCmdPipelineBarrier(commandBuffer,
                         sourceStageForLayout(oldLayout),
                         destinationStageForLayout(newLayout),
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);
}

VkRect2D clampScissor(const core::Rect& rect, int windowWidth, int windowHeight) {
    const float left = std::clamp(rect.x, 0.0f, static_cast<float>(std::max(0, windowWidth)));
    const float top = std::clamp(rect.y, 0.0f, static_cast<float>(std::max(0, windowHeight)));
    const float right = std::clamp(rect.x + rect.width, left, static_cast<float>(std::max(0, windowWidth)));
    const float bottom = std::clamp(rect.y + rect.height, top, static_cast<float>(std::max(0, windowHeight)));

    VkRect2D result{};
    result.offset = {
        static_cast<std::int32_t>(left),
        static_cast<std::int32_t>(top)
    };
    result.extent = {
        static_cast<std::uint32_t>(std::max(0.0f, right - left)),
        static_cast<std::uint32_t>(std::max(0.0f, bottom - top))
    };
    return result;
}

} // namespace

VulkanRenderBackend::VulkanRenderBackend(core::window::Handle window, RenderBackend*) : window_(window) {}

VulkanRenderBackend::~VulkanRenderBackend() {
    destroy();
}

bool VulkanRenderBackend::initialize() {
    if (window_ == nullptr) {
        std::fprintf(stderr, "EUI Vulkan: initialize failed: window is null\n");
        return false;
    }
    if (!createInstance()) {
        std::fprintf(stderr, "EUI Vulkan: initialize failed: createInstance\n");
        return false;
    }
    if (!createSurface()) {
        std::fprintf(stderr, "EUI Vulkan: initialize failed: createSurface\n");
        return false;
    }
    if (!pickDevice()) {
        std::fprintf(stderr, "EUI Vulkan: initialize failed: pickDevice\n");
        return false;
    }
    if (!createDevice()) {
        std::fprintf(stderr, "EUI Vulkan: initialize failed: createDevice\n");
        return false;
    }
    initialized_ = true;
    return initialized_;
}

bool VulkanRenderBackend::valid() const {
    return initialized_;
}

void VulkanRenderBackend::makeCurrent() {}

void VulkanRenderBackend::beginFrame(const RenderSurface& surface) {
    if (!initialized_ || surface.framebufferWidth <= 0 || surface.framebufferHeight <= 0) {
        return;
    }
    if (swapchain_ == VK_NULL_HANDLE ||
        swapchainExtent_.width != static_cast<std::uint32_t>(surface.framebufferWidth) ||
        swapchainExtent_.height != static_cast<std::uint32_t>(surface.framebufferHeight)) {
        recreateSwapchain(surface);
    }
    if (swapchain_ == VK_NULL_HANDLE || commandBuffers_.empty()) {
        return;
    }

    vkWaitForFences(device_, 1, &inFlight_, VK_TRUE, UINT64_MAX);
    releasePendingTextureDeletes();
    releasePendingUploads();

    const VkResult acquire = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, imageAvailable_, VK_NULL_HANDLE, &currentImage_);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain(surface);
        return;
    }
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
        return;
    }

    vkResetCommandBuffer(commandBuffers_[currentImage_], 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(commandBuffers_[currentImage_], &beginInfo) != VK_SUCCESS) {
        return;
    }
    frameActive_ = true;
    frameRecorded_ = false;
    renderPassActive_ = false;
    renderingToCache_ = false;
    backdropReady_ = false;
    primitiveVertexUsed_ = 0;
    textVertexUsed_ = 0;
    imageVertexUsed_ = 0;
}

void VulkanRenderBackend::present() {
    if (!frameActive_) {
        return;
    }
    if (!frameRecorded_) {
        recordClearPass(clearColor_);
    }
    if (renderPassActive_) {
        vkCmdEndRenderPass(commandBuffers_[currentImage_]);
        renderPassActive_ = false;
    }
    transitionSwapchainImage(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    vkEndCommandBuffer(commandBuffers_[currentImage_]);

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailable_;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[currentImage_];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinished_;
    vkResetFences(device_, 1, &inFlight_);
    if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlight_) != VK_SUCCESS) {
        vkDestroyFence(device_, inFlight_, nullptr);
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(device_, &fenceInfo, nullptr, &inFlight_);
        frameActive_ = false;
        renderPassActive_ = false;
        return;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinished_;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &currentImage_;
    const VkResult presentResult = vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        swapchainExtent_ = {};
    }
    frameActive_ = false;
    backdropReady_ = false;
}

bool VulkanRenderBackend::ensureRenderCache(int width, int height) {
    renderCacheRecreated_ = false;
    if (!frameActive_ || device_ == VK_NULL_HANDLE || renderPass_ == VK_NULL_HANDLE ||
        swapchainFormat_ == VK_FORMAT_UNDEFINED || !swapchainTransferDstSupported_) {
        return false;
    }

    width = std::max(1, width);
    height = std::max(1, height);
    const VkExtent2D extent{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
    if (renderCacheImage_ != VK_NULL_HANDLE &&
        renderCacheView_ != VK_NULL_HANDLE &&
        renderCacheFramebuffer_ != VK_NULL_HANDLE &&
        renderCacheExtent_.width == extent.width &&
        renderCacheExtent_.height == extent.height) {
        return true;
    }

    destroyRenderCacheResources();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {extent.width, extent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = swapchainFormat_;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(device_, &imageInfo, nullptr, &renderCacheImage_) != VK_SUCCESS) {
        destroyRenderCacheResources();
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(device_, renderCacheImage_, &memoryRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (allocInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
        vkAllocateMemory(device_, &allocInfo, nullptr, &renderCacheMemory_) != VK_SUCCESS ||
        vkBindImageMemory(device_, renderCacheImage_, renderCacheMemory_, 0) != VK_SUCCESS) {
        destroyRenderCacheResources();
        return false;
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = renderCacheImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = swapchainFormat_;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device_, &viewInfo, nullptr, &renderCacheView_) != VK_SUCCESS) {
        destroyRenderCacheResources();
        return false;
    }

    VkImageView attachments[] = {renderCacheView_};
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass_;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = extent.width;
    framebufferInfo.height = extent.height;
    framebufferInfo.layers = 1;
    if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &renderCacheFramebuffer_) != VK_SUCCESS) {
        destroyRenderCacheResources();
        return false;
    }

    renderCacheExtent_ = extent;
    renderCacheLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    renderCacheRecreated_ = true;
    return true;
}

bool VulkanRenderBackend::renderCacheWasRecreated() const {
    return renderCacheRecreated_;
}

void VulkanRenderBackend::releaseRenderCache() {
    if (device_ != VK_NULL_HANDLE && !frameActive_) {
        vkDeviceWaitIdle(device_);
    }
    destroyRenderCacheResources();
    renderCacheRecreated_ = false;
}

void VulkanRenderBackend::beginRenderCacheFrame(int, int) {
    if (!frameActive_ || renderCacheFramebuffer_ == VK_NULL_HANDLE) {
        return;
    }
    if (renderPassActive_) {
        vkCmdEndRenderPass(commandBuffers_[currentImage_]);
        renderPassActive_ = false;
    }
    renderingToCache_ = true;
}

void VulkanRenderBackend::endRenderCacheFrame() {
    if (renderPassActive_) {
        vkCmdEndRenderPass(commandBuffers_[currentImage_]);
        renderPassActive_ = false;
    }
    renderingToCache_ = false;
}

void VulkanRenderBackend::blitRenderCache(int width, int height) {
    if (!frameActive_ || renderCacheImage_ == VK_NULL_HANDLE || renderCacheExtent_.width == 0 ||
        renderCacheExtent_.height == 0 || commandBuffers_.empty() || currentImage_ >= commandBuffers_.size() ||
        currentImage_ >= swapchainImages_.size()) {
        return;
    }
    if (renderPassActive_) {
        vkCmdEndRenderPass(commandBuffers_[currentImage_]);
        renderPassActive_ = false;
    }
    renderingToCache_ = false;

    VkCommandBuffer commandBuffer = commandBuffers_[currentImage_];
    transitionRenderCacheImage(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transitionSwapchainImage(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkImageCopy copyRegion{};
    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.extent = {
        static_cast<std::uint32_t>(std::max(1, width)),
        static_cast<std::uint32_t>(std::max(1, height)),
        1
    };
    copyRegion.extent.width = std::min(copyRegion.extent.width, renderCacheExtent_.width);
    copyRegion.extent.height = std::min(copyRegion.extent.height, renderCacheExtent_.height);
    copyRegion.extent.width = std::min(copyRegion.extent.width, swapchainExtent_.width);
    copyRegion.extent.height = std::min(copyRegion.extent.height, swapchainExtent_.height);
    if (copyRegion.extent.width == 0 || copyRegion.extent.height == 0) {
        return;
    }

    vkCmdCopyImage(commandBuffer,
                   renderCacheImage_,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swapchainImages_[currentImage_],
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &copyRegion);
    frameRecorded_ = true;
}

void VulkanRenderBackend::clear(const core::Color& color) {
    clearColor_ = color;
    if (!frameActive_) {
        return;
    }
    if (!renderPassActive_) {
        recordClearPass(color);
        return;
    }

    VkClearValue clearValue{};
    clearValue.color = {{color.r, color.g, color.b, color.a}};

    VkClearAttachment clearAttachment{};
    clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearAttachment.colorAttachment = 0;
    clearAttachment.clearValue = clearValue;

    const VkExtent2D extent = currentRenderExtent();
    const core::Rect fullRect{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height)};
    VkClearRect clearRect{};
    clearRect.rect = clampScissor(scissorEnabled_ ? scissorRect_ : fullRect,
                                  static_cast<int>(extent.width),
                                  static_cast<int>(extent.height));
    clearRect.layerCount = 1;
    if (clearRect.rect.extent.width == 0 || clearRect.rect.extent.height == 0) {
        return;
    }
    vkCmdClearAttachments(commandBuffers_[currentImage_], 1, &clearAttachment, 1, &clearRect);
}

void VulkanRenderBackend::setScissor(bool enabled, const core::Rect& rect, int) {
    scissorEnabled_ = enabled;
    scissorRect_ = rect;
}

void VulkanRenderBackend::prepareBackdropBlur(const core::Rect&, float blur, int windowWidth, int windowHeight) {
    const bool sourceIsCache = renderingToCache_;
    if (!frameActive_ || blur <= 0.0f || windowWidth <= 0 || windowHeight <= 0 ||
        (!sourceIsCache && !swapchainTransferSrcSupported_) ||
        swapchainExtent_.width == 0 || swapchainExtent_.height == 0 ||
        (sourceIsCache && (renderCacheImage_ == VK_NULL_HANDLE ||
                           renderCacheExtent_.width == 0 ||
                           renderCacheExtent_.height == 0))) {
        backdropReady_ = false;
        return;
    }

    if (!ensureRoundedRectPipeline() || !ensureBackdropResources()) {
        backdropReady_ = false;
        return;
    }
    if (!frameRecorded_) {
        recordClearPass(clearColor_);
    }
    if (renderPassActive_) {
        vkCmdEndRenderPass(commandBuffers_[currentImage_]);
        renderPassActive_ = false;
    }

    VkCommandBuffer commandBuffer = commandBuffers_[currentImage_];
    if (sourceIsCache) {
        transitionRenderCacheImage(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    } else {
        transitionSwapchainImage(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    }
    transitionImageLayout(commandBuffer, backdropImage_, backdropImageLayout_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    backdropImageLayout_ = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    const VkExtent2D sourceExtent = sourceIsCache ? renderCacheExtent_ : swapchainExtent_;
    VkImageCopy copyRegion{};
    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.extent = {sourceExtent.width, sourceExtent.height, 1};
    vkCmdCopyImage(commandBuffer,
                   sourceIsCache ? renderCacheImage_ : swapchainImages_[currentImage_],
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   backdropImage_,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &copyRegion);

    transitionImageLayout(commandBuffer, backdropImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    backdropImageLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (sourceIsCache) {
        transitionRenderCacheImage(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    } else {
        transitionSwapchainImage(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
    backdropReady_ = true;
    beginLoadPass();
}

void VulkanRenderBackend::drawRoundedRect(const RoundedRectDrawCommand& command, int windowWidth, int windowHeight) {
    if (!frameActive_ || windowWidth <= 0 || windowHeight <= 0 || !roundedRectHasVisibleContent(command)) {
        return;
    }
    if (!ensureRoundedRectPipeline() || !ensureBackdropResources()) {
        return;
    }
    if (!frameRecorded_) {
        recordClearPass(clearColor_);
    }
    if (!renderPassActive_ || !ensurePrimitiveVertexBuffer(command.vertices.size())) {
        return;
    }

    const std::size_t vertexOffset = primitiveVertexUsed_;
    auto* mappedVertices = static_cast<PrimitiveGeometryVertex*>(primitiveVertexMapped_);
    std::copy(command.vertices.begin(), command.vertices.end(), mappedVertices + vertexOffset);
    primitiveVertexUsed_ += command.vertices.size();

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(windowWidth);
    viewport.height = static_cast<float>(windowHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffers_[currentImage_], 0, 1, &viewport);

    const core::Rect fullRect{0.0f, 0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight)};
    const VkRect2D scissor = clampScissor(scissorEnabled_ ? scissorRect_ : fullRect, windowWidth, windowHeight);
    if (scissor.extent.width == 0 || scissor.extent.height == 0) {
        return;
    }
    vkCmdSetScissor(commandBuffers_[currentImage_], 0, 1, &scissor);

    RoundedRectPushConstants constants{};
    constants.windowAndShape[0] = static_cast<float>(windowWidth);
    constants.windowAndShape[1] = static_cast<float>(windowHeight);
    constants.windowAndShape[2] = command.radius;
    constants.windowAndShape[3] = command.shadowPass ? 0.0f : command.border.width;
    writeColor(constants.fillColor, command.fillColor);
    writeColor(constants.gradientStart, command.gradient.start);
    writeColor(constants.gradientEnd, command.gradient.end);
    writeColor(constants.borderColor, command.border.color);
    constants.rect[0] = command.rect.x;
    constants.rect[1] = command.rect.y;
    constants.rect[2] = command.rect.width;
    constants.rect[3] = command.rect.height;
    constants.flags[0] = command.opacity;
    constants.flags[1] = command.shadowBlur;
    constants.flags[2] = command.gradient.enabled && !command.shadowPass ? 1.0f : 0.0f;
    constants.flags[3] = static_cast<float>(command.gradient.direction == core::GradientDirection::Horizontal ? 0 : 1);
    constants.flags2[0] = command.shadowPass ? 1.0f : 0.0f;
    constants.flags2[1] = command.shadowPass ? 0.0f : command.backdropBlur;
    constants.flags2[2] = (!command.shadowPass && command.backdropBlur > 0.0f && backdropReady_) ? 1.0f : 0.0f;

    const VkDeviceSize bufferOffset = static_cast<VkDeviceSize>(vertexOffset * sizeof(PrimitiveGeometryVertex));
    vkCmdBindPipeline(commandBuffers_[currentImage_], VK_PIPELINE_BIND_POINT_GRAPHICS, roundedRectPipeline_);
    vkCmdBindDescriptorSets(commandBuffers_[currentImage_],
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            roundedRectPipelineLayout_,
                            0,
                            1,
                            &roundedRectDescriptorSet_,
                            0,
                            nullptr);
    vkCmdBindVertexBuffers(commandBuffers_[currentImage_], 0, 1, &primitiveVertexBuffer_, &bufferOffset);
    vkCmdPushConstants(commandBuffers_[currentImage_],
                       roundedRectPipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(constants),
                       &constants);
    vkCmdDraw(commandBuffers_[currentImage_], static_cast<std::uint32_t>(command.vertices.size()), 1, 0, 0);
}

void VulkanRenderBackend::drawText(const TextDrawCommand& command, int windowWidth, int windowHeight) {
    if (!frameActive_ || command.vertices == nullptr || command.vertexFloatCount == 0 ||
        windowWidth <= 0 || windowHeight <= 0 || command.color.a <= 0.001f) {
        return;
    }

    if (!ensureTextPipeline()) {
        return;
    }
    if (!frameRecorded_) {
        recordClearPass(clearColor_);
    }

    const bool uploadNeeded = textAtlasNeedsUpload(command.grayAtlas) || textAtlasNeedsUpload(command.colorAtlas);
    if (uploadNeeded && renderPassActive_) {
        vkCmdEndRenderPass(commandBuffers_[currentImage_]);
        renderPassActive_ = false;
    }

    if (!ensureTextAtlas(command.grayAtlas)) {
        if (!renderPassActive_) {
            beginLoadPass();
        }
        return;
    }
    if (command.colorAtlas.pixels != nullptr && command.colorAtlas.width > 0 && command.colorAtlas.height > 0) {
        ensureTextAtlas(command.colorAtlas);
    }
    if (uploadNeeded && !renderPassActive_) {
        beginLoadPass();
    }
    if (!renderPassActive_ || !ensureTextDescriptor() || !ensureTextVertexBuffer(command.vertexFloatCount)) {
        return;
    }

    const std::size_t floatOffset = textVertexUsed_;
    auto* mappedTextVertices = static_cast<float*>(textVertexMapped_);
    std::memcpy(mappedTextVertices + floatOffset, command.vertices, command.vertexFloatCount * sizeof(float));
    textVertexUsed_ += command.vertexFloatCount;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(windowWidth);
    viewport.height = static_cast<float>(windowHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffers_[currentImage_], 0, 1, &viewport);

    const core::Rect fullRect{0.0f, 0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight)};
    const VkRect2D scissor = clampScissor(scissorEnabled_ ? scissorRect_ : fullRect, windowWidth, windowHeight);
    if (scissor.extent.width == 0 || scissor.extent.height == 0) {
        return;
    }
    vkCmdSetScissor(commandBuffers_[currentImage_], 0, 1, &scissor);

    TextPushConstants constants{};
    constants.windowSize[0] = static_cast<float>(windowWidth);
    constants.windowSize[1] = static_cast<float>(windowHeight);
    writeColor(constants.color, command.color);

    const VkDeviceSize offset = static_cast<VkDeviceSize>(floatOffset * sizeof(float));
    vkCmdBindPipeline(commandBuffers_[currentImage_], VK_PIPELINE_BIND_POINT_GRAPHICS, textPipeline_);
    vkCmdBindDescriptorSets(commandBuffers_[currentImage_],
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            textPipelineLayout_,
                            0,
                            1,
                            &textDescriptorSet_,
                            0,
                            nullptr);
    vkCmdBindVertexBuffers(commandBuffers_[currentImage_], 0, 1, &textVertexBuffer_, &offset);
    vkCmdPushConstants(commandBuffers_[currentImage_],
                       textPipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(constants),
                       &constants);
    vkCmdDraw(commandBuffers_[currentImage_], static_cast<std::uint32_t>(command.vertexFloatCount / 5), 1, 0, 0);
}

VulkanRenderBackend::TextureHandle VulkanRenderBackend::createTexture(const unsigned char* pixels, int width, int height) {
    if (pixels == nullptr || width <= 0 || height <= 0 || !frameActive_) {
        return nullptr;
    }
    auto* texture = new TextureResource();
    if (!updateTexture(texture, pixels, width, height)) {
        delete texture;
        return nullptr;
    }
    return texture;
}

bool VulkanRenderBackend::updateTexture(TextureHandle handle, const unsigned char* pixels, int width, int height) {
    auto* texture = static_cast<TextureResource*>(handle);
    if (texture == nullptr || pixels == nullptr || width <= 0 || height <= 0 || !frameActive_) {
        return false;
    }
    if (renderPassActive_) {
        vkCmdEndRenderPass(commandBuffers_[currentImage_]);
        renderPassActive_ = false;
    }

    if (texture->image == VK_NULL_HANDLE || texture->width != width || texture->height != height || texture->channels != 4) {
        destroyTextureResource(*texture);

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(device_, &imageInfo, nullptr, &texture->image) != VK_SUCCESS) {
            destroyTextureResource(*texture);
            return false;
        }

        VkMemoryRequirements memoryRequirements{};
        vkGetImageMemoryRequirements(device_, texture->image, &memoryRequirements);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (allocInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
            vkAllocateMemory(device_, &allocInfo, nullptr, &texture->memory) != VK_SUCCESS ||
            vkBindImageMemory(device_, texture->image, texture->memory, 0) != VK_SUCCESS) {
            destroyTextureResource(*texture);
            return false;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = texture->image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &viewInfo, nullptr, &texture->view) != VK_SUCCESS) {
            destroyTextureResource(*texture);
            return false;
        }

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = 1.0f;
        if (vkCreateSampler(device_, &samplerInfo, nullptr, &texture->sampler) != VK_SUCCESS) {
            destroyTextureResource(*texture);
            return false;
        }

        texture->width = width;
        texture->height = height;
        texture->channels = 4;
        texture->layout = VK_IMAGE_LAYOUT_UNDEFINED;
        texture->descriptorSet = VK_NULL_HANDLE;
    }

    const VkDeviceSize uploadSize = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4u;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = uploadSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(device_, stagingBuffer, &memoryRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (allocInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
        vkAllocateMemory(device_, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS ||
        vkBindBufferMemory(device_, stagingBuffer, stagingMemory, 0) != VK_SUCCESS) {
        if (stagingMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, stagingMemory, nullptr);
        }
        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        return false;
    }

    void* mapped = nullptr;
    if (vkMapMemory(device_, stagingMemory, 0, uploadSize, 0, &mapped) != VK_SUCCESS) {
        vkFreeMemory(device_, stagingMemory, nullptr);
        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        return false;
    }
    std::memcpy(mapped, pixels, static_cast<std::size_t>(uploadSize));
    vkUnmapMemory(device_, stagingMemory);

    VkCommandBuffer commandBuffer = commandBuffers_[currentImage_];
    transitionImageLayout(commandBuffer, texture->image, texture->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    texture->layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VkBufferImageCopy copyRegion{};
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    transitionImageLayout(commandBuffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    texture->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ++texture->generation;
    pendingUploadBuffers_.push_back(stagingBuffer);
    pendingUploadMemories_.push_back(stagingMemory);
    beginLoadPass();
    return true;
}

void VulkanRenderBackend::destroyTexture(TextureHandle handle) {
    auto* texture = static_cast<TextureResource*>(handle);
    if (texture == nullptr) {
        return;
    }
    if (!frameActive_ && device_ != VK_NULL_HANDLE && inFlight_ != VK_NULL_HANDLE &&
        vkGetFenceStatus(device_, inFlight_) == VK_SUCCESS) {
        destroyTextureResource(*texture);
        delete texture;
        return;
    }
    pendingTextureDeletes_.push_back(texture);
}

void VulkanRenderBackend::drawTexture(TextureHandle handle,
                                      const float* vertices,
                                      std::size_t vertexFloatCount,
                                      const core::Color& tint,
                                      const core::Rect& rect,
                                      float radius,
                                      int windowWidth,
                                      int windowHeight) {
    auto* texture = static_cast<TextureResource*>(handle);
    if (!frameActive_ || texture == nullptr || texture->view == VK_NULL_HANDLE || vertices == nullptr ||
        vertexFloatCount < 42 || tint.a <= 0.001f || windowWidth <= 0 || windowHeight <= 0) {
        return;
    }
    if (!ensureImagePipeline()) {
        return;
    }
    if (!frameRecorded_) {
        recordClearPass(clearColor_);
    }
    if (!renderPassActive_) {
        beginLoadPass();
    }
    if (!renderPassActive_ || !ensureImageDescriptor(*texture) || !ensureImageVertexBuffer()) {
        return;
    }

    if (imageVertexUsed_ + vertexFloatCount > imageVertexCapacity_) {
        return;
    }
    const std::size_t floatOffset = imageVertexUsed_;
    auto* mappedImageVertices = static_cast<float*>(imageVertexMapped_);
    std::memcpy(mappedImageVertices + floatOffset, vertices, vertexFloatCount * sizeof(float));
    imageVertexUsed_ += vertexFloatCount;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(windowWidth);
    viewport.height = static_cast<float>(windowHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffers_[currentImage_], 0, 1, &viewport);

    const core::Rect fullRect{0.0f, 0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight)};
    const VkRect2D scissor = clampScissor(scissorEnabled_ ? scissorRect_ : fullRect, windowWidth, windowHeight);
    if (scissor.extent.width == 0 || scissor.extent.height == 0) {
        return;
    }
    vkCmdSetScissor(commandBuffers_[currentImage_], 0, 1, &scissor);

    ImagePushConstants constants{};
    constants.windowSize[0] = static_cast<float>(windowWidth);
    constants.windowSize[1] = static_cast<float>(windowHeight);
    writeColor(constants.tint, tint);
    constants.rect[0] = rect.x;
    constants.rect[1] = rect.y;
    constants.rect[2] = rect.width;
    constants.rect[3] = rect.height;
    constants.flags[0] = radius;

    const VkDeviceSize offset = static_cast<VkDeviceSize>(floatOffset * sizeof(float));
    vkCmdBindPipeline(commandBuffers_[currentImage_], VK_PIPELINE_BIND_POINT_GRAPHICS, imagePipeline_);
    vkCmdBindDescriptorSets(commandBuffers_[currentImage_],
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            imagePipelineLayout_,
                            0,
                            1,
                            &texture->descriptorSet,
                            0,
                            nullptr);
    vkCmdBindVertexBuffers(commandBuffers_[currentImage_], 0, 1, &imageVertexBuffer_, &offset);
    vkCmdPushConstants(commandBuffers_[currentImage_],
                       imagePipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(constants),
                       &constants);
    vkCmdDraw(commandBuffers_[currentImage_], static_cast<std::uint32_t>(vertexFloatCount / 7), 1, 0, 0);
}

bool VulkanRenderBackend::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "EUI-NEO";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "EUI-NEO";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    std::vector<const char*> extensions;
#if defined(EUI_WINDOW_BACKEND_SDL2)
    unsigned int sdlExtensionCount = 0;
    if (SDL_Vulkan_GetInstanceExtensions(static_cast<SDL_Window*>(window_), &sdlExtensionCount, nullptr) != SDL_TRUE) {
        return false;
    }
    extensions.resize(sdlExtensionCount);
    if (SDL_Vulkan_GetInstanceExtensions(static_cast<SDL_Window*>(window_), &sdlExtensionCount, extensions.data()) != SDL_TRUE) {
        return false;
    }
#else
    std::uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (glfwExtensions == nullptr) {
        const char* description = nullptr;
        const int code = glfwGetError(&description);
        std::fprintf(stderr, "EUI Vulkan: glfwGetRequiredInstanceExtensions failed: %d %s\n",
                     code,
                     description != nullptr ? description : "");
        return false;
    }
    extensions.assign(glfwExtensions, glfwExtensions + glfwExtensionCount);
#endif

    VkInstanceCreateFlags flags = 0;
    const std::vector<VkExtensionProperties> availableExtensions = instanceExtensions();
    if (hasExtension(availableExtensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        addUniqueExtension(extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
    if (hasExtension(availableExtensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
        addUniqueExtension(extensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.flags = flags;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    const VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        std::fprintf(stderr, "EUI Vulkan: vkCreateInstance failed: %d\n", static_cast<int>(result));
        for (const char* extension : extensions) {
            std::fprintf(stderr, "EUI Vulkan: requested instance extension: %s\n", extension);
        }
        return false;
    }
    return true;
}

bool VulkanRenderBackend::createSurface() {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    return SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(window_), instance_, &surface_) == SDL_TRUE;
#else
    return glfwCreateWindowSurface(instance_, static_cast<GLFWwindow*>(window_), nullptr, &surface_) == VK_SUCCESS;
#endif
}

bool VulkanRenderBackend::pickDevice() {
    std::uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        return false;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    for (VkPhysicalDevice device : devices) {
        std::uint32_t queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> queues(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, queues.data());
        for (std::uint32_t i = 0; i < queueCount; ++i) {
            VkBool32 presentSupported = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupported);
            if ((queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupported) {
                physicalDevice_ = device;
                graphicsFamily_ = i;
                presentFamily_ = i;
                return true;
            }
        }
    }
    return false;
}

bool VulkanRenderBackend::createDevice() {
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = graphicsFamily_;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    std::vector<const char*> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const std::vector<VkExtensionProperties> availableExtensions = deviceExtensions(physicalDevice_);
    if (hasExtension(availableExtensions, "VK_KHR_portability_subset")) {
        extensions.push_back("VK_KHR_portability_subset");
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
        return false;
    }
    vkGetDeviceQueue(device_, graphicsFamily_, 0, &graphicsQueue_);
    presentQueue_ = graphicsQueue_;
    return true;
}

bool VulkanRenderBackend::recreateSwapchain(const RenderSurface& surface) {
    vkDeviceWaitIdle(device_);
    destroySwapchain();

    VkSurfaceCapabilitiesKHR capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &capabilities);

    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount > 0) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data());
    }

    std::uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount > 0) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, presentModes.data());
    }

    const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(formats);
    const VkPresentModeKHR presentMode = choosePresentMode(presentModes);
    VkExtent2D extent{};
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent.width = std::clamp(static_cast<std::uint32_t>(surface.framebufferWidth),
                                  capabilities.minImageExtent.width,
                                  capabilities.maxImageExtent.width);
        extent.height = std::clamp(static_cast<std::uint32_t>(surface.framebufferHeight),
                                   capabilities.minImageExtent.height,
                                   capabilities.maxImageExtent.height);
    }

    std::uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = surface_;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = surfaceFormat.format;
    swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainInfo.imageExtent = extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainTransferSrcSupported_ = (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
    swapchainTransferDstSupported_ = (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (swapchainTransferSrcSupported_) {
        swapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if (swapchainTransferDstSupported_) {
        swapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform = capabilities.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = presentMode;
    swapchainInfo.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(device_, &swapchainInfo, nullptr, &swapchain_) != VK_SUCCESS) {
        return false;
    }

    swapchainFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());
    swapchainImageLayouts_.assign(swapchainImages_.size(), VK_IMAGE_LAYOUT_UNDEFINED);

    swapchainImageViews_.resize(swapchainImages_.size());
    for (std::size_t i = 0; i < swapchainImages_.size(); ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainImages_[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainFormat_;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &viewInfo, nullptr, &swapchainImageViews_[i]) != VK_SUCCESS) {
            return false;
        }
    }

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_) != VK_SUCCESS) {
        return false;
    }
    if (!ensureRoundedRectPipeline()) {
        return false;
    }

    framebuffers_.resize(swapchainImageViews_.size());
    for (std::size_t i = 0; i < swapchainImageViews_.size(); ++i) {
        VkImageView attachments[] = {swapchainImageViews_[i]};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass_;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainExtent_.width;
        framebufferInfo.height = swapchainExtent_.height;
        framebufferInfo.layers = 1;
        if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            return false;
        }
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsFamily_;
    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
        return false;
    }

    commandBuffers_.resize(framebuffers_.size());
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<std::uint32_t>(commandBuffers_.size());
    if (vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()) != VK_SUCCESS) {
        return false;
    }

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    return vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailable_) == VK_SUCCESS &&
           vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinished_) == VK_SUCCESS &&
           vkCreateFence(device_, &fenceInfo, nullptr, &inFlight_) == VK_SUCCESS;
}

void VulkanRenderBackend::recordClearPass(const core::Color& color) {
    if (renderingToCache_) {
        transitionRenderCacheImage(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    } else {
        transitionSwapchainImage(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
    initializeBackdropImageIfNeeded();

    beginLoadPass();
    if (!renderPassActive_) {
        return;
    }

    VkClearValue clearValue{};
    clearValue.color = {{color.r, color.g, color.b, color.a}};

    VkClearAttachment clearAttachment{};
    clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearAttachment.colorAttachment = 0;
    clearAttachment.clearValue = clearValue;

    VkClearRect clearRect{};
    const VkExtent2D extent = currentRenderExtent();
    const core::Rect fullRect{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height)};
    clearRect.rect = clampScissor(scissorEnabled_ ? scissorRect_ : fullRect,
                                  static_cast<int>(extent.width),
                                  static_cast<int>(extent.height));
    clearRect.layerCount = 1;
    if (clearRect.rect.extent.width == 0 || clearRect.rect.extent.height == 0) {
        return;
    }
    vkCmdClearAttachments(commandBuffers_[currentImage_], 1, &clearAttachment, 1, &clearRect);
}

void VulkanRenderBackend::beginLoadPass() {
    if (!frameActive_ || renderPassActive_ || renderPass_ == VK_NULL_HANDLE || framebuffers_.empty()) {
        return;
    }

    if (renderingToCache_) {
        if (renderCacheFramebuffer_ == VK_NULL_HANDLE || renderCacheExtent_.width == 0 || renderCacheExtent_.height == 0) {
            return;
        }
        transitionRenderCacheImage(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    } else {
        transitionSwapchainImage(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = currentFramebuffer();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = currentRenderExtent();

    vkCmdBeginRenderPass(commandBuffers_[currentImage_], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    renderPassActive_ = true;
    frameRecorded_ = true;
}

VkExtent2D VulkanRenderBackend::currentRenderExtent() const {
    return renderingToCache_ ? renderCacheExtent_ : swapchainExtent_;
}

VkFramebuffer VulkanRenderBackend::currentFramebuffer() const {
    return renderingToCache_ ? renderCacheFramebuffer_ : framebuffers_[currentImage_];
}

void VulkanRenderBackend::transitionRenderCacheImage(VkImageLayout newLayout) {
    if (commandBuffers_.empty() || renderCacheImage_ == VK_NULL_HANDLE || currentImage_ >= commandBuffers_.size()) {
        return;
    }
    transitionImageLayout(commandBuffers_[currentImage_], renderCacheImage_, renderCacheLayout_, newLayout);
    renderCacheLayout_ = newLayout;
}

void VulkanRenderBackend::destroyRenderCacheResources() {
    if (device_ == VK_NULL_HANDLE) {
        renderCacheImage_ = VK_NULL_HANDLE;
        renderCacheMemory_ = VK_NULL_HANDLE;
        renderCacheView_ = VK_NULL_HANDLE;
        renderCacheFramebuffer_ = VK_NULL_HANDLE;
        renderCacheLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
        renderCacheExtent_ = {};
        renderingToCache_ = false;
        return;
    }
    if (renderCacheFramebuffer_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, renderCacheFramebuffer_, nullptr);
        renderCacheFramebuffer_ = VK_NULL_HANDLE;
    }
    if (renderCacheView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, renderCacheView_, nullptr);
        renderCacheView_ = VK_NULL_HANDLE;
    }
    if (renderCacheImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, renderCacheImage_, nullptr);
        renderCacheImage_ = VK_NULL_HANDLE;
    }
    if (renderCacheMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, renderCacheMemory_, nullptr);
        renderCacheMemory_ = VK_NULL_HANDLE;
    }
    renderCacheLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    renderCacheExtent_ = {};
    renderingToCache_ = false;
}

void VulkanRenderBackend::destroySwapchain() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    frameActive_ = false;
    frameRecorded_ = false;
    renderPassActive_ = false;
    destroyRoundedRectPipeline();
    destroyBackdropResources();
    destroyTextPipeline();
    destroyTextResources();
    destroyImagePipeline();
    destroyImageResources();
    destroyRenderCacheResources();
    releasePendingTextureDeletes();
    releasePendingUploads();
    if (inFlight_ != VK_NULL_HANDLE) {
        vkDestroyFence(device_, inFlight_, nullptr);
        inFlight_ = VK_NULL_HANDLE;
    }
    if (renderFinished_ != VK_NULL_HANDLE) {
        vkDestroySemaphore(device_, renderFinished_, nullptr);
        renderFinished_ = VK_NULL_HANDLE;
    }
    if (imageAvailable_ != VK_NULL_HANDLE) {
        vkDestroySemaphore(device_, imageAvailable_, nullptr);
        imageAvailable_ = VK_NULL_HANDLE;
    }
    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }
    for (VkFramebuffer framebuffer : framebuffers_) {
        vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    framebuffers_.clear();
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
    for (VkImageView view : swapchainImageViews_) {
        vkDestroyImageView(device_, view, nullptr);
    }
    swapchainImageViews_.clear();
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
    commandBuffers_.clear();
    swapchainImages_.clear();
    swapchainImageLayouts_.clear();
    swapchainTransferSrcSupported_ = false;
    swapchainTransferDstSupported_ = false;
    backdropReady_ = false;
    renderingToCache_ = false;
}

void VulkanRenderBackend::destroy() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
    destroySwapchain();
    destroyPrimitiveVertexBuffer();
    if (imageDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, imageDescriptorPool_, nullptr);
        imageDescriptorPool_ = VK_NULL_HANDLE;
    }
    if (imageDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, imageDescriptorSetLayout_, nullptr);
        imageDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
    releasePendingUploads();
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    initialized_ = false;
}

bool VulkanRenderBackend::ensureRoundedRectPipeline() {
    if (roundedRectPipeline_ != VK_NULL_HANDLE) {
        return true;
    }
    if (device_ == VK_NULL_HANDLE || renderPass_ == VK_NULL_HANDLE) {
        return false;
    }

    if (roundedRectDescriptorSetLayout_ == VK_NULL_HANDLE) {
        VkDescriptorSetLayoutBinding backdropBinding{};
        backdropBinding.binding = 0;
        backdropBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        backdropBinding.descriptorCount = 1;
        backdropBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
        descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorLayoutInfo.bindingCount = 1;
        descriptorLayoutInfo.pBindings = &backdropBinding;
        if (vkCreateDescriptorSetLayout(device_, &descriptorLayoutInfo, nullptr, &roundedRectDescriptorSetLayout_) != VK_SUCCESS) {
            return false;
        }
    }

    VkShaderModule vertexShader = createShaderModule(device_,
                                                     shaders::kRoundedRectVertexSpirv,
                                                     shaders::kRoundedRectVertexSpirvSize);
    VkShaderModule fragmentShader = createShaderModule(device_,
                                                       shaders::kRoundedRectFragmentSpirv,
                                                       shaders::kRoundedRectFragmentSpirvSize);
    if (vertexShader == VK_NULL_HANDLE || fragmentShader == VK_NULL_HANDLE) {
        if (vertexShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, vertexShader, nullptr);
        }
        if (fragmentShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, fragmentShader, nullptr);
        }
        return false;
    }

    VkPipelineShaderStageCreateInfo shaderStages[2]{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertexShader;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragmentShader;
    shaderStages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(PrimitiveGeometryVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attributes{};
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(PrimitiveGeometryVertex, screen);
    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[1].offset = offsetof(PrimitiveGeometryVertex, local);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                          VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT |
                                          VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(RoundedRectPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &roundedRectDescriptorSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &roundedRectPipelineLayout_) != VK_SUCCESS) {
        vkDestroyShaderModule(device_, fragmentShader, nullptr);
        vkDestroyShaderModule(device_, vertexShader, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = roundedRectPipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    const bool created = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &roundedRectPipeline_) == VK_SUCCESS;
    vkDestroyShaderModule(device_, fragmentShader, nullptr);
    vkDestroyShaderModule(device_, vertexShader, nullptr);
    if (!created) {
        destroyRoundedRectPipeline();
    }
    return created;
}

bool VulkanRenderBackend::ensureBackdropResources() {
    if (device_ == VK_NULL_HANDLE || swapchainExtent_.width == 0 || swapchainExtent_.height == 0 ||
        swapchainFormat_ == VK_FORMAT_UNDEFINED || roundedRectDescriptorSetLayout_ == VK_NULL_HANDLE) {
        return false;
    }

    const VkExtent2D targetExtent{swapchainExtent_.width, swapchainExtent_.height};
    if (backdropImage_ != VK_NULL_HANDLE &&
        backdropImageView_ != VK_NULL_HANDLE &&
        backdropSampler_ != VK_NULL_HANDLE &&
        backdropExtent_.width == targetExtent.width &&
        backdropExtent_.height == targetExtent.height) {
        return ensureBackdropDescriptor();
    }

    destroyBackdropResources();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {targetExtent.width, targetExtent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = swapchainFormat_;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(device_, &imageInfo, nullptr, &backdropImage_) != VK_SUCCESS) {
        destroyBackdropResources();
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(device_, backdropImage_, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (allocInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
        vkAllocateMemory(device_, &allocInfo, nullptr, &backdropImageMemory_) != VK_SUCCESS ||
        vkBindImageMemory(device_, backdropImage_, backdropImageMemory_, 0) != VK_SUCCESS) {
        destroyBackdropResources();
        return false;
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = backdropImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = swapchainFormat_;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device_, &viewInfo, nullptr, &backdropImageView_) != VK_SUCCESS) {
        destroyBackdropResources();
        return false;
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 1.0f;
    if (vkCreateSampler(device_, &samplerInfo, nullptr, &backdropSampler_) != VK_SUCCESS) {
        destroyBackdropResources();
        return false;
    }

    backdropExtent_ = targetExtent;
    backdropImageLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    return ensureBackdropDescriptor();
}

bool VulkanRenderBackend::ensureBackdropDescriptor() {
    if (device_ == VK_NULL_HANDLE || roundedRectDescriptorSetLayout_ == VK_NULL_HANDLE ||
        backdropImageView_ == VK_NULL_HANDLE || backdropSampler_ == VK_NULL_HANDLE) {
        return false;
    }
    if (roundedRectDescriptorSet_ != VK_NULL_HANDLE) {
        return true;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &roundedRectDescriptorPool_) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = roundedRectDescriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &roundedRectDescriptorSetLayout_;
    if (vkAllocateDescriptorSets(device_, &allocInfo, &roundedRectDescriptorSet_) != VK_SUCCESS) {
        destroyBackdropDescriptorPool();
        return false;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = backdropSampler_;
    imageInfo.imageView = backdropImageView_;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = roundedRectDescriptorSet_;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(device_, 1, &descriptorWrite, 0, nullptr);
    return true;
}

void VulkanRenderBackend::initializeBackdropImageIfNeeded() {
    if (backdropImage_ == VK_NULL_HANDLE || backdropImageLayout_ != VK_IMAGE_LAYOUT_UNDEFINED ||
        commandBuffers_.empty() || currentImage_ >= commandBuffers_.size()) {
        return;
    }

    VkCommandBuffer commandBuffer = commandBuffers_[currentImage_];
    transitionImageLayout(commandBuffer, backdropImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    backdropImageLayout_ = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VkClearColorValue clearColor{};
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.levelCount = 1;
    range.layerCount = 1;
    vkCmdClearColorImage(commandBuffer, backdropImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

    transitionImageLayout(commandBuffer, backdropImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    backdropImageLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

bool VulkanRenderBackend::ensureTextPipeline() {
    if (textPipeline_ != VK_NULL_HANDLE) {
        return true;
    }
    if (device_ == VK_NULL_HANDLE || renderPass_ == VK_NULL_HANDLE) {
        return false;
    }

    if (textDescriptorSetLayout_ == VK_NULL_HANDLE) {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
        for (std::uint32_t i = 0; i < bindings.size(); ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &textDescriptorSetLayout_) != VK_SUCCESS) {
            return false;
        }
    }

    VkShaderModule vertexShader = createShaderModule(device_, shaders::kTextVertexSpirv, shaders::kTextVertexSpirvSize);
    VkShaderModule fragmentShader = createShaderModule(device_, shaders::kTextFragmentSpirv, shaders::kTextFragmentSpirvSize);
    if (vertexShader == VK_NULL_HANDLE || fragmentShader == VK_NULL_HANDLE) {
        if (vertexShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, vertexShader, nullptr);
        }
        if (fragmentShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, fragmentShader, nullptr);
        }
        return false;
    }

    VkPipelineShaderStageCreateInfo shaderStages[2]{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertexShader;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragmentShader;
    shaderStages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(float) * 5;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attributes{};
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[0].offset = 0;
    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[1].offset = sizeof(float) * 2;
    attributes[2].binding = 0;
    attributes[2].location = 2;
    attributes[2].format = VK_FORMAT_R32_SFLOAT;
    attributes[2].offset = sizeof(float) * 4;

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                          VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT |
                                          VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(TextPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &textDescriptorSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &textPipelineLayout_) != VK_SUCCESS) {
        vkDestroyShaderModule(device_, fragmentShader, nullptr);
        vkDestroyShaderModule(device_, vertexShader, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = textPipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    const bool created = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &textPipeline_) == VK_SUCCESS;
    vkDestroyShaderModule(device_, fragmentShader, nullptr);
    vkDestroyShaderModule(device_, vertexShader, nullptr);
    if (!created) {
        destroyTextPipeline();
    }
    return created;
}

bool VulkanRenderBackend::textAtlasNeedsUpload(const TextAtlasPageData& page) const {
    if (page.pixels == nullptr || page.width <= 0 || page.height <= 0 || page.channels <= 0) {
        return false;
    }
    const TextureResource& texture = page.kind == TextAtlasPageKind::Color ? textColorAtlas_ : textGrayAtlas_;
    return texture.image == VK_NULL_HANDLE ||
           texture.width != page.width ||
           texture.height != page.height ||
           texture.channels != page.channels ||
           texture.generation != page.generation;
}

bool VulkanRenderBackend::ensureTextAtlas(const TextAtlasPageData& page) {
    if (page.pixels == nullptr || page.width <= 0 || page.height <= 0 || page.channels <= 0 ||
        commandBuffers_.empty() || currentImage_ >= commandBuffers_.size()) {
        return false;
    }
    TextureResource& texture = page.kind == TextAtlasPageKind::Color ? textColorAtlas_ : textGrayAtlas_;
    if (!textAtlasNeedsUpload(page)) {
        return true;
    }

    const VkFormat format = page.channels == 4 ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8_UNORM;
    if (texture.image == VK_NULL_HANDLE ||
        texture.width != page.width ||
        texture.height != page.height ||
        texture.channels != page.channels) {
        destroyTextureResource(texture);

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {static_cast<std::uint32_t>(page.width), static_cast<std::uint32_t>(page.height), 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(device_, &imageInfo, nullptr, &texture.image) != VK_SUCCESS) {
            destroyTextureResource(texture);
            return false;
        }

        VkMemoryRequirements memoryRequirements{};
        vkGetImageMemoryRequirements(device_, texture.image, &memoryRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (allocInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
            vkAllocateMemory(device_, &allocInfo, nullptr, &texture.memory) != VK_SUCCESS ||
            vkBindImageMemory(device_, texture.image, texture.memory, 0) != VK_SUCCESS) {
            destroyTextureResource(texture);
            return false;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = texture.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &viewInfo, nullptr, &texture.view) != VK_SUCCESS) {
            destroyTextureResource(texture);
            return false;
        }

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = 1.0f;
        if (vkCreateSampler(device_, &samplerInfo, nullptr, &texture.sampler) != VK_SUCCESS) {
            destroyTextureResource(texture);
            return false;
        }

        texture.width = page.width;
        texture.height = page.height;
        texture.channels = page.channels;
        texture.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        textDescriptorDirty_ = true;
    }

    const VkDeviceSize uploadSize = static_cast<VkDeviceSize>(page.width) *
                                    static_cast<VkDeviceSize>(page.height) *
                                    static_cast<VkDeviceSize>(page.channels);
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = uploadSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(device_, stagingBuffer, &memoryRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (allocInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
        vkAllocateMemory(device_, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS ||
        vkBindBufferMemory(device_, stagingBuffer, stagingMemory, 0) != VK_SUCCESS) {
        if (stagingMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, stagingMemory, nullptr);
        }
        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        return false;
    }

    void* mapped = nullptr;
    if (vkMapMemory(device_, stagingMemory, 0, uploadSize, 0, &mapped) != VK_SUCCESS) {
        vkFreeMemory(device_, stagingMemory, nullptr);
        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        return false;
    }
    std::memcpy(mapped, page.pixels, static_cast<std::size_t>(uploadSize));
    vkUnmapMemory(device_, stagingMemory);

    VkCommandBuffer commandBuffer = commandBuffers_[currentImage_];
    transitionImageLayout(commandBuffer, texture.image, texture.layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    texture.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VkBufferImageCopy copyRegion{};
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = {static_cast<std::uint32_t>(page.width), static_cast<std::uint32_t>(page.height), 1};
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    transitionImageLayout(commandBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    texture.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    texture.generation = page.generation;
    textDescriptorDirty_ = true;

    pendingUploadBuffers_.push_back(stagingBuffer);
    pendingUploadMemories_.push_back(stagingMemory);
    return true;
}

bool VulkanRenderBackend::ensureTextDescriptor() {
    if (device_ == VK_NULL_HANDLE ||
        textDescriptorSetLayout_ == VK_NULL_HANDLE ||
        textGrayAtlas_.view == VK_NULL_HANDLE ||
        textGrayAtlas_.sampler == VK_NULL_HANDLE) {
        return false;
    }
    if (textDescriptorPool_ == VK_NULL_HANDLE) {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 2;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &textDescriptorPool_) != VK_SUCCESS) {
            return false;
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = textDescriptorPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &textDescriptorSetLayout_;
        if (vkAllocateDescriptorSets(device_, &allocInfo, &textDescriptorSet_) != VK_SUCCESS) {
            vkDestroyDescriptorPool(device_, textDescriptorPool_, nullptr);
            textDescriptorPool_ = VK_NULL_HANDLE;
            textDescriptorSet_ = VK_NULL_HANDLE;
            return false;
        }
        textDescriptorDirty_ = true;
    }

    if (!textDescriptorDirty_) {
        return true;
    }

    const TextureResource& color = textColorAtlas_.view != VK_NULL_HANDLE ? textColorAtlas_ : textGrayAtlas_;
    std::array<VkDescriptorImageInfo, 2> images{};
    images[0].sampler = textGrayAtlas_.sampler;
    images[0].imageView = textGrayAtlas_.view;
    images[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    images[1].sampler = color.sampler;
    images[1].imageView = color.view;
    images[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array<VkWriteDescriptorSet, 2> writes{};
    for (std::uint32_t i = 0; i < writes.size(); ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = textDescriptorSet_;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo = &images[i];
    }
    vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    textDescriptorDirty_ = false;
    return true;
}

bool VulkanRenderBackend::ensureTextVertexBuffer(std::size_t floatCount) {
    if (floatCount == 0 || floatCount > kTextVertexFloatCapacity) {
        return false;
    }
    if (textVertexBuffer_ == VK_NULL_HANDLE) {
        const VkDeviceSize size = static_cast<VkDeviceSize>(kTextVertexFloatCapacity * sizeof(float));
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &bufferInfo, nullptr, &textVertexBuffer_) != VK_SUCCESS) {
            return false;
        }

        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(device_, textVertexBuffer_, &memoryRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (allocInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
            vkAllocateMemory(device_, &allocInfo, nullptr, &textVertexMemory_) != VK_SUCCESS ||
            vkBindBufferMemory(device_, textVertexBuffer_, textVertexMemory_, 0) != VK_SUCCESS ||
            vkMapMemory(device_, textVertexMemory_, 0, size, 0, &textVertexMapped_) != VK_SUCCESS) {
            destroyTextResources();
            return false;
        }
        textVertexCapacity_ = kTextVertexFloatCapacity;
    }
    return textVertexMapped_ != nullptr && textVertexUsed_ + floatCount <= textVertexCapacity_;
}

bool VulkanRenderBackend::ensureImagePipeline() {
    if (imagePipeline_ != VK_NULL_HANDLE) {
        return true;
    }
    if (device_ == VK_NULL_HANDLE || renderPass_ == VK_NULL_HANDLE) {
        return false;
    }
    if (imageDescriptorSetLayout_ == VK_NULL_HANDLE) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;
        if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &imageDescriptorSetLayout_) != VK_SUCCESS) {
            return false;
        }
    }

    VkShaderModule vertexShader = createShaderModule(device_, shaders::kImageVertexSpirv, shaders::kImageVertexSpirvSize);
    VkShaderModule fragmentShader = createShaderModule(device_, shaders::kImageFragmentSpirv, shaders::kImageFragmentSpirvSize);
    if (vertexShader == VK_NULL_HANDLE || fragmentShader == VK_NULL_HANDLE) {
        if (vertexShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, vertexShader, nullptr);
        }
        if (fragmentShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, fragmentShader, nullptr);
        }
        return false;
    }

    VkPipelineShaderStageCreateInfo shaderStages[2]{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertexShader;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragmentShader;
    shaderStages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(float) * 7;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attributes{};
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = 0;
    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[1].offset = sizeof(float) * 3;
    attributes[2].binding = 0;
    attributes[2].location = 2;
    attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[2].offset = sizeof(float) * 5;

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                          VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT |
                                          VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ImagePushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &imageDescriptorSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &imagePipelineLayout_) != VK_SUCCESS) {
        vkDestroyShaderModule(device_, fragmentShader, nullptr);
        vkDestroyShaderModule(device_, vertexShader, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = imagePipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    const bool created = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &imagePipeline_) == VK_SUCCESS;
    vkDestroyShaderModule(device_, fragmentShader, nullptr);
    vkDestroyShaderModule(device_, vertexShader, nullptr);
    if (!created) {
        destroyImagePipeline();
    }
    return created;
}

bool VulkanRenderBackend::ensureImageDescriptor(TextureResource& texture) {
    if (imageDescriptorSetLayout_ == VK_NULL_HANDLE || texture.view == VK_NULL_HANDLE || texture.sampler == VK_NULL_HANDLE) {
        return false;
    }
    if (imageDescriptorPool_ == VK_NULL_HANDLE) {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 256;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 256;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &imageDescriptorPool_) != VK_SUCCESS) {
            return false;
        }
    }
    if (texture.descriptorSet == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = imageDescriptorPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &imageDescriptorSetLayout_;
        if (vkAllocateDescriptorSets(device_, &allocInfo, &texture.descriptorSet) != VK_SUCCESS) {
            return false;
        }
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = texture.sampler;
    imageInfo.imageView = texture.view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = texture.descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    return true;
}

bool VulkanRenderBackend::ensureImageVertexBuffer() {
    if (imageVertexBuffer_ == VK_NULL_HANDLE) {
        const VkDeviceSize size = sizeof(float) * kImageVertexFloatCapacity;
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &bufferInfo, nullptr, &imageVertexBuffer_) != VK_SUCCESS) {
            return false;
        }

        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(device_, imageVertexBuffer_, &memoryRequirements);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (allocInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
            vkAllocateMemory(device_, &allocInfo, nullptr, &imageVertexMemory_) != VK_SUCCESS ||
            vkBindBufferMemory(device_, imageVertexBuffer_, imageVertexMemory_, 0) != VK_SUCCESS ||
            vkMapMemory(device_, imageVertexMemory_, 0, size, 0, &imageVertexMapped_) != VK_SUCCESS) {
            destroyImageResources();
            return false;
        }
        imageVertexCapacity_ = kImageVertexFloatCapacity;
    }
    return imageVertexMapped_ != nullptr && imageVertexCapacity_ >= 42;
}

bool VulkanRenderBackend::ensurePrimitiveVertexBuffer(std::size_t vertexCount) {
    if (vertexCount == 0 || vertexCount > kPrimitiveVertexCapacity) {
        return false;
    }
    if (primitiveVertexBuffer_ == VK_NULL_HANDLE) {
        const VkDeviceSize size = static_cast<VkDeviceSize>(kPrimitiveVertexCapacity * sizeof(PrimitiveGeometryVertex));
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &bufferInfo, nullptr, &primitiveVertexBuffer_) != VK_SUCCESS) {
            return false;
        }

        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(device_, primitiveVertexBuffer_, &memoryRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (allocInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
            vkAllocateMemory(device_, &allocInfo, nullptr, &primitiveVertexMemory_) != VK_SUCCESS ||
            vkBindBufferMemory(device_, primitiveVertexBuffer_, primitiveVertexMemory_, 0) != VK_SUCCESS ||
            vkMapMemory(device_, primitiveVertexMemory_, 0, size, 0, &primitiveVertexMapped_) != VK_SUCCESS) {
            destroyPrimitiveVertexBuffer();
            return false;
        }
        primitiveVertexCapacity_ = kPrimitiveVertexCapacity;
    }
    return primitiveVertexMapped_ != nullptr && primitiveVertexUsed_ + vertexCount <= primitiveVertexCapacity_;
}

void VulkanRenderBackend::destroyRoundedRectPipeline() {
    if (roundedRectPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, roundedRectPipeline_, nullptr);
        roundedRectPipeline_ = VK_NULL_HANDLE;
    }
    if (roundedRectPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, roundedRectPipelineLayout_, nullptr);
        roundedRectPipelineLayout_ = VK_NULL_HANDLE;
    }
    destroyBackdropDescriptorPool();
    if (roundedRectDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, roundedRectDescriptorSetLayout_, nullptr);
        roundedRectDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderBackend::destroyBackdropResources() {
    destroyBackdropDescriptorPool();
    if (device_ == VK_NULL_HANDLE) {
        backdropImage_ = VK_NULL_HANDLE;
        backdropImageMemory_ = VK_NULL_HANDLE;
        backdropImageView_ = VK_NULL_HANDLE;
        backdropSampler_ = VK_NULL_HANDLE;
        backdropImageLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
        backdropExtent_ = {};
        backdropReady_ = false;
        return;
    }
    if (backdropSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, backdropSampler_, nullptr);
        backdropSampler_ = VK_NULL_HANDLE;
    }
    if (backdropImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, backdropImageView_, nullptr);
        backdropImageView_ = VK_NULL_HANDLE;
    }
    if (backdropImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, backdropImage_, nullptr);
        backdropImage_ = VK_NULL_HANDLE;
    }
    if (backdropImageMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, backdropImageMemory_, nullptr);
        backdropImageMemory_ = VK_NULL_HANDLE;
    }
    backdropImageLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    backdropExtent_ = {};
    backdropReady_ = false;
}

void VulkanRenderBackend::destroyBackdropDescriptorPool() {
    if (device_ != VK_NULL_HANDLE && roundedRectDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, roundedRectDescriptorPool_, nullptr);
    }
    roundedRectDescriptorPool_ = VK_NULL_HANDLE;
    roundedRectDescriptorSet_ = VK_NULL_HANDLE;
}

void VulkanRenderBackend::destroyPrimitiveVertexBuffer() {
    if (device_ == VK_NULL_HANDLE) {
        primitiveVertexBuffer_ = VK_NULL_HANDLE;
        primitiveVertexMemory_ = VK_NULL_HANDLE;
        primitiveVertexMapped_ = nullptr;
        primitiveVertexCapacity_ = 0;
        primitiveVertexUsed_ = 0;
        return;
    }
    if (primitiveVertexMemory_ != VK_NULL_HANDLE && primitiveVertexMapped_ != nullptr) {
        vkUnmapMemory(device_, primitiveVertexMemory_);
        primitiveVertexMapped_ = nullptr;
    }
    if (primitiveVertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, primitiveVertexBuffer_, nullptr);
        primitiveVertexBuffer_ = VK_NULL_HANDLE;
    }
    if (primitiveVertexMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, primitiveVertexMemory_, nullptr);
        primitiveVertexMemory_ = VK_NULL_HANDLE;
    }
    primitiveVertexCapacity_ = 0;
    primitiveVertexUsed_ = 0;
}

void VulkanRenderBackend::destroyTextPipeline() {
    if (textPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, textPipeline_, nullptr);
        textPipeline_ = VK_NULL_HANDLE;
    }
    if (textPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, textPipelineLayout_, nullptr);
        textPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (textDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, textDescriptorPool_, nullptr);
        textDescriptorPool_ = VK_NULL_HANDLE;
        textDescriptorSet_ = VK_NULL_HANDLE;
    }
    if (textDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, textDescriptorSetLayout_, nullptr);
        textDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
    textDescriptorDirty_ = true;
}

void VulkanRenderBackend::destroyTextResources() {
    if (device_ == VK_NULL_HANDLE) {
        textVertexBuffer_ = VK_NULL_HANDLE;
        textVertexMemory_ = VK_NULL_HANDLE;
        textVertexMapped_ = nullptr;
        textVertexCapacity_ = 0;
        textVertexUsed_ = 0;
        textGrayAtlas_ = {};
        textColorAtlas_ = {};
        textDescriptorDirty_ = true;
        return;
    }
    if (textVertexMemory_ != VK_NULL_HANDLE && textVertexMapped_ != nullptr) {
        vkUnmapMemory(device_, textVertexMemory_);
        textVertexMapped_ = nullptr;
    }
    if (textVertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, textVertexBuffer_, nullptr);
        textVertexBuffer_ = VK_NULL_HANDLE;
    }
    if (textVertexMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, textVertexMemory_, nullptr);
        textVertexMemory_ = VK_NULL_HANDLE;
    }
    textVertexCapacity_ = 0;
    textVertexUsed_ = 0;
    destroyTextureResource(textGrayAtlas_);
    destroyTextureResource(textColorAtlas_);
    textDescriptorDirty_ = true;
}

void VulkanRenderBackend::destroyImagePipeline() {
    if (imagePipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, imagePipeline_, nullptr);
        imagePipeline_ = VK_NULL_HANDLE;
    }
    if (imagePipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, imagePipelineLayout_, nullptr);
        imagePipelineLayout_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderBackend::destroyImageResources() {
    if (device_ == VK_NULL_HANDLE) {
        imageVertexBuffer_ = VK_NULL_HANDLE;
        imageVertexMemory_ = VK_NULL_HANDLE;
        imageVertexMapped_ = nullptr;
        imageVertexCapacity_ = 0;
        imageVertexUsed_ = 0;
        return;
    }
    if (imageVertexMemory_ != VK_NULL_HANDLE && imageVertexMapped_ != nullptr) {
        vkUnmapMemory(device_, imageVertexMemory_);
        imageVertexMapped_ = nullptr;
    }
    if (imageVertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, imageVertexBuffer_, nullptr);
        imageVertexBuffer_ = VK_NULL_HANDLE;
    }
    if (imageVertexMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, imageVertexMemory_, nullptr);
        imageVertexMemory_ = VK_NULL_HANDLE;
    }
    imageVertexCapacity_ = 0;
    imageVertexUsed_ = 0;
}

void VulkanRenderBackend::destroyTextureResource(TextureResource& texture) {
    if (device_ == VK_NULL_HANDLE) {
        texture = {};
        return;
    }
    if (imageDescriptorPool_ != VK_NULL_HANDLE && texture.descriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device_, imageDescriptorPool_, 1, &texture.descriptorSet);
        texture.descriptorSet = VK_NULL_HANDLE;
    }
    if (texture.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device_, texture.sampler, nullptr);
        texture.sampler = VK_NULL_HANDLE;
    }
    if (texture.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, texture.view, nullptr);
        texture.view = VK_NULL_HANDLE;
    }
    if (texture.image != VK_NULL_HANDLE) {
        vkDestroyImage(device_, texture.image, nullptr);
        texture.image = VK_NULL_HANDLE;
    }
    if (texture.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, texture.memory, nullptr);
        texture.memory = VK_NULL_HANDLE;
    }
    texture = {};
}

void VulkanRenderBackend::releasePendingTextureDeletes() {
    if (device_ == VK_NULL_HANDLE) {
        for (TextureResource* texture : pendingTextureDeletes_) {
            delete texture;
        }
        pendingTextureDeletes_.clear();
        return;
    }
    for (TextureResource* texture : pendingTextureDeletes_) {
        if (texture != nullptr) {
            destroyTextureResource(*texture);
            delete texture;
        }
    }
    pendingTextureDeletes_.clear();
}

void VulkanRenderBackend::releasePendingUploads() {
    if (device_ == VK_NULL_HANDLE) {
        pendingUploadBuffers_.clear();
        pendingUploadMemories_.clear();
        return;
    }
    for (VkBuffer buffer : pendingUploadBuffers_) {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, buffer, nullptr);
        }
    }
    for (VkDeviceMemory memory : pendingUploadMemories_) {
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, memory, nullptr);
        }
    }
    pendingUploadBuffers_.clear();
    pendingUploadMemories_.clear();
}

void VulkanRenderBackend::transitionSwapchainImage(VkImageLayout newLayout) {
    if (commandBuffers_.empty() || swapchainImages_.empty() || swapchainImageLayouts_.empty() ||
        currentImage_ >= swapchainImages_.size()) {
        return;
    }
    const VkImageLayout oldLayout = swapchainImageLayouts_[currentImage_];
    transitionImageLayout(commandBuffers_[currentImage_], swapchainImages_[currentImage_], oldLayout, newLayout);
    swapchainImageLayouts_[currentImage_] = newLayout;
}

std::uint32_t VulkanRenderBackend::findMemoryType(std::uint32_t filter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties);
    for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if ((filter & (1u << i)) &&
            (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return std::numeric_limits<std::uint32_t>::max();
}

} // namespace core::render::vulkan
