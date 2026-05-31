#pragma once

#include "core/render/render_types.h"

#include <glad/glad.h>
#include "core/platform/window_backend.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace core {

class RoundedRectPrimitive {
public:
    RoundedRectPrimitive() = default;

    RoundedRectPrimitive(float x, float y, float width, float height)
        : bounds_{x, y, width, height} {}

    bool initialize() {
        const char* vertexSource =
            "#version 330 core\n"
            "layout(location = 0) in vec3 aScreenPos;\n"
            "layout(location = 1) in vec2 aLocalPos;\n"
            "uniform vec2 uWindowSize;\n"
            "out vec2 vLocalPos;\n"
            "void main() {\n"
            "    vLocalPos = aLocalPos;\n"
            "    vec2 ndc = vec2((aScreenPos.x / uWindowSize.x) * 2.0 - 1.0,\n"
            "                    1.0 - (aScreenPos.y / uWindowSize.y) * 2.0);\n"
            "    gl_Position = vec4(ndc * aScreenPos.z, 0.0, aScreenPos.z);\n"
            "}\n";

        const char* fragmentSource =
            "#version 330 core\n"
            "in vec2 vLocalPos;\n"
            "out vec4 FragColor;\n"
            "uniform vec4 uFillColor;\n"
            "uniform vec4 uGradientStart;\n"
            "uniform vec4 uGradientEnd;\n"
            "uniform vec4 uBorderColor;\n"
            "uniform vec4 uShadowColor;\n"
            "uniform vec2 uWindowSize;\n"
            "uniform vec4 uRect;\n"
            "uniform float uRadius;\n"
            "uniform float uBorderWidth;\n"
            "uniform float uOpacity;\n"
            "uniform float uShadowBlur;\n"
            "uniform float uBlurAmount;\n"
            "uniform vec4 uBackdropRect;\n"
            "uniform int uUseGradient;\n"
            "uniform int uGradientDirection;\n"
            "uniform int uShadowPass;\n"
            "uniform sampler2D uBackdrop;\n"
            "float rand(vec2 co) {\n"
            "    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);\n"
            "}\n"
            "float roundedBoxDistance(vec2 point, vec2 halfSize, float radius) {\n"
            "    vec2 cornerVector = abs(point) - halfSize + vec2(radius);\n"
            "    return length(max(cornerVector, 0.0)) + min(max(cornerVector.x, cornerVector.y), 0.0) - radius;\n"
            "}\n"
            "vec3 backdropBlur(vec2 uv) {\n"
            "    vec2 pixelStep = 1.0 / max(uBackdropRect.zw, vec2(1.0));\n"
            "    float blurRadiusPx = uBlurAmount;\n"
            "    vec3 blurred = texture(uBackdrop, uv).rgb;\n"
            "    float repeats = mix(8.0, 24.0, clamp(blurRadiusPx / 36.0, 0.0, 1.0));\n"
            "    const float tau = 6.28318530718;\n"
            "    for (float i = 0.0; i < 24.0; i += 1.0) {\n"
            "        if (i >= repeats) break;\n"
            "        float angle = (i / repeats) * tau;\n"
            "        vec2 dir = vec2(cos(angle), sin(angle));\n"
            "        float radiusA = blurRadiusPx * (0.35 + 0.65 * rand(vec2(i, uv.x + uv.y)));\n"
            "        vec2 uvA = clamp(uv + dir * radiusA * pixelStep, pixelStep * 0.5, vec2(1.0) - pixelStep * 0.5);\n"
            "        blurred += texture(uBackdrop, uvA).rgb;\n"
            "        float angleB = angle + (0.5 * tau / repeats);\n"
            "        vec2 dirB = vec2(cos(angleB), sin(angleB));\n"
            "        float radiusB = blurRadiusPx * (0.20 + 0.80 * rand(vec2(i + 2.0, uv.x + uv.y + 24.0)));\n"
            "        vec2 uvB = clamp(uv + dirB * radiusB * pixelStep, pixelStep * 0.5, vec2(1.0) - pixelStep * 0.5);\n"
            "        blurred += texture(uBackdrop, uvB).rgb;\n"
            "    }\n"
            "    return blurred / (repeats * 2.0 + 1.0);\n"
            "}\n"
            "void main() {\n"
            "    vec2 center = uRect.xy + uRect.zw * 0.5;\n"
            "    float distanceToEdge = roundedBoxDistance(vLocalPos - center, uRect.zw * 0.5, uRadius);\n"
            "    float blur = max(uShadowBlur, 1.0);\n"
            "    if (uShadowPass == 1) {\n"
            "        float shadowAlpha = 1.0 - smoothstep(-blur, blur, distanceToEdge);\n"
            "        if (shadowAlpha <= 0.0) discard;\n"
            "        FragColor = vec4(uShadowColor.rgb, uShadowColor.a * shadowAlpha * uOpacity);\n"
            "        return;\n"
            "    }\n"
            "    float edgeWidth = max(fwidth(distanceToEdge), 0.75);\n"
            "    float shapeAlpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, distanceToEdge);\n"
            "    if (shapeAlpha <= 0.0) discard;\n"
            "    float gradientAmount = uGradientDirection == 0 ?\n"
            "        clamp((vLocalPos.x - uRect.x) / max(uRect.z, 1.0), 0.0, 1.0) :\n"
            "        clamp((vLocalPos.y - uRect.y) / max(uRect.w, 1.0), 0.0, 1.0);\n"
            "    vec4 fill = uUseGradient == 1 ? mix(uGradientStart, uGradientEnd, gradientAmount) : uFillColor;\n"
            "    if (uBlurAmount > 0.0) {\n"
            "        vec2 backdropUv = (gl_FragCoord.xy - uBackdropRect.xy) / max(uBackdropRect.zw, vec2(1.0));\n"
            "        backdropUv = clamp(backdropUv, vec2(0.0), vec2(1.0));\n"
            "        vec3 blurred = backdropBlur(backdropUv);\n"
            "        fill = vec4(mix(blurred, fill.rgb, fill.a), 1.0);\n"
            "    }\n"
            "    float borderAlpha = uBorderWidth > 0.0 ? smoothstep(-uBorderWidth - edgeWidth, -uBorderWidth + edgeWidth, distanceToEdge) : 0.0;\n"
            "    vec4 color = mix(fill, uBorderColor, borderAlpha);\n"
            "    FragColor = vec4(color.rgb, color.a * shapeAlpha * uOpacity);\n"
            "}\n";

        if (!retainSharedResources(vertexSource, fragmentSource)) {
            return false;
        }

        const SharedResources& resources = sharedResources();
        vao_ = resources.vao;
        vbo_ = resources.vbo;
        shaderProgram_ = resources.shaderProgram;
        windowSizeLocation_ = resources.windowSizeLocation;
        fillColorLocation_ = resources.fillColorLocation;
        gradientStartLocation_ = resources.gradientStartLocation;
        gradientEndLocation_ = resources.gradientEndLocation;
        borderColorLocation_ = resources.borderColorLocation;
        shadowColorLocation_ = resources.shadowColorLocation;
        rectLocation_ = resources.rectLocation;
        radiusLocation_ = resources.radiusLocation;
        borderWidthLocation_ = resources.borderWidthLocation;
        opacityLocation_ = resources.opacityLocation;
        shadowBlurLocation_ = resources.shadowBlurLocation;
        blurAmountLocation_ = resources.blurAmountLocation;
        backdropRectLocation_ = resources.backdropRectLocation;
        useGradientLocation_ = resources.useGradientLocation;
        gradientDirectionLocation_ = resources.gradientDirectionLocation;
        shadowPassLocation_ = resources.shadowPassLocation;
        backdropLocation_ = resources.backdropLocation;
        return true;
    }

