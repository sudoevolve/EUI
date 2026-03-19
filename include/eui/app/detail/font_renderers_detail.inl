#if defined(_WIN32) && !defined(EUI_OPENGL_ES)

class Win32FontRenderer {
public:
    explicit Win32FontRenderer(const AppOptions& options)
        : text_family_(utf8_to_wide(options.text_font_family != nullptr ? options.text_font_family : "Segoe UI")),
          text_font_weight_(std::clamp(options.text_font_weight, 100, 900)),
          icon_family_(utf8_to_wide(options.icon_font_family != nullptr ? options.icon_font_family
                                                                       : "Font Awesome 7 Free Solid")),
          text_font_file_(utf8_to_wide(options.text_font_file != nullptr ? options.text_font_file : "")),
          icon_font_file_(utf8_to_wide(resolve_icon_font_file_path(options.icon_font_file))),
          enable_icon_fallback_(options.enable_icon_font_fallback) {}

    ~Win32FontRenderer() {
        if (wglGetCurrentContext() != nullptr) {
            release_gl_resources();
        }
        for (auto& pair : fonts_) {
            if (pair.second.handle != nullptr) {
                DeleteObject(pair.second.handle);
                pair.second.handle = nullptr;
            }
        }
        for (const std::wstring& path : loaded_private_fonts_) {
            RemoveFontResourceExW(path.c_str(), FR_PRIVATE, nullptr);
        }
    }

    Win32FontRenderer(const Win32FontRenderer&) = delete;
    Win32FontRenderer& operator=(const Win32FontRenderer&) = delete;

    void release_gl_resources() const {
        for (auto& pair : fonts_) {
            auto& glyphs = pair.second.glyphs;
            for (auto& glyph_pair : glyphs) {
                GlyphData& glyph = glyph_pair.second;
                if (glyph.list_id != 0u) {
                    glDeleteLists(glyph.list_id, 1);
                    glyph.list_id = 0u;
                }
            }
            glyphs.clear();
        }
    }

    bool draw_text(const DrawCommand& cmd, std::string_view text) const {
        if (text.empty()) {
            return true;
        }

        HDC hdc = wglGetCurrentDC();
        if (hdc == nullptr) {
            return false;
        }

        ensure_private_fonts_loaded();

        // Outline path appears visually smaller than legacy bitmap; lift render px for parity.
        const int text_font_px = std::max(8, static_cast<int>(std::round(cmd.font_size * 1.2f)));
        const int icon_font_px = std::max(8, static_cast<int>(std::round(text_font_px * k_icon_visual_scale)));
        FontInstance* text_font = ensure_font(false, text_font_px, hdc);
        if (text_font == nullptr) {
            return false;
        }
        FontInstance* icon_font = enable_icon_fallback_ ? ensure_font(true, icon_font_px, hdc) : nullptr;

        std::vector<GlyphData> glyphs;
        glyphs.reserve(text.size());
        float width = 0.0f;
        float max_above_baseline = 0.0f;
        float max_below_baseline = 0.0f;

        std::size_t index = 0u;
        while (index < text.size()) {
            std::uint32_t cp = 0u;
            if (!utf8_next(text, index, cp)) {
                break;
            }
            const GlyphData* glyph = resolve_glyph(cp, text_font, icon_font, hdc);
            if (glyph == nullptr || !glyph->valid) {
                continue;
            }
            glyphs.push_back(*glyph);
            width += glyph->advance;
            max_above_baseline = std::max(max_above_baseline, glyph->ascent);
            max_below_baseline =
                std::max(max_below_baseline, std::max(0.0f, glyph->line_height - glyph->ascent));
        }

        if (glyphs.empty()) {
            return false;
        }

        float x = cmd.rect.x;
        if (cmd.align == TextAlign::Center) {
            x += std::max(0.0f, (cmd.rect.w - width) * 0.5f);
        } else if (cmd.align == TextAlign::Right) {
            x += std::max(0.0f, cmd.rect.w - width);
        }
        const float resolved_text_h =
            std::max(1.0f, max_above_baseline + max_below_baseline);
        const float baseline_y =
            cmd.rect.y + std::max(0.0f, (cmd.rect.h - resolved_text_h) * 0.5f) + max_above_baseline;

        glColor4f(cmd.color.r, cmd.color.g, cmd.color.b, cmd.color.a);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        float pen_x = x;
        for (const GlyphData& glyph : glyphs) {
            if (glyph.list_id != 0u) {
                if (glyph.is_bitmap) {
                    glRasterPos2f(pen_x, baseline_y);
                    glCallList(glyph.list_id);
                } else {
                    glPushMatrix();
                    glTranslatef(pen_x, baseline_y, 0.0f);
                    // wgl outline glyph coordinates are y-up; UI projection here is y-down.
                    const float scale = std::max(1.0f, glyph.outline_scale);
                    glScalef(scale, -scale, 1.0f);
                    glCallList(glyph.list_id);
                    glPopMatrix();
                }
            }
            pen_x += glyph.advance;
        }
        return true;
    }

private:
    struct GlyphData {
        unsigned int list_id{0u};
        float advance{0.0f};
        float ascent{0.0f};
        float line_height{0.0f};
        float outline_scale{1.0f};
        bool is_bitmap{false};
        bool valid{false};
    };

    struct FontInstance {
        HFONT handle{nullptr};
        int ascent{0};
        int line_height{0};
        int pixel_size{0};
        std::unordered_map<std::uint32_t, GlyphData> glyphs{};
    };

    static int to_utf16(std::uint32_t cp, wchar_t out[2]) {
        if (cp <= 0xFFFFu) {
            out[0] = static_cast<wchar_t>(cp);
            return 1;
        }
        out[0] = L'\0';
        out[1] = L'\0';
        return 0;
    }

    static bool is_private_use_codepoint(std::uint32_t cp) {
        return cp >= 0xE000u && cp <= 0xF8FFu;
    }

    static bool is_known_icon_face(const std::wstring& face_name) {
        return face_name == L"Segoe Fluent Icons" || face_name == L"Segoe MDL2 Assets" ||
               face_name == L"Segoe UI Symbol";
    }

    static bool wide_iequals(const std::wstring& lhs, const std::wstring& rhs) {
        if (lhs.empty() || rhs.empty()) {
            return false;
        }
        return CompareStringOrdinal(lhs.c_str(), -1, rhs.c_str(), -1, TRUE) == CSTR_EQUAL;
    }

    std::string font_key(bool icon_font, int px) const {
        return std::string(icon_font ? "icon:" : "text:") + std::to_string(px);
    }

    void ensure_private_fonts_loaded() const {
        if (private_fonts_loaded_) {
            return;
        }
        private_fonts_loaded_ = true;

        auto load_private = [&](const std::wstring& path) {
            if (path.empty()) {
                return;
            }
            const int added = AddFontResourceExW(path.c_str(), FR_PRIVATE, nullptr);
            if (added > 0) {
                loaded_private_fonts_.push_back(path);
            }
        };
        load_private(text_font_file_);
        load_private(icon_font_file_);
    }

