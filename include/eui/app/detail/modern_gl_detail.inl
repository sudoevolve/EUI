#ifndef GLchar
using GLchar = char;
#endif

#ifndef GLsizeiptr
using GLsizeiptr = std::ptrdiff_t;
#endif

#ifndef GLintptr
using GLintptr = std::ptrdiff_t;
#endif

#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH 0x8B84
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88E0
#endif
#ifndef GL_VERSION
#define GL_VERSION 0x1F02
#endif

using GlProcAddressFn = void* (*)(const char*);

using GlCreateShaderProc = GLuint(APIENTRY*)(GLenum type);
using GlShaderSourceProc = void(APIENTRY*)(GLuint shader, GLsizei count, const GLchar* const* string,
                                           const GLint* length);
using GlCompileShaderProc = void(APIENTRY*)(GLuint shader);
using GlGetShaderivProc = void(APIENTRY*)(GLuint shader, GLenum pname, GLint* params);
using GlGetShaderInfoLogProc = void(APIENTRY*)(GLuint shader, GLsizei buf_size, GLsizei* length, GLchar* info_log);
using GlDeleteShaderProc = void(APIENTRY*)(GLuint shader);
using GlCreateProgramProc = GLuint(APIENTRY*)(void);
using GlAttachShaderProc = void(APIENTRY*)(GLuint program, GLuint shader);
using GlBindAttribLocationProc = void(APIENTRY*)(GLuint program, GLuint index, const GLchar* name);
using GlLinkProgramProc = void(APIENTRY*)(GLuint program);
using GlGetProgramivProc = void(APIENTRY*)(GLuint program, GLenum pname, GLint* params);
using GlGetProgramInfoLogProc = void(APIENTRY*)(GLuint program, GLsizei buf_size, GLsizei* length,
                                                GLchar* info_log);
using GlDeleteProgramProc = void(APIENTRY*)(GLuint program);
using GlUseProgramProc = void(APIENTRY*)(GLuint program);
using GlGetUniformLocationProc = GLint(APIENTRY*)(GLuint program, const GLchar* name);
using GlUniform2fProc = void(APIENTRY*)(GLint location, GLfloat v0, GLfloat v1);
using GlUniform1iProc = void(APIENTRY*)(GLint location, GLint v0);
using GlActiveTextureProc = void(APIENTRY*)(GLenum texture);
using GlGenBuffersProc = void(APIENTRY*)(GLsizei n, GLuint* buffers);
using GlDeleteBuffersProc = void(APIENTRY*)(GLsizei n, const GLuint* buffers);
using GlBindBufferProc = void(APIENTRY*)(GLenum target, GLuint buffer);
using GlBufferDataProc = void(APIENTRY*)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
using GlBufferSubDataProc = void(APIENTRY*)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
using GlEnableVertexAttribArrayProc = void(APIENTRY*)(GLuint index);
using GlDisableVertexAttribArrayProc = void(APIENTRY*)(GLuint index);
using GlVertexAttribPointerProc = void(APIENTRY*)(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                                  GLsizei stride, const void* pointer);

struct ModernGlVertex {
    float x;
    float y;
    float r;
    float g;
    float b;
    float a;
    float u;
    float v;
};

enum class ModernGlTextureMode : int {
    None = 0,
    AlphaMask = 1,
    Rgba = 2,
};

struct ModernGlSharedState {
    bool initialized{false};
    bool available{false};
    bool init_error_logged{false};
    bool is_gles{false};

    GLuint program{0u};
    GLuint vbo{0u};
    std::size_t vbo_capacity_bytes{0u};

    GLint viewport_uniform{-1};
    GLint texture_uniform{-1};
    GLint texture_mode_uniform{-1};
    GLint blur_params_uniform{-1};
    GLint blur_uv_min_uniform{-1};
    GLint blur_uv_max_uniform{-1};