    void destroy() {
        if (shaderProgram_) {
            releaseSharedResources();
        }
        vbo_ = 0;
        vao_ = 0;
        shaderProgram_ = 0;
        windowSizeLocation_ = -1;
        fillColorLocation_ = -1;
        gradientStartLocation_ = -1;
        gradientEndLocation_ = -1;
        borderColorLocation_ = -1;
        shadowColorLocation_ = -1;
        rectLocation_ = -1;
        radiusLocation_ = -1;
        borderWidthLocation_ = -1;
        opacityLocation_ = -1;
        shadowBlurLocation_ = -1;
        blurAmountLocation_ = -1;
        backdropRectLocation_ = -1;
        useGradientLocation_ = -1;
        gradientDirectionLocation_ = -1;
        shadowPassLocation_ = -1;
        backdropLocation_ = -1;
    }

    void setBounds(float x, float y, float width, float height) { bounds_ = {x, y, width, height}; }
    void setColor(const Color& color) { color_ = color; }
    void setGradient(const Gradient& gradient) { gradient_ = gradient; }
    void setCornerRadius(float radius) { cornerRadius_ = radius; }
    void setOpacity(float opacity) { opacity_ = std::clamp(opacity, 0.0f, 1.0f); }
    void setBorder(const Border& border) { border_ = border; }
    void setShadow(const Shadow& shadow) { shadow_ = shadow; }
    void setBlur(float blur) { blur_ = std::max(0.0f, blur); }
    void setTranslate(float x, float y) {
        transform_.translate = {x, y};
        hasTransformMatrix_ = false;
    }
    void setScale(float x, float y) {
        transform_.scale = {x, y};
        hasTransformMatrix_ = false;
    }
    void setRotate(float radians) {
        transform_.rotate = radians;
        hasTransformMatrix_ = false;
    }
    void setTransformOrigin(float x, float y) {
        transform_.origin = {x, y};
        hasTransformMatrix_ = false;
    }
    void setTransform(const Transform& transform) {
        transform_ = transform;
        hasTransformMatrix_ = false;
    }
    void setTransformMatrix(const TransformMatrix& matrix) {
        transformMatrix_ = matrix;
        hasTransformMatrix_ = true;
    }