    FontInstance* ensure_font(bool icon_font, int px, HDC hdc) const {
        const std::string key = font_key(icon_font, px);
        auto it = fonts_.find(key);
        if (it != fonts_.end()) {
            return &it->second;
        }

        auto create_font_instance =
            [&](const std::wstring& family, int font_weight, DWORD charset, bool* out_is_known_icon_face,
                std::wstring* out_resolved_face) -> FontInstance {
            FontInstance instance{};
            if (family.empty()) {
                if (out_is_known_icon_face != nullptr) {
                    *out_is_known_icon_face = false;
                }
                if (out_resolved_face != nullptr) {
                    out_resolved_face->clear();
                }
                return instance;
            }

            HFONT font = CreateFontW(-px, 0, 0, 0, std::clamp(font_weight, 100, 900), FALSE, FALSE, FALSE, charset, OUT_TT_PRECIS,
                                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                     family.c_str());
            if (font == nullptr) {
                if (out_is_known_icon_face != nullptr) {
                    *out_is_known_icon_face = false;
                }
                if (out_resolved_face != nullptr) {
                    out_resolved_face->clear();
                }
                return instance;
            }

            HGDIOBJ old = SelectObject(hdc, font);
            TEXTMETRICW tm{};
            GetTextMetricsW(hdc, &tm);
            wchar_t face_name[LF_FACESIZE]{};
            GetTextFaceW(hdc, LF_FACESIZE, face_name);
            SelectObject(hdc, old);

            if (out_is_known_icon_face != nullptr) {
                *out_is_known_icon_face = is_known_icon_face(face_name);
            }
            if (out_resolved_face != nullptr) {
                *out_resolved_face = face_name;
            }

            instance.handle = font;
            instance.ascent = std::max(1, static_cast<int>(tm.tmAscent));
            instance.line_height = std::max(1, static_cast<int>(tm.tmHeight));
            instance.pixel_size = std::max(1, px);
            return instance;
        };

        FontInstance instance{};
        if (icon_font) {
            std::vector<std::wstring> candidates;
            candidates.reserve(4);
            auto add_unique = [&](const std::wstring& family) {
                if (family.empty()) {
                    return;
                }
                for (const std::wstring& existing : candidates) {
                    if (existing == family) {
                        return;
                    }
                }
                candidates.push_back(family);
            };

            add_unique(icon_family_);
            add_unique(L"Font Awesome 7 Free Solid");
            add_unique(L"Font Awesome 7 Free");
            add_unique(L"Segoe Fluent Icons");
            add_unique(L"Segoe MDL2 Assets");
            add_unique(L"Segoe UI Symbol");

            for (const std::wstring& family : candidates) {
                bool known_icon_face = false;
                std::wstring resolved_face{};
                FontInstance candidate =
                    create_font_instance(family, FW_NORMAL, DEFAULT_CHARSET, &known_icon_face, &resolved_face);
                if (candidate.handle == nullptr) {
                    continue;
                }

                const bool exact_family_match = wide_iequals(resolved_face, family);
                const bool usable_icon_face = known_icon_face || exact_family_match;
                if (usable_icon_face) {
                    instance = std::move(candidate);
                    break;
                }
                DeleteObject(candidate.handle);
            }
        } else {
            instance = create_font_instance(text_family_, text_font_weight_, DEFAULT_CHARSET, nullptr, nullptr);
        }

        if (instance.handle == nullptr) {
            return nullptr;
        }

        auto [inserted_it, _] = fonts_.emplace(key, std::move(instance));
        return &inserted_it->second;
    }

    GlyphData* ensure_glyph(FontInstance* font, std::uint32_t cp, HDC hdc) const {
        if (font == nullptr) {
            return nullptr;
        }

        auto it = font->glyphs.find(cp);
        if (it != font->glyphs.end()) {
            return &it->second;
        }

        GlyphData glyph{};
        wchar_t wide[2] = {L'\0', L'\0'};
        const int wide_len = to_utf16(cp, wide);
        if (wide_len == 1) {
            HGDIOBJ old = SelectObject(hdc, font->handle);
            bool glyph_available = true;
            if (is_private_use_codepoint(cp)) {
                WORD glyph_index = 0xFFFFu;
                const DWORD glyph_query =
                    GetGlyphIndicesW(hdc, wide, 1, &glyph_index, GGI_MARK_NONEXISTING_GLYPHS);
                glyph_available = glyph_query != GDI_ERROR && glyph_index != 0xFFFFu;
            }

            if (glyph_available) {
                const unsigned int list_id = glGenLists(1);
                bool built = false;
                const DWORD glyph_code = static_cast<DWORD>(wide[0]);
                if (list_id != 0u) {
                    GLYPHMETRICSFLOAT gm{};
                    if (wglUseFontOutlinesW(hdc, glyph_code, 1u, list_id, 0.0f, 0.0f,
                                            WGL_FONT_POLYGONS, &gm) != FALSE) {
                        const bool outline_metrics_ok =
                            gm.gmfBlackBoxX > 0.0f && gm.gmfBlackBoxY > 0.0f;
                        const float outline_scale = static_cast<float>(std::max(1, font->pixel_size));
                        glyph.advance = std::max(0.0f, gm.gmfCellIncX * outline_scale);
                        if (glyph.advance <= 0.0f) {
                            SIZE size{};
                            GetTextExtentPoint32W(hdc, wide, 1, &size);
                            glyph.advance = std::max(0.0f, static_cast<float>(size.cx));
                        }
                        if (outline_metrics_ok && glyph.advance >= 0.25f) {
                            const float above = std::max(0.0f, gm.gmfptGlyphOrigin.y * outline_scale);
                            const float below =
                                std::max(0.0f, (gm.gmfBlackBoxY - gm.gmfptGlyphOrigin.y) * outline_scale);
                            glyph.ascent = above;
                            glyph.line_height = std::max(1.0f, above + below);
                            if (glyph.line_height < 1.0f || glyph.ascent < 0.0f) {
                                glyph.ascent = static_cast<float>(std::max(1, font->ascent));
                                glyph.line_height = static_cast<float>(std::max(1, font->line_height));
                            }
                            glyph.outline_scale = outline_scale;
                            glyph.list_id = list_id;
                            glyph.is_bitmap = false;
                            glyph.valid = true;
                            built = true;
                        }
                    }
                }
                if (!built && list_id != 0u && wglUseFontBitmapsW(hdc, glyph_code, 1u, list_id) != FALSE) {
                    SIZE size{};
                    GetTextExtentPoint32W(hdc, wide, 1, &size);
                    glyph.advance = std::max(0.0f, static_cast<float>(size.cx));
                    glyph.ascent = static_cast<float>(std::max(1, font->ascent));
                    glyph.line_height = static_cast<float>(std::max(1, font->line_height));
                    glyph.outline_scale = 1.0f;
                    glyph.list_id = list_id;
                    glyph.is_bitmap = true;
                    glyph.valid = true;
                    built = true;
                }

                if (!built && list_id != 0u) {
                    glDeleteLists(list_id, 1);
                }
            }
            SelectObject(hdc, old);
        }

        auto [inserted_it, _] = font->glyphs.emplace(cp, glyph);
        return &inserted_it->second;
    }

