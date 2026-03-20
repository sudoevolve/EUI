#ifndef EUI_ENABLE_GLFW_OPENGL_BACKEND
#define EUI_ENABLE_GLFW_OPENGL_BACKEND 1
#endif
#include "EUI.h"

#include <algorithm>
#include <array>
#include <string>

struct MultiScrollbarDemoState {
    bool dark_mode{false};
    int active_filter{0};
    std::array<int, 3> selected_task{{0, 0, 0}};
    float delivery_progress{0.64f};
    float qa_progress{0.51f};
    float release_progress{0.33f};
};

static const std::array<const char*, 18> kFilters = {
    "All Tasks",
    "Today",
    "Needs Design",
    "Needs Copy",
    "Needs QA",
    "Ready To Ship",
    "High Priority",
    "Mobile",
    "Desktop",
    "Dashboard",
    "Billing",
    "Settings",
    "Onboarding",
    "Docs",
    "Accessibility",
    "Animations",
    "Empty States",
    "Archived",
};

static const std::array<const char*, 12> kBacklogTasks = {
    "Refresh dashboard information hierarchy",
    "Reduce empty-state visual noise",
    "Rewrite billing CTA copy",
    "Add keyboard hints to search panel",
    "Refine compact table spacing",
    "Prepare dark theme audit checklist",
    "Document drag-and-drop edge cases",
    "Align chart legend interaction states",
    "Shorten onboarding completion step",
    "Polish workspace loading skeleton",
    "Update release note screenshots",
    "Collect customer onboarding feedback",
};

static const std::array<const char*, 10> kInProgressTasks = {
    "Build segmented analytics summary",
    "Tune scroll momentum for narrow lists",
    "Verify sidebar icon baseline alignment",
    "Add reminder chips to inbox view",
    "Polish pricing card contrast ratios",
    "Refactor panel spacing tokens",
    "Trim verbose helper text on settings",
    "Stress-test multiline editor selection",
    "Add hover states to activity rows",
    "Finalize dashboard spacing pass",
};

static const std::array<const char*, 11> kReviewTasks = {
    "Review board typography scale",
    "Check tablet breakpoints for metrics",
    "Validate scrollbar thumb hit area",
    "QA release candidate build 08",
    "Review localization fallback strings",
    "Confirm card radius consistency",
    "Approve support center empty state",
    "Review progress bar easing curve",
    "Validate focus outlines in dark mode",
    "Check project list wheel behavior",
    "Approve notification center copy",
};

static const std::array<const char*, 9> kLateSections = {
    "Finalize launch notes for the stakeholder review",
    "Cross-check accessibility items after nested scrolling",
    "Review content density for the compact workspace mode",
    "Verify empty states after board filters change",
    "Collect screenshots for the release announcement",
    "Prepare support docs for the new board behavior",
    "Run a final keyboard-navigation smoke test",
    "Archive stale drafts after launch approval",
    "Write follow-up tasks for the next sprint",
};

static const char* kActivityLog =
    "09:10  Design review started for workspace board.\n"
    "09:18  Backlog list expanded with release follow-up tasks.\n"
    "09:26  Product requested a denser compact mode.\n"
    "09:33  QA flagged one missing hover state on filters.\n"
    "09:41  Scroll lane interaction feels stable on wheel input.\n"
    "09:55  Support asked for more visible archived projects entry.\n"
    "10:02  Copy team updated billing card headlines.\n"
    "10:14  Team agreed to keep lane scrollbars always visible.\n"
    "10:26  Animation pass trimmed to reduce unnecessary motion.\n"
    "10:38  Release notes draft moved into review.\n"
    "10:49  Accessibility checklist reached contrast verification.\n"
    "11:05  Demo branch prepared for stakeholder walkthrough.\n"
    "11:18  Final QA pass scheduled after lunch.\n"
    "11:32  Window now shows multiple independent scrollbars.\n";

static const char* kBoardNotes =
    "This example intentionally keeps several scrollbars visible at the same time.\n"
    "\n"
    "Visible at once:\n"
    "- Filters list on the left.\n"
    "- Activity log textarea on the left.\n"
    "- The outer scroll area on the right.\n"
    "- Three inner task lanes inside that outer area.\n"
    "- This notes textarea further down in the outer area.\n"
    "\n"
    "The right side now uses a parent scroll area first, then places multiple inner scroll areas inside it. "
    "The three task lanes use begin_scroll_area/end_scroll_area with slightly different ScrollAreaOptions so the "
    "example also shows scrollbar width, padding, and background choices.\n"
    "\n"
    "Try wheel scrolling the whole right side first, then move into one lane and scroll only that lane. Each "
    "region keeps its own state independently.\n"
    "\n"
    "Quick checks:\n"
    "- Scroll the outer right area until this notes card appears.\n"
    "- Drag the backlog thumb and keep the parent right area untouched.\n"
    "- Move the review column separately and confirm the other lanes keep position.\n"
    "- Toggle theme and make sure scrollbar contrast still reads clearly.\n"
    "- Resize the window and verify scroll offsets remain independent.";

