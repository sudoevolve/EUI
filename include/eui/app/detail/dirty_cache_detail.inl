inline bool float_eq(float lhs, float rhs, float eps = 1e-5f) {
    return std::fabs(lhs - rhs) <= eps;
}

inline bool color_eq(const Color& lhs, const Color& rhs, float eps = 1e-4f) {
    return float_eq(lhs.r, rhs.r, eps) && float_eq(lhs.g, rhs.g, eps) &&
           float_eq(lhs.b, rhs.b, eps) && float_eq(lhs.a, rhs.a, eps);
}

inline bool rect_eq(const Rect& lhs, const Rect& rhs, float eps = 0.01f) {
    return float_eq(lhs.x, rhs.x, eps) && float_eq(lhs.y, rhs.y, eps) &&
           float_eq(lhs.w, rhs.w, eps) && float_eq(lhs.h, rhs.h, eps);
}

inline bool rect_intersects(const Rect& lhs, const Rect& rhs) {
    if (lhs.w <= 0.0f || lhs.h <= 0.0f || rhs.w <= 0.0f || rhs.h <= 0.0f) {
        return false;
    }
    return lhs.x < rhs.x + rhs.w && lhs.x + lhs.w > rhs.x && lhs.y < rhs.y + rhs.h &&
           lhs.y + lhs.h > rhs.y;
}

inline bool rect_intersection(const Rect& lhs, const Rect& rhs, Rect& out) {
    const float x1 = std::max(lhs.x, rhs.x);
    const float y1 = std::max(lhs.y, rhs.y);
    const float x2 = std::min(lhs.x + lhs.w, rhs.x + rhs.w);
    const float y2 = std::min(lhs.y + lhs.h, rhs.y + rhs.h);
    if (x2 <= x1 || y2 <= y1) {
        out = Rect{};
        return false;
    }
    out = Rect{x1, y1, x2 - x1, y2 - y1};
    return true;
}

inline Rect rect_union(const Rect& lhs, const Rect& rhs) {
    const float x1 = std::min(lhs.x, rhs.x);
    const float y1 = std::min(lhs.y, rhs.y);
    const float x2 = std::max(lhs.x + lhs.w, rhs.x + rhs.w);
    const float y2 = std::max(lhs.y + lhs.h, rhs.y + rhs.h);
    return Rect{x1, y1, std::max(0.0f, x2 - x1), std::max(0.0f, y2 - y1)};
}

inline bool command_payload_equal(const DrawCommand& lhs, const DrawCommand& rhs,
                                  const std::vector<char>& lhs_arena,
                                  const std::vector<char>& rhs_arena) {
    (void)lhs_arena;
    (void)rhs_arena;
    return lhs.hash == rhs.hash;
}

inline Rect command_visible_rect(const DrawCommand& cmd) {
    Rect bounds = projected_rect_bounds(cmd.rect, cmd.transform_3d);
    if (!cmd.has_clip) {
        return bounds;
    }
    Rect visible{};
    if (!rect_intersection(bounds, cmd.clip_rect, visible)) {
        return Rect{};
    }
    return visible;
}

inline Rect expanded_and_clamped(const Rect& rect, int width, int height, float expand_px = 2.0f) {
    Rect out{
        rect.x - expand_px,
        rect.y - expand_px,
        rect.w + expand_px * 2.0f,
        rect.h + expand_px * 2.0f,
    };
    out.x = std::clamp(out.x, 0.0f, static_cast<float>(width));
    out.y = std::clamp(out.y, 0.0f, static_cast<float>(height));
    const float right = std::clamp(out.x + out.w, 0.0f, static_cast<float>(width));
    const float bottom = std::clamp(out.y + out.h, 0.0f, static_cast<float>(height));
    out.w = std::max(0.0f, right - out.x);
    out.h = std::max(0.0f, bottom - out.y);
    return out;
}