    const GlyphData* resolve_glyph(std::uint32_t cp, FontInstance* text_font, FontInstance* icon_font,
                                   HDC hdc) const {
        auto try_font = [&](FontInstance* font) -> GlyphData* {
            GlyphData* glyph = ensure_glyph(font, cp, hdc);
            if (glyph != nullptr && glyph->valid) {
                return glyph;
            }
            return nullptr;
        };

        const bool prefer_icon_font = enable_icon_fallback_ && icon_font != nullptr && is_private_use_codepoint(cp);
        if (prefer_icon_font) {
            if (GlyphData* icon_glyph = try_font(icon_font)) {
                return icon_glyph;
            }
            return nullptr;
        } else {
            if (GlyphData* text_glyph = try_font(text_font)) {
                return text_glyph;
            }
            if (enable_icon_fallback_ && icon_font != nullptr) {
                if (GlyphData* icon_glyph = try_font(icon_font)) {
                    return icon_glyph;
                }
            }
        }

        GlyphData* fallback_glyph = ensure_glyph(text_font, static_cast<std::uint32_t>('?'), hdc);
        if (fallback_glyph != nullptr && fallback_glyph->valid) {
            return fallback_glyph;
        }
        return nullptr;
    }

    std::wstring text_family_{};
    int text_font_weight_{FW_NORMAL};
    std::wstring icon_family_{};
    std::wstring text_font_file_{};
    std::wstring icon_font_file_{};
    bool enable_icon_fallback_{true};

    mutable bool private_fonts_loaded_{false};
    mutable std::unordered_map<std::string, FontInstance> fonts_{};
    mutable std::vector<std::wstring> loaded_private_fonts_{};
};
#endif

#if EUI_ENABLE_STB_TRUETYPE
class StbFontRenderer {
    struct GlyphData;
    struct PreparedGlyph {
        const GlyphData* glyph{nullptr};
        float pen_x{0.0f};
    };

public:
    static std::string resolve_font_path_for_measure(const char* explicit_file, const char* family, bool icon_font) {
        return resolve_font_path(explicit_file, family, icon_font);
    }

    explicit StbFontRenderer(const AppOptions& options)
        : text_font_path_(resolve_font_path(options.text_font_file, options.text_font_family, false)),
          icon_font_path_(resolve_font_path(options.icon_font_file, options.icon_font_family, true)),
          enable_icon_fallback_(options.enable_icon_font_fallback) {}

    void begin_frame(std::uint64_t frame_hash) const {
        const std::uint64_t frame_token = frame_hash != 0ull ? frame_hash : (last_frame_hash_ + 1ull);
        if (frame_token == last_frame_hash_) {
            return;
        }
        last_frame_hash_ = frame_token;
        frame_epoch_ += 1u;
        if ((frame_epoch_ & 31u) == 0u) {
            prune_idle_face_atlases();
        }
    }

    void release_gl_resources() const {
        for (auto& pair : faces_) {
            release_face_gl_resources(pair.second);
        }
        faces_.clear();
        font_blob_cache_.clear();
        frame_epoch_ = 0u;
        last_frame_hash_ = 0ull;
        modern_gl_trim_scratch(draw_glyphs_scratch_, 0u);
        modern_gl_trim_scratch(text_vertices_scratch_, 0u);
        std::vector<unsigned char>{}.swap(atlas_zero_scratch_);
        std::vector<unsigned char>{}.swap(glyph_bitmap_scratch_);
        std::vector<unsigned char>{}.swap(glyph_upload_scratch_);
        draw_glyphs_trim_frames_ = 0u;
        text_vertices_trim_frames_ = 0u;
        atlas_zero_trim_frames_ = 0u;
        glyph_bitmap_trim_frames_ = 0u;
        glyph_upload_trim_frames_ = 0u;
    }

    bool draw_text(const DrawCommand& cmd, std::string_view text) const {
        if (text.empty()) {
            return true;
        }

        const int text_font_px = std::max(8, static_cast<int>(std::round(cmd.font_size * 1.20f)));
        const int icon_font_px = std::max(8, static_cast<int>(std::round(text_font_px * k_icon_visual_scale)));
        FontFace* text_face = ensure_face(false, text_font_px);
        FontFace* icon_face = nullptr;
        auto ensure_icon_face = [&]() -> FontFace* {
            if (!enable_icon_fallback_) {
                return nullptr;
            }
            if (icon_face == nullptr) {
                icon_face = ensure_face(true, icon_font_px);
            }
            return icon_face;
        };
        if (text_face == nullptr) {
            if (!enable_icon_fallback_ || ensure_icon_face() == nullptr) {
                return false;
            }
        }

        auto& draw_glyphs = draw_glyphs_scratch_;
        modern_gl_prepare_scratch(draw_glyphs, text.size());
        float width = 0.0f;
        float max_above = 0.0f;
        float max_below = 0.0f;
        std::size_t text_vertices_peak = 0u;
        auto finish = [&](bool ok) {
            const std::size_t draw_glyph_keep =
                std::max<std::size_t>(512u, draw_glyphs.size() + draw_glyphs.size() / 2u + 16u);
            const std::size_t text_vertices_keep =
                std::max<std::size_t>(4096u, text_vertices_peak + text_vertices_peak / 2u + 32u);
            eui::detail::context_retain_vector_hysteresis(text_vertices_scratch_, text_vertices_keep,
                                                          text_vertices_trim_frames_, 120u);
            eui::detail::context_retain_vector_hysteresis(draw_glyphs_scratch_, draw_glyph_keep,
                                                          draw_glyphs_trim_frames_, 120u);
            return ok;
        };

        auto fallback_glyph = [&](FontFace* face) -> const GlyphData* {
            if (face == nullptr) {
                return nullptr;
            }
            GlyphData* fallback = ensure_glyph(face, static_cast<std::uint32_t>('?'));
            if (fallback != nullptr && fallback->valid) {
                return fallback;
            }
            return nullptr;
        };

        auto pick_glyph = [&](std::uint32_t cp) -> const GlyphData* {
            auto try_face = [&](FontFace* face) -> const GlyphData* {
                if (face == nullptr) {
                    return nullptr;
                }
                GlyphData* glyph = ensure_glyph(face, cp);
                if (glyph != nullptr && glyph->valid) {
                    return glyph;
                }
                return nullptr;
            };

            const bool private_use = cp >= 0xE000u && cp <= 0xF8FFu;
            if (private_use && enable_icon_fallback_) {
                if (const GlyphData* icon = try_face(ensure_icon_face())) {
                    return icon;
                }
                return fallback_glyph(text_face);
            }
            if (const GlyphData* regular = try_face(text_face)) {
                return regular;
            }
            if (enable_icon_fallback_) {
                if (const GlyphData* icon = try_face(ensure_icon_face())) {
                    return icon;
                }
            }
            if (const GlyphData* fallback = fallback_glyph(text_face)) {
                return fallback;
            }
            return fallback_glyph(ensure_icon_face());
        };

        std::size_t index = 0u;
        while (index < text.size()) {
            std::uint32_t cp = 0u;
            if (!utf8_next(text, index, cp)) {
                break;
            }
            const GlyphData* glyph = pick_glyph(cp);
            if (glyph == nullptr) {
                continue;
            }

            draw_glyphs.push_back(PreparedGlyph{glyph, width});
            width += std::max(0.0f, glyph->advance);
            max_above = std::max(max_above, std::max(0.0f, -glyph->yoff));
            max_below = std::max(max_below, std::max(0.0f, glyph->h + glyph->yoff));
        }

        if (draw_glyphs.empty()) {
            return finish(false);
        }

        float x = cmd.rect.x;
        if (cmd.align == TextAlign::Center) {
            x += std::max(0.0f, (cmd.rect.w - width) * 0.5f);
        } else if (cmd.align == TextAlign::Right) {
            x += std::max(0.0f, cmd.rect.w - width);
        }

        if (max_above + max_below < 1.0f) {
            if (text_face != nullptr && text_face->line_height > 1.0f) {
                max_above = std::max(max_above, text_face->ascent);
                max_below = std::max(max_below, text_face->line_height - text_face->ascent);
            } else if (icon_face != nullptr && icon_face->line_height > 1.0f) {
                max_above = std::max(max_above, icon_face->ascent);
                max_below = std::max(max_below, icon_face->line_height - icon_face->ascent);
            } else {
                max_above = std::max(max_above, cmd.font_size * 0.72f);
                max_below = std::max(max_below, cmd.font_size * 0.28f);
            }
        }

        const float text_h = std::max(1.0f, max_above + max_below);
        const float baseline_y = std::round(cmd.rect.y + std::max(0.0f, (cmd.rect.h - text_h) * 0.5f) + max_above);
        x = std::round(x);

        unsigned int bound_texture = 0u;
        auto& verts = text_vertices_scratch_;
        modern_gl_prepare_scratch(verts, draw_glyphs.size() * 6u);
        auto flush = [&]() {
            if (bound_texture == 0u || verts.empty()) {
                verts.clear();
                return true;
            }
            text_vertices_peak = std::max(text_vertices_peak, verts.size());
            const bool ok = modern_gl_draw_vertices_current_viewport(GL_TRIANGLES, verts.data(), verts.size(),
                                                                     bound_texture, ModernGlTextureMode::AlphaMask);
            verts.clear();
            return ok;
        };

        for (const PreparedGlyph& draw : draw_glyphs) {
            const GlyphData& glyph = *draw.glyph;
            if (!glyph.has_bitmap || glyph.texture_id == 0u || glyph.w <= 0.0f || glyph.h <= 0.0f) {
                continue;
            }
            const float x0 = std::round(x + draw.pen_x + glyph.xoff);
            const float y0 = std::round(baseline_y + glyph.yoff);
            const float x1 = x0 + glyph.w;
            const float y1 = y0 + glyph.h;

            if (bound_texture != glyph.texture_id && !verts.empty()) {
                if (!flush()) {
                    return finish(false);
                }
            }
            bound_texture = glyph.texture_id;

            const ModernGlVertex p0 = modern_gl_vertex(x0, y0, cmd.color, glyph.u0, glyph.v0);
            const ModernGlVertex p1 = modern_gl_vertex(x1, y0, cmd.color, glyph.u1, glyph.v0);
            const ModernGlVertex p2 = modern_gl_vertex(x1, y1, cmd.color, glyph.u1, glyph.v1);
            const ModernGlVertex p3 = modern_gl_vertex(x0, y1, cmd.color, glyph.u0, glyph.v1);
            modern_gl_push_quad(verts, p0, p1, p2, p3);
        }
        return finish(flush());
    }

private:
    static constexpr int k_atlas_padding = 1;
    static constexpr int k_max_atlas_pages = 3;
    static constexpr int k_tiny_atlas_size = 256;
    static constexpr int k_small_atlas_size = 512;
    static constexpr int k_large_atlas_size = 1024;
    static constexpr std::size_t k_max_face_cache = 20u;
    static constexpr int k_glyph_raster_oversample = 2;