    const Rect& bounds() const { return bounds_; }
    const Color& color() const { return color_; }
    const Gradient& gradient() const { return gradient_; }
    const Border& border() const { return border_; }
    const Shadow& shadow() const { return shadow_; }
    float blur() const { return blur_; }
    const Transform& transform() const { return transform_; }
    float cornerRadius() const { return cornerRadius_; }
    float opacity() const { return opacity_; }

    void render(int windowWidth, int windowHeight) const {
        if (!shaderProgram_ || !vao_ || !vbo_) {
            return;
        }

        const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (blur_ > 0.0f) {
            captureBackdrop(windowWidth, windowHeight, bounds_, blur_);
        }

        if (shadow_.enabled) {
            drawShadow(windowWidth, windowHeight);
        }

        drawLayer(windowWidth, windowHeight, bounds_, bounds_, false, color_, shadow_.blur);

        if (!blendEnabled) {
            glDisable(GL_BLEND);
        }
    }

private:
    struct SharedResources {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint shaderProgram = 0;
        GLint windowSizeLocation = -1;
        GLint fillColorLocation = -1;
        GLint gradientStartLocation = -1;
        GLint gradientEndLocation = -1;
        GLint borderColorLocation = -1;
        GLint shadowColorLocation = -1;
        GLint rectLocation = -1;
        GLint radiusLocation = -1;
        GLint borderWidthLocation = -1;
        GLint opacityLocation = -1;
        GLint shadowBlurLocation = -1;
        GLint blurAmountLocation = -1;
        GLint backdropRectLocation = -1;
        GLint useGradientLocation = -1;
        GLint gradientDirectionLocation = -1;
        GLint shadowPassLocation = -1;
        GLint backdropLocation = -1;
        GLuint backdropTexture = 0;
        GLuint backdropFramebuffer = 0;
        int backdropX = 0;
        int backdropY = 0;
        int backdropWidth = 0;
        int backdropHeight = 0;
        int backdropTextureWidth = 0;
        int backdropTextureHeight = 0;
        int references = 0;
    };

    static SharedResources& sharedResources() {
        static std::unordered_map<window::ContextKey, SharedResources> resourcesByContext;
        return resourcesByContext[window::currentContextKey()];
    }