    GlCreateShaderProc create_shader{nullptr};
    GlShaderSourceProc shader_source{nullptr};
    GlCompileShaderProc compile_shader{nullptr};
    GlGetShaderivProc get_shader_iv{nullptr};
    GlGetShaderInfoLogProc get_shader_info_log{nullptr};
    GlDeleteShaderProc delete_shader{nullptr};
    GlCreateProgramProc create_program{nullptr};
    GlAttachShaderProc attach_shader{nullptr};
    GlBindAttribLocationProc bind_attrib_location{nullptr};
    GlLinkProgramProc link_program{nullptr};
    GlGetProgramivProc get_program_iv{nullptr};
    GlGetProgramInfoLogProc get_program_info_log{nullptr};
    GlDeleteProgramProc delete_program{nullptr};
    GlUseProgramProc use_program{nullptr};
    GlGetUniformLocationProc get_uniform_location{nullptr};
    GlUniform2fProc uniform_2f{nullptr};
    GlUniform1iProc uniform_1i{nullptr};
    GlActiveTextureProc active_texture{nullptr};
    GlGenBuffersProc gen_buffers{nullptr};
    GlDeleteBuffersProc delete_buffers{nullptr};
    GlBindBufferProc bind_buffer{nullptr};
    GlBufferDataProc buffer_data{nullptr};
    GlBufferSubDataProc buffer_sub_data{nullptr};
    GlEnableVertexAttribArrayProc enable_vertex_attrib_array{nullptr};
    GlDisableVertexAttribArrayProc disable_vertex_attrib_array{nullptr};
    GlVertexAttribPointerProc vertex_attrib_pointer{nullptr};
};

inline GlProcAddressFn& modern_gl_proc_loader() {
    static GlProcAddressFn loader = nullptr;
    return loader;
}

inline ModernGlSharedState& modern_gl_shared_state() {
    static ModernGlSharedState state{};
    return state;
}

inline void modern_gl_set_proc_loader(GlProcAddressFn loader) {
    modern_gl_proc_loader() = loader;
}

inline void* modern_gl_get_proc_address(const char* name) {
    if (name == nullptr || name[0] == '\0') {
        return nullptr;
    }
    GlProcAddressFn loader = modern_gl_proc_loader();
    if (loader == nullptr) {
        return nullptr;
    }
    return loader(name);
}

template <typename ProcT>
inline bool modern_gl_load_proc(ProcT& out_proc, const char* name) {
    out_proc = reinterpret_cast<ProcT>(modern_gl_get_proc_address(name));
    return out_proc != nullptr;
}

inline bool modern_gl_load_functions() {
    ModernGlSharedState& state = modern_gl_shared_state();

    return modern_gl_load_proc(state.create_shader, "glCreateShader") &&
           modern_gl_load_proc(state.shader_source, "glShaderSource") &&
           modern_gl_load_proc(state.compile_shader, "glCompileShader") &&
           modern_gl_load_proc(state.get_shader_iv, "glGetShaderiv") &&
           modern_gl_load_proc(state.get_shader_info_log, "glGetShaderInfoLog") &&
           modern_gl_load_proc(state.delete_shader, "glDeleteShader") &&
           modern_gl_load_proc(state.create_program, "glCreateProgram") &&
           modern_gl_load_proc(state.attach_shader, "glAttachShader") &&
           modern_gl_load_proc(state.bind_attrib_location, "glBindAttribLocation") &&
           modern_gl_load_proc(state.link_program, "glLinkProgram") &&
           modern_gl_load_proc(state.get_program_iv, "glGetProgramiv") &&
           modern_gl_load_proc(state.get_program_info_log, "glGetProgramInfoLog") &&
           modern_gl_load_proc(state.delete_program, "glDeleteProgram") &&
           modern_gl_load_proc(state.use_program, "glUseProgram") &&
           modern_gl_load_proc(state.get_uniform_location, "glGetUniformLocation") &&
           modern_gl_load_proc(state.uniform_2f, "glUniform2f") &&
           modern_gl_load_proc(state.uniform_1i, "glUniform1i") &&
           modern_gl_load_proc(state.active_texture, "glActiveTexture") &&
           modern_gl_load_proc(state.gen_buffers, "glGenBuffers") &&
           modern_gl_load_proc(state.delete_buffers, "glDeleteBuffers") &&
           modern_gl_load_proc(state.bind_buffer, "glBindBuffer") &&
           modern_gl_load_proc(state.buffer_data, "glBufferData") &&
           modern_gl_load_proc(state.buffer_sub_data, "glBufferSubData") &&
           modern_gl_load_proc(state.enable_vertex_attrib_array, "glEnableVertexAttribArray") &&
           modern_gl_load_proc(state.disable_vertex_attrib_array, "glDisableVertexAttribArray") &&
           modern_gl_load_proc(state.vertex_attrib_pointer, "glVertexAttribPointer");
}

