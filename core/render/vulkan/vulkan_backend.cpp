#include "core/render/vulkan/vulkan_backend.h"

#include "core/render/render_types.h"

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
#include <cmath>
#include <cstring>
#include <set>

namespace core::render::vulkan {

namespace {

bool hasExtension(const std::vector<VkExtensionProperties>& extensions, const char* name) {
    return std::any_of(extensions.begin(), extensions.end(), [&](const VkExtensionProperties& extension) {
        return std::strcmp(extension.extensionName, name) == 0;
    });
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

} // namespace

VulkanRenderBackend::VulkanRenderBackend(core::window::Handle window, RenderBackend*) : window_(window) {}

VulkanRenderBackend::~VulkanRenderBackend() {
    destroy();
}

bool VulkanRenderBackend::initialize() {
    if (window_ == nullptr) {
        return false;
    }
    initialized_ = createInstance() && createSurface() && pickDevice() && createDevice();
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
    vkEndCommandBuffer(commandBuffers_[currentImage_]);

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
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
}

bool VulkanRenderBackend::ensureRenderCache(int, int) {
    return false;
}

bool VulkanRenderBackend::renderCacheWasRecreated() const {
    return false;
}

void VulkanRenderBackend::releaseRenderCache() {}
void VulkanRenderBackend::beginRenderCacheFrame(int, int) {}
void VulkanRenderBackend::endRenderCacheFrame() {}
void VulkanRenderBackend::blitRenderCache(int, int) {}

void VulkanRenderBackend::clear(const core::Color& color) {
    clearColor_ = color;
    if (frameActive_ && !frameRecorded_) {
        recordClearPass(color);
    }
}

void VulkanRenderBackend::setScissor(bool enabled, const core::Rect& rect, int) {
    scissorEnabled_ = enabled;
    scissorRect_ = rect;
}

void VulkanRenderBackend::fillRect(const core::Rect& rect, const core::Color& color, int windowWidth, int windowHeight) {
    if (!frameActive_ || rect.width <= 0.0f || rect.height <= 0.0f || color.a <= 0.0f) {
        return;
    }
    if (!frameRecorded_) {
        recordClearPass(clearColor_);
    }
    if (!renderPassActive_) {
        return;
    }

    core::Rect clipped = rect;
    const core::Rect framebufferRect{0.0f, 0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight)};
    auto intersect = [](const core::Rect& a, const core::Rect& b, core::Rect& out) {
        const float left = std::max(a.x, b.x);
        const float top = std::max(a.y, b.y);
        const float right = std::min(a.x + a.width, b.x + b.width);
        const float bottom = std::min(a.y + a.height, b.y + b.height);
        if (right <= left || bottom <= top) {
            return false;
        }
        out = {left, top, right - left, bottom - top};
        return true;
    };
    if (!intersect(clipped, framebufferRect, clipped)) {
        return;
    }
    if (scissorEnabled_ && !intersect(clipped, scissorRect_, clipped)) {
        return;
    }

    const int32_t x = static_cast<int32_t>(std::floor(clipped.x));
    const int32_t y = static_cast<int32_t>(std::floor(clipped.y));
    const int32_t right = static_cast<int32_t>(std::ceil(clipped.x + clipped.width));
    const int32_t bottom = static_cast<int32_t>(std::ceil(clipped.y + clipped.height));
    if (right <= x || bottom <= y) {
        return;
    }

    VkClearAttachment attachment{};
    attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    attachment.colorAttachment = 0;
    attachment.clearValue.color = {{color.r, color.g, color.b, color.a}};

    VkClearRect clearRect{};
    clearRect.rect.offset = {x, y};
    clearRect.rect.extent = {
        static_cast<std::uint32_t>(right - x),
        static_cast<std::uint32_t>(bottom - y)
    };
    clearRect.baseArrayLayer = 0;
    clearRect.layerCount = 1;
    vkCmdClearAttachments(commandBuffers_[currentImage_], 1, &attachment, 1, &clearRect);
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
        return false;
    }
    extensions.assign(glfwExtensions, glfwExtensions + glfwExtensionCount);
#endif

    VkInstanceCreateFlags flags = 0;
    const std::vector<VkExtensionProperties> availableExtensions = instanceExtensions();
    if (hasExtension(availableExtensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
    if (hasExtension(availableExtensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.flags = flags;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    return vkCreateInstance(&createInfo, nullptr, &instance_) == VK_SUCCESS;
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
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
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
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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
    VkClearValue clearValue{};
    clearValue.color = {{color.r, color.g, color.b, color.a}};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = framebuffers_[currentImage_];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchainExtent_;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffers_[currentImage_], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    renderPassActive_ = true;
    frameRecorded_ = true;
}

void VulkanRenderBackend::destroySwapchain() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    frameActive_ = false;
    frameRecorded_ = false;
    renderPassActive_ = false;
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
}

void VulkanRenderBackend::destroy() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
    destroySwapchain();
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

} // namespace core::render::vulkan