    static bool retainSharedResources(const char* vertexSource, const char* fragmentSource) {
        SharedResources& resources = sharedResources();
        ++resources.references;
        if (resources.shaderProgram != 0) {
            return true;
        }

        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
        if (!vertexShader || !fragmentShader) {
            if (vertexShader) {
                glDeleteShader(vertexShader);
            }
            if (fragmentShader) {
                glDeleteShader(fragmentShader);
            }
            resources.references = std::max(0, resources.references - 1);
            return false;
        }

        resources.shaderProgram = glCreateProgram();
        glAttachShader(resources.shaderProgram, vertexShader);
        glAttachShader(resources.shaderProgram, fragmentShader);
        glLinkProgram(resources.shaderProgram);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        GLint linked = 0;
        glGetProgramiv(resources.shaderProgram, GL_LINK_STATUS, &linked);
        if (!linked) {
            glDeleteProgram(resources.shaderProgram);
            resources.shaderProgram = 0;
            resources.references = std::max(0, resources.references - 1);
            return false;
        }

        resources.windowSizeLocation = glGetUniformLocation(resources.shaderProgram, "uWindowSize");
        resources.fillColorLocation = glGetUniformLocation(resources.shaderProgram, "uFillColor");
        resources.gradientStartLocation = glGetUniformLocation(resources.shaderProgram, "uGradientStart");
        resources.gradientEndLocation = glGetUniformLocation(resources.shaderProgram, "uGradientEnd");
        resources.borderColorLocation = glGetUniformLocation(resources.shaderProgram, "uBorderColor");
        resources.shadowColorLocation = glGetUniformLocation(resources.shaderProgram, "uShadowColor");
        resources.rectLocation = glGetUniformLocation(resources.shaderProgram, "uRect");
        resources.radiusLocation = glGetUniformLocation(resources.shaderProgram, "uRadius");
        resources.borderWidthLocation = glGetUniformLocation(resources.shaderProgram, "uBorderWidth");
        resources.opacityLocation = glGetUniformLocation(resources.shaderProgram, "uOpacity");
        resources.shadowBlurLocation = glGetUniformLocation(resources.shaderProgram, "uShadowBlur");
        resources.blurAmountLocation = glGetUniformLocation(resources.shaderProgram, "uBlurAmount");
        resources.backdropRectLocation = glGetUniformLocation(resources.shaderProgram, "uBackdropRect");
        resources.useGradientLocation = glGetUniformLocation(resources.shaderProgram, "uUseGradient");
        resources.gradientDirectionLocation = glGetUniformLocation(resources.shaderProgram, "uGradientDirection");
        resources.shadowPassLocation = glGetUniformLocation(resources.shaderProgram, "uShadowPass");
        resources.backdropLocation = glGetUniformLocation(resources.shaderProgram, "uBackdrop");

        glGenVertexArrays(1, &resources.vao);
        glGenBuffers(1, &resources.vbo);
        glBindVertexArray(resources.vao);
        glBindBuffer(GL_ARRAY_BUFFER, resources.vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 30, nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, reinterpret_cast<void*>(sizeof(float) * 3));
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        return resources.shaderProgram != 0 && resources.vao != 0 && resources.vbo != 0;
    }

    static void releaseSharedResources() {
        SharedResources& resources = sharedResources();
        resources.references = std::max(0, resources.references - 1);
        if (resources.references > 0) {
            return;
        }

        if (resources.vbo) {
            glDeleteBuffers(1, &resources.vbo);
            resources.vbo = 0;
        }
        if (resources.vao) {
            glDeleteVertexArrays(1, &resources.vao);
            resources.vao = 0;
        }
        if (resources.shaderProgram) {
            glDeleteProgram(resources.shaderProgram);
            resources.shaderProgram = 0;
        }
        if (resources.backdropTexture) {
            glDeleteTextures(1, &resources.backdropTexture);
            resources.backdropTexture = 0;
        }
        if (resources.backdropFramebuffer) {
            glDeleteFramebuffers(1, &resources.backdropFramebuffer);
            resources.backdropFramebuffer = 0;
        }
        resources.backdropX = 0;
        resources.backdropY = 0;
        resources.backdropWidth = 0;
        resources.backdropHeight = 0;
        resources.backdropTextureWidth = 0;
        resources.backdropTextureHeight = 0;
        resources.windowSizeLocation = -1;
        resources.fillColorLocation = -1;
        resources.gradientStartLocation = -1;
        resources.gradientEndLocation = -1;
        resources.borderColorLocation = -1;
        resources.shadowColorLocation = -1;
        resources.rectLocation = -1;
        resources.radiusLocation = -1;
        resources.borderWidthLocation = -1;
        resources.opacityLocation = -1;
        resources.shadowBlurLocation = -1;
        resources.blurAmountLocation = -1;
        resources.backdropRectLocation = -1;
        resources.useGradientLocation = -1;
        resources.gradientDirectionLocation = -1;
        resources.shadowPassLocation = -1;
        resources.backdropLocation = -1;
    }