inline bool modern_gl_report_error_once(const char* message) {
    ModernGlSharedState& state = modern_gl_shared_state();
    if (!state.init_error_logged) {
        std::cerr << message << std::endl;
        state.init_error_logged = true;
    }
    return false;
}

inline bool modern_gl_compile_shader(GLuint& out_shader, GLenum type, const char* source) {
    ModernGlSharedState& state = modern_gl_shared_state();
    out_shader = state.create_shader(type);
    if (out_shader == 0u) {
        return modern_gl_report_error_once("EUI modern GL: failed to create shader.");
    }

    const GLchar* sources[] = {source};
    state.shader_source(out_shader, 1, sources, nullptr);
    state.compile_shader(out_shader);

    GLint compiled = 0;
    state.get_shader_iv(out_shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == 0) {
        GLint log_len = 0;
        state.get_shader_iv(out_shader, GL_INFO_LOG_LENGTH, &log_len);
        std::string log(static_cast<std::size_t>(std::max(1, log_len)), '\0');
        GLsizei written = 0;
        state.get_shader_info_log(out_shader, static_cast<GLsizei>(log.size()), &written, log.data());
        state.delete_shader(out_shader);
        out_shader = 0u;
        const std::string message = "EUI modern GL shader compile failed: " + log;
        return modern_gl_report_error_once(message.c_str());
    }
    return true;
}

inline bool modern_gl_link_program(GLuint vertex_shader, GLuint fragment_shader) {
    ModernGlSharedState& state = modern_gl_shared_state();
    state.program = state.create_program();
    if (state.program == 0u) {
        return modern_gl_report_error_once("EUI modern GL: failed to create shader program.");
    }

    state.attach_shader(state.program, vertex_shader);
    state.attach_shader(state.program, fragment_shader);
    state.bind_attrib_location(state.program, 0u, "a_position");
    state.bind_attrib_location(state.program, 1u, "a_color");
    state.bind_attrib_location(state.program, 2u, "a_uv");
    state.link_program(state.program);

    GLint linked = 0;
    state.get_program_iv(state.program, GL_LINK_STATUS, &linked);
    if (linked == 0) {
        GLint log_len = 0;
        state.get_program_iv(state.program, GL_INFO_LOG_LENGTH, &log_len);
        std::string log(static_cast<std::size_t>(std::max(1, log_len)), '\0');
        GLsizei written = 0;
        state.get_program_info_log(state.program, static_cast<GLsizei>(log.size()), &written, log.data());
        state.delete_program(state.program);
        state.program = 0u;
        const std::string message = "EUI modern GL program link failed: " + log;
        return modern_gl_report_error_once(message.c_str());
    }

    state.viewport_uniform = state.get_uniform_location(state.program, "u_viewport");
    state.texture_uniform = state.get_uniform_location(state.program, "u_texture");
    state.texture_mode_uniform = state.get_uniform_location(state.program, "u_texture_mode");
    state.blur_params_uniform = state.get_uniform_location(state.program, "u_blur_params");
    state.blur_uv_min_uniform = state.get_uniform_location(state.program, "u_blur_uv_min");
    state.blur_uv_max_uniform = state.get_uniform_location(state.program, "u_blur_uv_max");
    if (state.viewport_uniform < 0 || state.texture_uniform < 0 || state.texture_mode_uniform < 0 ||
        state.blur_params_uniform < 0 || state.blur_uv_min_uniform < 0 || state.blur_uv_max_uniform < 0) {
        return modern_gl_report_error_once("EUI modern GL: failed to resolve shader uniforms.");
    }
    return true;
}