inline std::uint64_t hash_frame_payload(const std::vector<DrawCommand>& commands, const Color& bg) {
    std::uint64_t hash = 1469598103934665603ull;
    auto mix = [](std::uint64_t h, std::uint64_t v) {
        h ^= v;
        h *= 1099511628211ull;
        return h;
    };
    auto float_to_u32 = [](float value) {
        std::uint32_t out = 0u;
        std::memcpy(&out, &value, sizeof(out));
        return out;
    };

    hash = mix(hash, static_cast<std::uint64_t>(float_to_u32(bg.r)));
    hash = mix(hash, static_cast<std::uint64_t>(float_to_u32(bg.g)));
    hash = mix(hash, static_cast<std::uint64_t>(float_to_u32(bg.b)));
    hash = mix(hash, static_cast<std::uint64_t>(float_to_u32(bg.a)));
    hash = mix(hash, static_cast<std::uint64_t>(commands.size()));
    for (const DrawCommand& cmd : commands) {
        hash = mix(hash, cmd.hash);
    }
    return hash;
}

inline void append_dirty_rect(const Rect& rect, int width, int height, std::vector<Rect>& out_regions,
                              std::size_t max_regions = 16u) {
    Rect region = expanded_and_clamped(rect, width, height);
    if (region.w <= 0.0f || region.h <= 0.0f) {
        return;
    }

    for (Rect& existing : out_regions) {
        if (rect_intersects(existing, region)) {
            existing = rect_union(existing, region);
            return;
        }
    }

    if (out_regions.size() < max_regions) {
        out_regions.push_back(region);
        return;
    }

    // Region budget exceeded: collapse to one bounding dirty area to cap CPU overhead.
    Rect merged = out_regions.front();
    for (std::size_t i = 1; i < out_regions.size(); ++i) {
        merged = rect_union(merged, out_regions[i]);
    }
    merged = rect_union(merged, region);
    out_regions.clear();
    out_regions.push_back(expanded_and_clamped(merged, width, height));
}

inline void merge_dirty_overlaps(std::vector<Rect>& regions, int width, int height) {
    if (regions.size() < 2u) {
        return;
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (std::size_t i = 0; i < regions.size() && !changed; ++i) {
            for (std::size_t j = i + 1; j < regions.size(); ++j) {
                if (rect_intersects(regions[i], regions[j])) {
                    regions[i] = expanded_and_clamped(rect_union(regions[i], regions[j]), width, height, 0.0f);
                    regions.erase(regions.begin() + static_cast<std::ptrdiff_t>(j));
                    changed = true;
                    break;
                }
            }
        }
    }
}

inline bool compute_dirty_regions(const std::vector<DrawCommand>& commands,
                                  const std::vector<char>& text_arena, const RuntimeState& runtime,
                                  const Color& bg, int width, int height, bool force_full,
                                  std::vector<Rect>& out_regions) {
    (void)text_arena;
    out_regions.clear();
    if (force_full || !runtime.has_prev_frame || !color_eq(bg, runtime.prev_bg)) {
        out_regions.push_back(Rect{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)});
        return true;
    }

    const std::size_t max_count = std::max(commands.size(), runtime.prev_commands.size());
    for (std::size_t i = 0; i < max_count; ++i) {
        const bool has_curr = i < commands.size();
        const bool has_prev = i < runtime.prev_commands.size();

        if (has_curr && has_prev &&
            command_payload_equal(commands[i], runtime.prev_commands[i], text_arena, runtime.prev_text_arena)) {
            continue;
        }

        if (has_curr) {
            append_dirty_rect(command_visible_rect(commands[i]), width, height, out_regions);
        }
        if (has_prev) {
            append_dirty_rect(command_visible_rect(runtime.prev_commands[i]), width, height, out_regions);
        }
    }

    if (out_regions.empty()) {
        return false;
    }

    const std::size_t base_region_count = out_regions.size();
    for (const DrawCommand& cmd : commands) {
        if (cmd.type != CommandType::BackdropBlur) {
            continue;
        }
        const Rect blur_rect = command_visible_rect(cmd);
        if (blur_rect.w <= 0.0f || blur_rect.h <= 0.0f) {
            continue;
        }
        bool overlaps_dirty = false;
        for (std::size_t i = 0; i < base_region_count; ++i) {
            if (rect_intersects(blur_rect, out_regions[i])) {
                overlaps_dirty = true;
                break;
            }
        }
        if (overlaps_dirty) {
            append_dirty_rect(blur_rect, width, height, out_regions);
        }
    }

    merge_dirty_overlaps(out_regions, width, height);
    return true;
}

