#pragma once

#include "core/layout.h"
#include "core/primitive.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace core {

enum class Ease {
    Linear,
    InQuad,
    OutQuad,
    InOutQuad,
    OutCubic,
    InOutCubic,
    OutBack
};

enum class AnimProperty : std::uint32_t {
    None = 0,
    Frame = 1u << 0,
    Color = 1u << 1,
    TextColor = 1u << 2,
    Opacity = 1u << 3,
    Radius = 1u << 4,
    Border = 1u << 5,
    Shadow = 1u << 6,
    Blur = 1u << 7,
    Transform = 1u << 8,
    All = 0xFFFFFFFFu
};

inline AnimProperty operator|(AnimProperty left, AnimProperty right) {
    return static_cast<AnimProperty>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

inline AnimProperty operator&(AnimProperty left, AnimProperty right) {
    return static_cast<AnimProperty>(static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
}

inline AnimProperty& operator|=(AnimProperty& left, AnimProperty right) {
    left = left | right;
    return left;
}

inline bool hasAnimProperty(AnimProperty mask, AnimProperty property) {
    return (static_cast<std::uint32_t>(mask & property) != 0u);
}

struct Transition {
    bool enabled = false;
    float durationSeconds = 0.18f;
    float delaySeconds = 0.0f;
    Ease ease = Ease::OutCubic;
    AnimProperty properties = AnimProperty::All;

    static Transition none() {
        return {};
    }

    static Transition make(float duration, Ease easing = Ease::OutCubic) {
        Transition transition;
        transition.enabled = true;
        transition.durationSeconds = std::max(0.0f, duration);
        transition.ease = easing;
        return transition;
    }

    Transition& duration(float value) {
        enabled = true;
        durationSeconds = std::max(0.0f, value);
        return *this;
    }

    Transition& delay(float value) {
        enabled = true;
        delaySeconds = std::max(0.0f, value);
        return *this;
    }

    Transition& easing(Ease value) {
        enabled = true;
        ease = value;
        return *this;
    }

    Transition& animate(AnimProperty value) {
        enabled = true;
        properties = value;
        return *this;
    }
};

inline float applyEase(Ease ease, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    switch (ease) {
    case Ease::Linear:
        return t;
    case Ease::InQuad:
        return t * t;
    case Ease::OutQuad:
        return 1.0f - (1.0f - t) * (1.0f - t);
    case Ease::InOutQuad:
        return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
    case Ease::OutCubic:
        return 1.0f - std::pow(1.0f - t, 3.0f);
    case Ease::InOutCubic:
        return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
    case Ease::OutBack: {
        constexpr float c1 = 1.70158f;
        constexpr float c3 = c1 + 1.0f;
        return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
    }
    }
    return t;
}

inline bool closeEnough(float left, float right, float epsilon = 0.001f) {
    return std::fabs(left - right) <= epsilon;
}

inline bool closeEnough(const Vec2& left, const Vec2& right, float epsilon = 0.001f) {
    return closeEnough(left.x, right.x, epsilon) && closeEnough(left.y, right.y, epsilon);
}

inline bool closeEnough(const Color& left, const Color& right, float epsilon = 0.001f) {
    return closeEnough(left.r, right.r, epsilon) &&
           closeEnough(left.g, right.g, epsilon) &&
           closeEnough(left.b, right.b, epsilon) &&
           closeEnough(left.a, right.a, epsilon);
}

inline bool closeEnough(const Rect& left, const Rect& right, float epsilon = 0.001f) {
    return closeEnough(left.x, right.x, epsilon) &&
           closeEnough(left.y, right.y, epsilon) &&
           closeEnough(left.width, right.width, epsilon) &&
           closeEnough(left.height, right.height, epsilon);
}

inline bool closeEnough(const LayoutRect& left, const LayoutRect& right, float epsilon = 0.001f) {
    return closeEnough(left.x, right.x, epsilon) &&
           closeEnough(left.y, right.y, epsilon) &&
           closeEnough(left.width, right.width, epsilon) &&
           closeEnough(left.height, right.height, epsilon);
}

inline bool closeEnough(const Border& left, const Border& right, float epsilon = 0.001f) {
    return closeEnough(left.width, right.width, epsilon) && closeEnough(left.color, right.color, epsilon);
}

inline bool closeEnough(const Shadow& left, const Shadow& right, float epsilon = 0.001f) {
    return left.enabled == right.enabled &&
           closeEnough(left.offset, right.offset, epsilon) &&
           closeEnough(left.blur, right.blur, epsilon) &&
           closeEnough(left.spread, right.spread, epsilon) &&
           closeEnough(left.color, right.color, epsilon);
}

inline bool closeEnough(const Transform& left, const Transform& right, float epsilon = 0.001f) {
    return closeEnough(left.translate, right.translate, epsilon) &&
           closeEnough(left.scale, right.scale, epsilon) &&
           closeEnough(left.rotate, right.rotate, epsilon) &&
           closeEnough(left.origin, right.origin, epsilon);
}

inline bool closeEnoughSmoothTarget(float left, float right) {
    return closeEnough(left, right, 0.012f);
}

template <typename T>
inline bool closeEnoughSmoothTarget(const T& left, const T& right) {
    return closeEnough(left, right);
}

inline float lerpValue(float from, float to, float amount) {
    return from + (to - from) * amount;
}

inline Vec2 lerpValue(const Vec2& from, const Vec2& to, float amount) {
    return {lerpValue(from.x, to.x, amount), lerpValue(from.y, to.y, amount)};
}

inline Color lerpValue(const Color& from, const Color& to, float amount) {
    return mixColor(from, to, amount);
}

inline Rect lerpValue(const Rect& from, const Rect& to, float amount) {
    return {
        lerpValue(from.x, to.x, amount),
        lerpValue(from.y, to.y, amount),
        lerpValue(from.width, to.width, amount),
        lerpValue(from.height, to.height, amount)
    };
}

inline LayoutRect lerpValue(const LayoutRect& from, const LayoutRect& to, float amount) {
    return {
        lerpValue(from.x, to.x, amount),
        lerpValue(from.y, to.y, amount),
        lerpValue(from.width, to.width, amount),
        lerpValue(from.height, to.height, amount)
    };
}

inline Border lerpValue(const Border& from, const Border& to, float amount) {
    return {lerpValue(from.width, to.width, amount), lerpValue(from.color, to.color, amount)};
}

inline Shadow lerpValue(Shadow from, Shadow to, float amount) {
    if (!from.enabled) {
        from.color.a = 0.0f;
    }
    if (!to.enabled) {
        to.color.a = 0.0f;
    }

    return {
        from.enabled || to.enabled,
        lerpValue(from.offset, to.offset, amount),
        lerpValue(from.blur, to.blur, amount),
        lerpValue(from.spread, to.spread, amount),
        lerpValue(from.color, to.color, amount)
    };
}

inline Transform lerpValue(const Transform& from, const Transform& to, float amount) {
    return {
        lerpValue(from.translate, to.translate, amount),
        lerpValue(from.scale, to.scale, amount),
        lerpValue(from.rotate, to.rotate, amount),
        lerpValue(from.origin, to.origin, amount)
    };
}

template <typename T>
class AnimatedValue {
public:
    bool setTarget(const T& target, const Transition& transition, bool animateProperty = true) {
        if (!initialized_) {
            initialized_ = true;
            start_ = target;
            current_ = target;
            target_ = target;
            active_ = false;
            return true;
        }

        if (closeEnough(target_, target)) {
            return false;
        }

        target_ = target;
        if (!transition.enabled || !animateProperty || transition.durationSeconds <= 0.0f) {
            start_ = target;
            current_ = target;
            active_ = false;
            elapsedSeconds_ = 0.0f;
            return true;
        }

        start_ = current_;
        spec_ = transition;
        elapsedSeconds_ = 0.0f;
        active_ = true;
        return true;
    }

    bool tick(float deltaSeconds) {
        if (!active_) {
            return false;
        }

        const T previous = current_;
        elapsedSeconds_ += std::max(0.0f, deltaSeconds);
        const float localTime = elapsedSeconds_ - spec_.delaySeconds;
        if (localTime <= 0.0f) {
            return false;
        }

        const float duration = std::max(0.0001f, spec_.durationSeconds);
        const float amount = applyEase(spec_.ease, localTime / duration);
        current_ = lerpValue(start_, target_, amount);

        if (localTime >= duration || closeEnough(current_, target_)) {
            current_ = target_;
            active_ = false;
        }

        return !closeEnough(previous, current_);
    }

    const T& value() const {
        return current_;
    }

    bool isActive() const {
        return active_;
    }

private:
    bool initialized_ = false;
    bool active_ = false;
    float elapsedSeconds_ = 0.0f;
    Transition spec_;
    T start_{};
    T current_{};
    T target_{};
};

template <typename T>
class SmoothedValue {
public:
    bool update(const T& target, float speed, float deltaSeconds) {
        if (!initialized_) {
            initialized_ = true;
            current_ = target;
            return true;
        }

        const T previous = current_;
        if (closeEnoughSmoothTarget(current_, target)) {
            current_ = target;
            return !closeEnough(previous, current_);
        }

        if (deltaSeconds <= 0.0f || speed <= 0.0f) {
            current_ = target;
        } else {
            const float amount = 1.0f - std::exp(-std::min(speed, 32.0f) * deltaSeconds);
            current_ = lerpValue(current_, target, amount);
        }

        if (closeEnoughSmoothTarget(current_, target)) {
            current_ = target;
        }

        return !closeEnough(previous, current_);
    }

    const T& value() const {
        return current_;
    }

    bool isMovingTo(const T& target) const {
        return !closeEnoughSmoothTarget(current_, target);
    }

private:
    bool initialized_ = false;
    T current_{};
};

} // namespace core