    struct AtlasSlot {
        bool occupied{false};
        std::uint32_t codepoint{0u};
        std::uint64_t last_used{0u};
    };

    struct AtlasPage {
        unsigned int texture_id{0u};
        int width{0};
        int height{0};
        int cell_size{0};
        int columns{0};
        int rows{0};
        std::vector<AtlasSlot> slots{};
    };

    struct GlyphData {
        unsigned int texture_id{0u};
        std::uint16_t page_index{0u};
        std::uint16_t slot_index{0u};
        float advance{0.0f};
        float xoff{0.0f};
        float yoff{0.0f};
        float w{0.0f};
        float h{0.0f};
        float u0{0.0f};
        float v0{0.0f};
        float u1{0.0f};
        float v1{0.0f};
        bool has_bitmap{false};
        bool valid{false};
    };

    struct FontFace {
        std::shared_ptr<std::vector<unsigned char>> data_blob{};
        stbtt_fontinfo info{};
        float scale{0.0f};
        float ascent{0.0f};
        float line_height{0.0f};
        int atlas_size{k_small_atlas_size};
        int cell_size{64};
        int max_pages{k_max_atlas_pages};
        std::uint64_t lru_tick{1u};
        std::uint64_t last_used{0u};
        bool valid{false};
        std::unordered_map<std::uint32_t, GlyphData> glyphs{};
        std::vector<AtlasPage> atlas_pages{};
    };

    struct SlotHandle {
        AtlasPage* page{nullptr};
        std::size_t page_index{0u};
        std::size_t slot_index{0u};
        int x{0};
        int y{0};
        bool valid{false};
    };

    static int next_power_of_two(int value) {
        int result = 1;
        while (result < value && result < 4096) {
            result <<= 1;
        }
        return std::max(1, result);
    }

    static int quantize_font_px(int px) {
        int clamped = std::max(8, px);
        if (clamped <= 32) {
            clamped = ((clamped + 1) / 2) * 2;
        } else if (clamped <= 72) {
            clamped = ((clamped + 2) / 4) * 4;
        } else {
            clamped = ((clamped + 4) / 8) * 8;
        }
        return std::clamp(clamped, 8, 200);
    }

    static bool file_exists(const std::string& path) {
        if (path.empty()) {
            return false;
        }
        std::ifstream ifs(path, std::ios::binary);
        return static_cast<bool>(ifs);
    }