    static void ensureBackdropTexture(int width, int height) {
        SharedResources& resources = sharedResources();
        width = std::max(1, width);
        height = std::max(1, height);
        if (resources.backdropTexture != 0 &&
            resources.backdropTextureWidth == width &&
            resources.backdropTextureHeight == height) {
            return;
        }

        if (resources.backdropTexture == 0) {
            glGenTextures(1, &resources.backdropTexture);
        }
        resources.backdropTextureWidth = width;
        resources.backdropTextureHeight = height;
        glBindTexture(GL_TEXTURE_2D, resources.backdropTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    static void captureBackdrop(int windowWidth, int windowHeight, const Rect& bounds, float blur) {
        const int safeWindowWidth = std::max(1, windowWidth);
        const int safeWindowHeight = std::max(1, windowHeight);
        const int left = std::clamp(static_cast<int>(std::floor(bounds.x - blur)), 0, safeWindowWidth - 1);
        const int top = std::clamp(static_cast<int>(std::floor(bounds.y - blur)), 0, safeWindowHeight - 1);
        const int right = std::clamp(static_cast<int>(std::ceil(bounds.x + bounds.width + blur)), left + 1, safeWindowWidth);
        const int bottom = std::clamp(static_cast<int>(std::ceil(bounds.y + bounds.height + blur)), top + 1, safeWindowHeight);
        const int captureWidth = right - left;
        const int captureHeight = bottom - top;
        const int sourceY = safeWindowHeight - bottom;
        constexpr float backdropScale = 0.5f;
        const int textureWidth = std::max(1, static_cast<int>(std::ceil(static_cast<float>(captureWidth) * backdropScale)));
        const int textureHeight = std::max(1, static_cast<int>(std::ceil(static_cast<float>(captureHeight) * backdropScale)));

        ensureBackdropTexture(textureWidth, textureHeight);
        SharedResources& resources = sharedResources();
        resources.backdropX = left;
        resources.backdropY = sourceY;
        resources.backdropWidth = captureWidth;
        resources.backdropHeight = captureHeight;

        if (resources.backdropFramebuffer == 0) {
            glGenFramebuffers(1, &resources.backdropFramebuffer);
        }

        GLint previousReadFramebuffer = 0;
        GLint previousDrawFramebuffer = 0;
        GLint previousTexture = 0;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        const GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);

        glBindTexture(GL_TEXTURE_2D, resources.backdropTexture);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, previousDrawFramebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resources.backdropFramebuffer);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resources.backdropTexture, 0);

