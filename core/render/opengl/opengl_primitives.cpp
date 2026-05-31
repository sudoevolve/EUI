#include "core/render/opengl/opengl_backend.h"

#include "core/window/window_backend.h"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <vector>

namespace core::render::opengl {

namespace {

struct PrimitiveResources {
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
};

PrimitiveResources& primitiveResources() {
    static std::unordered_map<window::ContextKey, PrimitiveResources> resourcesByContext;
    return resourcesByContext[window::currentContextKey()];
}

GLuint compileShader(GLenum type, const char* source) {
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

void releaseResources(PrimitiveResources& resources) {
    if (resources.vbo) {
        glDeleteBuffers(1, &resources.vbo);
    }
    if (resources.vao) {
        glDeleteVertexArrays(1, &resources.vao);
    }
    if (resources.shaderProgram) {
        glDeleteProgram(resources.shaderProgram);
    }
    if (resources.backdropTexture) {
        glDeleteTextures(1, &resources.backdropTexture);
    }
    if (resources.backdropFramebuffer) {
        glDeleteFramebuffers(1, &resources.backdropFramebuffer);
    }
    resources = {};
}

bool ensurePrimitiveResources() {
    PrimitiveResources& resources = primitiveResources();
    if (resources.shaderProgram != 0 && resources.vao != 0 && resources.vbo != 0) {
        return true;
    }

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

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vertexShader || !fragmentShader) {
        if (vertexShader) {
            glDeleteShader(vertexShader);
        }
        if (fragmentShader) {
            glDeleteShader(fragmentShader);
        }
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
        releaseResources(resources);
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PrimitiveGeometryVertex), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(PrimitiveGeometryVertex), reinterpret_cast<void*>(offsetof(PrimitiveGeometryVertex, local)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return resources.shaderProgram != 0 && resources.vao != 0 && resources.vbo != 0;
}

void ensureBackdropTexture(int width, int height) {
    PrimitiveResources& resources = primitiveResources();
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

} // namespace

void OpenGLRenderBackend::prepareBackdropBlur(const core::Rect& bounds, float blur, int windowWidth, int windowHeight) {
    if (blur <= 0.0f || windowWidth <= 0 || windowHeight <= 0) {
        return;
    }

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
    PrimitiveResources& resources = primitiveResources();
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

void OpenGLRenderBackend::drawRoundedRect(const RoundedRectDrawCommand& command, int windowWidth, int windowHeight) {
    const float effectiveAlpha = command.gradient.enabled && !command.shadowPass
        ? std::max(command.gradient.start.a, command.gradient.end.a)
        : command.fillColor.a;
    if (command.vertices.empty() || windowWidth <= 0 || windowHeight <= 0 ||
        command.opacity <= 0.001f || effectiveAlpha <= 0.001f || !ensurePrimitiveResources()) {
        return;
    }

    PrimitiveResources& resources = primitiveResources();
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(resources.shaderProgram);
    glUniform2f(resources.windowSizeLocation, static_cast<float>(windowWidth), static_cast<float>(windowHeight));
    glUniform4f(resources.fillColorLocation, command.fillColor.r, command.fillColor.g, command.fillColor.b, command.fillColor.a);
    glUniform4f(resources.gradientStartLocation, command.gradient.start.r, command.gradient.start.g, command.gradient.start.b, command.gradient.start.a);
    glUniform4f(resources.gradientEndLocation, command.gradient.end.r, command.gradient.end.g, command.gradient.end.b, command.gradient.end.a);
    glUniform4f(resources.borderColorLocation, command.border.color.r, command.border.color.g, command.border.color.b, command.border.color.a);
    glUniform4f(resources.shadowColorLocation, command.fillColor.r, command.fillColor.g, command.fillColor.b, command.fillColor.a);
    glUniform4f(resources.rectLocation, command.rect.x, command.rect.y, command.rect.width, command.rect.height);
    glUniform1f(resources.radiusLocation, command.radius);
    glUniform1f(resources.borderWidthLocation, command.shadowPass ? 0.0f : command.border.width);
    glUniform1f(resources.opacityLocation, command.opacity);
    glUniform1f(resources.shadowBlurLocation, command.shadowBlur);
    glUniform1f(resources.blurAmountLocation, command.shadowPass ? 0.0f : command.backdropBlur);
    glUniform4f(resources.backdropRectLocation,
                static_cast<float>(resources.backdropX),
                static_cast<float>(resources.backdropY),
                static_cast<float>(std::max(1, resources.backdropWidth)),
                static_cast<float>(std::max(1, resources.backdropHeight)));
    glUniform1i(resources.useGradientLocation, command.gradient.enabled && !command.shadowPass ? 1 : 0);
    glUniform1i(resources.gradientDirectionLocation, static_cast<int>(command.gradient.direction));
    glUniform1i(resources.shadowPassLocation, command.shadowPass ? 1 : 0);
    glUniform1i(resources.backdropLocation, 0);

    if (resources.backdropTexture == 0) {
        ensureBackdropTexture(1, 1);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, resources.backdropTexture);
    glBindVertexArray(resources.vao);
    glBindBuffer(GL_ARRAY_BUFFER, resources.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(command.vertices.size() * sizeof(PrimitiveGeometryVertex)),
                 command.vertices.data(),
                 GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(command.vertices.size()));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!blendEnabled) {
        glDisable(GL_BLEND);
    }
}

void OpenGLRenderBackend::releasePrimitiveResources() {
    releaseResources(primitiveResources());
}

} // namespace core::render::opengl