    static std::vector<unsigned char> read_file(const std::string& path) {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) {
            return {};
        }
        ifs.seekg(0, std::ios::end);
        const std::streamoff size = ifs.tellg();
        if (size <= 0) {
            return {};
        }
        ifs.seekg(0, std::ios::beg);
        std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
        ifs.read(reinterpret_cast<char*>(bytes.data()), size);
        if (!ifs) {
            return {};
        }
        return bytes;
    }

    std::shared_ptr<std::vector<unsigned char>> load_font_blob(const std::string& path) const {
        if (path.empty()) {
            return {};
        }
        auto it = font_blob_cache_.find(path);
        if (it != font_blob_cache_.end()) {
            return it->second;
        }
        auto data = std::make_shared<std::vector<unsigned char>>(read_file(path));
        if (data->empty()) {
            return {};
        }
        font_blob_cache_[path] = data;
        return data;
    }

    static void release_face_atlas_resources(FontFace& face) {
        for (AtlasPage& page : face.atlas_pages) {
            if (page.texture_id != 0u) {
                glDeleteTextures(1, &page.texture_id);
                page.texture_id = 0u;
            }
        }
        face.atlas_pages.clear();
        face.glyphs.clear();
    }

    static void release_face_gl_resources(FontFace& face) {
        release_face_atlas_resources(face);
        face.data_blob.reset();
    }

    void prune_idle_face_atlases() const {
        static constexpr std::uint64_t k_idle_face_release_frames = 180u;
        for (auto& pair : faces_) {
            FontFace& face = pair.second;
            if (face.atlas_pages.empty()) {
                continue;
            }
            const std::uint64_t age = frame_epoch_ - face.last_used;
            if (age >= k_idle_face_release_frames) {
                release_face_atlas_resources(face);
            }
        }
    }

    void prune_face_cache_if_needed(const std::string& preserve_key) const {
        while (faces_.size() > k_max_face_cache) {
            auto victim = faces_.end();
            for (auto it = faces_.begin(); it != faces_.end(); ++it) {
                if (it->first == preserve_key) {
                    continue;
                }
                if (victim == faces_.end() || it->second.last_used < victim->second.last_used) {
                    victim = it;
                }
            }
            if (victim == faces_.end()) {
                break;
            }
            release_face_gl_resources(victim->second);
            faces_.erase(victim);
        }
    }

    static std::string to_lower_ascii(std::string_view text) {
        std::string out;
        out.reserve(text.size());
        for (char ch : text) {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
        return out;
    }

    static std::string join_path(const std::string& base, const char* file_name) {
        if (base.empty() || file_name == nullptr || *file_name == '\0') {
            return {};
        }
        const char last = base.back();
        if (last == '/' || last == '\\') {
            return base + file_name;
        }
#ifdef _WIN32
        return base + "\\" + file_name;
#else
        return base + "/" + file_name;
#endif
    }

    static std::string resolve_font_path(const char* explicit_file, const char* family, bool icon_font) {
        if (icon_font) {
            const std::string icon_path = resolve_icon_font_file_path(explicit_file);
            if (!icon_path.empty() && file_exists(icon_path)) {
                return icon_path;
            }
        } else if (explicit_file != nullptr && explicit_file[0] != '\0') {
            const std::string explicit_path = explicit_file;
            if (file_exists(explicit_path)) {
                return explicit_path;
            }
        }

        std::vector<std::string> candidates;
        const std::string family_lower = to_lower_ascii(family != nullptr ? std::string_view(family) : "");

#ifdef _WIN32
        std::string windows_dir = "C:\\Windows";
#if defined(_MSC_VER)
        char* env_value = nullptr;
        std::size_t env_len = 0u;
        if (_dupenv_s(&env_value, &env_len, "WINDIR") == 0 && env_value != nullptr) {
            if (env_value[0] != '\0') {
                windows_dir = env_value;
            }
        }
        std::free(env_value);
#else
        if (const char* env = std::getenv("WINDIR")) {
            if (env[0] != '\0') {
                windows_dir = env;
            }
        }
#endif
        const std::string fonts_dir = join_path(windows_dir, "Fonts");
        auto add_win = [&](const char* file_name) {
            const std::string full = join_path(fonts_dir, file_name);
            if (!full.empty()) {
                candidates.push_back(full);
            }
        };

        if (icon_font) {
            if (family_lower.find("fluent") != std::string::npos) {
                add_win("segfluenticons.ttf");
            }
            if (family_lower.find("mdl2") != std::string::npos) {
                add_win("segmdl2.ttf");
            }
            if (family_lower.find("symbol") != std::string::npos) {
                add_win("seguisym.ttf");
            }
            add_win("segmdl2.ttf");
            add_win("segfluenticons.ttf");
            add_win("seguisym.ttf");
        } else {
            if (family_lower.find("segoe") != std::string::npos) {
                add_win("segoeui.ttf");
            }
            if (family_lower.find("arial") != std::string::npos) {
                add_win("arial.ttf");
            }
            add_win("segoeui.ttf");
            add_win("arial.ttf");
        }
#else
        auto add_unix = [&](const char* path) {
            candidates.emplace_back(path);
        };

        if (icon_font) {
            if (family_lower.find("emoji") != std::string::npos) {
                add_unix("/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf");
            }
            add_unix("/usr/share/fonts/truetype/noto/NotoSansSymbols2-Regular.ttf");
            add_unix("/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf");
            add_unix("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
        } else {
            if (family_lower.find("noto") != std::string::npos) {
                add_unix("/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf");
            }
            add_unix("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
            add_unix("/usr/share/fonts/dejavu/DejaVuSans.ttf");
            add_unix("/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf");
        }
#endif

        for (const std::string& path : candidates) {
            if (file_exists(path)) {
                return path;
            }
        }
        return {};
    }

    static std::size_t slot_count_for(const AtlasPage& page) {
        return static_cast<std::size_t>(std::max(0, page.columns) * std::max(0, page.rows));
    }

    bool create_atlas_page(FontFace* face) const {
        if (face == nullptr || static_cast<int>(face->atlas_pages.size()) >= face->max_pages) {
            return false;
        }

        AtlasPage page{};
        page.width = std::max(128, face->atlas_size);
        page.height = std::max(128, face->atlas_size);
        page.cell_size = std::max(8, face->cell_size);
        page.columns = std::max(1, page.width / page.cell_size);
        page.rows = std::max(1, page.height / page.cell_size);
        page.slots.assign(slot_count_for(page), AtlasSlot{});

        glGenTextures(1, &page.texture_id);
        if (page.texture_id == 0u) {
            return false;
        }

        GLint prev_unpack = 4;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_unpack);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glBindTexture(GL_TEXTURE_2D, page.texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        auto& zero = atlas_zero_scratch_;
        zero.assign(static_cast<std::size_t>(page.width * page.height), 0u);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, page.width, page.height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                     zero.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, prev_unpack);
        const std::size_t zero_keep =
            std::max<std::size_t>(16u * 1024u, zero.size() + zero.size() / 4u + 64u);
        eui::detail::context_retain_vector_hysteresis(atlas_zero_scratch_, zero_keep, atlas_zero_trim_frames_, 120u);

        face->atlas_pages.push_back(std::move(page));
        return true;
    }

    void touch_slot(FontFace* face, const GlyphData& glyph) const {
        if (face == nullptr || !glyph.has_bitmap) {
            return;
        }
        const std::size_t page_index = static_cast<std::size_t>(glyph.page_index);
        if (page_index >= face->atlas_pages.size()) {
            return;
        }
        AtlasPage& page = face->atlas_pages[page_index];
        const std::size_t slot_index = static_cast<std::size_t>(glyph.slot_index);
        if (slot_index >= page.slots.size()) {
            return;
        }
        AtlasSlot& slot = page.slots[slot_index];
        if (!slot.occupied) {
            return;
        }
        slot.last_used = ++face->lru_tick;
    }

    SlotHandle acquire_slot(FontFace* face, std::uint32_t cp) const {
        if (face == nullptr) {
            return {};
        }

        auto make_handle = [&](AtlasPage& page, std::size_t page_index, std::size_t slot_index) -> SlotHandle {
            AtlasSlot& slot = page.slots[slot_index];
            slot.occupied = true;
            slot.codepoint = cp;
            slot.last_used = ++face->lru_tick;
            const int col = static_cast<int>(slot_index % static_cast<std::size_t>(std::max(1, page.columns)));
            const int row = static_cast<int>(slot_index / static_cast<std::size_t>(std::max(1, page.columns)));
            return SlotHandle{
                &page,
                page_index,
                slot_index,
                col * page.cell_size,
                row * page.cell_size,
                true,
            };
        };

        for (std::size_t page_index = 0; page_index < face->atlas_pages.size(); ++page_index) {
            AtlasPage& page = face->atlas_pages[page_index];
            for (std::size_t slot_index = 0; slot_index < page.slots.size(); ++slot_index) {
                if (!page.slots[slot_index].occupied) {
                    return make_handle(page, page_index, slot_index);
                }
            }
        }

        if (static_cast<int>(face->atlas_pages.size()) < face->max_pages && create_atlas_page(face)) {
            AtlasPage& page = face->atlas_pages.back();
            if (!page.slots.empty()) {
                return make_handle(page, face->atlas_pages.size() - 1u, 0u);
            }
        }

        std::uint64_t oldest_tick = std::numeric_limits<std::uint64_t>::max();
        std::size_t oldest_page_index = 0u;
        std::size_t oldest_slot_index = 0u;
        bool found = false;
        for (std::size_t page_index = 0; page_index < face->atlas_pages.size(); ++page_index) {
            AtlasPage& page = face->atlas_pages[page_index];
            for (std::size_t slot_index = 0; slot_index < page.slots.size(); ++slot_index) {
                const AtlasSlot& slot = page.slots[slot_index];
                if (!slot.occupied) {
                    continue;
                }
                if (!found || slot.last_used < oldest_tick) {
                    found = true;
                    oldest_tick = slot.last_used;
                    oldest_page_index = page_index;
                    oldest_slot_index = slot_index;
                }
            }
        }
        if (!found) {
            return {};
        }

        AtlasPage& page = face->atlas_pages[oldest_page_index];
        AtlasSlot& slot = page.slots[oldest_slot_index];
        if (slot.occupied) {
            face->glyphs.erase(slot.codepoint);
        }
        return make_handle(page, oldest_page_index, oldest_slot_index);
    }

    FontFace* ensure_face(bool icon_font, int px) const {
        const int quantized_px = quantize_font_px(px);
        const std::string key = std::string(icon_font ? "icon:" : "text:") + std::to_string(quantized_px);
        auto it = faces_.find(key);
        if (it != faces_.end()) {
            it->second.last_used = frame_epoch_;
            return it->second.valid ? &it->second : nullptr;
        }

        FontFace face{};
        const std::string& path = icon_font ? icon_font_path_ : text_font_path_;
        face.data_blob = load_font_blob(path);
        if (face.data_blob != nullptr && !face.data_blob->empty()) {
            const int offset = stbtt_GetFontOffsetForIndex(face.data_blob->data(), 0);
            if (offset >= 0 && stbtt_InitFont(&face.info, face.data_blob->data(), offset) != 0) {
                const int raster_px = quantized_px * k_glyph_raster_oversample;
                const float inv_oversample = 1.0f / static_cast<float>(k_glyph_raster_oversample);
                face.scale = stbtt_ScaleForPixelHeight(&face.info, static_cast<float>(raster_px));
                int ascent = 0;
                int descent = 0;
                int line_gap = 0;
                stbtt_GetFontVMetrics(&face.info, &ascent, &descent, &line_gap);
                face.ascent = static_cast<float>(ascent) * face.scale * inv_oversample;
                face.line_height = static_cast<float>(ascent - descent + line_gap) * face.scale * inv_oversample;
                face.cell_size = std::clamp(next_power_of_two(std::max(20, raster_px + raster_px / 2)), 24, 256);
                if (face.cell_size <= 32) {
                    face.atlas_size = k_tiny_atlas_size;
                    face.max_pages = 2;
                } else if (face.cell_size <= 64) {
                    face.atlas_size = k_small_atlas_size;
                    face.max_pages = 2;
                } else {
                    face.atlas_size = k_large_atlas_size;
                    face.max_pages = k_max_atlas_pages;
                }
                face.lru_tick = 1u;
                face.last_used = frame_epoch_;
                face.valid = face.scale > 0.0f;
            }
        }

        auto [inserted, _] = faces_.emplace(key, std::move(face));
        return inserted->second.valid ? &inserted->second : nullptr;
    }

    GlyphData* ensure_glyph(FontFace* face, std::uint32_t cp) const {
        if (face == nullptr) {
            return nullptr;
        }
        auto it = face->glyphs.find(cp);
        if (it != face->glyphs.end()) {
            touch_slot(face, it->second);
            return &it->second;
        }

        GlyphData glyph{};
        const int glyph_index = stbtt_FindGlyphIndex(&face->info, static_cast<int>(cp));
        if (glyph_index == 0 && cp != 0u) {
            auto [inserted, _] = face->glyphs.emplace(cp, glyph);
            return &inserted->second;
        }

        int advance = 0;
        int lsb = 0;
        stbtt_GetCodepointHMetrics(&face->info, static_cast<int>(cp), &advance, &lsb);
        const float inv_oversample = 1.0f / static_cast<float>(k_glyph_raster_oversample);
        glyph.advance = std::max(0.0f, static_cast<float>(advance) * face->scale * inv_oversample);

        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        stbtt_GetCodepointBitmapBox(&face->info, static_cast<int>(cp), face->scale, face->scale, &x0, &y0, &x1, &y1);
        const int w = std::max(0, x1 - x0);
        const int h = std::max(0, y1 - y0);
        glyph.xoff = static_cast<float>(x0) * inv_oversample;
        glyph.yoff = static_cast<float>(y0) * inv_oversample;
        glyph.w = static_cast<float>(w) * inv_oversample;
        glyph.h = static_cast<float>(h) * inv_oversample;
        glyph.valid = true;

        if (w > 0 && h > 0) {
            auto& bitmap = glyph_bitmap_scratch_;
            bitmap.assign(static_cast<std::size_t>(w * h), 0u);
            stbtt_MakeCodepointBitmap(&face->info, bitmap.data(), w, h, w, face->scale, face->scale,
                                      static_cast<int>(cp));

            int pad = k_atlas_padding;
            int upload_w = w + pad * 2;
            int upload_h = h + pad * 2;
            if (upload_w > face->cell_size || upload_h > face->cell_size) {
                pad = 0;
                upload_w = w;
                upload_h = h;
            }

            if (upload_w <= face->cell_size && upload_h <= face->cell_size) {
                const SlotHandle slot = acquire_slot(face, cp);
                if (slot.valid && slot.page != nullptr && slot.page->texture_id != 0u) {
                    const unsigned char* upload_data = bitmap.data();
                    if (pad > 0) {
                        auto& padded_bitmap = glyph_upload_scratch_;
                        padded_bitmap.assign(static_cast<std::size_t>(upload_w * upload_h), 0u);
                        for (int row = 0; row < h; ++row) {
                            std::memcpy(&padded_bitmap[static_cast<std::size_t>((row + pad) * upload_w + pad)],
                                        &bitmap[static_cast<std::size_t>(row * w)], static_cast<std::size_t>(w));
                        }
                        upload_data = padded_bitmap.data();
                    }

                    GLint prev_unpack = 4;
                    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_unpack);
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                    glBindTexture(GL_TEXTURE_2D, slot.page->texture_id);
                    glTexSubImage2D(GL_TEXTURE_2D, 0, slot.x, slot.y, upload_w, upload_h, GL_LUMINANCE,
                                    GL_UNSIGNED_BYTE, upload_data);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glPixelStorei(GL_UNPACK_ALIGNMENT, prev_unpack);

                    const float inv_w = 1.0f / static_cast<float>(slot.page->width);
                    const float inv_h = 1.0f / static_cast<float>(slot.page->height);
                    const int uv_x0 = slot.x + pad;
                    const int uv_y0 = slot.y + pad;
                    const int uv_x1 = uv_x0 + w;
                    const int uv_y1 = uv_y0 + h;

                    glyph.texture_id = slot.page->texture_id;
                    glyph.page_index = static_cast<std::uint16_t>(slot.page_index);
                    glyph.slot_index = static_cast<std::uint16_t>(slot.slot_index);
                    glyph.u0 = static_cast<float>(uv_x0) * inv_w;
                    glyph.v0 = static_cast<float>(uv_y0) * inv_h;
                    glyph.u1 = static_cast<float>(uv_x1) * inv_w;
                    glyph.v1 = static_cast<float>(uv_y1) * inv_h;
                    glyph.has_bitmap = true;
                }
            }
            const std::size_t bitmap_keep =
                std::max<std::size_t>(4u * 1024u, bitmap.size() + bitmap.size() / 4u + 32u);
            eui::detail::context_retain_vector_hysteresis(glyph_bitmap_scratch_, bitmap_keep,
                                                          glyph_bitmap_trim_frames_, 120u);
            const std::size_t upload_bytes =
                pad > 0 ? static_cast<std::size_t>(std::max(0, upload_w)) *
                              static_cast<std::size_t>(std::max(0, upload_h))
                        : 0u;
            const std::size_t upload_keep =
                upload_bytes == 0u ? 0u : std::max<std::size_t>(4u * 1024u, upload_bytes + upload_bytes / 4u + 32u);
            eui::detail::context_retain_vector_hysteresis(glyph_upload_scratch_, upload_keep,
                                                          glyph_upload_trim_frames_, 120u);
        }

        auto [inserted, _] = face->glyphs.emplace(cp, std::move(glyph));
        return &inserted->second;
    }

    std::string text_font_path_{};
    std::string icon_font_path_{};
    bool enable_icon_fallback_{true};
    mutable std::unordered_map<std::string, FontFace> faces_{};
    mutable std::unordered_map<std::string, std::shared_ptr<std::vector<unsigned char>>> font_blob_cache_{};
    mutable std::uint64_t frame_epoch_{0u};
    mutable std::uint64_t last_frame_hash_{0ull};
    mutable std::vector<PreparedGlyph> draw_glyphs_scratch_{};
    mutable std::vector<ModernGlVertex> text_vertices_scratch_{};
    mutable std::vector<unsigned char> atlas_zero_scratch_{};
    mutable std::vector<unsigned char> glyph_bitmap_scratch_{};
    mutable std::vector<unsigned char> glyph_upload_scratch_{};
    mutable std::uint32_t draw_glyphs_trim_frames_{0u};
    mutable std::uint32_t text_vertices_trim_frames_{0u};
    mutable std::uint32_t atlas_zero_trim_frames_{0u};
    mutable std::uint32_t glyph_bitmap_trim_frames_{0u};
    mutable std::uint32_t glyph_upload_trim_frames_{0u};
};
#else
class StbFontRenderer {
public:
    static std::string resolve_font_path_for_measure(const char*, const char*, bool) {
        return {};
    }
    explicit StbFontRenderer(const AppOptions&) {}
    void begin_frame(std::uint64_t) const {}
    void release_gl_resources() const {}
    bool draw_text(const DrawCommand&, std::string_view) const {
        return false;
    }
};
#endif

