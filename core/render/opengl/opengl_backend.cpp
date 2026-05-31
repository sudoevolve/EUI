#include "core/render/opengl/opengl_backend.h"

#include "core/render/render_types.h"

#include <algorithm>
#include <cmath>
#include <glad/glad.h>

#if defined(EUI_WINDOW_BACKEND_SDL2)
#include <SDL.h>
#else
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#endif

namespace core::render::opengl {

namespace {

bool& gladLoaded() {
    static bool loaded = false;
    return loaded;
}

#if defined(EUI_WINDOW_BACKEND_SDL2)
bool loadOpenGLFunctions() {
    if (gladLoaded()) {
        return true;
    }
    gladLoaded() = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)) != 0;
    return gladLoaded();
}
#else
bool loadOpenGLFunctions() {
    if (gladLoaded()) {
        return true;
    }
    gladLoaded() = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0;
    return gladLoaded();
}
#endif

} // namespace

OpenGLRenderBackend::OpenGLRenderBackend(core::window::Handle window, RenderBackend* shareContext)
    : window_(window), shareContext_(shareContext) {}

OpenGLRenderBackend::~OpenGLRenderBackend() {
    if (!initialized_) {
        return;
    }
    makeCurrent();
    releaseRenderCache();
    releasePrimitiveResources();
#if defined(EUI_WINDOW_BACKEND_SDL2)
    if (context_ != nullptr) {
        SDL_GL_DeleteContext(context_);
        context_ = nullptr;
    }
#endif
    initialized_ = false;
}

bool OpenGLRenderBackend::initialize() {
    if (window_ == nullptr) {
        return false;
    }

#if defined(EUI_WINDOW_BACKEND_SDL2)
    if (shareContext_ != nullptr) {
        shareContext_->makeCurrent();
    }
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, shareContext_ != nullptr ? 1 : 0);
    context_ = SDL_GL_CreateContext(static_cast<SDL_Window*>(window_));
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
    if (context_ == nullptr) {
        return false;
    }
    makeCurrent();
    SDL_GL_SetSwapInterval(0);
#else
    context_ = window_;
    makeCurrent();
    glfwSwapInterval(0);
#endif

    if (!loadOpenGLFunctions()) {
#if defined(EUI_WINDOW_BACKEND_SDL2)
        SDL_GL_DeleteContext(context_);
        context_ = nullptr;
#endif
        return false;
    }

    initialized_ = true;
    return true;
}

bool OpenGLRenderBackend::valid() const {
    return initialized_ && window_ != nullptr;
}

void OpenGLRenderBackend::makeCurrent() {
    if (window_ == nullptr) {
        return;
    }
#if defined(EUI_WINDOW_BACKEND_SDL2)
    if (context_ != nullptr) {
        SDL_GL_MakeCurrent(static_cast<SDL_Window*>(window_), context_);
    }
#else
    glfwMakeContextCurrent(static_cast<GLFWwindow*>(window_));
#endif
}

void OpenGLRenderBackend::beginFrame(const RenderSurface& surface) {
    makeCurrent();
    glViewport(0, 0, surface.framebufferWidth, surface.framebufferHeight);
}

void OpenGLRenderBackend::present() {
    if (window_ == nullptr) {
        return;
    }
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_GL_SwapWindow(static_cast<SDL_Window*>(window_));
#else
    glfwSwapBuffers(static_cast<GLFWwindow*>(window_));
#endif
}

bool OpenGLRenderBackend::ensureRenderCache(int width, int height) {
    cacheRecreated_ = false;
    width = std::max(1, width);
    height = std::max(1, height);
    if (cacheFramebuffer_ != 0 && cacheTexture_ != 0 && cacheWidth_ == width && cacheHeight_ == height) {
        return true;
    }

    releaseRenderCache();

    glGenFramebuffers(1, &cacheFramebuffer_);
    glGenTextures(1, &cacheTexture_);
    glBindTexture(GL_TEXTURE_2D, cacheTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, cacheFramebuffer_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cacheTexture_, 0);
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!complete) {
        releaseRenderCache();
        return false;
    }

    cacheWidth_ = width;
    cacheHeight_ = height;
    cacheRecreated_ = true;
    return true;
}

bool OpenGLRenderBackend::renderCacheWasRecreated() const {
    return cacheRecreated_;
}

void OpenGLRenderBackend::releaseRenderCache() {
    if (cacheTexture_ != 0) {
        glDeleteTextures(1, &cacheTexture_);
        cacheTexture_ = 0;
    }
    if (cacheFramebuffer_ != 0) {
        glDeleteFramebuffers(1, &cacheFramebuffer_);
        cacheFramebuffer_ = 0;
    }
    cacheWidth_ = 0;
    cacheHeight_ = 0;
}

void OpenGLRenderBackend::beginRenderCacheFrame(int width, int height) {
    glBindFramebuffer(GL_FRAMEBUFFER, cacheFramebuffer_);
    glViewport(0, 0, width, height);
}

void OpenGLRenderBackend::endRenderCacheFrame() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLRenderBackend::blitRenderCache(int width, int height) {
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, cacheFramebuffer_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, width, height,
                      0, 0, width, height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLRenderBackend::clear(const core::Color& color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLRenderBackend::setScissor(bool enabled, const core::Rect& rect, int framebufferHeight) {
    if (!enabled) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }

    const GLint x = static_cast<GLint>(std::floor(rect.x));
    const GLint y = static_cast<GLint>(std::floor(static_cast<float>(framebufferHeight) - rect.y - rect.height));
    const GLsizei width = static_cast<GLsizei>(std::ceil(rect.width));
    const GLsizei height = static_cast<GLsizei>(std::ceil(rect.height));
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, std::max<GLint>(0, y), std::max<GLsizei>(1, width), std::max<GLsizei>(1, height));
}

} // namespace core::render::opengl
