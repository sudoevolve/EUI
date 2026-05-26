#pragma once

#include "components/theme.h"
#include "core/dsl.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace components {

struct CheckboxStyle {
    CheckboxStyle() : CheckboxStyle(theme::DarkThemeColors()) {}

    explicit CheckboxStyle(const theme::ThemeColorTokens& tokens) {
        box = tokens.surface;
        boxHover = tokens.surfaceHover;
        checked = tokens.primary;
        checkedHover = theme::buttonHover(tokens, checked);
        checkedPressed = theme::buttonPressed(tokens, checked);
        boxPressed = theme::buttonPressed(tokens, boxHover);
        border = tokens.border;
        mark = theme::color(1.0f, 1.0f, 1.0f, 1.0f);
        text = tokens.text;
        rowHover = theme::withAlpha(tokens.text, tokens.dark ? 0.06f : 0.05f);
        rowPressed = theme::withAlpha(tokens.text, tokens.dark ? 0.10f : 0.08f);
    }

    core::Color box;
    core::Color boxHover;
    core::Color boxPressed;
    core::Color checked;
    core::Color checkedHover;
    core::Color checkedPressed;
    core::Color border;
    core::Color mark;
    core::Color text;
    core::Color rowHover;
    core::Color rowPressed;
    float radius = 6.0f;
};

class CheckboxBuilder {
public:
    CheckboxBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    CheckboxBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    CheckboxBuilder& checked(bool value) { checked_ = value; return *this; }
    CheckboxBuilder& text(std::string value) { text_ = std::move(value); return *this; }
    CheckboxBuilder& fontSize(float value) { fontSize_ = std::max(1.0f, value); return *this; }
    CheckboxBuilder& boxSize(float value) { boxSize_ = std::max(10.0f, value); return *this; }
    CheckboxBuilder& style(const CheckboxStyle& value) { style_ = value; return *this; }
    CheckboxBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = CheckboxStyle(tokens); return *this; }
    CheckboxBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    CheckboxBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }
    CheckboxBuilder& onChange(std::function<void(bool)> callback) { onChange_ = std::move(callback); return *this; }

    void build() {
        const float box = std::min(boxSize_, height_);
        const float boxY = (height_ - box) * 0.5f;
        const float labelX = box + gap_;
        const float labelWidth = std::max(0.0f, width_ - labelX);
        const float labelLineHeight = fontSize_;
        const float labelY = std::max(0.0f, (height_ - labelLineHeight) * 0.5f);
        const float markThickness = std::max(2.0f, box * 0.12f);
        const float markAngleCos = 0.7109f;
        const float markAngleSin = 0.7033f;
        const float markShort = box * 0.28f;
        const float markLong = box * 0.46f;
        const float markHalf = markThickness * 0.5f;
        const float markStartX = box * 0.26f;
        const float markStartY = box * 0.55f;
        const float markCornerX = markStartX + markShort * markAngleCos;
        const float markCornerY = markStartY + markShort * markAngleSin;
        const float markEndX = markCornerX + markLong * markAngleCos;
        const float markEndY = markCornerY - markLong * markAngleSin;
        const core::Vec2 shortNormal{-markAngleSin * markHalf, markAngleCos * markHalf};
        const core::Vec2 longNormal{markAngleSin * markHalf, markAngleCos * markHalf};
        const core::Vec2 outerCorner{markCornerX, markCornerY + markHalf / markAngleCos};
        const core::Vec2 innerCorner{markCornerX, markCornerY - markHalf / markAngleCos};
        const core::Vec2 markStart{markStartX, markStartY};
        const core::Vec2 markEnd{markEndX, markEndY};
        const std::vector<core::Vec2> markShortPoints{
            {markStart.x + shortNormal.x, markStart.y + shortNormal.y},
            outerCorner,
            innerCorner,
            {markStart.x - shortNormal.x, markStart.y - shortNormal.y}
        };
        const std::vector<core::Vec2> markLongPoints{
            outerCorner,
            {markEnd.x + longNormal.x, markEnd.y + longNormal.y},
            {markEnd.x - longNormal.x, markEnd.y - longNormal.y},
            innerCorner
        };
        core::Transition markTransition = transition_;
        markTransition.durationSeconds = 0.12f;
        markTransition.ease = core::Ease::OutCubic;
        const float hitWidth = text_.empty()
            ? box
            : std::min(width_, labelX + textWidth(text_, fontSize_));
        const core::Color idle = checked_ ? style_.checked : style_.box;
        const core::Color hover = checked_ ? style_.checkedHover : style_.boxHover;
        const core::Color pressed = checked_ ? style_.checkedPressed : style_.boxPressed;
        const bool nextChecked = !checked_;
        const std::function<void(bool)> onChange = onChange_;

        ui_.stack(id_)
            .size(width_, height_)
            .content([&] {
                ui_.rect(id_ + ".hit")
                    .size(hitWidth, height_)
                    .states(theme::color(0.0f, 0.0f, 0.0f, 0.0f), style_.rowHover, style_.rowPressed)
                    .radius(std::max(6.0f, height_ * 0.20f))
                    .transition(transition_)
                    .onClick([onChange, nextChecked] {
                        if (onChange) {
                            onChange(nextChecked);
                        }
                    })
                    .build();

                ui_.rect(id_ + ".box")
                    .y(boxY)
                    .size(box, box)
                    .color(idle)
                    .radius(style_.radius)
                    .border(1.5f, checked_ ? style_.checked : style_.border)
                    .transition(transition_)
                    .animate(core::AnimProperty::Color | core::AnimProperty::Border)
                    .build();

                ui_.stack(id_ + ".mark.clip")
                    .y(boxY)
                    .size(box, box)
                    .clip()
                    .content([&] {
                        ui_.polygon(id_ + ".mark.short")
                            .points(markShortPoints)
                            .color(style_.mark)
                            .opacity(checked_ ? 1.0f : 0.0f)
                            .transition(markTransition)
                            .animate(core::AnimProperty::Opacity)
                            .build();

                        ui_.polygon(id_ + ".mark.long")
                            .points(markLongPoints)
                            .color(style_.mark)
                            .opacity(checked_ ? 1.0f : 0.0f)
                            .transition(markTransition)
                            .animate(core::AnimProperty::Opacity)
                            .build();
                    })
                    .build();

                if (!text_.empty()) {
                    ui_.text(id_ + ".label")
                        .x(labelX)
                        .y(labelY)
                        .size(labelWidth, labelLineHeight)
                        .text(text_)
                        .fontSize(fontSize_)
                        .lineHeight(labelLineHeight)
                        .color(style_.text)
                        .verticalAlign(core::VerticalAlign::Top)
                        .build();
                }
            })
            .build();
    }

private:
    static float textWidth(const std::string& value, float fontSize) {
        return core::TextPrimitive::measureTextWidth(value, {}, fontSize, 400);
    }

    core::dsl::Ui& ui_;
    std::string id_;
    CheckboxStyle style_;
    core::Transition transition_ = core::Transition::make(0.16f, core::Ease::OutCubic);
    std::function<void(bool)> onChange_;
    std::string text_;
    bool checked_ = false;
    float width_ = 180.0f;
    float height_ = 28.0f;
    float boxSize_ = 22.0f;
    float gap_ = 10.0f;
    float fontSize_ = 18.0f;
};

inline CheckboxBuilder checkbox(core::dsl::Ui& ui, const std::string& id) {
    return CheckboxBuilder(ui, id);
}

} // namespace components