inline const Glyph& glyph_for(char ch) {
    static const Glyph kUnknown = {0x1E, 0x11, 0x02, 0x04, 0x04, 0x00, 0x04};
    static const Glyph kSpace = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const Glyph kDot = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
    static const Glyph kMinus = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    static const Glyph kGreater = {0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10};
    static const Glyph kPipe = {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    static const Glyph kPercent = {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13};
    static const Glyph kSlash = {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00};

    static const Glyph k0 = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
    static const Glyph k1 = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
    static const Glyph k2 = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    static const Glyph k3 = {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
    static const Glyph k4 = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
    static const Glyph k5 = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E};
    static const Glyph k6 = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
    static const Glyph k7 = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    static const Glyph k8 = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
    static const Glyph k9 = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C};

    static const Glyph kA = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    static const Glyph kB = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
    static const Glyph kC = {0x0F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0F};
    static const Glyph kD = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
    static const Glyph kE = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    static const Glyph kF = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    static const Glyph kG = {0x0F, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0F};
    static const Glyph kH = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    static const Glyph kI = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
    static const Glyph kJ = {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C};
    static const Glyph kK = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    static const Glyph kL = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    static const Glyph kM = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    static const Glyph kN = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    static const Glyph kO = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    static const Glyph kP = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    static const Glyph kQ = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
    static const Glyph kR = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    static const Glyph kS = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    static const Glyph kT = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    static const Glyph kU = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    static const Glyph kV = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
    static const Glyph kW = {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
    static const Glyph kX = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
    static const Glyph kY = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
    static const Glyph kZ = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};

    switch (ch) {
        case ' ':
            return kSpace;
        case '.':
            return kDot;
        case '-':
            return kMinus;
        case '>':
            return kGreater;
        case '|':
            return kPipe;
        case '%':
            return kPercent;
        case '/':
            return kSlash;
        case '0':
            return k0;
        case '1':
            return k1;
        case '2':
            return k2;
        case '3':
            return k3;
        case '4':
            return k4;
        case '5':
            return k5;
        case '6':
            return k6;
        case '7':
            return k7;
        case '8':
            return k8;
        case '9':
            return k9;
        case 'A':
            return kA;
        case 'B':
            return kB;
        case 'C':
            return kC;
        case 'D':
            return kD;
        case 'E':
            return kE;
        case 'F':
            return kF;
        case 'G':
            return kG;
        case 'H':
            return kH;
        case 'I':
            return kI;
        case 'J':
            return kJ;
        case 'K':
            return kK;
        case 'L':
            return kL;
        case 'M':
            return kM;
        case 'N':
            return kN;
        case 'O':
            return kO;
        case 'P':
            return kP;
        case 'Q':
            return kQ;
        case 'R':
            return kR;
        case 'S':
            return kS;
        case 'T':
            return kT;
        case 'U':
            return kU;
        case 'V':
            return kV;
        case 'W':
            return kW;
        case 'X':
            return kX;
        case 'Y':
            return kY;
        case 'Z':
            return kZ;
        default:
            return kUnknown;
    }
}

