#pragma once

#include "core/animation.h"
#include "core/layout.h"
#include "core/platform/event.h"
#include "core/render/render_types.h"
#include "core/render/text.h"

namespace eui {

using Align = core::Align;
using AnimProperty = core::AnimProperty;
using Ease = core::Ease;
using Transition = core::Transition;
using Color = core::Color;
using Vec2 = core::Vec2;
using Vec3 = core::Vec3;
using Rect = core::Rect;
using SizeValue = core::SizeValue;
using Gradient = core::Gradient;
using GradientDirection = core::GradientDirection;
using Border = core::Border;
using Shadow = core::Shadow;
using Transform = core::Transform;
using TransformMatrix = core::TransformMatrix;
using HorizontalAlign = core::HorizontalAlign;
using VerticalAlign = core::VerticalAlign;
using TextStyle = core::TextStyle;
using CursorShape = core::CursorShape;
using PointerEvent = core::PointerEvent;

inline Color mixColor(const Color& from, const Color& to, float amount) {
    return core::mixColor(from, to, amount);
}

} // namespace eui