        if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
            if (scissorEnabled) {
                glDisable(GL_SCISSOR_TEST);
            }
            glBlitFramebuffer(left, sourceY, right, sourceY + captureHeight,
                              0, 0, textureWidth, textureHeight,
                              GL_COLOR_BUFFER_BIT, GL_LINEAR);
            if (scissorEnabled) {
                glEnable(GL_SCISSOR_TEST);
            }
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousReadFramebuffer));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDrawFramebuffer));
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    }

    static GLuint compileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            glDeleteShader(shader);
            return 0;
        }

        return shader;
    }

    Vec3 transformPoint(float x, float y) const {
        if (hasTransformMatrix_) {
            return core::transformPointWithW(transformMatrix_, x, y);
        }

        const Vec2 origin = {
            bounds_.x + bounds_.width * transform_.origin.x,
            bounds_.y + bounds_.height * transform_.origin.y
        };

        const float scaledX = (x - origin.x) * transform_.scale.x;
        const float scaledY = (y - origin.y) * transform_.scale.y;
        const float cosine = std::cos(transform_.rotate);
        const float sine = std::sin(transform_.rotate);

        return {
            origin.x + scaledX * cosine - scaledY * sine + transform_.translate.x,
            origin.y + scaledX * sine + scaledY * cosine + transform_.translate.y,
            1.0f
        };
    }

    static Rect expandRect(const Rect& rect, float amount) {
        return {
            rect.x - amount,
            rect.y - amount,
            rect.width + amount * 2.0f,
            rect.height + amount * 2.0f
        };
    }

    static Color withAlpha(Color color, float alphaScale) {
        color.a *= alphaScale;
        return color;
    }

    void drawShadow(int windowWidth, int windowHeight) const {
        if (bounds_.width <= 0.0f || bounds_.height <= 0.0f ||
            opacity_ <= 0.001f || shadow_.color.a <= 0.001f) {
            return;
        }

        Rect shadowShape = bounds_;
        shadowShape.x += shadow_.offset.x - shadow_.spread;
        shadowShape.y += shadow_.offset.y - shadow_.spread;
        shadowShape.width += shadow_.spread * 2.0f;
        shadowShape.height += shadow_.spread * 2.0f;

        const float blur = std::max(shadow_.blur, 1.0f);
        const float offsetMagnitude = std::max(std::fabs(shadow_.offset.x), std::fabs(shadow_.offset.y));
        const float shadowBlur = blur * 1.08f;
        const float shadowExtent = shadowBlur * 1.18f + offsetMagnitude * 0.20f + 1.0f;
        drawLayer(windowWidth, windowHeight, expandRect(shadowShape, shadowExtent), shadowShape,
                  true, withAlpha(shadow_.color, 0.74f), shadowBlur);
    }

    void drawLayer(int windowWidth,
                   int windowHeight,
                   const Rect& geometryBounds,
                   const Rect& sdfBounds,
                   bool shadowPass,
                   const Color& layerColor,
                   float blur) const {
        const float left = geometryBounds.x;
        const float top = geometryBounds.y;
        const float right = geometryBounds.x + geometryBounds.width;
        const float bottom = geometryBounds.y + geometryBounds.height;

        const Vec3 p0 = transformPoint(left, top);
        const Vec3 p1 = transformPoint(right, top);
        const Vec3 p2 = transformPoint(right, bottom);
        const Vec3 p3 = transformPoint(left, bottom);

        const float vertices[] = {
            p0.x, p0.y, p0.z, left, top,
            p1.x, p1.y, p1.z, right, top,
            p2.x, p2.y, p2.z, right, bottom,
            p0.x, p0.y, p0.z, left, top,
            p2.x, p2.y, p2.z, right, bottom,
            p3.x, p3.y, p3.z, left, bottom
        };

        const float radius = std::clamp(cornerRadius_, 0.0f, std::min(sdfBounds.width, sdfBounds.height) * 0.5f);
        const float borderWidth = shadowPass ? 0.0f : std::clamp(border_.width, 0.0f, std::min(sdfBounds.width, sdfBounds.height) * 0.5f);

        glUseProgram(shaderProgram_);
        glUniform2f(windowSizeLocation_, static_cast<float>(windowWidth), static_cast<float>(windowHeight));
        glUniform4f(fillColorLocation_, color_.r, color_.g, color_.b, color_.a);
        glUniform4f(gradientStartLocation_, gradient_.start.r, gradient_.start.g, gradient_.start.b, gradient_.start.a);
        glUniform4f(gradientEndLocation_, gradient_.end.r, gradient_.end.g, gradient_.end.b, gradient_.end.a);
        glUniform4f(borderColorLocation_, border_.color.r, border_.color.g, border_.color.b, border_.color.a);
        glUniform4f(shadowColorLocation_, layerColor.r, layerColor.g, layerColor.b, layerColor.a);
        glUniform4f(rectLocation_, sdfBounds.x, sdfBounds.y, sdfBounds.width, sdfBounds.height);
        glUniform1f(radiusLocation_, radius);
        glUniform1f(borderWidthLocation_, borderWidth);
        glUniform1f(opacityLocation_, opacity_);
        glUniform1f(shadowBlurLocation_, blur);
        glUniform1f(blurAmountLocation_, shadowPass ? 0.0f : blur_);
        glUniform4f(backdropRectLocation_,
                    static_cast<float>(sharedResources().backdropX),
                    static_cast<float>(sharedResources().backdropY),
                    static_cast<float>(std::max(1, sharedResources().backdropWidth)),
                    static_cast<float>(std::max(1, sharedResources().backdropHeight)));
        glUniform1i(useGradientLocation_, gradient_.enabled && !shadowPass ? 1 : 0);
        glUniform1i(gradientDirectionLocation_, static_cast<int>(gradient_.direction));
        glUniform1i(shadowPassLocation_, shadowPass ? 1 : 0);
        glUniform1i(backdropLocation_, 0);

        if (sharedResources().backdropTexture == 0) {
            ensureBackdropTexture(1, 1);
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sharedResources().backdropTexture);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    Rect bounds_;
    Color color_ = {1.0f, 1.0f, 1.0f, 1.0f};
    Gradient gradient_;
    Border border_;
    Shadow shadow_;
    Transform transform_;
    TransformMatrix transformMatrix_;
    bool hasTransformMatrix_ = false;
    float cornerRadius_ = 0.0f;
    float blur_ = 0.0f;
    float opacity_ = 1.0f;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint shaderProgram_ = 0;
    GLint windowSizeLocation_ = -1;
    GLint fillColorLocation_ = -1;
    GLint gradientStartLocation_ = -1;
    GLint gradientEndLocation_ = -1;
    GLint borderColorLocation_ = -1;
    GLint shadowColorLocation_ = -1;
    GLint rectLocation_ = -1;
    GLint radiusLocation_ = -1;
    GLint borderWidthLocation_ = -1;
    GLint opacityLocation_ = -1;
    GLint shadowBlurLocation_ = -1;
    GLint blurAmountLocation_ = -1;
    GLint backdropRectLocation_ = -1;
    GLint useGradientLocation_ = -1;
    GLint gradientDirectionLocation_ = -1;
    GLint shadowPassLocation_ = -1;
    GLint backdropLocation_ = -1;
};