inline Color& legacy_gl_helper_color() {
    static thread_local Color color = rgba(1.0f, 1.0f, 1.0f, 1.0f);
    return color;
}

inline void gl_set_color(const Color& color) {
    legacy_gl_helper_color() = color;
}

inline int build_rounded_points(const Rect& rect, float radius, Point* out_points, int max_points) {
    if (max_points < 4) {
        return 0;
    }

    radius = std::clamp(radius, 0.0f, std::min(rect.w, rect.h) * 0.5f);
    if (radius <= 0.0f) {
        out_points[0] = Point{rect.x, rect.y};
        out_points[1] = Point{rect.x + rect.w, rect.y};
        out_points[2] = Point{rect.x + rect.w, rect.y + rect.h};
        out_points[3] = Point{rect.x, rect.y + rect.h};
        return 4;
    }

    const float left = rect.x;
    const float right = rect.x + rect.w;
    const float top = rect.y;
    const float bottom = rect.y + rect.h;
    const int steps = std::clamp(static_cast<int>(radius * 0.65f), 3, 10);
    const float kPi = 3.1415926f;

    int count = 0;
    auto push_point = [&](float x, float y) {
        if (count < max_points) {
            out_points[count++] = Point{x, y};
        }
    };
    auto add_arc = [&](float cx, float cy, float start_angle, float end_angle, bool include_first) {
        for (int i = 0; i <= steps; ++i) {
            if (i == 0 && !include_first) {
                continue;
            }
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const float angle = start_angle + (end_angle - start_angle) * t;
            push_point(cx + std::cos(angle) * radius, cy + std::sin(angle) * radius);
        }
    };

    add_arc(left + radius, top + radius, kPi, 1.5f * kPi, true);
    add_arc(right - radius, top + radius, 1.5f * kPi, 2.0f * kPi, false);
    add_arc(right - radius, bottom - radius, 0.0f, 0.5f * kPi, false);
    add_arc(left + radius, bottom - radius, 0.5f * kPi, kPi, false);
    return count;
}

