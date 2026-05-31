#pragma once

#include "core/render/render_types.h"

#include <memory>
#include <vector>

namespace core {

class RoundedRectPrimitive {
public:
    RoundedRectPrimitive();
    RoundedRectPrimitive(float x, float y, float width, float height);
    ~RoundedRectPrimitive();

    RoundedRectPrimitive(const RoundedRectPrimitive&) = delete;
    RoundedRectPrimitive& operator=(const RoundedRectPrimitive&) = delete;
    RoundedRectPrimitive(RoundedRectPrimitive&&) noexcept;
    RoundedRectPrimitive& operator=(RoundedRectPrimitive&&) noexcept;

    bool initialize();
    void destroy();

    void setBounds(float x, float y, float width, float height);
    void setColor(const Color& color);
    void setGradient(const Gradient& gradient);
    void setCornerRadius(float radius);
    void setOpacity(float opacity);
    void setBorder(const Border& border);
    void setShadow(const Shadow& shadow);
    void setBlur(float blur);
    void setTranslate(float x, float y);
    void setScale(float x, float y);
    void setRotate(float radians);
    void setTransformOrigin(float x, float y);
    void setTransform(const Transform& transform);
    void setTransformMatrix(const TransformMatrix& matrix);

    const Rect& bounds() const;
    const Color& color() const;
    const Gradient& gradient() const;
    const Border& border() const;
    const Shadow& shadow() const;
    float blur() const;
    const Transform& transform() const;
    float cornerRadius() const;
    float opacity() const;

    void render(int windowWidth, int windowHeight) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class PolygonPrimitive {
public:
    PolygonPrimitive();
    ~PolygonPrimitive();

    PolygonPrimitive(const PolygonPrimitive&) = delete;
    PolygonPrimitive& operator=(const PolygonPrimitive&) = delete;
    PolygonPrimitive(PolygonPrimitive&&) noexcept;
    PolygonPrimitive& operator=(PolygonPrimitive&&) noexcept;

    bool initialize();
    void destroy();

    void setBounds(float x, float y, float width, float height);
    void setPoints(const std::vector<Vec2>& points);
    void setColor(const Color& color);
    void setOpacity(float opacity);
    void setTransform(const Transform& transform);
    void setTransformMatrix(const TransformMatrix& matrix);

    void render(int windowWidth, int windowHeight) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace core