class PolygonPrimitive {
public:
    PolygonPrimitive() = default;

    bool initialize() {
        const char* vertexSource =
            "#version 330 core\n"
            "layout(location = 0) in vec2 aScreenPos;\n"
            "uniform vec2 uWindowSize;\n"
            "void main() {\n"
            "    vec2 ndc = vec2((aScreenPos.x / uWindowSize.x) * 2.0 - 1.0,\n"
            "                    1.0 - (aScreenPos.y / uWindowSize.y) * 2.0);\n"
            "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
            "}\n";

        const char* fragmentSource =
            "#version 330 core\n"
            "out vec4 FragColor;\n"
            "uniform vec4 uFillColor;\n"
            "uniform float uOpacity;\n"
            "void main() {\n"
            "    FragColor = vec4(uFillColor.rgb, uFillColor.a * uOpacity);\n"
            "}\n";

        if (!retainSharedResources(vertexSource, fragmentSource)) {
            return false;
        }

        const SharedResources& resources = sharedResources();
        vao_ = resources.vao;
        vbo_ = resources.vbo;
        shaderProgram_ = resources.shaderProgram;
        windowSizeLocation_ = resources.windowSizeLocation;
        fillColorLocation_ = resources.fillColorLocation;
        opacityLocation_ = resources.opacityLocation;
        return true;
    }

    void destroy() {
        if (shaderProgram_) {
            releaseSharedResources();
        }
        vao_ = 0;
        vbo_ = 0;
        shaderProgram_ = 0;
        windowSizeLocation_ = -1;
        fillColorLocation_ = -1;
        opacityLocation_ = -1;
    }

    void setBounds(float x, float y, float width, float height) { bounds_ = {x, y, width, height}; }
    void setPoints(const std::vector<Vec2>& points) { points_ = points; }
    void setColor(const Color& color) { color_ = color; }
    void setOpacity(float opacity) { opacity_ = std::clamp(opacity, 0.0f, 1.0f); }
    void setTransform(const Transform& transform) {
        transform_ = transform;
        hasTransformMatrix_ = false;
    }
    void setTransformMatrix(const TransformMatrix& matrix) {
        transformMatrix_ = matrix;
        hasTransformMatrix_ = true;
    }

    void render(int windowWidth, int windowHeight) const {
        if (!shaderProgram_ || !vao_ || !vbo_ || points_.size() < 3 || opacity_ <= 0.0f || color_.a <= 0.0f) {
            return;
        }

        std::vector<float> vertices;
        vertices.reserve(points_.size() * 2u);
        for (const Vec2& point : points_) {
            const Vec2 transformed = transformPoint(bounds_.x + point.x, bounds_.y + point.y);
            vertices.push_back(transformed.x);
            vertices.push_back(transformed.y);
        }

        const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(shaderProgram_);
        glUniform2f(windowSizeLocation_, static_cast<float>(windowWidth), static_cast<float>(windowHeight));
        glUniform4f(fillColorLocation_, color_.r, color_.g, color_.b, color_.a);
        glUniform1f(opacityLocation_, opacity_);

        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLE_FAN, 0, static_cast<GLsizei>(points_.size()));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        if (!blendEnabled) {
            glDisable(GL_BLEND);
        }
    }