inline void draw_filled_rect(const Rect& rect, float radius) {
    static thread_local std::vector<ModernGlVertex> verts{};
    modern_gl_prepare_scratch(verts, radius <= 0.0f ? 6u : 96u);

    if (radius <= 0.0f) {
        const Color white = legacy_gl_helper_color();
        const ModernGlVertex p0 = modern_gl_vertex(rect.x, rect.y, white);
        const ModernGlVertex p1 = modern_gl_vertex(rect.x + rect.w, rect.y, white);
        const ModernGlVertex p2 = modern_gl_vertex(rect.x + rect.w, rect.y + rect.h, white);
        const ModernGlVertex p3 = modern_gl_vertex(rect.x, rect.y + rect.h, white);
        modern_gl_push_quad(verts, p0, p1, p2, p3);
        modern_gl_draw_vertices_current_viewport(GL_TRIANGLES, verts.data(), verts.size());
        modern_gl_trim_scratch(verts, 256u);
        return;
    }

    std::array<Point, 64> points{};
    const int count = build_rounded_points(rect, radius, points.data(), static_cast<int>(points.size()));
    if (count < 3) {
        modern_gl_trim_scratch(verts, 256u);
        return;
    }

    float center_x = 0.0f;
    float center_y = 0.0f;
    for (int i = 0; i < count; ++i) {
        center_x += points[static_cast<std::size_t>(i)][0];
        center_y += points[static_cast<std::size_t>(i)][1];
    }
    center_x /= static_cast<float>(count);
    center_y /= static_cast<float>(count);

    const Color white = legacy_gl_helper_color();
    const ModernGlVertex center = modern_gl_vertex(center_x, center_y, white);
    for (int i = 0; i < count; ++i) {
        const int next = (i + 1) % count;
        const Point& p0 = points[static_cast<std::size_t>(i)];
        const Point& p1 = points[static_cast<std::size_t>(next)];
        verts.push_back(center);
        verts.push_back(modern_gl_vertex(p0[0], p0[1], white));
        verts.push_back(modern_gl_vertex(p1[0], p1[1], white));
    }
    modern_gl_draw_vertices_current_viewport(GL_TRIANGLES, verts.data(), verts.size());
    modern_gl_trim_scratch(verts, 256u);
}

inline void draw_outline_rect(const Rect& rect, float radius, float thickness) {
    static thread_local std::vector<ModernGlVertex> verts{};
    modern_gl_prepare_scratch(verts, radius <= 0.0f ? 24u : 192u);
    const Color white = legacy_gl_helper_color();
    auto push_segment = [&](const Point& p0, const Point& p1) {
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
        const ModernGlVertex v0 = modern_gl_vertex(p0[0] - nx, p0[1] - ny, white);
        const ModernGlVertex v1 = modern_gl_vertex(p0[0] + nx, p0[1] + ny, white);
        const ModernGlVertex v2 = modern_gl_vertex(p1[0] + nx, p1[1] + ny, white);
        const ModernGlVertex v3 = modern_gl_vertex(p1[0] - nx, p1[1] - ny, white);
        modern_gl_push_quad(verts, v0, v1, v2, v3);
    };

    if (radius <= 0.0f) {
        push_segment(Point{rect.x, rect.y}, Point{rect.x + rect.w, rect.y});
        push_segment(Point{rect.x + rect.w, rect.y}, Point{rect.x + rect.w, rect.y + rect.h});
        push_segment(Point{rect.x + rect.w, rect.y + rect.h}, Point{rect.x, rect.y + rect.h});
        push_segment(Point{rect.x, rect.y + rect.h}, Point{rect.x, rect.y});
        modern_gl_draw_vertices_current_viewport(GL_TRIANGLES, verts.data(), verts.size());
        modern_gl_trim_scratch(verts, 256u);
        return;
    }

    std::array<Point, 64> points{};
    const int count = build_rounded_points(rect, radius, points.data(), static_cast<int>(points.size()));
    if (count < 3) {
        modern_gl_trim_scratch(verts, 256u);
        return;
    }

    for (int i = 0; i < count; ++i) {
        const int next = (i + 1) % count;
        push_segment(points[static_cast<std::size_t>(i)], points[static_cast<std::size_t>(next)]);
    }
    modern_gl_draw_vertices_current_viewport(GL_TRIANGLES, verts.data(), verts.size());
    modern_gl_trim_scratch(verts, 256u);
}

inline float text_width(std::size_t glyph_count, float scale) {
    if (glyph_count == 0u) {
        return 0.0f;
    }
    const float advance = 6.0f * scale;
    return advance * static_cast<float>(glyph_count) - scale;
}

inline void draw_text_bitmap(const DrawCommand& cmd, std::string_view text) {
    if (text.empty()) {
        return;
    }

    const float scale = std::max(1.0f, cmd.font_size / 8.0f);
    const float advance = 6.0f * scale;
    const float width = text_width(text.size(), scale);
    const float glyph_h = 7.0f * scale;

    float x = cmd.rect.x;
    if (cmd.align == TextAlign::Center) {
        x += std::max(0.0f, (cmd.rect.w - width) * 0.5f);
    } else if (cmd.align == TextAlign::Right) {
        x += std::max(0.0f, cmd.rect.w - width);
    }
    const float y = cmd.rect.y + std::max(0.0f, (cmd.rect.h - glyph_h) * 0.5f);

    static thread_local std::vector<ModernGlVertex> verts{};
    modern_gl_prepare_scratch(verts, text.size() * 7u * 5u * 6u);
    auto push_v = [&](float px, float py) {
        verts.push_back(modern_gl_vertex(px, py, cmd.color));
    };

    float pen_x = x;
    for (char raw_ch : text) {
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(raw_ch)));
        if (ch == ' ') {
            pen_x += advance;
            continue;
        }

        const Glyph& glyph = glyph_for(ch);
        for (std::size_t row = 0; row < glyph.size(); ++row) {
            const std::uint8_t bits = glyph[row];
            for (int col = 0; col < 5; ++col) {
                const std::uint8_t mask = static_cast<std::uint8_t>(1u << (4 - col));
                if ((bits & mask) == 0u) {
                    continue;
                }
                const float px = pen_x + static_cast<float>(col) * scale;
                const float py = y + static_cast<float>(row) * scale;
                push_v(px, py);
                push_v(px + scale, py);
                push_v(px + scale, py + scale);
                push_v(px, py);
                push_v(px + scale, py + scale);
                push_v(px, py + scale);
            }
        }
        pen_x += advance;
    }
    if (!verts.empty()) {
        modern_gl_draw_vertices_current_viewport(GL_TRIANGLES, verts.data(), verts.size());
    }
    modern_gl_trim_scratch(verts, 4096u);
}
