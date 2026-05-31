#include "core/render/primitive.h"
#include "core/render/primitive_geometry.h"
#include "core/render/render_backend.h"
#include "core/render/vulkan/vulkan_backend.h"

#include <algorithm>

namespace core {

namespace {

core::render::vulkan::VulkanRenderBackend* activeVulkanBackend() {
    return static_cast<core::render::vulkan::VulkanRenderBackend*>(core::render::activeRenderBackend());
}

} // namespace

struct RoundedRectPrimitive::Impl {
    explicit Impl(float x = 0.0f, float y = 0.0f, float width = 0.0f, float height = 0.0f)
        : bounds{x, y, width, height} {}

    Rect bounds{};
    Color color{};
    Gradient gradient{};
    Border border{};
    Shadow shadow{};
    Transform transform{};
    TransformMatrix transformMatrix{};
    float blur = 0.0f;
    float cornerRadius = 0.0f;
    float opacity = 1.0f;
    bool hasTransformMatrix = false;

    void drawLayer(int windowWidth,
                   int windowHeight,
                   const Rect& geometryBounds,
                   const Rect& sdfBounds,
                   bool shadowPass,
                   const Color& layerColor,
                   float layerBlur) const {
        auto* backend = activeVulkanBackend();
        if (backend == nullptr) {
            return;
        }

        const auto vertices = core::render::roundedRectGeometryVertices(
            bounds, transform, transformMatrix, hasTransformMatrix, geometryBounds);

        core::render::vulkan::RoundedRectDrawData data{};
        data.vertices.reserve(vertices.size());
        for (const core::render::PrimitiveGeometryVertex& vertex : vertices) {
            data.vertices.push_back({vertex.screen.x, vertex.screen.y, vertex.screen.z, vertex.local.x, vertex.local.y});
        }
        data.fillColor = shadowPass ? layerColor : color;
        data.gradient = gradient;
        data.border = shadowPass ? Border{} : border;
        data.rect = sdfBounds;
        data.radius = core::render::clampedPrimitiveRadius(cornerRadius, sdfBounds);
        data.border.width = core::render::clampedPrimitiveBorderWidth(data.border.width, sdfBounds);
        data.opacity = opacity;
        data.shadowBlur = layerBlur;
        data.shadowPass = shadowPass;
        backend->drawRoundedRect(data, windowWidth, windowHeight);
    }

