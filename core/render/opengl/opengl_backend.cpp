#include "core/render/opengl/opengl_backend.h"

#include "core/render/render_types.h"

#include <algorithm>
#include <cmath>
#include <glad/glad.h>

#include <utility>

namespace core::render::opengl {

OpenGLRenderBackend::OpenGLRenderBackend(Callback makeCurrent, Callback present)
    : makeCurrent_(std::move(makeCurrent)), present_(std::move(present)) {}

void OpenGLRenderBackend::makeCurrent() {
    if (makeCurrent_) {
        makeCurrent_();
    }
}

void OpenGLRenderBackend::beginFrame(int framebufferWidth, int framebufferHeight, float) {
    makeCurrent();
    glViewport(0, 0, framebufferWidth, framebufferHeight);
}

void OpenGLRenderBackend::present() {
    if (present_) {
        present_();
    }
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
