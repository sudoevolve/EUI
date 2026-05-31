#pragma once

#include "core/render/render_types.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace core::render {

struct PrimitiveGeometryVertex {
    Vec3 screen;
    Vec2 local;
};

inline Vec3 transformPrimitivePoint(const Rect& bounds,
                                    const Transform& transform,
                                    const TransformMatrix& matrix,
                                    bool hasTransformMatrix,
                                    float x,
                                    float y) {
    if (hasTransformMatrix) {
        return core::transformPointWithW(matrix, x, y);
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
        origin.y + scaledX * sine + scaledY * cosine + transform.translate.y,
        1.0f
    };
}

inline Rect expandPrimitiveRect(const Rect& rect, float amount) {
    return {
        rect.x - amount,
        rect.y - amount,
        rect.width + amount * 2.0f,
        rect.height + amount * 2.0f
    };
}

inline Color scalePrimitiveAlpha(Color color, float alphaScale) {
    color.a *= alphaScale;
    return color;
}

inline Rect primitiveShadowShape(const Rect& bounds, const Shadow& shadow) {
    return {
        bounds.x + shadow.offset.x - shadow.spread,
        bounds.y + shadow.offset.y - shadow.spread,
        bounds.width + shadow.spread * 2.0f,
        bounds.height + shadow.spread * 2.0f
    };
}

inline float primitiveShadowBlur(const Shadow& shadow) {
    return std::max(shadow.blur, 1.0f) * 1.08f;
}

inline float primitiveShadowExtent(const Shadow& shadow, float shadowBlur) {
    const float offsetMagnitude = std::max(std::fabs(shadow.offset.x), std::fabs(shadow.offset.y));
    return shadowBlur * 1.18f + offsetMagnitude * 0.20f + 1.0f;
}

inline float clampedPrimitiveRadius(float radius, const Rect& rect) {
    return std::clamp(radius, 0.0f, std::min(rect.width, rect.height) * 0.5f);
}

inline float clampedPrimitiveBorderWidth(float width, const Rect& rect) {
    return std::clamp(width, 0.0f, std::min(rect.width, rect.height) * 0.5f);
}

inline std::array<PrimitiveGeometryVertex, 6> roundedRectGeometryVertices(const Rect& bounds,
                                                                         const Transform& transform,
                                                                         const TransformMatrix& matrix,
                                                                         bool hasTransformMatrix,
                                                                         const Rect& geometryBounds) {
    const float left = geometryBounds.x;
    const float top = geometryBounds.y;
    const float right = geometryBounds.x + geometryBounds.width;
    const float bottom = geometryBounds.y + geometryBounds.height;

    const Vec3 p0 = transformPrimitivePoint(bounds, transform, matrix, hasTransformMatrix, left, top);
    const Vec3 p1 = transformPrimitivePoint(bounds, transform, matrix, hasTransformMatrix, right, top);
    const Vec3 p2 = transformPrimitivePoint(bounds, transform, matrix, hasTransformMatrix, right, bottom);
    const Vec3 p3 = transformPrimitivePoint(bounds, transform, matrix, hasTransformMatrix, left, bottom);

    return {{
        {{p0.x, p0.y, p0.z}, {left, top}},
        {{p1.x, p1.y, p1.z}, {right, top}},
        {{p2.x, p2.y, p2.z}, {right, bottom}},
        {{p0.x, p0.y, p0.z}, {left, top}},
        {{p2.x, p2.y, p2.z}, {right, bottom}},
        {{p3.x, p3.y, p3.z}, {left, bottom}}
    }};
}

inline void appendPolygonTriangleFan(std::vector<PrimitiveGeometryVertex>& vertices,
                                     const Rect& bounds,
                                     const Transform& transform,
                                     const TransformMatrix& matrix,
                                     bool hasTransformMatrix,
                                     const std::vector<Vec2>& points) {
    if (points.size() < 3) {
        return;
    }

    auto appendPoint = [&](const Vec2& point) {
        const float localX = bounds.x + point.x;
        const float localY = bounds.y + point.y;
        const Vec3 transformed = transformPrimitivePoint(bounds, transform, matrix, hasTransformMatrix, localX, localY);
        vertices.push_back({{transformed.x, transformed.y, transformed.z}, {localX, localY}});
    };

    for (std::size_t i = 1; i + 1 < points.size(); ++i) {
        appendPoint(points.front());
        appendPoint(points[i]);
        appendPoint(points[i + 1]);
    }
}

} // namespace core::render
