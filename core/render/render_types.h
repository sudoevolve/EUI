#pragma once

#include <cmath>

namespace core {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    bool contains(double pointX, double pointY) const {
        return pointX >= x && pointX <= x + width &&
               pointY >= y && pointY <= y + height;
    }
};

enum class GradientDirection {
    Horizontal = 0,
    Vertical = 1
};

struct Gradient {
    bool enabled = false;
    Color start = {1.0f, 1.0f, 1.0f, 1.0f};
    Color end = {1.0f, 1.0f, 1.0f, 1.0f};
    GradientDirection direction = GradientDirection::Vertical;
};

struct Border {
    float width = 0.0f;
    Color color = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct Shadow {
    bool enabled = false;
    Vec2 offset = {0.0f, 4.0f};
    float blur = 8.0f;
    float spread = 0.0f;
    Color color = {0.0f, 0.0f, 0.0f, 0.28f};
};

struct Transform {
    Vec2 translate = {0.0f, 0.0f};
    float translateZ = 0.0f;
    Vec2 scale = {1.0f, 1.0f};
    float rotate = 0.0f;
    float rotateX = 0.0f;
    float rotateY = 0.0f;
    Vec2 origin = {0.5f, 0.5f};
    float perspective = 0.0f;
};

struct TransformMatrix {
    float m00 = 1.0f;
    float m01 = 0.0f;
    float tx = 0.0f;
    float m10 = 0.0f;
    float m11 = 1.0f;
    float ty = 0.0f;
    float px = 0.0f;
    float py = 0.0f;
    float pw = 1.0f;
};

inline Vec2 transformPoint(const TransformMatrix& matrix, float x, float y) {
    const float w = matrix.px * x + matrix.py * y + matrix.pw;
    const float invW = std::fabs(w) > 0.0001f ? 1.0f / w : 1.0f;
    return {
        (matrix.m00 * x + matrix.m01 * y + matrix.tx) * invW,
        (matrix.m10 * x + matrix.m11 * y + matrix.ty) * invW
    };
}

inline Vec3 transformPointWithW(const TransformMatrix& matrix, float x, float y) {
    float w = matrix.px * x + matrix.py * y + matrix.pw;
    if (std::fabs(w) <= 0.0001f) {
        w = w < 0.0f ? -0.0001f : 0.0001f;
    }
    const float invW = 1.0f / w;
    return {
        (matrix.m00 * x + matrix.m01 * y + matrix.tx) * invW,
        (matrix.m10 * x + matrix.m11 * y + matrix.ty) * invW,
        w
    };
}

} // namespace core
