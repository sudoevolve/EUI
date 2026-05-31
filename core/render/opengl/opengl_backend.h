#pragma once

#include "core/render/render_backend.h"
#include "core/window/window_types.h"

namespace core::render::opengl {

class OpenGLRenderBackend final : public RenderBackend {
public:
    explicit OpenGLRenderBackend(core::window::Handle window, RenderBackend* shareContext = nullptr);
    ~OpenGLRenderBackend() override;

    OpenGLRenderBackend(const OpenGLRenderBackend&) = delete;
    OpenGLRenderBackend& operator=(const OpenGLRenderBackend&) = delete;

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

private:
    void releasePrimitiveResources();

    core::window::Handle window_ = nullptr;
    RenderBackend* shareContext_ = nullptr;
    void* context_ = nullptr;
    bool initialized_ = false;
    unsigned int cacheFramebuffer_ = 0;
    unsigned int cacheTexture_ = 0;
    int cacheWidth_ = 0;
    int cacheHeight_ = 0;
    bool cacheRecreated_ = false;
};

} // namespace core::render::opengl
