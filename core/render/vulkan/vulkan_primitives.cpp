#include "core/render/primitive.h"
#include "core/render/render_backend.h"
#include "core/render/vulkan/vulkan_backend.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace core {

namespace {

Vec2 transformedPoint(float x,
                      float y,
                      const Rect& bounds,
                      const Transform& transform,
                      const TransformMatrix& matrix,
                      bool hasTransformMatrix) {
    if (hasTransformMatrix) {
        return transformPoint(matrix, x, y);
    }

    const Vec2 origin = {
        bounds.x + bounds.width * transform.origin.x,
        bounds.y + bounds.height * transform.origin.y
    };
    const float scaledX = (x - origin.x) * transform.scale.x;
    const float scaledY = (y - origin.y) * transform.scale.y;
    const float cosine = std::cos(transform.rotate);
    const float sine = std::sin(transform.rotate);

    return {
        origin.x + scaledX * cosine - scaledY * sine + transform.translate.x,
        origin.y + scaledX * sine + scaledY * cosine + transform.translate.y
    };
}

Rect transformedBounds(const Rect& bounds, const Transform& transform, const TransformMatrix& matrix, bool hasTransformMatrix) {
    const std::array<Vec2, 4> corners = {
        transformedPoint(bounds.x, bounds.y, bounds, transform, matrix, hasTransformMatrix),
        transformedPoint(bounds.x + bounds.width, bounds.y, bounds, transform, matrix, hasTransformMatrix),
        transformedPoint(bounds.x + bounds.width, bounds.y + bounds.height, bounds, transform, matrix, hasTransformMatrix),
        transformedPoint(bounds.x, bounds.y + bounds.height, bounds, transform, matrix, hasTransformMatrix)
    };
    float left = corners[0].x;
    float right = corners[0].x;
    float top = corners[0].y;
    float bottom = corners[0].y;
    for (const Vec2& corner : corners) {
        left = std::min(left, corner.x);
        right = std::max(right, corner.x);
        top = std::min(top, corner.y);
        bottom = std::max(bottom, corner.y);
    }
    return {left, top, right - left, bottom - top};
}

Rect transformedPolygonBounds(const Rect& bounds,
                              const std::vector<Vec2>& points,
                              const Transform& transform,
                              const TransformMatrix& matrix,
                              bool hasTransformMatrix) {
    if (points.empty()) {
        return {};
    }
    Vec2 first = transformedPoint(bounds.x + points.front().x,
                                  bounds.y + points.front().y,
                                  bounds,
                                  transform,
                                  matrix,
                                  hasTransformMatrix);

    float left = first.x;
    float right = first.x;
    float top = first.y;
    float bottom = first.y;
    for (const Vec2& point : points) {
        Vec2 transformed = transformedPoint(bounds.x + point.x,
                                            bounds.y + point.y,
                                            bounds,
                                            transform,
                                            matrix,
                                            hasTransformMatrix);
        left = std::min(left, transformed.x);
        right = std::max(right, transformed.x);
        top = std::min(top, transformed.y);
        bottom = std::max(bottom, transformed.y);
    }
    return {left, top, right - left, bottom - top};
}

void fillRectWithActiveBackend(const Rect& rect, const Color& color, float opacity, int windowWidth, int windowHeight) {
    if (opacity <= 0.0f || color.a <= 0.0f) {
        return;
    }
    core::render::RenderBackend* backend = core::render::activeRenderBackend();
    if (backend == nullptr) {
        return;
    }
    Color resolved = color;
    resolved.a *= opacity;
    static_cast<core::render::vulkan::VulkanRenderBackend*>(backend)->fillRect(rect, resolved, windowWidth, windowHeight);
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
    Color fill = impl_->gradient.enabled ? mixColor(impl_->gradient.start, impl_->gradient.end, 0.5f) : impl_->color;
    const Rect rect = transformedBounds(impl_->bounds, impl_->transform, impl_->transformMatrix, impl_->hasTransformMatrix);
    fillRectWithActiveBackend(rect, fill, impl_->opacity, windowWidth, windowHeight);
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
    const Rect rect = transformedPolygonBounds(impl_->bounds,
                                               impl_->points,
                                               impl_->transform,
                                               impl_->transformMatrix,
                                               impl_->hasTransformMatrix);
    fillRectWithActiveBackend(rect, impl_->color, impl_->opacity, windowWidth, windowHeight);
}

} // namespace core