inline bool modern_gl_detect_gles_context() {
#ifdef EUI_OPENGL_ES
    return true;
#else
    const GLubyte* version_bytes = glGetString(GL_VERSION);
    if (version_bytes == nullptr) {
        return false;
    }
    const std::string_view version_text(reinterpret_cast<const char*>(version_bytes));
    return version_text.find("OpenGL ES") != std::string_view::npos;
#endif
}

inline bool modern_gl_context_is_gles() {
    return modern_gl_detect_gles_context();
}

inline bool modern_gl_ensure_ready() {
    ModernGlSharedState& state = modern_gl_shared_state();
    if (state.initialized) {
        return state.available;
    }
    state.initialized = true;

    if (!modern_gl_load_functions()) {
        state.available = false;
        return modern_gl_report_error_once("EUI modern GL: required GL 2.0 buffer/shader functions are unavailable.");
    }

    state.is_gles = modern_gl_detect_gles_context();

    static constexpr const char* k_vertex_shader_source_desktop =
        "attribute vec2 a_position;\n"
        "attribute vec4 a_color;\n"
        "attribute vec2 a_uv;\n"
        "uniform vec2 u_viewport;\n"
        "varying vec4 v_color;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  vec2 clip = vec2((a_position.x / u_viewport.x) * 2.0 - 1.0,\n"
        "                   1.0 - (a_position.y / u_viewport.y) * 2.0);\n"
        "  gl_Position = vec4(clip, 0.0, 1.0);\n"
        "  v_color = a_color;\n"
        "  v_uv = a_uv;\n"
        "}\n";

    static constexpr const char* k_vertex_shader_source_gles =
        "precision mediump float;\n"
        "attribute vec2 a_position;\n"
        "attribute vec4 a_color;\n"
        "attribute vec2 a_uv;\n"
        "uniform vec2 u_viewport;\n"
        "varying vec4 v_color;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "  vec2 clip = vec2((a_position.x / u_viewport.x) * 2.0 - 1.0,\n"
        "                   1.0 - (a_position.y / u_viewport.y) * 2.0);\n"
        "  gl_Position = vec4(clip, 0.0, 1.0);\n"
        "  v_color = a_color;\n"
        "  v_uv = a_uv;\n"
        "}\n";

    static constexpr const char* k_fragment_shader_source_desktop =
        "uniform sampler2D u_texture;\n"
        "uniform int u_texture_mode;\n"
        "uniform vec2 u_blur_params;\n"
        "uniform vec2 u_blur_uv_min;\n"
        "uniform vec2 u_blur_uv_max;\n"
        "varying vec4 v_color;\n"
        "varying vec2 v_uv;\n"
        "float blur_rand(vec2 co) {\n"
        "  return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);\n"
        "}\n"
        "void main() {\n"
        "  vec4 color = v_color;\n"
        "  if (u_texture_mode == 1) {\n"
        "    color *= vec4(1.0, 1.0, 1.0, texture2D(u_texture, v_uv).r);\n"
        "  } else if (u_texture_mode == 2) {\n"
        "    color *= texture2D(u_texture, v_uv);\n"
        "  } else if (u_texture_mode == 3) {\n"
        "    vec4 base = texture2D(u_texture, v_uv);\n"
        "    vec4 blurred = vec4(0.0);\n"
        "    float blur_amount = u_blur_params.x;\n"
        "    float mix_alpha = clamp(u_blur_params.y, 0.0, 1.0);\n"
        "    const int repeats = 24;\n"
        "    const float tau = 6.28318530718;\n"
        "    for (int i = 0; i < repeats; ++i) {\n"
        "      float fi = float(i);\n"
        "      float angle = (fi / float(repeats)) * tau;\n"
        "      vec2 dir = vec2(cos(angle), sin(angle));\n"
        "      float j0 = blur_rand(vec2(fi, v_uv.x + v_uv.y)) + blur_amount;\n"
        "      vec2 uv0 = clamp(v_uv + dir * (j0 * blur_amount), u_blur_uv_min, u_blur_uv_max);\n"
        "      blurred += texture2D(u_texture, uv0) * 0.5;\n"
        "      float j1 = blur_rand(vec2(fi + 2.0, v_uv.x + v_uv.y + 24.0)) + blur_amount;\n"
        "      vec2 uv1 = clamp(v_uv + dir * (j1 * blur_amount), u_blur_uv_min, u_blur_uv_max);\n"
        "      blurred += texture2D(u_texture, uv1) * 0.5;\n"
        "    }\n"
        "    blurred /= float(repeats);\n"
        "    color *= vec4(mix(base.rgb, blurred.rgb, mix_alpha), 1.0);\n"
        "  }\n"
        "  gl_FragColor = color;\n"
        "}\n";

    static constexpr const char* k_fragment_shader_source_gles =
        "precision mediump float;\n"
        "uniform sampler2D u_texture;\n"
        "uniform int u_texture_mode;\n"
        "uniform vec2 u_blur_params;\n"
        "uniform vec2 u_blur_uv_min;\n"
        "uniform vec2 u_blur_uv_max;\n"
        "varying vec4 v_color;\n"
        "varying vec2 v_uv;\n"
        "float blur_rand(vec2 co) {\n"
        "  return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);\n"
        "}\n"
        "void main() {\n"
        "  vec4 color = v_color;\n"
        "  if (u_texture_mode == 1) {\n"
        "    color *= vec4(1.0, 1.0, 1.0, texture2D(u_texture, v_uv).r);\n"
        "  } else if (u_texture_mode == 2) {\n"
        "    color *= texture2D(u_texture, v_uv);\n"
        "  } else if (u_texture_mode == 3) {\n"
        "    vec4 base = texture2D(u_texture, v_uv);\n"
        "    vec4 blurred = vec4(0.0);\n"
        "    float blur_amount = u_blur_params.x;\n"
        "    float mix_alpha = clamp(u_blur_params.y, 0.0, 1.0);\n"
        "    const int repeats = 24;\n"
        "    const float tau = 6.28318530718;\n"
        "    for (int i = 0; i < repeats; ++i) {\n"
        "      float fi = float(i);\n"
        "      float angle = (fi / float(repeats)) * tau;\n"
        "      vec2 dir = vec2(cos(angle), sin(angle));\n"
        "      float j0 = blur_rand(vec2(fi, v_uv.x + v_uv.y)) + blur_amount;\n"
        "      vec2 uv0 = clamp(v_uv + dir * (j0 * blur_amount), u_blur_uv_min, u_blur_uv_max);\n"
        "      blurred += texture2D(u_texture, uv0) * 0.5;\n"
        "      float j1 = blur_rand(vec2(fi + 2.0, v_uv.x + v_uv.y + 24.0)) + blur_amount;\n"
        "      vec2 uv1 = clamp(v_uv + dir * (j1 * blur_amount), u_blur_uv_min, u_blur_uv_max);\n"
        "      blurred += texture2D(u_texture, uv1) * 0.5;\n"
        "    }\n"
        "    blurred /= float(repeats);\n"
        "    color *= vec4(mix(base.rgb, blurred.rgb, mix_alpha), 1.0);\n"
        "  }\n"
        "  gl_FragColor = color;\n"
        "}\n";

    const char* vertex_shader_source = state.is_gles ? k_vertex_shader_source_gles : k_vertex_shader_source_desktop;
    const char* fragment_shader_source =
        state.is_gles ? k_fragment_shader_source_gles : k_fragment_shader_source_desktop;

    GLuint vertex_shader = 0u;
    GLuint fragment_shader = 0u;
    if (!modern_gl_compile_shader(vertex_shader, GL_VERTEX_SHADER, vertex_shader_source) ||
        !modern_gl_compile_shader(fragment_shader, GL_FRAGMENT_SHADER, fragment_shader_source) ||
        !modern_gl_link_program(vertex_shader, fragment_shader)) {
        if (vertex_shader != 0u) {
            state.delete_shader(vertex_shader);
        }
        if (fragment_shader != 0u) {
            state.delete_shader(fragment_shader);
        }
        state.available = false;
        return false;
    }

    state.delete_shader(vertex_shader);
    state.delete_shader(fragment_shader);
    state.gen_buffers(1, &state.vbo);
    state.available = state.vbo != 0u;
    if (!state.available) {
        return modern_gl_report_error_once("EUI modern GL: failed to create vertex buffer.");
    }
    return true;
}

inline void modern_gl_release_shared_resources() {
    ModernGlSharedState& state = modern_gl_shared_state();
    if (state.delete_buffers != nullptr && state.vbo != 0u) {
        state.delete_buffers(1, &state.vbo);
    }
    if (state.delete_program != nullptr && state.program != 0u) {
        state.delete_program(state.program);
    }
    state = ModernGlSharedState{};
}

inline ModernGlVertex modern_gl_vertex(float x, float y, const Color& color, float u = 0.0f, float v = 0.0f) {
    return ModernGlVertex{x, y, color.r, color.g, color.b, color.a, u, v};
}

inline void modern_gl_push_triangle(std::vector<ModernGlVertex>& out_vertices, const ModernGlVertex& a,
                                    const ModernGlVertex& b, const ModernGlVertex& c) {
    out_vertices.push_back(a);
    out_vertices.push_back(b);
    out_vertices.push_back(c);
}

inline void modern_gl_push_quad(std::vector<ModernGlVertex>& out_vertices, const ModernGlVertex& p0,
                                const ModernGlVertex& p1, const ModernGlVertex& p2, const ModernGlVertex& p3) {
    modern_gl_push_triangle(out_vertices, p0, p1, p2);
    modern_gl_push_triangle(out_vertices, p0, p2, p3);
}

inline std::size_t modern_gl_next_buffer_capacity(std::size_t required_bytes) {
    std::size_t capacity = 4096u;
    while (capacity < required_bytes && capacity <= (std::numeric_limits<std::size_t>::max() / 2u)) {
        capacity *= 2u;
    }
    return std::max(capacity, required_bytes);
}

template <typename T>
inline void modern_gl_prepare_scratch(std::vector<T>& scratch, std::size_t min_capacity = 0u) {
    scratch.clear();
    if (scratch.capacity() < min_capacity) {
        scratch.reserve(min_capacity);
    }
}

template <typename T>
inline void modern_gl_trim_scratch(std::vector<T>& scratch, std::size_t keep_capacity) {
    scratch.clear();
    if (scratch.capacity() <= keep_capacity) {
        return;
    }
    std::vector<T> trimmed{};
    if (keep_capacity > 0u) {
        trimmed.reserve(keep_capacity);
    }
    scratch.swap(trimmed);
}

inline bool modern_gl_draw_vertices(GLenum primitive, const ModernGlVertex* vertices, std::size_t vertex_count,
                                    int width, int height, GLuint texture = 0u,
                                    ModernGlTextureMode texture_mode = ModernGlTextureMode::None) {
    if (vertices == nullptr || vertex_count == 0u) {
        return true;
    }
    if (!modern_gl_ensure_ready()) {
        return false;
    }

    ModernGlSharedState& state = modern_gl_shared_state();
    state.use_program(state.program);
    state.uniform_2f(state.viewport_uniform, static_cast<float>(std::max(1, width)),
                     static_cast<float>(std::max(1, height)));
    state.uniform_1i(state.texture_uniform, 0);
    state.uniform_1i(state.texture_mode_uniform, static_cast<int>(texture_mode));
    state.active_texture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,
                  texture_mode == ModernGlTextureMode::None ? 0u : texture);

    state.bind_buffer(GL_ARRAY_BUFFER, state.vbo);
    const std::size_t required_bytes = vertex_count * sizeof(ModernGlVertex);
    if (required_bytes > state.vbo_capacity_bytes) {
        state.vbo_capacity_bytes = modern_gl_next_buffer_capacity(required_bytes);
        state.buffer_data(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(state.vbo_capacity_bytes), nullptr,
                          GL_DYNAMIC_DRAW);
    }
    state.buffer_sub_data(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(required_bytes), vertices);

    state.enable_vertex_attrib_array(0u);
    state.enable_vertex_attrib_array(1u);
    state.enable_vertex_attrib_array(2u);
    state.vertex_attrib_pointer(0u, 2, GL_FLOAT, GL_FALSE, sizeof(ModernGlVertex),
                                reinterpret_cast<const void*>(offsetof(ModernGlVertex, x)));
    state.vertex_attrib_pointer(1u, 4, GL_FLOAT, GL_FALSE, sizeof(ModernGlVertex),
                                reinterpret_cast<const void*>(offsetof(ModernGlVertex, r)));
    state.vertex_attrib_pointer(2u, 2, GL_FLOAT, GL_FALSE, sizeof(ModernGlVertex),
                                reinterpret_cast<const void*>(offsetof(ModernGlVertex, u)));

    glDrawArrays(primitive, 0, static_cast<GLsizei>(vertex_count));

    state.disable_vertex_attrib_array(2u);
    state.disable_vertex_attrib_array(1u);
    state.disable_vertex_attrib_array(0u);
    state.bind_buffer(GL_ARRAY_BUFFER, 0u);
    glBindTexture(GL_TEXTURE_2D, 0u);
    state.use_program(0u);
    return true;
}

