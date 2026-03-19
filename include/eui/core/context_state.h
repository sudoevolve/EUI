#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "eui/core/foundation.h"

namespace eui::detail {

enum class ContextScopeKind {
    Card,
    DropdownBody,
    ScrollArea,
};

struct ContextRowState {
    bool active{false};
    int columns{1};
    int index{0};
    int next_span{1};
    float gap{8.0f};
    float y{0.0f};
    float max_height{0.0f};
};

struct ContextWaterfallState {
    bool active{false};
    int columns{1};
    float gap{8.0f};
    float start_y{0.0f};
    float item_width{0.0f};
    std::vector<float> column_y{};
};

struct ContextFlexRowState {
    bool active{false};
    std::vector<FlexLength> items{};
    std::vector<float> widths{};
    std::vector<float> heights{};
    std::vector<Rect> item_rects{};
    std::vector<std::size_t> cmd_begin{};
    std::vector<std::size_t> cmd_end{};
    int index{0};
    float gap{8.0f};
    float y{0.0f};
    float row_height{0.0f};
    FlexAlign align{FlexAlign::Top};
};

struct ContextLayoutRectState {
    Rect layout_rect{};
    float cursor_y{0.0f};
    Rect last_item_rect{};
    ContextRowState row{};
    ContextFlexRowState flex_row{};
    ContextWaterfallState waterfall{};
};

struct ContextTextAreaState {
    float scroll{0.0f};
    float preferred_x{-1.0f};
    std::uint64_t last_touched_frame{0u};
};

struct ContextScrollAreaState {
    float scroll{0.0f};
    float velocity{0.0f};
    float content_height{0.0f};
    std::uint64_t last_touched_frame{0u};
};

struct ContextMotionState {
    float hover{0.0f};
    float press{0.0f};
    float focus{0.0f};
    float active{0.0f};
    float value{0.0f};
    bool initialized{false};
    bool value_initialized{false};
    std::uint64_t last_touched_frame{0u};
};

inline constexpr std::size_t k_context_invalid_command_index = static_cast<std::size_t>(-1);

struct ContextGlowCommandRange {
    std::size_t outer_cmd_index{k_context_invalid_command_index};
    std::size_t inner_cmd_index{k_context_invalid_command_index};
    float outer_spread{0.0f};
    float inner_spread{0.0f};
};

struct ContextScopeState {
    ContextScopeKind kind{ContextScopeKind::Card};
    float content_x{0.0f};
    float content_width{0.0f};
    Rect layout_rect{};
    float cursor_y_after{0.0f};
    std::size_t fill_cmd_index{0};
    std::size_t outline_cmd_index{0};
    float top_y{0.0f};
    float min_height{0.0f};
    float padding{0.0f};
    bool had_outer_row{false};
    ContextRowState outer_row{};
    bool had_outer_flex_row{false};
    ContextFlexRowState outer_flex_row{};
    ContextWaterfallState outer_waterfall{};
    bool in_waterfall{false};
    int column_index{-1};
    Rect fixed_rect{};
    bool lock_shell_rect{false};
    std::uint64_t scroll_state_id{0u};
    Rect scroll_viewport{};
    float scroll_content_origin_y{0.0f};
    bool pushed_clip{false};
    float reveal{1.0f};
    std::size_t glow_outer_cmd_index{k_context_invalid_command_index};
    std::size_t glow_inner_cmd_index{k_context_invalid_command_index};
    float glow_outer_spread{0.0f};
    float glow_inner_spread{0.0f};
    std::size_t content_cmd_begin{0u};
    bool show_content{true};
};

}  // namespace eui::detail
