using Glyph = std::array<std::uint8_t, 7>;
using Point = std::array<float, 2>;
using TextureId = unsigned int;

struct CachedDrawCommand {
    Rect visible_rect{};
    std::uint64_t hash{0ull};
};

struct RuntimeState {
    std::string text_input{};
    double scroll_y_accum{0.0};
    bool pending_backspace{false};
    bool pending_delete{false};
    bool pending_enter{false};
    bool pending_escape{false};
    bool pending_left{false};
    bool pending_right{false};
    bool pending_up{false};
    bool pending_down{false};
    bool pending_home{false};
    bool pending_end{false};
    bool prev_left_mouse{false};
    bool prev_right_mouse{false};
    bool prev_a_key{false};
    bool prev_c_key{false};
    bool prev_v_key{false};
    bool prev_x_key{false};
    double prev_mouse_x{0.0};
    double prev_mouse_y{0.0};
    bool has_prev_mouse{false};
    int prev_framebuffer_w{0};
    int prev_framebuffer_h{0};

    std::vector<DrawCommand> curr_commands{};
    std::vector<char> curr_text_arena{};
    std::vector<eui::graphics::Brush> curr_brush_payloads{};
    std::vector<eui::graphics::Transform3D> curr_transform_payloads{};
    std::vector<Rect> dirty_regions{};
    std::vector<CachedDrawCommand> prev_commands{};
    std::uint32_t dirty_regions_trim_frames{0u};
    std::uint32_t prev_commands_trim_frames{0u};
    Color prev_bg{};
    std::uint64_t prev_frame_hash{0ull};
    bool has_prev_frame{false};

    TextureId cache_texture{0};
    int cache_w{0};
    int cache_h{0};
    std::uint32_t cache_inactive_frames{0u};
    bool has_cache{false};
    bool quit_requested{false};
};