inline bool modern_gl_draw_vertices_current_viewport(GLenum primitive, const ModernGlVertex* vertices,
                                                     std::size_t vertex_count, GLuint texture = 0u,
                                                     ModernGlTextureMode texture_mode = ModernGlTextureMode::None) {
    GLint viewport[4] = {0, 0, 1, 1};
    glGetIntegerv(GL_VIEWPORT, viewport);
    return modern_gl_draw_vertices(primitive, vertices, vertex_count, std::max(1, viewport[2]),
                                   std::max(1, viewport[3]), texture, texture_mode);
}

inline bool modern_gl_draw_backdrop_blur_vertices(const ModernGlVertex* vertices, std::size_t vertex_count, int width,
                                                  int height, GLuint texture, float blur_amount, float mix_alpha,
                                                  float uv_min_x, float uv_min_y, float uv_max_x, float uv_max_y) {
    if (vertices == nullptr || vertex_count == 0u) {
        return true;
    }
    if (!modern_gl_ensure_ready()) {
        return false;
    }

    ModernGlSharedState& state = modern_gl_shared_state();
    state.use_program(state.program);
    state.uniform_2f(state.viewport_uniform, static_cast<float>(std::max(1, width)),
                     static_cast<float>(std::max(1, height)));
    state.uniform_1i(state.texture_uniform, 0);
    state.uniform_1i(state.texture_mode_uniform, 3);
    state.uniform_2f(state.blur_params_uniform, blur_amount, mix_alpha);
    state.uniform_2f(state.blur_uv_min_uniform, uv_min_x, uv_min_y);
    state.uniform_2f(state.blur_uv_max_uniform, uv_max_x, uv_max_y);
    state.active_texture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    state.bind_buffer(GL_ARRAY_BUFFER, state.vbo);
    const std::size_t required_bytes = vertex_count * sizeof(ModernGlVertex);
    if (required_bytes > state.vbo_capacity_bytes) {
        state.vbo_capacity_bytes = modern_gl_next_buffer_capacity(required_bytes);
        state.buffer_data(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(state.vbo_capacity_bytes), nullptr,
                          GL_DYNAMIC_DRAW);
    }
    state.buffer_sub_data(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(required_bytes), vertices);

    state.enable_vertex_attrib_array(0u);
    state.enable_vertex_attrib_array(1u);
    state.enable_vertex_attrib_array(2u);
    state.vertex_attrib_pointer(0u, 2, GL_FLOAT, GL_FALSE, sizeof(ModernGlVertex),
                                reinterpret_cast<const void*>(offsetof(ModernGlVertex, x)));
    state.vertex_attrib_pointer(1u, 4, GL_FLOAT, GL_FALSE, sizeof(ModernGlVertex),
                                reinterpret_cast<const void*>(offsetof(ModernGlVertex, r)));
    state.vertex_attrib_pointer(2u, 2, GL_FLOAT, GL_FALSE, sizeof(ModernGlVertex),
                                reinterpret_cast<const void*>(offsetof(ModernGlVertex, u)));

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertex_count));

    state.disable_vertex_attrib_array(2u);
    state.disable_vertex_attrib_array(1u);
    state.disable_vertex_attrib_array(0u);
    state.bind_buffer(GL_ARRAY_BUFFER, 0u);
    glBindTexture(GL_TEXTURE_2D, 0u);
    state.use_program(0u);
    return true;
}
