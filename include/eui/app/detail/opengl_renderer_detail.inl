class Renderer {
public:
    explicit Renderer(const AppOptions& options = {})
#if defined(_WIN32) && !defined(EUI_OPENGL_ES)
        : text_backend_(resolve_text_backend(options))
#else
        : stb_font_renderer_(std::make_unique<StbFontRenderer>(options))
#endif
    {
#if defined(_WIN32) && !defined(EUI_OPENGL_ES)
        if (text_backend_ == AppOptions::TextBackend::Win32) {
            win32_font_renderer_ = std::make_unique<Win32FontRenderer>(options);
        } else {
#if EUI_ENABLE_STB_TRUETYPE
            stb_font_renderer_ = std::make_unique<StbFontRenderer>(options);
#endif
            // Keep native outlines as the last Windows fallback after stb truetype.
            win32_font_renderer_ = std::make_unique<Win32FontRenderer>(options);
        }
#endif
    }

    void release_gl_resources() const {
#if defined(_WIN32) && !defined(EUI_OPENGL_ES)
        if (win32_font_renderer_ != nullptr) {
            win32_font_renderer_->release_gl_resources();
        }
#endif
        if (stb_font_renderer_ != nullptr) {
            stb_font_renderer_->release_gl_resources();
        }
        modern_gl_release_shared_resources();
        if (backdrop_texture_ != 0u) {
            glDeleteTextures(1, &backdrop_texture_);
            backdrop_texture_ = 0u;
            backdrop_texture_w_ = 0;
            backdrop_texture_h_ = 0;
        }
        std::vector<std::uint32_t>{}.swap(spatial_bucket_offsets_);
        std::vector<std::uint32_t>{}.swap(spatial_bucket_cursor_);
        std::vector<std::uint32_t>{}.swap(spatial_bucket_indices_);
        std::vector<std::uint16_t>{}.swap(spatial_marks_);
        std::vector<std::uint32_t>{}.swap(visible_indices_);
        spatial_commands_ptr_ = nullptr;
        spatial_command_count_ = 0u;
        spatial_frame_hash_ = 0ull;
        spatial_width_ = 0;
        spatial_height_ = 0;
        spatial_cols_ = 0;
        spatial_rows_ = 0;
        filled_batch_trim_frames_ = 0u;
        outline_batch_trim_frames_ = 0u;
        blur_batch_trim_frames_ = 0u;
        backdrop_frame_token_ = 0ull;
        backdrop_idle_frames_ = 0u;
        backdrop_used_in_frame_ = false;
        modern_gl_trim_scratch(filled_batch_scratch_, 0u);
        modern_gl_trim_scratch(outline_batch_scratch_, 0u);
        modern_gl_trim_scratch(blur_pass_vertices_scratch_, 0u);
    }

    void render(const std::vector<DrawCommand>& commands, const std::vector<char>& text_arena,
                const std::vector<eui::graphics::Brush>& brush_payloads,
                const std::vector<eui::graphics::Transform3D>& transform_payloads, int width, int height,
                const Rect* clip_rect = nullptr, std::uint64_t frame_hash = 0ull) const {
        glViewport(0, 0, width, height);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_MULTISAMPLE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        const bool has_outer_clip = clip_rect != nullptr;
        const IRect outer_gl_clip = has_outer_clip ? to_gl_rect(*clip_rect, width, height) : IRect{};
        const bool outer_clip_valid = !has_outer_clip || (outer_gl_clip.w > 0 && outer_gl_clip.h > 0);
        if (!outer_clip_valid) {
            return;
        }
        if (stb_font_renderer_ != nullptr) {
            stb_font_renderer_->begin_frame(frame_hash);
        }
        begin_backdrop_frame(frame_hash);

        auto irect_equal = [](const IRect& lhs, const IRect& rhs) {
            return lhs.x == rhs.x && lhs.y == rhs.y && lhs.w == rhs.w && lhs.h == rhs.h;
        };

        bool scissor_enabled = false;
        IRect active_scissor{};
        auto apply_scissor = [&](const IRect* desired) {
            if (desired == nullptr) {
                if (scissor_enabled) {
                    glDisable(GL_SCISSOR_TEST);
                    scissor_enabled = false;
                }
                return;
            }
            if (desired->w <= 0 || desired->h <= 0) {
                if (scissor_enabled) {
                    glDisable(GL_SCISSOR_TEST);
                    scissor_enabled = false;
                }
                return;
            }
            if (!scissor_enabled) {
                glEnable(GL_SCISSOR_TEST);
                glScissor(desired->x, desired->y, desired->w, desired->h);
                active_scissor = *desired;
                scissor_enabled = true;
                return;
            }
            if (!irect_equal(active_scissor, *desired)) {
                glScissor(desired->x, desired->y, desired->w, desired->h);
                active_scissor = *desired;
            }
        };

        visible_indices_.clear();
        if (clip_rect == nullptr || commands.size() < 96u) {
            visible_indices_.reserve(commands.size());
            for (std::size_t i = 0; i < commands.size(); ++i) {
                const Rect visible_rect = command_visible_rect(commands[i]);
                if (clip_rect == nullptr || rect_intersects(visible_rect, *clip_rect)) {
                    visible_indices_.push_back(static_cast<std::uint32_t>(i));
                }
            }
        } else {
            ensure_spatial_index(commands, width, height, frame_hash);

            if (spatial_marks_.size() < commands.size()) {
                spatial_marks_.assign(commands.size(), static_cast<std::uint16_t>(0));
                spatial_mark_id_ = 1u;
            }
            spatial_mark_id_ = static_cast<std::uint16_t>(spatial_mark_id_ + 1u);
            if (spatial_mark_id_ == 0u) {
                std::fill(spatial_marks_.begin(), spatial_marks_.end(), static_cast<std::uint16_t>(0));
                spatial_mark_id_ = 1u;
            }

            visible_indices_.reserve(commands.size() / 4u + 8u);
            const int clip_x0 = std::clamp(static_cast<int>(std::floor(clip_rect->x / k_spatial_tile_px)), 0,
                                           spatial_cols_ - 1);
            const int clip_y0 = std::clamp(static_cast<int>(std::floor(clip_rect->y / k_spatial_tile_px)), 0,
                                           spatial_rows_ - 1);
            const int clip_x1 =
                std::clamp(static_cast<int>(std::floor((clip_rect->x + clip_rect->w) / k_spatial_tile_px)), 0,
                           spatial_cols_ - 1);
            const int clip_y1 =
                std::clamp(static_cast<int>(std::floor((clip_rect->y + clip_rect->h) / k_spatial_tile_px)), 0,
                           spatial_rows_ - 1);
            for (int y = clip_y0; y <= clip_y1; ++y) {
                for (int x = clip_x0; x <= clip_x1; ++x) {
                    const std::size_t bucket_index = static_cast<std::size_t>(y * spatial_cols_ + x);
                    const std::uint32_t begin = spatial_bucket_offsets_[bucket_index];
                    const std::uint32_t end = spatial_bucket_offsets_[bucket_index + 1u];
                    for (std::uint32_t bucket_pos = begin; bucket_pos < end; ++bucket_pos) {
                        const std::uint32_t idx = spatial_bucket_indices_[bucket_pos];
                        if (spatial_marks_[idx] == spatial_mark_id_) {
                            continue;
                        }
                        if (!rect_intersects(command_visible_rect(commands[static_cast<std::size_t>(idx)]), *clip_rect)) {
                            continue;
                        }
                        spatial_marks_[idx] = spatial_mark_id_;
                        visible_indices_.push_back(idx);
                    }
                }
            }
            std::sort(visible_indices_.begin(), visible_indices_.end());
        }

        apply_scissor(nullptr);

        enum class PendingBatch {
            None,
            Filled,
            Outline,
        };
        PendingBatch pending_batch = PendingBatch::None;
        float pending_outline_thickness = 1.0f;
        const bool use_triangle_strokes = modern_gl_context_is_gles();
        auto& filled_batch = filled_batch_scratch_;
        auto& outline_batch = outline_batch_scratch_;
        modern_gl_prepare_scratch(filled_batch, 1024u);
        modern_gl_prepare_scratch(outline_batch, 1024u);
        std::size_t filled_batch_peak = 0u;
        std::size_t outline_batch_peak = 0u;
        std::size_t blur_pass_peak = 0u;

        auto push_colored_vertex = [](std::vector<ModernGlVertex>& batch, float x, float y, const Color& color) {
            batch.push_back(modern_gl_vertex(x, y, color));
        };

        auto sample_stops = [](const auto& stops, std::size_t stop_count, float t) -> Color {
            if (stop_count == 0u) {
                return Color{};
            }
            const float clamped = std::clamp(t, 0.0f, 1.0f);
            if (stop_count == 1u || clamped <= stops[0].position) {
                return to_legacy_color(stops[0].color);
            }
            for (std::size_t i = 1u; i < stop_count; ++i) {
                if (clamped <= stops[i].position) {
                    const float span = std::max(1e-6f, stops[i].position - stops[i - 1u].position);
                    const float local_t = std::clamp((clamped - stops[i - 1u].position) / span, 0.0f, 1.0f);
                    return mix(to_legacy_color(stops[i - 1u].color), to_legacy_color(stops[i].color), local_t);
                }
            }
            return to_legacy_color(stops[stop_count - 1u].color);
        };

        auto resolve_brush = [&](const DrawCommand& cmd) -> const eui::graphics::Brush* {
            if (cmd.brush_payload_index == DrawCommand::k_invalid_payload_index ||
                static_cast<std::size_t>(cmd.brush_payload_index) >= brush_payloads.size()) {
                return nullptr;
            }
            return &brush_payloads[cmd.brush_payload_index];
        };

        auto resolve_transform = [&](const DrawCommand& cmd) -> const eui::graphics::Transform3D& {
            static const eui::graphics::Transform3D identity{};
            if (cmd.transform_payload_index == DrawCommand::k_invalid_payload_index ||
                static_cast<std::size_t>(cmd.transform_payload_index) >= transform_payloads.size()) {
                return identity;
            }
            return transform_payloads[cmd.transform_payload_index];
        };

        auto sample_brush = [&](const DrawCommand& cmd, float x, float y) -> Color {
            const eui::graphics::Brush* brush = resolve_brush(cmd);
            if (brush == nullptr || brush->kind == eui::graphics::BrushKind::none) {
                return cmd.color;
            }
            const float u = cmd.rect.w > 1e-6f ? std::clamp((x - cmd.rect.x) / cmd.rect.w, 0.0f, 1.0f) : 0.0f;
            const float v = cmd.rect.h > 1e-6f ? std::clamp((y - cmd.rect.y) / cmd.rect.h, 0.0f, 1.0f) : 0.0f;
            switch (brush->kind) {
                case eui::graphics::BrushKind::solid:
                    return to_legacy_color(brush->solid);
                case eui::graphics::BrushKind::linear_gradient: {
                    const float dx = brush->linear.end.x - brush->linear.start.x;
                    const float dy = brush->linear.end.y - brush->linear.start.y;
                    const float denom = dx * dx + dy * dy;
                    const float t =
                        denom > 1e-6f
                            ? (((u - brush->linear.start.x) * dx + (v - brush->linear.start.y) * dy) / denom)
                                      : 0.0f;
                    return sample_stops(brush->linear.stops, brush->linear.stop_count, t);
                }
                case eui::graphics::BrushKind::radial_gradient: {
                    const float radius = std::max(1e-6f, brush->radial.radius);
                    const float dx = u - brush->radial.center.x;
                    const float dy = v - brush->radial.center.y;
                    const float t = std::sqrt(dx * dx + dy * dy) / radius;
                    return sample_stops(brush->radial.stops, brush->radial.stop_count, t);
                }
                case eui::graphics::BrushKind::none:
                default:
                    return cmd.color;
            }
        };

        auto project_vertex = [&](const DrawCommand& cmd, float x, float y) -> Point {
            const ProjectedPoint projected = project_rect_point_3d(x, y, cmd.rect, resolve_transform(cmd));
            return Point{projected.x, projected.y};
        };

        auto push_stroke_segment = [&](std::vector<ModernGlVertex>& batch, const Point& p0, const Point& p1,
                                       const Color& color, float thickness) {
            const float dx = p1[0] - p0[0];
            const float dy = p1[1] - p0[1];
            const float len_sq = dx * dx + dy * dy;
            if (len_sq <= 1e-6f) {
                return;
            }
            const float inv_len = 1.0f / std::sqrt(len_sq);
            const float half_t = std::max(0.5f, thickness * 0.5f);
            const float nx = -dy * inv_len * half_t;
            const float ny = dx * inv_len * half_t;
            const ModernGlVertex v0 = modern_gl_vertex(p0[0] - nx, p0[1] - ny, color);
            const ModernGlVertex v1 = modern_gl_vertex(p0[0] + nx, p0[1] + ny, color);
            const ModernGlVertex v2 = modern_gl_vertex(p1[0] + nx, p1[1] + ny, color);
            const ModernGlVertex v3 = modern_gl_vertex(p1[0] - nx, p1[1] - ny, color);
            modern_gl_push_quad(batch, v0, v1, v2, v3);
        };

        auto draw_backdrop_blur = [&](const DrawCommand& cmd) {
            Rect bounds = projected_rect_bounds(cmd.rect, resolve_transform(cmd));
            if (cmd.has_clip) {
                Rect clipped{};
                if (!rect_intersection(bounds, cmd.clip_rect, clipped)) {
                    return;
                }
                bounds = clipped;
            }
            if (bounds.w <= 0.0f || bounds.h <= 0.0f || cmd.blur_radius <= 1e-4f || cmd.effect_alpha <= 1e-4f) {
                return;
            }

            const float pad = std::max(2.0f, cmd.blur_radius * 1.45f);
            const int src_x = std::max(0, static_cast<int>(std::floor(bounds.x - pad)));
            const int src_y = std::max(0, static_cast<int>(std::floor(bounds.y - pad)));
            const int src_right = std::min(width, static_cast<int>(std::ceil(bounds.x + bounds.w + pad)));
            const int src_bottom = std::min(height, static_cast<int>(std::ceil(bounds.y + bounds.h + pad)));
            const int src_w = src_right - src_x;
            const int src_h = src_bottom - src_y;
            if (src_w <= 1 || src_h <= 1) {
                return;
            }

            backdrop_used_in_frame_ = true;
            ensure_backdrop_texture(src_w, src_h);
            glBindTexture(GL_TEXTURE_2D, backdrop_texture_);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, src_x, height - src_bottom, src_w, src_h);

            const float src_left = static_cast<float>(src_x);
            const float src_top = static_cast<float>(src_y);
            const float src_wf = static_cast<float>(src_w);
            const float src_hf = static_cast<float>(src_h);
            const bool rounded = cmd.radius > 0.0f;
            std::array<Point, 64> polygon{};
            int polygon_count = 0;
            Point center{};

            if (!rounded) {
                polygon[0] = project_vertex(cmd, cmd.rect.x, cmd.rect.y);
                polygon[1] = project_vertex(cmd, cmd.rect.x + cmd.rect.w, cmd.rect.y);
                polygon[2] = project_vertex(cmd, cmd.rect.x + cmd.rect.w, cmd.rect.y + cmd.rect.h);
                polygon[3] = project_vertex(cmd, cmd.rect.x, cmd.rect.y + cmd.rect.h);
                polygon_count = 4;
            } else {
                std::array<Point, 64> boundary{};
                polygon_count =
                    build_rounded_points(cmd.rect, cmd.radius, boundary.data(), static_cast<int>(boundary.size()));
                if (polygon_count < 3) {
                    return;
                }
                for (int i = 0; i < polygon_count; ++i) {
                    polygon[static_cast<std::size_t>(i)] =
                        project_vertex(cmd, boundary[static_cast<std::size_t>(i)][0],
                                       boundary[static_cast<std::size_t>(i)][1]);
                }
                center = project_vertex(cmd, cmd.rect.x + cmd.rect.w * 0.5f, cmd.rect.y + cmd.rect.h * 0.5f);
            }

            auto emit_tex_vertex = [&](std::vector<ModernGlVertex>& out_vertices, const Point& screen_point,
                                       const Color& color) {
                const float sample_x = std::clamp(screen_point[0], bounds.x, bounds.x + bounds.w);
                const float sample_y = std::clamp(screen_point[1], bounds.y, bounds.y + bounds.h);
                const float u = std::clamp((sample_x - src_left) / src_wf, 0.0f, 1.0f);
                const float v = std::clamp(1.0f - ((sample_y - src_top) / src_hf), 0.0f, 1.0f);
                out_vertices.push_back(modern_gl_vertex(screen_point[0], screen_point[1], color, u, v));
            };

            const Color pass_color = rgba(1.0f, 1.0f, 1.0f, 1.0f);
            auto& pass_vertices = blur_pass_vertices_scratch_;
            modern_gl_prepare_scratch(pass_vertices, static_cast<std::size_t>(std::max(6, polygon_count * 3)));
            if (!rounded) {
                emit_tex_vertex(pass_vertices, polygon[0], pass_color);
                emit_tex_vertex(pass_vertices, polygon[1], pass_color);
                emit_tex_vertex(pass_vertices, polygon[2], pass_color);
                emit_tex_vertex(pass_vertices, polygon[0], pass_color);
                emit_tex_vertex(pass_vertices, polygon[2], pass_color);
                emit_tex_vertex(pass_vertices, polygon[3], pass_color);
            } else {
                for (int i = 0; i < polygon_count; ++i) {
                    const int next = (i + 1) % polygon_count;
                    emit_tex_vertex(pass_vertices, center, pass_color);
                    emit_tex_vertex(pass_vertices, polygon[static_cast<std::size_t>(i)], pass_color);
                    emit_tex_vertex(pass_vertices, polygon[static_cast<std::size_t>(next)], pass_color);
                }
            }
            blur_pass_peak = std::max(blur_pass_peak, pass_vertices.size());

            const float u0 = std::clamp((bounds.x - src_left) / src_wf, 0.0f, 1.0f);
            const float u1 = std::clamp((bounds.x + bounds.w - src_left) / src_wf, 0.0f, 1.0f);
            const float v0 = std::clamp(1.0f - ((bounds.y + bounds.h - src_top) / src_hf), 0.0f, 1.0f);
            const float v1 = std::clamp(1.0f - ((bounds.y - src_top) / src_hf), 0.0f, 1.0f);
            const float uv_pad_x = 0.5f / std::max(1.0f, src_wf);
            const float uv_pad_y = 0.5f / std::max(1.0f, src_hf);
            const float uv_min_x = std::clamp(std::min(u0, u1) + uv_pad_x, 0.0f, 1.0f);
            const float uv_max_x = std::clamp(std::max(u0, u1) - uv_pad_x, 0.0f, 1.0f);
            const float uv_min_y = std::clamp(std::min(v0, v1) + uv_pad_y, 0.0f, 1.0f);
            const float uv_max_y = std::clamp(std::max(v0, v1) - uv_pad_y, 0.0f, 1.0f);
            const float blur_amount = std::clamp(cmd.blur_radius / std::max(1.0f, std::max(src_wf, src_hf)),
                                                 0.0015f, 0.08f);
            const float mix_amount = std::clamp(cmd.effect_alpha, 0.0f, 1.0f);

            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            modern_gl_draw_backdrop_blur_vertices(pass_vertices.data(), pass_vertices.size(), width, height,
                                                  backdrop_texture_, blur_amount, mix_amount, uv_min_x, uv_min_y,
                                                  uv_max_x, uv_max_y);
        };

        auto append_transformed_brush_mesh = [&](const DrawCommand& cmd) -> bool {
            const eui::graphics::Brush* brush = resolve_brush(cmd);
            const eui::graphics::Transform3D& transform = resolve_transform(cmd);
            const bool has_projected_gradient =
                brush != nullptr && !transform_3d_is_identity(transform) &&
                (brush->kind == eui::graphics::BrushKind::linear_gradient ||
                 brush->kind == eui::graphics::BrushKind::radial_gradient);
            if (!has_projected_gradient) {
                return false;
            }

            std::array<Point, 64> boundary{};
            const int count = build_rounded_points(cmd.rect, cmd.radius, boundary.data(),
                                                   static_cast<int>(boundary.size()));
            if (count < 3) {
                return false;
            }

            const float center_src_x = cmd.rect.x + cmd.rect.w * 0.5f;
            const float center_src_y = cmd.rect.y + cmd.rect.h * 0.5f;
            const Point center_src{center_src_x, center_src_y};
            const Point center = project_vertex(cmd, center_src_x, center_src_y);
            const Color center_color = sample_brush(cmd, center_src_x, center_src_y);
            const float size_hint = std::max(cmd.rect.w, cmd.rect.h);
            const float rotation_hint =
                std::fabs(transform.rotation_x_deg) + std::fabs(transform.rotation_y_deg);
            const int ring_count =
                std::clamp(static_cast<int>(size_hint / 28.0f) + static_cast<int>(rotation_hint / 10.0f) + 4, 6, 18);

            auto interpolate_source = [&](const Point& edge, float t) -> Point {
                return Point{
                    center_src[0] + (edge[0] - center_src[0]) * t,
                    center_src[1] + (edge[1] - center_src[1]) * t,
                };
            };

            for (int ring = 1; ring <= ring_count; ++ring) {
                const float inner_t = static_cast<float>(ring - 1) / static_cast<float>(ring_count);
                const float outer_t = static_cast<float>(ring) / static_cast<float>(ring_count);

                for (int i = 0; i < count; ++i) {
                    const int next = (i + 1) % count;
                    const Point outer_src0 = interpolate_source(boundary[static_cast<std::size_t>(i)], outer_t);
                    const Point outer_src1 = interpolate_source(boundary[static_cast<std::size_t>(next)], outer_t);
                    const Point outer0 = project_vertex(cmd, outer_src0[0], outer_src0[1]);
                    const Point outer1 = project_vertex(cmd, outer_src1[0], outer_src1[1]);
                    const Color outer_c0 = sample_brush(cmd, outer_src0[0], outer_src0[1]);
                    const Color outer_c1 = sample_brush(cmd, outer_src1[0], outer_src1[1]);

                    if (ring == 1) {
                        push_colored_vertex(filled_batch, center[0], center[1], center_color);
                        push_colored_vertex(filled_batch, outer0[0], outer0[1], outer_c0);
                        push_colored_vertex(filled_batch, outer1[0], outer1[1], outer_c1);
                        continue;
                    }

                    const Point inner_src0 = interpolate_source(boundary[static_cast<std::size_t>(i)], inner_t);
                    const Point inner_src1 = interpolate_source(boundary[static_cast<std::size_t>(next)], inner_t);
                    const Point inner0 = project_vertex(cmd, inner_src0[0], inner_src0[1]);
                    const Point inner1 = project_vertex(cmd, inner_src1[0], inner_src1[1]);
                    const Color inner_c0 = sample_brush(cmd, inner_src0[0], inner_src0[1]);
                    const Color inner_c1 = sample_brush(cmd, inner_src1[0], inner_src1[1]);

                    push_colored_vertex(filled_batch, inner0[0], inner0[1], inner_c0);
                    push_colored_vertex(filled_batch, outer0[0], outer0[1], outer_c0);
                    push_colored_vertex(filled_batch, outer1[0], outer1[1], outer_c1);
                    push_colored_vertex(filled_batch, inner0[0], inner0[1], inner_c0);
                    push_colored_vertex(filled_batch, outer1[0], outer1[1], outer_c1);
                    push_colored_vertex(filled_batch, inner1[0], inner1[1], inner_c1);
                }
            }

            return true;
        };

        auto append_filled_rect = [&](const DrawCommand& cmd) {
            if (append_transformed_brush_mesh(cmd)) {
                return;
            }

            if (cmd.radius <= 0.0f) {
                const float x0 = cmd.rect.x;
                const float y0 = cmd.rect.y;
                const float x1 = cmd.rect.x + cmd.rect.w;
                const float y1 = cmd.rect.y + cmd.rect.h;
                const Point p0 = project_vertex(cmd, x0, y0);
                const Point p1 = project_vertex(cmd, x1, y0);
                const Point p2 = project_vertex(cmd, x1, y1);
                const Point p3 = project_vertex(cmd, x0, y1);
                const Color c0 = sample_brush(cmd, x0, y0);
                const Color c1 = sample_brush(cmd, x1, y0);
                const Color c2 = sample_brush(cmd, x1, y1);
                const Color c3 = sample_brush(cmd, x0, y1);
                push_colored_vertex(filled_batch, p0[0], p0[1], c0);
                push_colored_vertex(filled_batch, p1[0], p1[1], c1);
                push_colored_vertex(filled_batch, p2[0], p2[1], c2);
                push_colored_vertex(filled_batch, p0[0], p0[1], c0);
                push_colored_vertex(filled_batch, p2[0], p2[1], c2);
                push_colored_vertex(filled_batch, p3[0], p3[1], c3);
                return;
            }

            std::array<Point, 64> points{};
            const int count = build_rounded_points(cmd.rect, cmd.radius, points.data(),
                                                   static_cast<int>(points.size()));
            if (count < 3) {
                return;
            }
            const float center_src_x = cmd.rect.x + cmd.rect.w * 0.5f;
            const float center_src_y = cmd.rect.y + cmd.rect.h * 0.5f;
            const Point center = project_vertex(cmd, center_src_x, center_src_y);
            const Color center_color = sample_brush(cmd, center_src_x, center_src_y);

            for (int i = 0; i < count; ++i) {
                const int next = (i + 1) % count;
                const float src_x0 = points[static_cast<std::size_t>(i)][0];
                const float src_y0 = points[static_cast<std::size_t>(i)][1];
                const float src_x1 = points[static_cast<std::size_t>(next)][0];
                const float src_y1 = points[static_cast<std::size_t>(next)][1];
                const Point p0 = project_vertex(cmd, src_x0, src_y0);
                const Point p1 = project_vertex(cmd, src_x1, src_y1);
                push_colored_vertex(filled_batch, center[0], center[1], center_color);
                push_colored_vertex(filled_batch, p0[0], p0[1], sample_brush(cmd, src_x0, src_y0));
                push_colored_vertex(filled_batch, p1[0], p1[1], sample_brush(cmd, src_x1, src_y1));
            }
        };

        auto append_outline_rect = [&](const DrawCommand& cmd) {
            if (cmd.radius <= 0.0f) {
                const float x0 = cmd.rect.x;
                const float y0 = cmd.rect.y;
                const float x1 = cmd.rect.x + cmd.rect.w;
                const float y1 = cmd.rect.y + cmd.rect.h;
                const Point p0 = project_vertex(cmd, x0, y0);
                const Point p1 = project_vertex(cmd, x1, y0);
                const Point p2 = project_vertex(cmd, x1, y1);
                const Point p3 = project_vertex(cmd, x0, y1);
                if (use_triangle_strokes) {
                    push_stroke_segment(outline_batch, p0, p1, cmd.color, pending_outline_thickness);
                    push_stroke_segment(outline_batch, p1, p2, cmd.color, pending_outline_thickness);
                    push_stroke_segment(outline_batch, p2, p3, cmd.color, pending_outline_thickness);
                    push_stroke_segment(outline_batch, p3, p0, cmd.color, pending_outline_thickness);
                } else {
                    push_colored_vertex(outline_batch, p0[0], p0[1], cmd.color);
                    push_colored_vertex(outline_batch, p1[0], p1[1], cmd.color);
                    push_colored_vertex(outline_batch, p1[0], p1[1], cmd.color);
                    push_colored_vertex(outline_batch, p2[0], p2[1], cmd.color);
                    push_colored_vertex(outline_batch, p2[0], p2[1], cmd.color);
                    push_colored_vertex(outline_batch, p3[0], p3[1], cmd.color);
                    push_colored_vertex(outline_batch, p3[0], p3[1], cmd.color);
                    push_colored_vertex(outline_batch, p0[0], p0[1], cmd.color);
                }
                return;
            }

            std::array<Point, 64> points{};
            const int count = build_rounded_points(cmd.rect, cmd.radius, points.data(),
                                                   static_cast<int>(points.size()));
            if (count < 3) {
                return;
            }
            for (int i = 0; i < count; ++i) {
                const int next = (i + 1) % count;
                const Point p0 = project_vertex(cmd, points[static_cast<std::size_t>(i)][0],
                                                points[static_cast<std::size_t>(i)][1]);
                const Point p1 = project_vertex(cmd, points[static_cast<std::size_t>(next)][0],
                                                points[static_cast<std::size_t>(next)][1]);
                if (use_triangle_strokes) {
                    push_stroke_segment(outline_batch, p0, p1, cmd.color, pending_outline_thickness);
                } else {
                    push_colored_vertex(outline_batch, p0[0], p0[1], cmd.color);
                    push_colored_vertex(outline_batch, p1[0], p1[1], cmd.color);
                }
            }
        };

        auto append_chevron = [&](const DrawCommand& cmd) {
            const float cx = cmd.rect.x + cmd.rect.w * 0.5f;
            const float cy = cmd.rect.y + cmd.rect.h * 0.5f;
            const float half_w = std::max(2.0f, cmd.rect.w * 0.34f);
            const float half_h = std::max(2.0f, cmd.rect.h * 0.28f);
            const float cos_a = std::cos(cmd.rotation);
            const float sin_a = std::sin(cmd.rotation);
            auto rotate_point = [&](float x, float y) -> Point {
                return Point{
                    cx + x * cos_a - y * sin_a,
                    cy + x * sin_a + y * cos_a,
                };
            };
            const Point p0 = rotate_point(-half_w, -half_h);
            const Point p1 = rotate_point(half_w, 0.0f);
            const Point p2 = rotate_point(-half_w, half_h);
            if (use_triangle_strokes) {
                push_stroke_segment(outline_batch, p0, p1, cmd.color, pending_outline_thickness);
                push_stroke_segment(outline_batch, p1, p2, cmd.color, pending_outline_thickness);
            } else {
                push_colored_vertex(outline_batch, p0[0], p0[1], cmd.color);
                push_colored_vertex(outline_batch, p1[0], p1[1], cmd.color);
                push_colored_vertex(outline_batch, p1[0], p1[1], cmd.color);
                push_colored_vertex(outline_batch, p2[0], p2[1], cmd.color);
            }
        };

        auto flush_filled_batch = [&]() {
            if (filled_batch.empty()) {
                return;
            }
            filled_batch_peak = std::max(filled_batch_peak, filled_batch.size());
            modern_gl_draw_vertices(GL_TRIANGLES, filled_batch.data(), filled_batch.size(), width, height);
            filled_batch.clear();
        };

        auto flush_outline_batch = [&]() {
            if (outline_batch.empty()) {
                return;
            }
            outline_batch_peak = std::max(outline_batch_peak, outline_batch.size());
            if (use_triangle_strokes) {
                modern_gl_draw_vertices(GL_TRIANGLES, outline_batch.data(), outline_batch.size(), width, height);
            } else {
                glLineWidth(std::max(1.0f, pending_outline_thickness));
                modern_gl_draw_vertices(GL_LINES, outline_batch.data(), outline_batch.size(), width, height);
                glLineWidth(1.0f);
            }
            outline_batch.clear();
        };

        auto flush_pending_batch = [&]() {
            if (pending_batch == PendingBatch::Filled) {
                flush_filled_batch();
            } else if (pending_batch == PendingBatch::Outline) {
                flush_outline_batch();
            }
            pending_batch = PendingBatch::None;
        };

        auto scissor_matches = [&](const IRect* desired) {
            if (desired == nullptr) {
                return !scissor_enabled;
            }
            if (!scissor_enabled) {
                return false;
            }
            return irect_equal(active_scissor, *desired);
        };

        for (const std::uint32_t draw_idx : visible_indices_) {
            const DrawCommand& cmd = commands[static_cast<std::size_t>(draw_idx)];
            const Rect visible_rect = command_visible_rect(cmd);
            if (clip_rect != nullptr && !rect_intersects(visible_rect, *clip_rect)) {
                continue;
            }
            IRect cmd_gl_clip{};
            const IRect* desired_clip = nullptr;
            if (cmd.has_clip) {
                Rect effective_clip = cmd.clip_rect;
                if (has_outer_clip) {
                    Rect clipped{};
                    if (!rect_intersection(effective_clip, *clip_rect, clipped)) {
                        continue;
                    }
                    effective_clip = clipped;
                }
                cmd_gl_clip = to_gl_rect(effective_clip, width, height);
                if (cmd_gl_clip.w <= 0 || cmd_gl_clip.h <= 0) {
                    continue;
                }
                desired_clip = &cmd_gl_clip;
            } else if (has_outer_clip) {
                desired_clip = &outer_gl_clip;
            }

            switch (cmd.type) {
                case CommandType::FilledRect:
                    if (!scissor_matches(desired_clip)) {
                        flush_pending_batch();
                        apply_scissor(desired_clip);
                    }
                    if (pending_batch != PendingBatch::Filled) {
                        flush_pending_batch();
                        pending_batch = PendingBatch::Filled;
                    }
                    append_filled_rect(cmd);
                    break;
                case CommandType::RectOutline:
                    if (!scissor_matches(desired_clip)) {
                        flush_pending_batch();
                        apply_scissor(desired_clip);
                    }
                    if (pending_batch != PendingBatch::Outline ||
                        !float_eq(pending_outline_thickness, std::max(1.0f, cmd.thickness), 1e-3f)) {
                        flush_pending_batch();
                        pending_batch = PendingBatch::Outline;
                        pending_outline_thickness = std::max(1.0f, cmd.thickness);
                    }
                    append_outline_rect(cmd);
                    break;
                case CommandType::BackdropBlur:
                    flush_pending_batch();
                    apply_scissor(desired_clip);
                    draw_backdrop_blur(cmd);
                    break;
                case CommandType::Chevron:
                    if (!scissor_matches(desired_clip)) {
                        flush_pending_batch();
                        apply_scissor(desired_clip);
                    }
                    if (pending_batch != PendingBatch::Outline ||
                        !float_eq(pending_outline_thickness, std::max(1.0f, cmd.thickness), 1e-3f)) {
                        flush_pending_batch();
                        pending_batch = PendingBatch::Outline;
                        pending_outline_thickness = std::max(1.0f, cmd.thickness);
                    }
                    append_chevron(cmd);
                    break;
                case CommandType::Text: {
                    flush_pending_batch();
                    apply_scissor(desired_clip);
                    std::string_view text{};
                    const std::size_t offset = static_cast<std::size_t>(cmd.text_offset);
                    const std::size_t length = static_cast<std::size_t>(cmd.text_length);
                    if (offset + length <= text_arena.size()) {
                        text = std::string_view(text_arena.data() + offset, length);
                    }

#if defined(_WIN32) && !defined(EUI_OPENGL_ES)
                    bool rendered = false;
                    if (text_backend_ != AppOptions::TextBackend::Win32 && stb_font_renderer_ != nullptr) {
                        rendered = stb_font_renderer_->draw_text(cmd, text);
                    }
                    if (!rendered && text_backend_ == AppOptions::TextBackend::Win32 &&
                        win32_font_renderer_ != nullptr) {
                        glMatrixMode(GL_PROJECTION);
                        glLoadIdentity();
                        glOrtho(0.0, static_cast<double>(width), static_cast<double>(height), 0.0, -1.0, 1.0);
                        glMatrixMode(GL_MODELVIEW);
                        glLoadIdentity();
                        rendered = win32_font_renderer_->draw_text(cmd, text);
                    }
                    if (!rendered) {
                        draw_text_bitmap(cmd, text);
                    }
#else
                    if (stb_font_renderer_ == nullptr || !stb_font_renderer_->draw_text(cmd, text)) {
                        draw_text_bitmap(cmd, text);
                    }
#endif
                    break;
                }
            }
        }

        flush_pending_batch();
        apply_scissor(nullptr);
        const std::size_t filled_keep = std::max<std::size_t>(4096u, filled_batch_peak + filled_batch_peak / 2u + 64u);
        const std::size_t outline_keep =
            std::max<std::size_t>(4096u, outline_batch_peak + outline_batch_peak / 2u + 64u);
        const std::size_t blur_keep = std::max<std::size_t>(2048u, blur_pass_peak + blur_pass_peak / 2u + 32u);
        eui::detail::context_retain_vector_hysteresis(filled_batch_scratch_, filled_keep, filled_batch_trim_frames_,
                                                      120u);
        eui::detail::context_retain_vector_hysteresis(outline_batch_scratch_, outline_keep,
                                                      outline_batch_trim_frames_, 120u);
        eui::detail::context_retain_vector_hysteresis(blur_pass_vertices_scratch_, blur_keep, blur_batch_trim_frames_,
                                                      120u);
    }