    void drawShadow(int windowWidth, int windowHeight) const {
        if (bounds.width <= 0.0f || bounds.height <= 0.0f ||
            opacity <= 0.001f || shadow.color.a <= 0.001f) {
            return;
        }

        const Rect shadowShape = core::render::primitiveShadowShape(bounds, shadow);
        const float shadowBlur = core::render::primitiveShadowBlur(shadow);
        const float shadowExtent = core::render::primitiveShadowExtent(shadow, shadowBlur);
        drawLayer(windowWidth, windowHeight, core::render::expandPrimitiveRect(shadowShape, shadowExtent), shadowShape,
                  true, core::render::scalePrimitiveAlpha(shadow.color, 0.74f), shadowBlur);
    }
};

RoundedRectPrimitive::RoundedRectPrimitive()
    : impl_(std::make_unique<Impl>()) {}

RoundedRectPrimitive::RoundedRectPrimitive(float x, float y, float width, float height)
    : impl_(std::make_unique<Impl>(x, y, width, height)) {}

RoundedRectPrimitive::~RoundedRectPrimitive() = default;
RoundedRectPrimitive::RoundedRectPrimitive(RoundedRectPrimitive&&) noexcept = default;
RoundedRectPrimitive& RoundedRectPrimitive::operator=(RoundedRectPrimitive&&) noexcept = default;

bool RoundedRectPrimitive::initialize() { return true; }
void RoundedRectPrimitive::destroy() {}
void RoundedRectPrimitive::setBounds(float x, float y, float width, float height) { impl_->bounds = {x, y, width, height}; }
void RoundedRectPrimitive::setColor(const Color& color) { impl_->color = color; }
void RoundedRectPrimitive::setGradient(const Gradient& gradient) { impl_->gradient = gradient; }
void RoundedRectPrimitive::setCornerRadius(float radius) { impl_->cornerRadius = radius; }
void RoundedRectPrimitive::setOpacity(float opacity) { impl_->opacity = std::clamp(opacity, 0.0f, 1.0f); }
void RoundedRectPrimitive::setBorder(const Border& border) { impl_->border = border; }
void RoundedRectPrimitive::setShadow(const Shadow& shadow) { impl_->shadow = shadow; }
void RoundedRectPrimitive::setBlur(float blur) { impl_->blur = std::max(0.0f, blur); }
void RoundedRectPrimitive::setTranslate(float x, float y) {
    impl_->transform.translate = {x, y};
    impl_->hasTransformMatrix = false;
}
void RoundedRectPrimitive::setScale(float x, float y) {
    impl_->transform.scale = {x, y};
    impl_->hasTransformMatrix = false;
}
void RoundedRectPrimitive::setRotate(float radians) {
    impl_->transform.rotate = radians;
    impl_->hasTransformMatrix = false;
}
void RoundedRectPrimitive::setTransformOrigin(float x, float y) {
    impl_->transform.origin = {x, y};
    impl_->hasTransformMatrix = false;
}
void RoundedRectPrimitive::setTransform(const Transform& transform) {
    impl_->transform = transform;
    impl_->hasTransformMatrix = false;
}
void RoundedRectPrimitive::setTransformMatrix(const TransformMatrix& matrix) {
    impl_->transformMatrix = matrix;
    impl_->hasTransformMatrix = true;
}
const Rect& RoundedRectPrimitive::bounds() const { return impl_->bounds; }
const Color& RoundedRectPrimitive::color() const { return impl_->color; }
const Gradient& RoundedRectPrimitive::gradient() const { return impl_->gradient; }
const Border& RoundedRectPrimitive::border() const { return impl_->border; }
const Shadow& RoundedRectPrimitive::shadow() const { return impl_->shadow; }
float RoundedRectPrimitive::blur() const { return impl_->blur; }
const Transform& RoundedRectPrimitive::transform() const { return impl_->transform; }
float RoundedRectPrimitive::cornerRadius() const { return impl_->cornerRadius; }
float RoundedRectPrimitive::opacity() const { return impl_->opacity; }
void RoundedRectPrimitive::render(int windowWidth, int windowHeight) const {
    if (impl_->bounds.width <= 0.0f || impl_->bounds.height <= 0.0f || impl_->opacity <= 0.001f) {
        return;
    }
    if (impl_->shadow.enabled) {
        impl_->drawShadow(windowWidth, windowHeight);
    }
    impl_->drawLayer(windowWidth, windowHeight, impl_->bounds, impl_->bounds, false, impl_->color, impl_->shadow.blur);
}

struct PolygonPrimitive::Impl {
    Rect bounds{};
    std::vector<Vec2> points;
    Color color{};
    Transform transform{};
    TransformMatrix transformMatrix{};
    float opacity = 1.0f;
    bool hasTransformMatrix = false;
};

PolygonPrimitive::PolygonPrimitive()
    : impl_(std::make_unique<Impl>()) {}

PolygonPrimitive::~PolygonPrimitive() = default;
PolygonPrimitive::PolygonPrimitive(PolygonPrimitive&&) noexcept = default;
PolygonPrimitive& PolygonPrimitive::operator=(PolygonPrimitive&&) noexcept = default;

bool PolygonPrimitive::initialize() { return true; }
void PolygonPrimitive::destroy() {}
void PolygonPrimitive::setBounds(float x, float y, float width, float height) { impl_->bounds = {x, y, width, height}; }
void PolygonPrimitive::setPoints(const std::vector<Vec2>& points) { impl_->points = points; }
void PolygonPrimitive::setColor(const Color& color) { impl_->color = color; }
void PolygonPrimitive::setOpacity(float opacity) { impl_->opacity = std::clamp(opacity, 0.0f, 1.0f); }
void PolygonPrimitive::setTransform(const Transform& transform) {
    impl_->transform = transform;
    impl_->hasTransformMatrix = false;
}
void PolygonPrimitive::setTransformMatrix(const TransformMatrix& matrix) {
    impl_->transformMatrix = matrix;
    impl_->hasTransformMatrix = true;
}
void PolygonPrimitive::render(int windowWidth, int windowHeight) const {
    if (impl_->points.size() < 3 || impl_->opacity <= 0.001f || impl_->color.a <= 0.001f) {
        return;
    }
    auto* backend = activeVulkanBackend();
    if (backend == nullptr) {
        return;
    }

    core::render::vulkan::RoundedRectDrawData data{};
    std::vector<core::render::PrimitiveGeometryVertex> vertices;
    vertices.reserve((impl_->points.size() - 2u) * 3u);
    core::render::appendPolygonTriangleFan(vertices,
                                           impl_->bounds,
                                           impl_->transform,
                                           impl_->transformMatrix,
                                           impl_->hasTransformMatrix,
                                           impl_->points);
    data.vertices.reserve(vertices.size());
    for (const core::render::PrimitiveGeometryVertex& vertex : vertices) {
        data.vertices.push_back({vertex.screen.x, vertex.screen.y, vertex.screen.z, vertex.local.x, vertex.local.y});
    }

    data.fillColor = impl_->color;
    data.border = {};
    data.gradient = {};
    data.rect = impl_->bounds;
    data.radius = 0.0f;
    data.opacity = impl_->opacity;
    data.shadowBlur = 1.0f;
    data.shadowPass = false;
    backend->drawRoundedRect(data, windowWidth, windowHeight);
}

} // namespace core