template <std::size_t N>
static void draw_lane(eui::Context& ui, MultiScrollbarDemoState& state, float scale, int lane_index,
                      const char* title, const char* subtitle, float progress, const std::array<const char*, N>& tasks,
                      const char* scroll_id, const eui::Context::ScrollAreaOptions& options) {
    const auto dp = [scale](float value) { return value * scale; };
    const float item_h = dp(34.0f);
    state.selected_task[static_cast<std::size_t>(lane_index)] =
        std::clamp(state.selected_task[static_cast<std::size_t>(lane_index)], 0, static_cast<int>(tasks.size()) - 1);

    ui.begin_card(title, dp(540.0f), dp(10.0f));
    ui.label(subtitle, dp(12.0f), true);
    std::string progress_label = std::string(title) + " Progress";
    ui.progress(progress_label, progress, dp(7.0f));
    ui.spacer(dp(6.0f));

    if (ui.begin_scroll_area(scroll_id, dp(438.0f), options)) {
        for (std::size_t i = 0; i < tasks.size(); ++i) {
            const bool selected = state.selected_task[static_cast<std::size_t>(lane_index)] == static_cast<int>(i);
            std::string label = selected ? u8"\uF0DA  " : u8"\uF105  ";
            label += tasks[i];
            if (ui.button(label, selected ? eui::ButtonStyle::Primary : eui::ButtonStyle::Ghost, item_h)) {
                state.selected_task[static_cast<std::size_t>(lane_index)] = static_cast<int>(i);
            }
            ui.label(selected ? "Selected card in this lane." : "Assigned to the workspace UI team.",
                     dp(11.0f), true);
            ui.spacer(dp(5.0f));
        }
        ui.end_scroll_area();
    }

    ui.end_card();
}