#if defined(_WIN32) && !defined(EUI_OPENGL_ES)
    static AppOptions::TextBackend resolve_text_backend(const AppOptions& options) {
        using TextBackend = AppOptions::TextBackend;
        if (options.text_backend == TextBackend::Win32) {
            return TextBackend::Win32;
        }
        if (options.text_backend == TextBackend::Stb) {
#if EUI_ENABLE_STB_TRUETYPE
            return TextBackend::Stb;
#else
            return TextBackend::Win32;
#endif
        }
#if EUI_ENABLE_STB_TRUETYPE
        return TextBackend::Stb;
#else
        return TextBackend::Win32;
#endif
    }
#endif

#if defined(_WIN32) && !defined(EUI_OPENGL_ES)
private:
    AppOptions::TextBackend text_backend_{AppOptions::TextBackend::Auto};
    mutable std::unique_ptr<Win32FontRenderer> win32_font_renderer_{};
#endif
private:
    mutable std::unique_ptr<StbFontRenderer> stb_font_renderer_{};
    static constexpr int k_spatial_tile_px = 128;
    mutable std::vector<std::uint32_t> spatial_bucket_offsets_{};
    mutable std::vector<std::uint32_t> spatial_bucket_cursor_{};
    mutable std::vector<std::uint32_t> spatial_bucket_indices_{};
    mutable std::vector<std::uint16_t> spatial_marks_{};
    mutable std::vector<std::uint32_t> visible_indices_{};
    mutable std::uint16_t spatial_mark_id_{1u};
    mutable const DrawCommand* spatial_commands_ptr_{nullptr};
    mutable std::size_t spatial_command_count_{0u};
    mutable std::uint64_t spatial_frame_hash_{0ull};
    mutable int spatial_width_{0};
    mutable int spatial_height_{0};
    mutable int spatial_cols_{0};
    mutable int spatial_rows_{0};
    mutable std::vector<ModernGlVertex> filled_batch_scratch_{};
    mutable std::vector<ModernGlVertex> outline_batch_scratch_{};
    mutable std::vector<ModernGlVertex> blur_pass_vertices_scratch_{};
    mutable std::uint32_t filled_batch_trim_frames_{0u};
    mutable std::uint32_t outline_batch_trim_frames_{0u};
    mutable std::uint32_t blur_batch_trim_frames_{0u};
    mutable TextureId backdrop_texture_{0u};
    mutable int backdrop_texture_w_{0};
    mutable int backdrop_texture_h_{0};
    mutable std::uint64_t backdrop_frame_token_{0ull};
    mutable std::uint32_t backdrop_idle_frames_{0u};
    mutable bool backdrop_used_in_frame_{false};

    void ensure_backdrop_texture(int width, int height) const {
        if (width <= 0 || height <= 0) {
            return;
        }
        if (backdrop_texture_ == 0u) {
            glGenTextures(1, &backdrop_texture_);
            backdrop_texture_w_ = 0;
            backdrop_texture_h_ = 0;
        }
        glBindTexture(GL_TEXTURE_2D, backdrop_texture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (backdrop_texture_w_ != width || backdrop_texture_h_ != height) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            backdrop_texture_w_ = width;
            backdrop_texture_h_ = height;
        }
    }

    void begin_backdrop_frame(std::uint64_t frame_hash) const {
        const std::uint64_t frame_token = frame_hash != 0ull ? frame_hash : (backdrop_frame_token_ + 1ull);
        if (frame_token == backdrop_frame_token_) {
            return;
        }
        if (backdrop_frame_token_ != 0ull) {
            backdrop_idle_frames_ = backdrop_used_in_frame_ ? 0u : (backdrop_idle_frames_ + 1u);
            if (backdrop_idle_frames_ >= 240u && backdrop_texture_ != 0u) {
                glDeleteTextures(1, &backdrop_texture_);
                backdrop_texture_ = 0u;
                backdrop_texture_w_ = 0;
                backdrop_texture_h_ = 0;
                backdrop_idle_frames_ = 0u;
            }
        }
        backdrop_frame_token_ = frame_token;
        backdrop_used_in_frame_ = false;
    }

    void ensure_spatial_index(const std::vector<DrawCommand>& commands, int width, int height,
                              std::uint64_t frame_hash) const {
        if (commands.size() < 96u) {
            return;
        }
        if (spatial_commands_ptr_ == commands.data() && spatial_command_count_ == commands.size() &&
            spatial_frame_hash_ == frame_hash && spatial_width_ == width && spatial_height_ == height) {
            return;
        }

        spatial_cols_ = std::max(1, (width + k_spatial_tile_px - 1) / k_spatial_tile_px);
        spatial_rows_ = std::max(1, (height + k_spatial_tile_px - 1) / k_spatial_tile_px);
        const std::size_t bucket_count = static_cast<std::size_t>(spatial_cols_ * spatial_rows_);
        spatial_bucket_offsets_.assign(bucket_count + 1u, 0u);

        for (std::size_t i = 0; i < commands.size(); ++i) {
            const Rect rect = command_visible_rect(commands[i]);
            if (rect.w <= 0.0f || rect.h <= 0.0f) {
                continue;
            }

            const int x0 = std::clamp(static_cast<int>(std::floor(rect.x / k_spatial_tile_px)), 0, spatial_cols_ - 1);
            const int y0 = std::clamp(static_cast<int>(std::floor(rect.y / k_spatial_tile_px)), 0, spatial_rows_ - 1);
            const int x1 = std::clamp(static_cast<int>(std::floor((rect.x + rect.w) / k_spatial_tile_px)), 0,
                                      spatial_cols_ - 1);
            const int y1 = std::clamp(static_cast<int>(std::floor((rect.y + rect.h) / k_spatial_tile_px)), 0,
                                      spatial_rows_ - 1);
            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    spatial_bucket_offsets_[static_cast<std::size_t>(y * spatial_cols_ + x) + 1u] += 1u;
                }
            }
        }

        for (std::size_t i = 1; i < spatial_bucket_offsets_.size(); ++i) {
            spatial_bucket_offsets_[i] =
                static_cast<std::uint32_t>(spatial_bucket_offsets_[i] + spatial_bucket_offsets_[i - 1u]);
        }

        spatial_bucket_indices_.assign(spatial_bucket_offsets_.back(), 0u);
        spatial_bucket_cursor_ = spatial_bucket_offsets_;

        for (std::size_t i = 0; i < commands.size(); ++i) {
            const Rect rect = command_visible_rect(commands[i]);
            if (rect.w <= 0.0f || rect.h <= 0.0f) {
                continue;
            }

            const int x0 = std::clamp(static_cast<int>(std::floor(rect.x / k_spatial_tile_px)), 0, spatial_cols_ - 1);
            const int y0 = std::clamp(static_cast<int>(std::floor(rect.y / k_spatial_tile_px)), 0, spatial_rows_ - 1);
            const int x1 = std::clamp(static_cast<int>(std::floor((rect.x + rect.w) / k_spatial_tile_px)), 0,
                                      spatial_cols_ - 1);
            const int y1 = std::clamp(static_cast<int>(std::floor((rect.y + rect.h) / k_spatial_tile_px)), 0,
                                      spatial_rows_ - 1);
            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    const std::size_t bucket_index = static_cast<std::size_t>(y * spatial_cols_ + x);
                    const std::uint32_t cursor = spatial_bucket_cursor_[bucket_index];
                    spatial_bucket_indices_[cursor] = static_cast<std::uint32_t>(i);
                    spatial_bucket_cursor_[bucket_index] = cursor + 1u;
                }
            }
        }

        spatial_commands_ptr_ = commands.data();
        spatial_command_count_ = commands.size();
        spatial_frame_hash_ = frame_hash;
        spatial_width_ = width;
        spatial_height_ = height;
    }
};