struct IRect {
    int x{0};
    int y{0};
    int w{0};
    int h{0};
};

inline IRect to_gl_rect(const Rect& rect, int framebuffer_w, int framebuffer_h) {
    int x = static_cast<int>(std::floor(rect.x));
    int y_top = static_cast<int>(std::floor(rect.y));
    int w = static_cast<int>(std::ceil(rect.w));
    int h = static_cast<int>(std::ceil(rect.h));

    x = std::clamp(x, 0, framebuffer_w);
    y_top = std::clamp(y_top, 0, framebuffer_h);
    w = std::clamp(w, 0, framebuffer_w - x);
    h = std::clamp(h, 0, framebuffer_h - y_top);
    int y = framebuffer_h - (y_top + h);
    y = std::clamp(y, 0, framebuffer_h);
    return IRect{x, y, w, h};
}

inline void ensure_cache_texture(RuntimeState& runtime, int width, int height) {
    if (runtime.cache_texture == 0u) {
        glGenTextures(1, &runtime.cache_texture);
    }
    if (runtime.cache_w == width && runtime.cache_h == height && runtime.has_cache) {
        return;
    }
    runtime.cache_w = width;
    runtime.cache_h = height;
    runtime.has_cache = false;

    glBindTexture(GL_TEXTURE_2D, runtime.cache_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

inline void copy_full_to_cache(RuntimeState& runtime, int width, int height) {
    if (runtime.cache_texture == 0u) {
        return;
    }
    glBindTexture(GL_TEXTURE_2D, runtime.cache_texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
    runtime.has_cache = true;
}

inline void copy_region_to_cache(RuntimeState& runtime, const IRect& gl_rect) {
    if (runtime.cache_texture == 0u || gl_rect.w <= 0 || gl_rect.h <= 0) {
        return;
    }
    glBindTexture(GL_TEXTURE_2D, runtime.cache_texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, gl_rect.x, gl_rect.y, gl_rect.x, gl_rect.y, gl_rect.w, gl_rect.h);
    runtime.has_cache = true;
}

inline void draw_cache_texture(const RuntimeState& runtime, int width, int height) {
    if (!runtime.has_cache || runtime.cache_texture == 0u) {
        return;
    }
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_MULTISAMPLE);
    glDisable(GL_SCISSOR_TEST);
    const Color white = rgba(1.0f, 1.0f, 1.0f, 1.0f);
    std::array<ModernGlVertex, 6> verts = {
        modern_gl_vertex(0.0f, 0.0f, white, 0.0f, 1.0f),
        modern_gl_vertex(static_cast<float>(width), 0.0f, white, 1.0f, 1.0f),
        modern_gl_vertex(static_cast<float>(width), static_cast<float>(height), white, 1.0f, 0.0f),
        modern_gl_vertex(0.0f, 0.0f, white, 0.0f, 1.0f),
        modern_gl_vertex(static_cast<float>(width), static_cast<float>(height), white, 1.0f, 0.0f),
        modern_gl_vertex(0.0f, static_cast<float>(height), white, 0.0f, 0.0f),
    };
    modern_gl_draw_vertices(GL_TRIANGLES, verts.data(), verts.size(), width, height, runtime.cache_texture,
                            ModernGlTextureMode::Rgba);
}