private:
    struct SharedResources {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint shaderProgram = 0;
        GLint windowSizeLocation = -1;
        GLint fillColorLocation = -1;
        GLint opacityLocation = -1;
        int references = 0;
    };

    static SharedResources& sharedResources() {
        static std::unordered_map<window::ContextKey, SharedResources> resourcesByContext;
        return resourcesByContext[window::currentContextKey()];
    }

    static bool retainSharedResources(const char* vertexSource, const char* fragmentSource) {
        SharedResources& resources = sharedResources();
        ++resources.references;
        if (resources.shaderProgram != 0) {
            return true;
        }

        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
        if (!vertexShader || !fragmentShader) {
            if (vertexShader) {
                glDeleteShader(vertexShader);
            }
            if (fragmentShader) {
                glDeleteShader(fragmentShader);
            }
            resources.references = std::max(0, resources.references - 1);
            return false;
        }

        resources.shaderProgram = glCreateProgram();
        glAttachShader(resources.shaderProgram, vertexShader);
        glAttachShader(resources.shaderProgram, fragmentShader);
        glLinkProgram(resources.shaderProgram);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        GLint linked = 0;
        glGetProgramiv(resources.shaderProgram, GL_LINK_STATUS, &linked);
        if (!linked) {
            glDeleteProgram(resources.shaderProgram);
            resources.shaderProgram = 0;
            resources.references = std::max(0, resources.references - 1);
            return false;
        }

        resources.windowSizeLocation = glGetUniformLocation(resources.shaderProgram, "uWindowSize");
        resources.fillColorLocation = glGetUniformLocation(resources.shaderProgram, "uFillColor");
        resources.opacityLocation = glGetUniformLocation(resources.shaderProgram, "uOpacity");

        glGenVertexArrays(1, &resources.vao);
        glGenBuffers(1, &resources.vbo);
        glBindVertexArray(resources.vao);
        glBindBuffer(GL_ARRAY_BUFFER, resources.vbo);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        return resources.shaderProgram != 0 && resources.vao != 0 && resources.vbo != 0;
    }

    static void releaseSharedResources() {
        SharedResources& resources = sharedResources();
        resources.references = std::max(0, resources.references - 1);
        if (resources.references > 0) {
            return;
        }

        if (resources.vbo) {
            glDeleteBuffers(1, &resources.vbo);
            resources.vbo = 0;
        }
        if (resources.vao) {
            glDeleteVertexArrays(1, &resources.vao);
            resources.vao = 0;
        }
        if (resources.shaderProgram) {
            glDeleteProgram(resources.shaderProgram);
            resources.shaderProgram = 0;
        }
        resources.windowSizeLocation = -1;
        resources.fillColorLocation = -1;
        resources.opacityLocation = -1;
    }

    static GLuint compileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            glDeleteShader(shader);
            return 0;
        }

        return shader;
    }

    Vec2 transformPoint(float x, float y) const {
        if (hasTransformMatrix_) {
            return core::transformPoint(transformMatrix_, x, y);
        }

        const Vec2 origin = {
            bounds_.x + bounds_.width * transform_.origin.x,
            bounds_.y + bounds_.height * transform_.origin.y
        };

        const float scaledX = (x - origin.x) * transform_.scale.x;
        const float scaledY = (y - origin.y) * transform_.scale.y;
        const float cosine = std::cos(transform_.rotate);
        const float sine = std::sin(transform_.rotate);

        return {
            origin.x + scaledX * cosine - scaledY * sine + transform_.translate.x,
            origin.y + scaledX * sine + scaledY * cosine + transform_.translate.y
        };
    }

    Rect bounds_;
    std::vector<Vec2> points_;
    Color color_ = {1.0f, 1.0f, 1.0f, 1.0f};
    Transform transform_;
    TransformMatrix transformMatrix_;
    bool hasTransformMatrix_ = false;
    float opacity_ = 1.0f;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint shaderProgram_ = 0;
    GLint windowSizeLocation_ = -1;
    GLint fillColorLocation_ = -1;
    GLint opacityLocation_ = -1;
};

inline Color mixColor(const Color& from, const Color& to, float amount) {
    const float clampedAmount = std::clamp(amount, 0.0f, 1.0f);
    const float inverse = 1.0f - clampedAmount;
    return {
        from.r * inverse + to.r * clampedAmount,
        from.g * inverse + to.g * clampedAmount,
        from.b * inverse + to.b * clampedAmount,
        from.a * inverse + to.a * clampedAmount
    };
}

} // namespace core