int main() {
    MultiScrollbarDemoState state{};
    eui::demo::AppOptions options{};
    options.title = "EUI Multi Scrollbars Demo";
    options.width = 1420;
    options.height = 900;
    options.vsync = true;
    options.continuous_render = false;
    options.max_fps = 120.0;

    return eui::demo::run(
        [&](eui::demo::FrameContext frame) {
            auto& ui = frame.ui;
            const float scale = std::max(1.0f, frame.dpi_scale);
            const auto dp = [scale](float value) { return value * scale; };

            ui.set_theme_mode(state.dark_mode ? eui::ThemeMode::Dark : eui::ThemeMode::Light);
            ui.set_primary_color(eui::rgba(0.15f, 0.62f, 0.76f, 1.0f));
            ui.set_corner_radius(12.0f * scale);

            const float margin = dp(18.0f);
            const float gap = dp(12.0f);
            const float top = dp(18.0f);
            const float available_h =
                std::max(dp(540.0f), static_cast<float>(frame.framebuffer_h) - top - margin);
            const float sidebar_w = dp(286.0f);
            const float main_x = margin + sidebar_w + gap;
            const float main_w =
                std::max(dp(720.0f), static_cast<float>(frame.framebuffer_w) - main_x - margin);

            ui.begin_panel("", margin, top, sidebar_w, 0.0f, -1.0f);

            ui.begin_card("MULTI SCROLLBARS", 0.0f, dp(12.0f));
            ui.label("The right side now contains a parent scroll area with nested inner areas.", dp(12.0f), true);
            if (ui.button(state.dark_mode ? "Light Theme" : "Dark Theme", eui::ButtonStyle::Secondary, dp(34.0f))) {
                state.dark_mode = !state.dark_mode;
            }
            ui.input_readonly("Visible Scrollbars", "7+", dp(34.0f), false, 1.0f, false);
            ui.end_card();

            eui::Context::ScrollAreaOptions filter_options{};
            filter_options.padding = dp(8.0f);
            filter_options.scrollbar_width = dp(10.0f);
            filter_options.min_thumb_height = dp(24.0f);
            filter_options.wheel_step = dp(40.0f);
            filter_options.overscroll_limit = dp(24.0f);

            ui.begin_card("FILTERS", 0.0f, dp(10.0f));
            ui.label("This list has its own scrollbar.", dp(12.0f), true);
            if (ui.begin_scroll_area("FILTER_LIST_SCROLL", dp(260.0f), filter_options)) {
                for (std::size_t i = 0; i < kFilters.size(); ++i) {
                    const eui::ButtonStyle style =
                        (state.active_filter == static_cast<int>(i)) ? eui::ButtonStyle::Primary
                                                                     : eui::ButtonStyle::Secondary;
                    if (ui.button(kFilters[i], style, dp(33.0f))) {
                        state.active_filter = static_cast<int>(i);
                    }
                }
                ui.end_scroll_area();
            }
            ui.end_card();

            ui.begin_card("ACTIVITY LOG", 0.0f, dp(10.0f));
            ui.label("Readonly textarea with a separate scrollbar.", dp(12.0f), true);
            ui.text_area_readonly("ACTIVITY_TEXTAREA", kActivityLog, dp(250.0f));
            ui.end_card();

            ui.end_panel();

            ui.begin_panel("", main_x, top, main_w, 0.0f, -1.0f);

            eui::Context::ScrollAreaOptions outer_options{};
            outer_options.padding = dp(12.0f);
            outer_options.scrollbar_width = dp(12.0f);
            outer_options.min_thumb_height = dp(30.0f);
            outer_options.wheel_step = dp(46.0f);
            outer_options.overscroll_limit = dp(30.0f);

            eui::Context::ScrollAreaOptions backlog_options{};
            backlog_options.padding = dp(7.0f);
            backlog_options.scrollbar_width = dp(8.0f);
            backlog_options.min_thumb_height = dp(22.0f);
            backlog_options.wheel_step = dp(34.0f);
            backlog_options.draw_background = false;

            eui::Context::ScrollAreaOptions progress_options{};
            progress_options.padding = dp(8.0f);
            progress_options.scrollbar_width = dp(11.0f);
            progress_options.min_thumb_height = dp(26.0f);
            progress_options.wheel_step = dp(38.0f);
            progress_options.overscroll_limit = dp(28.0f);

            eui::Context::ScrollAreaOptions review_options{};
            review_options.padding = dp(10.0f);
            review_options.scrollbar_width = dp(12.0f);
            review_options.min_thumb_height = dp(28.0f);
            review_options.wheel_step = dp(42.0f);
            review_options.overscroll_limit = dp(26.0f);

            if (ui.begin_scroll_area("RIGHT_OUTER_SCROLL", available_h, outer_options)) {
                ui.begin_card("RIGHT OUTER SCROLL AREA", 0.0f, dp(12.0f));
                ui.label("Scroll this whole area first. Inside it, the three lanes below have their own nested "
                         "scrollbars.", dp(12.0f), true);
                ui.begin_row(3, dp(8.0f));
                ui.input_readonly("Active Filter", kFilters[static_cast<std::size_t>(state.active_filter)], dp(34.0f),
                                  false, 1.0f, false);
                ui.input_readonly("Delivery", "64%", dp(34.0f), true, 1.0f, false);
                ui.input_readonly("Release", "33%", dp(34.0f), true, 1.0f, false);
                ui.end_row();
                ui.spacer(dp(4.0f));
                ui.progress("Board Delivery", state.delivery_progress, dp(8.0f));
                ui.progress("Board QA", state.qa_progress, dp(8.0f));
                ui.progress("Board Release", state.release_progress, dp(8.0f));
                ui.end_card();

                ui.begin_card("OUTER OVERFLOW CONTENT", 0.0f, dp(12.0f));
                ui.label("This card exists to make the parent right-side scroll area overflow before the nested "
                         "lanes even start.", dp(12.0f), true);
                ui.button("Pinned Milestone 01  Stakeholder Preview", eui::ButtonStyle::Secondary, dp(34.0f));
                ui.button("Pinned Milestone 02  QA Sweep", eui::ButtonStyle::Secondary, dp(34.0f));
                ui.button("Pinned Milestone 03  Docs Freeze", eui::ButtonStyle::Secondary, dp(34.0f));
                ui.button("Pinned Milestone 04  Launch Approval", eui::ButtonStyle::Secondary, dp(34.0f));
                ui.end_card();

                ui.begin_row(3, dp(8.0f));
                draw_lane(ui, state, scale, 0, "BACKLOG", "Nested scroll area inside the parent right scroll area.",
                          state.delivery_progress, kBacklogTasks, "BACKLOG_SCROLL", backlog_options);
                draw_lane(ui, state, scale, 1, "IN PROGRESS",
                          "Nested scroll area with standard chrome and wider thumb.", state.qa_progress,
                          kInProgressTasks, "IN_PROGRESS_SCROLL", progress_options);
                draw_lane(ui, state, scale, 2, "REVIEW",
                          "Nested scroll area with the largest padding and scrollbar width.",
                          state.release_progress, kReviewTasks, "REVIEW_SCROLL", review_options);
                ui.end_row();

                ui.begin_card("NOTES INSIDE OUTER AREA", 0.0f, dp(12.0f));
                ui.label("This textarea lives lower in the parent right scroll area, so it only becomes visible after "
                         "you scroll down.", dp(12.0f), true);
                ui.text_area_readonly("BOARD_NOTES", kBoardNotes, dp(220.0f));
                ui.end_card();

                ui.begin_card("MORE HIDDEN CONTENT", 0.0f, dp(12.0f));
                ui.label("There is still more content after the nested lanes and notes card.", dp(12.0f), true);
                for (const char* section : kLateSections) {
                    ui.button(section, eui::ButtonStyle::Ghost, dp(34.0f));
                }
                ui.end_card();

                ui.end_scroll_area();
            }

            ui.end_panel();
        },
        options);
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main();
}
#endif
