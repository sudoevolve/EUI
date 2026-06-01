#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#include "core/render/text.h"
#include "core/render/render_backend.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#if defined(EUI_HAS_HARFBUZZ)
#include <hb.h>
#include <hb-ft.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace core {

namespace {

constexpr const char* kDefaultUiFontFile = "JingNanJunJunTi-JinNanJunJunTi-Bold-2.ttf";
constexpr const char* kDefaultIconFontFile = "Font Awesome 7 Free-Solid-900.otf";
constexpr int kGrayAtlasSize = 2048;
constexpr int kColorAtlasSize = 1024;
constexpr FT_Int32 kGlyphLoadFlags = FT_LOAD_DEFAULT | FT_LOAD_COLOR | FT_LOAD_NO_SVG | FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT;

struct FontFace {
    std::string path;
    FT_Face face = nullptr;
    float size = 16.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    float lineGap = 0.0f;
    float glyphScale = 1.0f;
    bool colored = false;

    FontFace() = default;
    FontFace(const FontFace&) = delete;
    FontFace& operator=(const FontFace&) = delete;

    FontFace(FontFace&& other) noexcept {
        *this = std::move(other);
    }

    FontFace& operator=(FontFace&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        if (face) {
            FT_Done_Face(face);
        }
        path = std::move(other.path);
        face = other.face;
        size = other.size;
        ascent = other.ascent;
        descent = other.descent;
        lineGap = other.lineGap;
        glyphScale = other.glyphScale;
        colored = other.colored;
        other.face = nullptr;
        return *this;
    }

    ~FontFace() {
        if (face) {
            FT_Done_Face(face);
        }
    }
};

struct FontInfoHolder {
    std::vector<FontFace> faces;
    std::vector<std::string> lazyFallbackPaths;
};

struct AtlasPage {
    int width = 0;
    int height = 0;
    int channels = 1;
    int x = 1;
    int y = 1;
    int rowHeight = 0;
    std::uint64_t generation = 0;
    std::vector<unsigned char> pixels;
    std::unordered_map<std::string, TextPrimitive::Glyph> glyphs;
};

struct SharedTextAtlas {
    AtlasPage gray;
    AtlasPage color;
    int references = 0;
};

struct CodepointInfo {
    unsigned int codepoint = 0;
    size_t byteOffset = 0;
    size_t byteLength = 0;
};

struct TextRun {
    size_t faceIndex = 0;
    size_t sourceOffset = 0;
    std::string text;
    std::vector<CodepointInfo> codepoints;
    bool rtl = false;
};

unsigned int readUtf8Codepoint(const std::string& text, size_t& index);

FT_Library sharedFreeTypeLibrary() {
    static FT_Library library = [] {
        FT_Library created = nullptr;
        return FT_Init_FreeType(&created) == 0 ? created : nullptr;
    }();
    return library;
}

SharedTextAtlas& sharedTextAtlas() {
    static SharedTextAtlas atlas;
    return atlas;
}

bool ensureAtlasPage(AtlasPage& page, int width, int height, int channels) {
    if (!page.pixels.empty()) {
        return true;
    }

    page.width = width;
    page.height = height;
    page.channels = channels;
    page.x = 1;
    page.y = 1;
    page.rowHeight = 0;
    page.generation = 1;
    page.pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * static_cast<std::size_t>(channels), 0);
    return !page.pixels.empty();
}

bool retainSharedTextAtlas() {
    SharedTextAtlas& atlas = sharedTextAtlas();
    ++atlas.references;
    if (ensureAtlasPage(atlas.gray, kGrayAtlasSize, kGrayAtlasSize, 1)) {
        return true;
    }
    atlas.references = std::max(0, atlas.references - 1);
    return false;
}

void releaseAtlasPage(AtlasPage& page) {
    page = {};
}

void releaseSharedTextAtlas() {
    SharedTextAtlas& atlas = sharedTextAtlas();
    atlas.references = std::max(0, atlas.references - 1);
    if (atlas.references > 0) {
        return;
    }

    releaseAtlasPage(atlas.gray);
    releaseAtlasPage(atlas.color);
}

std::uint64_t makeGlyphKey(size_t faceIndex, unsigned int glyphIndex) {
    return ((static_cast<std::uint64_t>(faceIndex) + 1ULL) << 32) | static_cast<std::uint64_t>(glyphIndex);
}

size_t faceIndexFromGlyphKey(std::uint64_t key) {
    return static_cast<size_t>((key >> 32) - 1ULL);
}

unsigned int glyphIndexFromGlyphKey(std::uint64_t key) {
    return static_cast<unsigned int>(key & 0xffffffffULL);
}

std::string glyphCacheKey(const FontFace& face, float fontSize, unsigned int glyphIndex, bool colored) {
    return face.path + "#" +
           std::to_string(static_cast<int>(std::round(fontSize * 64.0f))) + "#" +
           (colored ? "color#" : "gray#") +
           std::to_string(glyphIndex);
}

std::string existingPath(const std::filesystem::path& path) {
    std::error_code error;
    if (std::filesystem::exists(path, error)) {
        return path.string();
    }
    return {};
}

std::string& defaultUiFontFileOverride() {
    static std::string value;
    return value;
}

std::string& defaultIconFontFileOverride() {
    static std::string value;
    return value;
}

std::string resolveFontFilePath(const std::string& path);

std::filesystem::path executableDirectory() {
#ifdef _WIN32
    std::vector<char> buffer(MAX_PATH);
    DWORD length = 0;
    while (true) {
        length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size() - 1) {
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path(buffer.data()).parent_path();
#elif defined(__APPLE__)
    std::vector<char> buffer(4096);
    uint32_t size = static_cast<uint32_t>(buffer.size());
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        buffer.resize(size);
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
            return {};
        }
    }
    std::error_code error;
    return std::filesystem::absolute(std::filesystem::path(buffer.data()), error).parent_path();
#elif defined(__linux__)
    std::vector<char> buffer(4096);
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0) {
        return {};
    }
    buffer[static_cast<size_t>(length)] = '\0';
    std::error_code error;
    return std::filesystem::absolute(std::filesystem::path(buffer.data()), error).parent_path();
#else
    return {};
#endif
}

std::filesystem::path sourceRootDirectory() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::string resolveProjectAssetPath(const std::string& filename) {
    const std::filesystem::path sourceRoot = sourceRootDirectory();
    const std::filesystem::path exeDir = executableDirectory();
    const std::filesystem::path candidates[] = {
        exeDir / "assets" / filename,
        std::filesystem::path("assets") / filename,
        std::filesystem::path("..") / "assets" / filename,
        std::filesystem::path("..") / ".." / "assets" / filename,
        sourceRoot / "assets" / filename
    };

    for (const auto& candidate : candidates) {
        if (const std::string path = existingPath(candidate); !path.empty()) {
            return path;
        }
    }
    return (sourceRoot / "assets" / filename).string();
}

std::string resolveDefaultUiFontPath() {
    const std::string& override = defaultUiFontFileOverride();
    return override.empty() ? resolveProjectAssetPath(kDefaultUiFontFile) : resolveFontFilePath(override);
}

std::string resolveDefaultIconFontPath() {
    const std::string& override = defaultIconFontFileOverride();
    return override.empty() ? resolveProjectAssetPath(kDefaultIconFontFile) : resolveFontFilePath(override);
}

std::string resolveFontFilePath(const std::string& path) {
    const std::filesystem::path raw(path);
    if (const std::string existing = existingPath(raw); !existing.empty()) {
        return existing;
    }

    const std::filesystem::path sourceRoot = sourceRootDirectory();
    const std::filesystem::path exeDir = executableDirectory();
    const std::filesystem::path candidates[] = {
        exeDir / "assets" / raw.filename(),
        exeDir / raw,
        std::filesystem::path("assets") / raw.filename(),
        std::filesystem::path("..") / "assets" / raw.filename(),
        std::filesystem::path("..") / ".." / "assets" / raw.filename(),
        sourceRoot / "assets" / raw.filename()
    };

    for (const auto& candidate : candidates) {
        if (const std::string existing = existingPath(candidate); !existing.empty()) {
            return existing;
        }
    }
    return path;
}

bool isEmojiFontPath(const std::string& path) {
    const std::string filename = std::filesystem::path(path).filename().string();
    return filename.find("Emoji") != std::string::npos ||
           filename.find("emoji") != std::string::npos ||
           filename.find("seguiemj") != std::string::npos;
}

bool loadFontFace(const std::string& path, float fontSize, FontFace& face) {
    FT_Library library = sharedFreeTypeLibrary();
    if (!library) {
        return false;
    }

    FT_Face loadedFace = nullptr;
    if (FT_New_Face(library, path.c_str(), 0, &loadedFace) != 0 || !loadedFace) {
        return false;
    }

    const bool emojiFont = isEmojiFontPath(path);
    float pixelHeightScale = 1.0f;
    if (loadedFace->units_per_EM > 0 && loadedFace->ascender != loadedFace->descender) {
        const float designHeight = static_cast<float>(loadedFace->ascender - loadedFace->descender);
        pixelHeightScale = static_cast<float>(loadedFace->units_per_EM) / designHeight;
    }
    const float scaledFontSize = std::max(1.0f, fontSize * pixelHeightScale);

    FT_Error sizeError = 1;
    float glyphScale = 1.0f;
    if ((emojiFont || FT_HAS_COLOR(loadedFace)) && loadedFace->num_fixed_sizes > 0) {
        const float targetPpem = scaledFontSize * 64.0f;
        int bestStrike = 0;
        float bestDistance = std::numeric_limits<float>::max();
        for (int i = 0; i < loadedFace->num_fixed_sizes; ++i) {
            const float strikePpem = static_cast<float>(loadedFace->available_sizes[i].y_ppem);
            const float distance = std::fabs(strikePpem - targetPpem);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestStrike = i;
            }
        }
        sizeError = FT_Select_Size(loadedFace, bestStrike);
        const float strikePpem = static_cast<float>(loadedFace->available_sizes[bestStrike].y_ppem) / 64.0f;
        if (strikePpem > 0.0f) {
            glyphScale = fontSize / strikePpem;
        }
    }
    if (sizeError != 0) {
        sizeError = FT_Set_Char_Size(
            loadedFace,
            0,
            static_cast<FT_F26Dot6>(std::lround(scaledFontSize * 64.0f)),
            72,
            72);
        if (sizeError != 0) {
            const FT_UInt pixelSize = static_cast<FT_UInt>(std::max(1.0f, std::round(scaledFontSize)));
            sizeError = FT_Set_Pixel_Sizes(loadedFace, 0, pixelSize);
        }
    }
    if (sizeError != 0) {
        FT_Done_Face(loadedFace);
        return false;
    }

    face.path = path;
    face.face = loadedFace;
    face.size = fontSize;
    face.glyphScale = glyphScale;
    face.colored = emojiFont || FT_HAS_COLOR(loadedFace);
    if (loadedFace->size && loadedFace->size->metrics.y_ppem > 0) {
        face.ascent = static_cast<float>(loadedFace->size->metrics.ascender) / 64.0f * glyphScale;
        face.descent = static_cast<float>(loadedFace->size->metrics.descender) / 64.0f * glyphScale;
        const float height = static_cast<float>(loadedFace->size->metrics.height) / 64.0f * glyphScale;
        face.lineGap = height - (face.ascent - face.descent);
    } else {
        face.ascent = fontSize * 0.8f;
        face.descent = -fontSize * 0.2f;
        face.lineGap = 0.0f;
    }
    return true;
}

std::string fontStackCacheKey(const std::string& fontPath, float fontSize) {
    return fontPath + "#" + std::to_string(static_cast<int>(std::round(fontSize * 64.0f)));
}

std::shared_ptr<FontInfoHolder> loadSharedFontStack(const std::string& fontPath, float fontSize) {
    static std::unordered_map<std::string, std::weak_ptr<FontInfoHolder>> cache;

    const std::string cacheKey = fontStackCacheKey(fontPath, fontSize);
    if (auto cached = cache[cacheKey].lock()) {
        return cached;
    }

    auto holder = std::make_shared<FontInfoHolder>();

    FontFace primary;
    if (!loadFontFace(fontPath, fontSize, primary)) {
        return {};
    }
    holder->faces.push_back(std::move(primary));

    auto addLazyFallback = [&](const std::string& fallbackPath) {
        if (fallbackPath.empty() || fallbackPath == fontPath) {
            return;
        }
        if (std::find(holder->lazyFallbackPaths.begin(), holder->lazyFallbackPaths.end(), fallbackPath) == holder->lazyFallbackPaths.end()) {
            holder->lazyFallbackPaths.push_back(fallbackPath);
        }
    };

    addLazyFallback(resolveDefaultUiFontPath());
    addLazyFallback(resolveDefaultIconFontPath());

#ifdef _WIN32
    addLazyFallback("C:/Windows/Fonts/seguiemj.ttf");
    addLazyFallback("C:/Windows/Fonts/seguisym.ttf");
    addLazyFallback("C:/Windows/Fonts/msyh.ttc");
    addLazyFallback("C:/Windows/Fonts/simhei.ttf");
#elif defined(__APPLE__)
    addLazyFallback("/System/Library/Fonts/Apple Color Emoji.ttc");
    addLazyFallback("/System/Library/Fonts/Supplemental/Arial Unicode.ttf");
    addLazyFallback("/System/Library/Fonts/Supplemental/Arial.ttf");
#else
    addLazyFallback("/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf");
    addLazyFallback("/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc");
    addLazyFallback("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
#endif

    cache[cacheKey] = holder;
    return holder;
}

bool isCombiningMark(unsigned int codepoint) {
    return (codepoint >= 0x0300 && codepoint <= 0x036F) ||
           (codepoint >= 0x1AB0 && codepoint <= 0x1AFF) ||
           (codepoint >= 0x1DC0 && codepoint <= 0x1DFF) ||
           (codepoint >= 0x20D0 && codepoint <= 0x20FF) ||
           (codepoint >= 0xFE20 && codepoint <= 0xFE2F);
}

bool isRtlCodepoint(unsigned int codepoint) {
    return (codepoint >= 0x0590 && codepoint <= 0x08FF) ||
           (codepoint >= 0xFB1D && codepoint <= 0xFDFF) ||
           (codepoint >= 0xFE70 && codepoint <= 0xFEFF);
}

bool isVariationSelector(unsigned int codepoint) {
    return (codepoint >= 0xFE00 && codepoint <= 0xFE0F) ||
           (codepoint >= 0xE0100 && codepoint <= 0xE01EF);
}

bool isEmojiJoiner(unsigned int codepoint) {
    return codepoint == 0x200D ||
           codepoint == 0x20E3 ||
           isVariationSelector(codepoint);
}

bool isEmojiCodepoint(unsigned int codepoint) {
    return (codepoint >= 0x1F000 && codepoint <= 0x1FAFF) ||
           (codepoint >= 0x2600 && codepoint <= 0x27BF);
}

bool nextCodepointIsEmojiPresentation(const std::string& text, size_t index) {
    if (index >= text.size()) {
        return false;
    }
    const size_t saved = index;
    const unsigned int next = readUtf8Codepoint(text, index);
    (void)saved;
    return next == 0xFE0F;
}

size_t utf8LengthFromFirstByte(unsigned char first) {
    if (first < 0x80) {
        return 1;
    }
    if ((first >> 5) == 0x6) {
        return 2;
    }
    if ((first >> 4) == 0xE) {
        return 3;
    }
    if ((first >> 3) == 0x1E) {
        return 4;
    }
    return 1;
}

unsigned int readUtf8Codepoint(const std::string& text, size_t& index) {
    const unsigned char first = static_cast<unsigned char>(text[index++]);
    if (first < 0x80) {
        return first;
    }
    if ((first >> 5) == 0x6 && index < text.size()) {
        return ((first & 0x1F) << 6) | (static_cast<unsigned char>(text[index++]) & 0x3F);
    }
    if ((first >> 4) == 0xE && index + 1 < text.size()) {
        unsigned int cp = (first & 0x0F) << 12;
        cp |= (static_cast<unsigned char>(text[index++]) & 0x3F) << 6;
        cp |= static_cast<unsigned char>(text[index++]) & 0x3F;
        return cp;
    }
    if ((first >> 3) == 0x1E && index + 2 < text.size()) {
        unsigned int cp = (first & 0x07) << 18;
        cp |= (static_cast<unsigned char>(text[index++]) & 0x3F) << 12;
        cp |= (static_cast<unsigned char>(text[index++]) & 0x3F) << 6;
        cp |= static_cast<unsigned char>(text[index++]) & 0x3F;
        return cp;
    }
    return '?';
}

size_t findFaceForCodepoint(FontInfoHolder& holder, unsigned int codepoint, float fontSize) {
    if (holder.faces.empty()) {
        return 0;
    }
    if (codepoint == ' ' || codepoint == '\t' || codepoint == 0) {
        return 0;
    }

    for (size_t i = 0; i < holder.faces.size(); ++i) {
        if (FT_Get_Char_Index(holder.faces[i].face, codepoint) != 0) {
            return i;
        }
    }

    for (const std::string& fallbackPath : holder.lazyFallbackPaths) {
        if (fallbackPath.empty()) {
            continue;
        }

        const bool alreadyLoaded = std::any_of(holder.faces.begin(), holder.faces.end(),
                                               [&](const FontFace& loadedFace) {
                                                   return loadedFace.path == fallbackPath;
                                               });
        if (alreadyLoaded) {
            continue;
        }

        FontFace fallback;
        if (!loadFontFace(fallbackPath, fontSize, fallback)) {
            continue;
        }

        const bool hasGlyph = FT_Get_Char_Index(fallback.face, codepoint) != 0;
        holder.faces.push_back(std::move(fallback));
        if (hasGlyph) {
            return holder.faces.size() - 1;
        }
    }

    return 0;
}

size_t findEmojiFaceForCodepoint(FontInfoHolder& holder, unsigned int codepoint, float fontSize) {
    for (size_t i = 0; i < holder.faces.size(); ++i) {
        if (holder.faces[i].colored && FT_Get_Char_Index(holder.faces[i].face, codepoint) != 0) {
            return i;
        }
    }

    for (const std::string& fallbackPath : holder.lazyFallbackPaths) {
        if (fallbackPath.empty() || !isEmojiFontPath(fallbackPath)) {
            continue;
        }

        const auto loaded = std::find_if(holder.faces.begin(), holder.faces.end(),
                                         [&](const FontFace& loadedFace) {
                                             return loadedFace.path == fallbackPath;
                                         });
        if (loaded != holder.faces.end()) {
            if (FT_Get_Char_Index(loaded->face, codepoint) != 0) {
                return static_cast<size_t>(std::distance(holder.faces.begin(), loaded));
            }
            continue;
        }

        FontFace fallback;
        if (!loadFontFace(fallbackPath, fontSize, fallback)) {
            continue;
        }

        const bool hasGlyph = FT_Get_Char_Index(fallback.face, codepoint) != 0;
        holder.faces.push_back(std::move(fallback));
        if (hasGlyph) {
            return holder.faces.size() - 1;
        }
    }

    return findFaceForCodepoint(holder, codepoint, fontSize);
}

unsigned int codepointForCluster(const TextRun& run, unsigned int cluster) {
    if (run.codepoints.empty()) {
        return 0;
    }

    const CodepointInfo* chosen = &run.codepoints.front();
    for (const CodepointInfo& info : run.codepoints) {
        if (info.byteOffset <= cluster) {
            chosen = &info;
        } else {
            break;
        }
    }
    return chosen->codepoint;
}

CodepointInfo codepointInfoForCluster(const TextRun& run, unsigned int cluster) {
    if (run.codepoints.empty()) {
        return {};
    }

    const CodepointInfo* chosen = &run.codepoints.front();
    for (const CodepointInfo& info : run.codepoints) {
        if (info.byteOffset <= cluster) {
            chosen = &info;
        } else {
            break;
        }
    }
    return *chosen;
}

float loadGlyphAdvance(const FontFace& face, unsigned int glyphIndex, unsigned int codepoint, float fontSize) {
    if (codepoint == '\t') {
        return fontSize * 4.0f;
    }
    if (isCombiningMark(codepoint)) {
        return 0.0f;
    }
    if (FT_Load_Glyph(face.face, glyphIndex, kGlyphLoadFlags) != 0) {
        return fontSize * 0.5f;
    }

    return static_cast<float>(face.face->glyph->advance.x) / 64.0f * face.glyphScale;
}

std::vector<TextPrimitive::ShapedGlyph> shapeWithFallback(FontInfoHolder& holder,
                                                          const std::string& text,
                                                          float fontSize) {
    std::vector<TextPrimitive::ShapedGlyph> shaped;
    size_t index = 0;
    while (index < text.size()) {
        const size_t start = index;
        const unsigned int codepoint = readUtf8Codepoint(text, index);
        const bool preferEmoji = isEmojiCodepoint(codepoint) || nextCodepointIsEmojiPresentation(text, index);
        const size_t faceIndex = preferEmoji
            ? findEmojiFaceForCodepoint(holder, codepoint, fontSize)
            : findFaceForCodepoint(holder, codepoint, fontSize);
        const FontFace& face = holder.faces[faceIndex];
        const unsigned int glyphIndex = codepoint == '\t' ? 0 : FT_Get_Char_Index(face.face, codepoint);
        const float advance = loadGlyphAdvance(face, glyphIndex, codepoint, fontSize);
        shaped.push_back({glyphIndex == 0 && codepoint == '\t' ? 0 : makeGlyphKey(faceIndex, glyphIndex),
                          codepoint,
                          static_cast<int>(start),
                          static_cast<int>(index),
                          advance,
                          0.0f,
                          0.0f});
    }
    return shaped;
}

#if defined(EUI_HAS_HARFBUZZ)
std::vector<TextPrimitive::ShapedGlyph> shapeWithHarfBuzz(FontInfoHolder& holder,
                                                          const std::string& text,
                                                          float fontSize) {
    std::vector<TextRun> runs;
    size_t index = 0;
    size_t currentRun = static_cast<size_t>(-1);

    while (index < text.size()) {
        const size_t start = index;
        const unsigned char first = static_cast<unsigned char>(text[start]);
        const size_t byteLength = std::min(utf8LengthFromFirstByte(first), text.size() - start);
        const unsigned int codepoint = readUtf8Codepoint(text, index);
        const bool preferEmoji = isEmojiCodepoint(codepoint) || nextCodepointIsEmojiPresentation(text, index);
        const size_t faceIndex = (isEmojiJoiner(codepoint) && currentRun != static_cast<size_t>(-1))
            ? runs[currentRun].faceIndex
            : (preferEmoji
                ? findEmojiFaceForCodepoint(holder, codepoint, fontSize)
                : findFaceForCodepoint(holder, codepoint, fontSize));
        const bool rtl = isRtlCodepoint(codepoint);

        if (currentRun == static_cast<size_t>(-1) || runs[currentRun].faceIndex != faceIndex) {
            runs.push_back({faceIndex, start, {}, {}, false});
            currentRun = runs.size() - 1;
        }

        TextRun& run = runs[currentRun];
        run.rtl = run.rtl || rtl;
        const size_t runOffset = run.text.size();
        run.text.append(text.data() + start, byteLength);
        run.codepoints.push_back({codepoint, runOffset, byteLength});
    }

    std::vector<TextPrimitive::ShapedGlyph> shaped;
    for (const TextRun& run : runs) {
        if (run.text.empty() || run.faceIndex >= holder.faces.size()) {
            continue;
        }

        const FontFace& face = holder.faces[run.faceIndex];
        hb_font_t* hbFont = hb_ft_font_create_referenced(face.face);
        if (hbFont) {
            hb_ft_font_set_load_flags(hbFont, kGlyphLoadFlags);
        }
        hb_buffer_t* buffer = hb_buffer_create();
        if (!hbFont || !buffer) {
            if (buffer) {
                hb_buffer_destroy(buffer);
            }
            if (hbFont) {
                hb_font_destroy(hbFont);
            }
            continue;
        }

        hb_buffer_add_utf8(buffer, run.text.c_str(), static_cast<int>(run.text.size()), 0, static_cast<int>(run.text.size()));
        hb_buffer_set_cluster_level(buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
        hb_buffer_set_direction(buffer, run.rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
        hb_buffer_guess_segment_properties(buffer);
        hb_shape(hbFont, buffer, nullptr, 0);

        unsigned int glyphCount = 0;
        hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer, &glyphCount);
        hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(buffer, &glyphCount);

        for (unsigned int i = 0; i < glyphCount; ++i) {
            const CodepointInfo clusterInfo = codepointInfoForCluster(run, infos[i].cluster);
            const unsigned int codepoint = clusterInfo.codepoint;
            const float glyphScale = face.glyphScale;
            float advance = static_cast<float>(positions[i].x_advance) / 64.0f * glyphScale;
            if (isCombiningMark(codepoint)) {
                advance = 0.0f;
            }

            shaped.push_back({makeGlyphKey(run.faceIndex, infos[i].codepoint),
                              codepoint,
                              static_cast<int>(run.sourceOffset + clusterInfo.byteOffset),
                              static_cast<int>(run.sourceOffset + clusterInfo.byteOffset + clusterInfo.byteLength),
                              advance,
                              static_cast<float>(positions[i].x_offset) / 64.0f * glyphScale,
                              -static_cast<float>(positions[i].y_offset) / 64.0f * glyphScale});
        }

        hb_buffer_destroy(buffer);
        hb_font_destroy(hbFont);
    }

    return shaped;
}
#endif

std::vector<TextPrimitive::ShapedGlyph> shapeTextWithFontStack(FontInfoHolder& holder,
                                                               const std::string& text,
                                                               float fontSize) {
#if defined(EUI_HAS_HARFBUZZ)
    std::vector<TextPrimitive::ShapedGlyph> shaped = shapeWithHarfBuzz(holder, text, fontSize);
#else
    std::vector<TextPrimitive::ShapedGlyph> shaped = shapeWithFallback(holder, text, fontSize);
#endif
    for (TextPrimitive::ShapedGlyph& glyph : shaped) {
        int nextStart = static_cast<int>(text.size());
        for (const TextPrimitive::ShapedGlyph& candidate : shaped) {
            if (candidate.byteStart > glyph.byteStart && candidate.byteStart < nextStart) {
                nextStart = candidate.byteStart;
            }
        }
        glyph.byteEnd = std::max(glyph.byteEnd, nextStart);
    }
    return shaped;
}

TextPrimitive::TextMetrics makeTextMetrics(const std::string& text,
                                           const std::vector<TextPrimitive::ShapedGlyph>& shaped) {
    TextPrimitive::TextMetrics metrics;
    auto addStop = [&](int byteIndex, float x) {
        byteIndex = std::clamp(byteIndex, 0, static_cast<int>(text.size()));
        const auto it = std::lower_bound(metrics.byteIndices.begin(), metrics.byteIndices.end(), byteIndex);
        if (it != metrics.byteIndices.end() && *it == byteIndex) {
            const size_t slot = static_cast<size_t>(std::distance(metrics.byteIndices.begin(), it));
            metrics.caretX[slot] = x;
            return;
        }
        const size_t slot = static_cast<size_t>(std::distance(metrics.byteIndices.begin(), it));
        metrics.byteIndices.insert(it, byteIndex);
        metrics.caretX.insert(metrics.caretX.begin() + static_cast<std::ptrdiff_t>(slot), x);
    };

    addStop(0, 0.0f);

    float cursorX = 0.0f;
    for (const TextPrimitive::ShapedGlyph& glyph : shaped) {
        const float startX = cursorX;
        cursorX += glyph.advance;
        addStop(glyph.byteStart, startX);
        addStop(glyph.byteEnd, cursorX);
    }

    metrics.width = cursorX;
    addStop(static_cast<int>(text.size()), metrics.width);
    return metrics;
}

bool appendToAtlas(AtlasPage& page,
                   const unsigned char* pixels,
                   int width,
                   int height,
                   int channels,
                   TextPrimitive::Glyph& glyph) {
    if (width <= 0 || height <= 0 || pixels == nullptr || channels <= 0 || page.pixels.empty()) {
        return true;
    }

    if (page.x + width + 1 >= page.width) {
        page.x = 1;
        page.y += page.rowHeight + 1;
        page.rowHeight = 0;
    }
    if (page.y + height + 1 >= page.height) {
        return false;
    }

    const int copyChannels = std::min(channels, page.channels);
    for (int row = 0; row < height; ++row) {
        const auto dstOffset = (static_cast<std::size_t>(page.y + row) * static_cast<std::size_t>(page.width) +
                                static_cast<std::size_t>(page.x)) *
                               static_cast<std::size_t>(page.channels);
        const auto srcOffset = static_cast<std::size_t>(row) * static_cast<std::size_t>(width) * static_cast<std::size_t>(channels);
        unsigned char* dst = page.pixels.data() + dstOffset;
        const unsigned char* src = pixels + srcOffset;
        for (int x = 0; x < width; ++x) {
            std::copy(src + static_cast<std::size_t>(x) * static_cast<std::size_t>(channels),
                      src + static_cast<std::size_t>(x) * static_cast<std::size_t>(channels) + copyChannels,
                      dst + static_cast<std::size_t>(x) * static_cast<std::size_t>(page.channels));
        }
    }

    glyph.u0 = static_cast<float>(page.x) / static_cast<float>(page.width);
    glyph.v0 = static_cast<float>(page.y) / static_cast<float>(page.height);
    glyph.u1 = static_cast<float>(page.x + width) / static_cast<float>(page.width);
    glyph.v1 = static_cast<float>(page.y + height) / static_cast<float>(page.height);

    page.x += width + 1;
    page.rowHeight = std::max(page.rowHeight, height);
    ++page.generation;
    return true;
}

std::vector<unsigned char> copyGrayBitmap(const FT_Bitmap& bitmap) {
    if (bitmap.width == 0 || bitmap.rows == 0 || !bitmap.buffer ||
        std::abs(bitmap.pitch) < static_cast<int>(bitmap.width)) {
        return {};
    }
    std::vector<unsigned char> compact(static_cast<size_t>(bitmap.width) * bitmap.rows);
    const int pitch = std::abs(bitmap.pitch);
    const unsigned char* base = bitmap.pitch >= 0
        ? bitmap.buffer
        : bitmap.buffer + static_cast<ptrdiff_t>(bitmap.rows - 1) * pitch;
    for (unsigned int row = 0; row < bitmap.rows; ++row) {
        const unsigned char* source = bitmap.pitch >= 0
            ? base + static_cast<ptrdiff_t>(row) * pitch
            : base - static_cast<ptrdiff_t>(row) * pitch;
        std::copy(source, source + bitmap.width, compact.begin() + static_cast<ptrdiff_t>(row) * bitmap.width);
    }
    return compact;
}

std::vector<unsigned char> copyBgraBitmapAsRgba(const FT_Bitmap& bitmap) {
    if (bitmap.width == 0 || bitmap.rows == 0 || !bitmap.buffer ||
        std::abs(bitmap.pitch) < static_cast<int>(bitmap.width * 4)) {
        return {};
    }
    std::vector<unsigned char> compact(static_cast<size_t>(bitmap.width) * bitmap.rows * 4);
    const int pitch = std::abs(bitmap.pitch);
    const unsigned char* base = bitmap.pitch >= 0
        ? bitmap.buffer
        : bitmap.buffer + static_cast<ptrdiff_t>(bitmap.rows - 1) * pitch;
    for (unsigned int row = 0; row < bitmap.rows; ++row) {
        const unsigned char* source = bitmap.pitch >= 0
            ? base + static_cast<ptrdiff_t>(row) * pitch
            : base - static_cast<ptrdiff_t>(row) * pitch;
        unsigned char* target = compact.data() + static_cast<ptrdiff_t>(row) * bitmap.width * 4;
        for (unsigned int x = 0; x < bitmap.width; ++x) {
            target[x * 4 + 0] = source[x * 4 + 2];
            target[x * 4 + 1] = source[x * 4 + 1];
            target[x * 4 + 2] = source[x * 4 + 0];
            target[x * 4 + 3] = source[x * 4 + 3];
        }
    }
    return compact;
}

} // namespace

struct TextPrimitive::Impl {
    struct LaidOutGlyph {
        Glyph glyph;
        float x = 0.0f;
        float y = 0.0f;
    };

    struct Line {
        std::vector<LaidOutGlyph> glyphs;
        float width = 0.0f;
    };

    Impl() = default;
    Impl(float x, float y) : position_{x, y} {}

    bool initialize();
    void destroy();

    void setPosition(float x, float y);
    void setText(const std::string& text);
    void setFontFamily(const std::string& fontFamily);
    void setFontSize(float fontSize);
    void setFontWeight(int fontWeight);
    void setColor(const Color& color);
    void setMaxWidth(float maxWidth);
    void setWrap(bool wrap);
    void setHorizontalAlign(HorizontalAlign align);
    void setVerticalAlign(VerticalAlign align);
    void setLineHeight(float lineHeight);
    void setStyle(const TextStyle& style);
    void setVisualScale(float originX, float originY, float scale);
    void setTransform(const Transform& transform, const Rect& frame);
    void setTransformMatrix(const TransformMatrix& matrix);

    const TextStyle& style() const;
    Vec2 position() const;
    Vec2 measuredSize();
    static float measureTextWidth(const std::string& text,
                                  const std::string& fontFamily = {},
                                  float fontSize = 16.0f,
                                  int fontWeight = 400);
    static TextMetrics measureTextMetrics(const std::string& text,
                                          const std::string& fontFamily = {},
                                          float fontSize = 16.0f,
                                          int fontWeight = 400);
    static void setDefaultFontFiles(const std::string& textFontFile, const std::string& iconFontFile);

    void prepare();
    void render(int windowWidth, int windowHeight);

    bool loadFont();
    bool ensureGlyph(const ShapedGlyph& shaped);
    Glyph* findGlyph(std::uint64_t key);
    void cacheGlyph(std::uint64_t key, const Glyph& glyph);
    void invalidateLayout();
    void invalidateVertices();
    void rebuildLayout();
    void rebuildVertices();
    std::vector<ShapedGlyph> shapeText(const std::string& text);
    void appendShapedGlyphToLine(Line& line, const ShapedGlyph& shaped, float& cursorX);

    static unsigned int readCodepoint(const std::string& text, size_t& index);
    static std::string resolveFontPath(const std::string& fontFamily, int fontWeight);

    Vec2 position_;
    Vec2 visualScaleOrigin_;
    float visualScale_ = 1.0f;
    Transform transform_;
    Rect transformFrame_;
    TransformMatrix transformMatrix_;
    bool hasTransformMatrix_ = false;
    TextStyle style_;
    std::shared_ptr<void> fontInfoStorage_;
    float scale_ = 1.0f;
    float ascent_ = 0.0f;
    float descent_ = 0.0f;
    float lineGap_ = 0.0f;

    std::unordered_map<std::uint64_t, Glyph> glyphs_;

    std::vector<Line> lines_;
    std::vector<float> vertices_;
    Vec2 measuredSize_;
    bool layoutDirty_ = true;
    bool verticesDirty_ = true;
    bool fontDirty_ = true;
};

bool TextPrimitive::Impl::initialize() {
    if (!loadFont() || !retainSharedTextAtlas()) {
        return false;
    }

    return true;
}

void TextPrimitive::Impl::destroy() {
    releaseSharedTextAtlas();
    fontInfoStorage_.reset();
    glyphs_.clear();
    lines_.clear();
    vertices_.clear();
    measuredSize_ = {};
    layoutDirty_ = true;
    verticesDirty_ = true;
    fontDirty_ = true;
}

void TextPrimitive::Impl::setPosition(float x, float y) {
    if (position_.x == x && position_.y == y) {
        return;
    }
    position_ = {x, y};
    invalidateVertices();
}

void TextPrimitive::Impl::setText(const std::string& text) {
    if (style_.text == text) {
        return;
    }
    style_.text = text;
    invalidateLayout();
}

void TextPrimitive::Impl::setFontFamily(const std::string& fontFamily) {
    if (style_.fontFamily == fontFamily) {
        return;
    }
    style_.fontFamily = fontFamily;
    fontDirty_ = true;
    invalidateLayout();
}

void TextPrimitive::Impl::setFontSize(float fontSize) {
    if (style_.fontSize == fontSize) {
        return;
    }
    style_.fontSize = fontSize;
    fontDirty_ = true;
    invalidateLayout();
}

void TextPrimitive::Impl::setFontWeight(int fontWeight) {
    if (style_.fontWeight == fontWeight) {
        return;
    }
    style_.fontWeight = fontWeight;
    fontDirty_ = true;
    invalidateLayout();
}

void TextPrimitive::Impl::setColor(const Color& color) {
    style_.color = color;
}

void TextPrimitive::Impl::setMaxWidth(float maxWidth) {
    if (style_.maxWidth == maxWidth) {
        return;
    }
    style_.maxWidth = maxWidth;
    invalidateLayout();
}

void TextPrimitive::Impl::setWrap(bool wrap) {
    if (style_.wrap == wrap) {
        return;
    }
    style_.wrap = wrap;
    invalidateLayout();
}

void TextPrimitive::Impl::setHorizontalAlign(HorizontalAlign align) {
    if (style_.horizontalAlign == align) {
        return;
    }
    style_.horizontalAlign = align;
    invalidateVertices();
}

void TextPrimitive::Impl::setVerticalAlign(VerticalAlign align) {
    if (style_.verticalAlign == align) {
        return;
    }
    style_.verticalAlign = align;
    invalidateVertices();
}

void TextPrimitive::Impl::setLineHeight(float lineHeight) {
    if (style_.lineHeight == lineHeight) {
        return;
    }
    style_.lineHeight = lineHeight;
    invalidateLayout();
}

void TextPrimitive::Impl::setVisualScale(float originX, float originY, float scale) {
    const float nextScale = std::max(0.01f, scale);
    if (visualScaleOrigin_.x == originX && visualScaleOrigin_.y == originY && visualScale_ == nextScale) {
        return;
    }
    visualScaleOrigin_ = {originX, originY};
    visualScale_ = nextScale;
    invalidateVertices();
}

void TextPrimitive::Impl::setTransform(const Transform& transform, const Rect& frame) {
    auto close = [](float left, float right) {
        return std::fabs(left - right) <= 0.0001f;
    };
    auto closeVec = [&](const Vec2& left, const Vec2& right) {
        return close(left.x, right.x) && close(left.y, right.y);
    };
    const bool sameTransform =
        closeVec(transform_.translate, transform.translate) &&
        close(transform_.translateZ, transform.translateZ) &&
        closeVec(transform_.scale, transform.scale) &&
        close(transform_.rotate, transform.rotate) &&
        close(transform_.rotateX, transform.rotateX) &&
        close(transform_.rotateY, transform.rotateY) &&
        closeVec(transform_.origin, transform.origin) &&
        close(transform_.perspective, transform.perspective);
    const bool sameFrame =
        close(transformFrame_.x, frame.x) &&
        close(transformFrame_.y, frame.y) &&
        close(transformFrame_.width, frame.width) &&
        close(transformFrame_.height, frame.height);
    if (sameTransform && sameFrame) {
        return;
    }
    transform_ = transform;
    transformFrame_ = frame;
    hasTransformMatrix_ = false;
    invalidateVertices();
}

void TextPrimitive::Impl::setTransformMatrix(const TransformMatrix& matrix) {
    auto close = [](float left, float right) {
        return std::fabs(left - right) <= 0.0001f;
    };
    const bool same =
        close(transformMatrix_.m00, matrix.m00) &&
        close(transformMatrix_.m01, matrix.m01) &&
        close(transformMatrix_.tx, matrix.tx) &&
        close(transformMatrix_.m10, matrix.m10) &&
        close(transformMatrix_.m11, matrix.m11) &&
        close(transformMatrix_.ty, matrix.ty) &&
        close(transformMatrix_.px, matrix.px) &&
        close(transformMatrix_.py, matrix.py) &&
        close(transformMatrix_.pw, matrix.pw) &&
        hasTransformMatrix_;
    if (same) {
        return;
    }
    transformMatrix_ = matrix;
    hasTransformMatrix_ = true;
    invalidateVertices();
}

void TextPrimitive::Impl::setStyle(const TextStyle& style) {
    const bool fontChanged = style.fontFamily != style_.fontFamily ||
                             style.fontSize != style_.fontSize ||
                             style.fontWeight != style_.fontWeight;
    style_ = style;
    fontDirty_ = fontDirty_ || fontChanged;
    invalidateLayout();
}

const TextStyle& TextPrimitive::Impl::style() const {
    return style_;
}

Vec2 TextPrimitive::Impl::position() const {
    return position_;
}

Vec2 TextPrimitive::Impl::measuredSize() {
    if (layoutDirty_) {
        rebuildLayout();
    }
    return measuredSize_;
}

float TextPrimitive::Impl::measureTextWidth(const std::string& text,
                                      const std::string& fontFamily,
                                      float fontSize,
                                      int fontWeight) {
    return measureTextMetrics(text, fontFamily, fontSize, fontWeight).width;
}

TextPrimitive::TextMetrics TextPrimitive::Impl::measureTextMetrics(const std::string& text,
                                                             const std::string& fontFamily,
                                                             float fontSize,
                                                             int fontWeight) {
    TextMetrics empty;
    empty.byteIndices = {0};
    empty.caretX = {0.0f};
    if (text.empty()) {
        return empty;
    }

    const float size = std::max(1.0f, fontSize);
    const std::string fontPath = resolveFontPath(fontFamily, fontWeight);
    auto holder = loadSharedFontStack(fontPath, size);
    if (!holder || holder->faces.empty()) {
        return empty;
    }

    return makeTextMetrics(text, shapeTextWithFontStack(*holder, text, size));
}

void TextPrimitive::Impl::setDefaultFontFiles(const std::string& textFontFile, const std::string& iconFontFile) {
    defaultUiFontFileOverride() = textFontFile;
    defaultIconFontFileOverride() = iconFontFile;
}

void TextPrimitive::Impl::prepare() {
    if (layoutDirty_) {
        rebuildLayout();
    }
    if (verticesDirty_) {
        rebuildVertices();
    }
}

void TextPrimitive::Impl::render(int windowWidth, int windowHeight) {
    core::render::RenderBackend* backend = core::render::activeRenderBackend();
    if (backend == nullptr || windowWidth <= 0 || windowHeight <= 0) {
        return;
    }

    prepare();

    if (vertices_.empty()) {
        return;
    }

    const SharedTextAtlas& atlas = sharedTextAtlas();
    core::render::TextDrawCommand command{};
    command.vertices = vertices_.data();
    command.vertexFloatCount = vertices_.size();
    command.color = style_.color;
    command.grayAtlas = {
        core::render::TextAtlasPageKind::Gray,
        atlas.gray.width,
        atlas.gray.height,
        atlas.gray.channels,
        atlas.gray.generation,
        atlas.gray.pixels.empty() ? nullptr : atlas.gray.pixels.data()
    };
    command.colorAtlas = {
        core::render::TextAtlasPageKind::Color,
        atlas.color.width,
        atlas.color.height,
        atlas.color.channels,
        atlas.color.generation,
        atlas.color.pixels.empty() ? nullptr : atlas.color.pixels.data()
    };
    backend->drawText(command, windowWidth, windowHeight);
}

bool TextPrimitive::Impl::loadFont() {
    const std::string fontPath = resolveFontPath(style_.fontFamily, style_.fontWeight);
    auto holder = loadSharedFontStack(fontPath, style_.fontSize);
    if (!holder || holder->faces.empty()) {
        return false;
    }

    fontInfoStorage_ = holder;
    scale_ = 1.0f;
    ascent_ = holder->faces.front().ascent;
    descent_ = holder->faces.front().descent;
    lineGap_ = holder->faces.front().lineGap;

    glyphs_.clear();

    fontDirty_ = false;
    return true;
}

bool TextPrimitive::Impl::ensureGlyph(const ShapedGlyph& shaped) {
    if (shaped.key == 0) {
        return true;
    }
    if (findGlyph(shaped.key)) {
        return true;
    }

    if (fontDirty_ && !loadFont()) {
        return false;
    }

    auto holder = std::static_pointer_cast<FontInfoHolder>(fontInfoStorage_);
    if (!holder || holder->faces.empty()) {
        return false;
    }

    const size_t faceIndex = faceIndexFromGlyphKey(shaped.key);
    const unsigned int glyphIndex = glyphIndexFromGlyphKey(shaped.key);
    if (faceIndex >= holder->faces.size()) {
        return false;
    }

    FontFace& face = holder->faces[faceIndex];
    Glyph glyph;
    glyph.advance = shaped.advance;
    glyph.colored = face.colored;

    if (glyphIndex == 0 || shaped.codepoint == ' ' || shaped.codepoint == '\t') {
        cacheGlyph(shaped.key, glyph);
        return true;
    }

    if (FT_Load_Glyph(face.face, glyphIndex, kGlyphLoadFlags) != 0) {
        cacheGlyph(shaped.key, glyph);
        return true;
    }

    FT_GlyphSlot slot = face.face->glyph;
    glyph.advance = shaped.advance;

    if (slot->format != FT_GLYPH_FORMAT_BITMAP) {
        if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0) {
            cacheGlyph(shaped.key, glyph);
            return true;
        }
    }

    const FT_Bitmap& bitmap = slot->bitmap;
    const bool colorBitmap = bitmap.pixel_mode == FT_PIXEL_MODE_BGRA;
    glyph.colored = colorBitmap;
    glyph.xOffset = static_cast<float>(slot->bitmap_left) * face.glyphScale;
    glyph.yOffset = ascent_ - static_cast<float>(slot->bitmap_top) * face.glyphScale;
    glyph.width = static_cast<float>(bitmap.width) * face.glyphScale;
    glyph.height = static_cast<float>(bitmap.rows) * face.glyphScale;
    if (colorBitmap && face.colored) {
        glyph.yOffset = ascent_ - descent_ - glyph.height;
    }

    if (bitmap.width == 0 || bitmap.rows == 0 || !bitmap.buffer) {
        cacheGlyph(shaped.key, glyph);
        return true;
    }

    const std::string cacheKey = glyphCacheKey(face, style_.fontSize, glyphIndex, colorBitmap);
    SharedTextAtlas& atlas = sharedTextAtlas();
    AtlasPage& page = colorBitmap ? atlas.color : atlas.gray;
    if (const auto cached = page.glyphs.find(cacheKey); cached != page.glyphs.end()) {
        glyph = cached->second;
        glyph.advance = shaped.advance;
        cacheGlyph(shaped.key, glyph);
        return true;
    }

    if (colorBitmap) {
        if (!ensureAtlasPage(atlas.color, kColorAtlasSize, kColorAtlasSize, 4)) {
            cacheGlyph(shaped.key, glyph);
            return true;
        }
        std::vector<unsigned char> rgba = copyBgraBitmapAsRgba(bitmap);
        if (rgba.empty()) {
            cacheGlyph(shaped.key, glyph);
            return true;
        }
        if (!appendToAtlas(atlas.color, rgba.data(), static_cast<int>(bitmap.width), static_cast<int>(bitmap.rows), 4, glyph)) {
            cacheGlyph(shaped.key, glyph);
            return true;
        }
        atlas.color.glyphs[cacheKey] = glyph;
    } else if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
        std::vector<unsigned char> gray = copyGrayBitmap(bitmap);
        if (gray.empty()) {
            cacheGlyph(shaped.key, glyph);
            return true;
        }
        if (!appendToAtlas(atlas.gray, gray.data(), static_cast<int>(bitmap.width), static_cast<int>(bitmap.rows), 1, glyph)) {
            cacheGlyph(shaped.key, glyph);
            return true;
        }
        atlas.gray.glyphs[cacheKey] = glyph;
    } else {
        cacheGlyph(shaped.key, glyph);
        return true;
    }

    cacheGlyph(shaped.key, glyph);
    return true;
}

TextPrimitive::Glyph* TextPrimitive::Impl::findGlyph(std::uint64_t key) {
    const auto it = glyphs_.find(key);
    return it == glyphs_.end() ? nullptr : &it->second;
}

void TextPrimitive::Impl::cacheGlyph(std::uint64_t key, const Glyph& glyph) {
    if (key == 0) {
        return;
    }
    glyphs_[key] = glyph;
}

void TextPrimitive::Impl::invalidateLayout() {
    layoutDirty_ = true;
    invalidateVertices();
}

void TextPrimitive::Impl::rebuildLayout() {
    if (fontDirty_ && !loadFont()) {
        layoutDirty_ = false;
        return;
    }

    lines_.clear();
    measuredSize_ = {};

    Line currentLine;
    float cursorX = 0.0f;
    const float lineHeight = style_.lineHeight > 0.0f ? style_.lineHeight : style_.fontSize * 1.2f;
    const float maxWidth = style_.maxWidth > 0.0f ? style_.maxWidth : 0.0f;

    size_t paragraphStart = 0;
    while (paragraphStart <= style_.text.size()) {
        const size_t newline = style_.text.find('\n', paragraphStart);
        const size_t paragraphEnd = newline == std::string::npos ? style_.text.size() : newline;
        std::string paragraph = style_.text.substr(paragraphStart, paragraphEnd - paragraphStart);
        if (!paragraph.empty() && paragraph.back() == '\r') {
            paragraph.pop_back();
        }

        const std::vector<ShapedGlyph> shaped = shapeText(paragraph);
        for (const ShapedGlyph& glyph : shaped) {
            const float advance = glyph.advance;
            if (style_.wrap && maxWidth > 0.0f && cursorX > 0.0f && cursorX + advance > maxWidth) {
                measuredSize_.x = std::max(measuredSize_.x, currentLine.width);
                lines_.push_back(currentLine);
                currentLine = {};
                cursorX = 0.0f;
            }

            appendShapedGlyphToLine(currentLine, glyph, cursorX);
        }

        if (newline == std::string::npos) {
            break;
        }

        measuredSize_.x = std::max(measuredSize_.x, currentLine.width);
        lines_.push_back(currentLine);
        currentLine = {};
        cursorX = 0.0f;
        paragraphStart = newline + 1;
    }

    measuredSize_.x = std::max(measuredSize_.x, currentLine.width);
    lines_.push_back(currentLine);
    measuredSize_.y = lines_.empty() ? 0.0f : static_cast<float>(lines_.size()) * lineHeight;
    layoutDirty_ = false;
    invalidateVertices();
}

void TextPrimitive::Impl::invalidateVertices() {
    verticesDirty_ = true;
}

void TextPrimitive::Impl::rebuildVertices() {
    vertices_.clear();
    const float lineHeight = style_.lineHeight > 0.0f ? style_.lineHeight : style_.fontSize * 1.2f;
    float blockYOffset = 0.0f;
    if (style_.verticalAlign == VerticalAlign::Center) {
        blockYOffset = -measuredSize_.y * 0.5f;
    } else if (style_.verticalAlign == VerticalAlign::Bottom) {
        blockYOffset = -measuredSize_.y;
    }

    for (size_t lineIndex = 0; lineIndex < lines_.size(); ++lineIndex) {
        const Line& line = lines_[lineIndex];
        float lineX = position_.x;
        if (style_.horizontalAlign == HorizontalAlign::Center) {
            lineX -= line.width * 0.5f;
        } else if (style_.horizontalAlign == HorizontalAlign::Right) {
            lineX -= line.width;
        }

        const float lineY = position_.y + blockYOffset + static_cast<float>(lineIndex) * lineHeight;
        for (const LaidOutGlyph& laidOut : line.glyphs) {
            const Glyph& glyph = laidOut.glyph;
            const float x0 = lineX + laidOut.x + glyph.xOffset;
            const float y0 = lineY + laidOut.y + glyph.yOffset;
            const float x1 = x0 + glyph.width;
            const float y1 = y0 + glyph.height;
            const float colored = glyph.colored ? 1.0f : 0.0f;
            Vec2 p0{x0, y0};
            Vec2 p1{x1, y0};
            Vec2 p2{x1, y1};
            Vec2 p3{x0, y1};

            if (hasTransformMatrix_) {
                p0 = core::transformPoint(transformMatrix_, p0.x, p0.y);
                p1 = core::transformPoint(transformMatrix_, p1.x, p1.y);
                p2 = core::transformPoint(transformMatrix_, p2.x, p2.y);
                p3 = core::transformPoint(transformMatrix_, p3.x, p3.y);
            } else if (std::fabs(transform_.translate.x) > 0.0001f ||
                std::fabs(transform_.translate.y) > 0.0001f ||
                std::fabs(transform_.scale.x - 1.0f) > 0.0001f ||
                std::fabs(transform_.scale.y - 1.0f) > 0.0001f ||
                std::fabs(transform_.rotate) > 0.0001f) {
                const Vec2 origin{
                    transformFrame_.x + transformFrame_.width * transform_.origin.x,
                    transformFrame_.y + transformFrame_.height * transform_.origin.y
                };
                const float cosine = std::cos(transform_.rotate);
                const float sine = std::sin(transform_.rotate);
                auto transformPoint = [&](Vec2 point) {
                    const float scaledX = (point.x - origin.x) * transform_.scale.x;
                    const float scaledY = (point.y - origin.y) * transform_.scale.y;
                    return Vec2{
                        origin.x + scaledX * cosine - scaledY * sine + transform_.translate.x,
                        origin.y + scaledX * sine + scaledY * cosine + transform_.translate.y
                    };
                };
                p0 = transformPoint(p0);
                p1 = transformPoint(p1);
                p2 = transformPoint(p2);
                p3 = transformPoint(p3);
            }

            if (!hasTransformMatrix_ && std::fabs(visualScale_ - 1.0f) > 0.0001f) {
                auto scalePoint = [&](Vec2 point) {
                    return Vec2{
                        visualScaleOrigin_.x + (point.x - visualScaleOrigin_.x) * visualScale_,
                        visualScaleOrigin_.y + (point.y - visualScaleOrigin_.y) * visualScale_
                    };
                };
                p0 = scalePoint(p0);
                p1 = scalePoint(p1);
                p2 = scalePoint(p2);
                p3 = scalePoint(p3);
            }

            vertices_.insert(vertices_.end(), {
                p0.x, p0.y, glyph.u0, glyph.v0, colored,
                p1.x, p1.y, glyph.u1, glyph.v0, colored,
                p2.x, p2.y, glyph.u1, glyph.v1, colored,
                p0.x, p0.y, glyph.u0, glyph.v0, colored,
                p2.x, p2.y, glyph.u1, glyph.v1, colored,
                p3.x, p3.y, glyph.u0, glyph.v1, colored
            });
        }
    }
    verticesDirty_ = false;
}

std::vector<TextPrimitive::ShapedGlyph> TextPrimitive::Impl::shapeText(const std::string& text) {
    if (fontDirty_ && !loadFont()) {
        return {};
    }
    auto holder = std::static_pointer_cast<FontInfoHolder>(fontInfoStorage_);
    if (!holder || holder->faces.empty()) {
        return {};
    }
    return shapeTextWithFontStack(*holder, text, std::max(1.0f, style_.fontSize));
}

void TextPrimitive::Impl::appendShapedGlyphToLine(Line& line, const ShapedGlyph& shaped, float& cursorX) {
    if (!ensureGlyph(shaped)) {
        cursorX += shaped.advance;
        line.width = cursorX;
        return;
    }

    const Glyph* glyph = findGlyph(shaped.key);
    if (glyph && shaped.key != 0 && shaped.codepoint != ' ' && shaped.codepoint != '\t') {
        line.glyphs.push_back({*glyph, cursorX + shaped.xOffset, shaped.yOffset});
    }
    cursorX += shaped.advance;
    line.width = cursorX;
}

unsigned int TextPrimitive::Impl::readCodepoint(const std::string& text, size_t& index) {
    return readUtf8Codepoint(text, index);
}

std::string TextPrimitive::Impl::resolveFontPath(const std::string& fontFamily, int fontWeight) {
    if (!fontFamily.empty() && fontFamily.find('.') != std::string::npos) {
        return resolveFontFilePath(fontFamily);
    }

    if (fontFamily == "YouSheBiaoTiHei" || fontFamily == "YouShe") {
        return resolveFontFilePath("YouSheBiaoTiHei-2.ttf");
    }

    if (fontFamily == "Title" || fontFamily == "PingFang" || fontFamily == "PingFang SC") {
        return resolveDefaultUiFontPath();
    }

    if (fontFamily == "FontAwesome" || fontFamily == "Font Awesome" ||
        fontFamily == "Font Awesome 7 Free" || fontFamily == "Icon") {
        return resolveDefaultIconFontPath();
    }

#ifdef _WIN32
    if (fontFamily == "monospace" || fontFamily == "Mono" ||
        fontFamily == "Cascadia Mono" || fontFamily == "Cascadia Code") {
        if (const std::string path = existingPath("C:/Windows/Fonts/CascadiaMono.ttf"); !path.empty()) {
            return path;
        }
        if (const std::string path = existingPath("C:/Windows/Fonts/CascadiaCode.ttf"); !path.empty()) {
            return path;
        }
        if (const std::string path = existingPath("C:/Windows/Fonts/consola.ttf"); !path.empty()) {
            return path;
        }
    }
    if (fontFamily == "Microsoft YaHei" || fontFamily == "YaHei") {
        return "C:/Windows/Fonts/msyh.ttc";
    }
    if (fontFamily == "Segoe UI Emoji" || fontFamily == "Emoji") {
        return "C:/Windows/Fonts/seguiemj.ttf";
    }
    if (fontFamily == "SimHei") {
        return "C:/Windows/Fonts/simhei.ttf";
    }
    if (fontWeight >= 600) {
        return resolveDefaultUiFontPath();
    }
    return resolveDefaultUiFontPath();
#elif defined(__APPLE__)
    if (fontFamily == "monospace" || fontFamily == "Mono" ||
        fontFamily == "SF Mono" || fontFamily == "Menlo" || fontFamily == "Monaco") {
        if (const std::string path = existingPath("/System/Library/Fonts/SFNSMono.ttf"); !path.empty()) {
            return path;
        }
        if (const std::string path = existingPath("/System/Library/Fonts/Supplemental/Menlo.ttc"); !path.empty()) {
            return path;
        }
        if (const std::string path = existingPath("/System/Library/Fonts/Menlo.ttc"); !path.empty()) {
            return path;
        }
        if (const std::string path = existingPath("/System/Library/Fonts/Monaco.ttf"); !path.empty()) {
            return path;
        }
    }
    if (fontFamily == "Noto Color Emoji" || fontFamily == "Apple Color Emoji" || fontFamily == "Emoji") {
        return "/System/Library/Fonts/Apple Color Emoji.ttc";
    }
    if (fontWeight >= 600) {
        return resolveDefaultUiFontPath();
    }
    return resolveDefaultUiFontPath();
#else
    (void)fontWeight;
    if (fontFamily == "monospace" || fontFamily == "Mono") {
        if (const std::string path = existingPath("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"); !path.empty()) {
            return path;
        }
        if (const std::string path = existingPath("/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf"); !path.empty()) {
            return path;
        }
        if (const std::string path = existingPath("/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf"); !path.empty()) {
            return path;
        }
        if (const std::string path = existingPath("/usr/share/fonts/opentype/noto/NotoSansMono-Regular.ttf"); !path.empty()) {
            return path;
        }
        return "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
    }
    if (fontFamily == "Noto Color Emoji" || fontFamily == "Emoji") {
        return "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    }
    return resolveDefaultUiFontPath();
#endif
}

TextPrimitive::TextPrimitive()
    : impl_(std::make_unique<Impl>()) {}

TextPrimitive::TextPrimitive(float x, float y)
    : impl_(std::make_unique<Impl>(x, y)) {}

TextPrimitive::~TextPrimitive() = default;
TextPrimitive::TextPrimitive(TextPrimitive&&) noexcept = default;
TextPrimitive& TextPrimitive::operator=(TextPrimitive&&) noexcept = default;

bool TextPrimitive::initialize() { return impl_->initialize(); }
void TextPrimitive::destroy() { impl_->destroy(); }
void TextPrimitive::setPosition(float x, float y) { impl_->setPosition(x, y); }
void TextPrimitive::setText(const std::string& text) { impl_->setText(text); }
void TextPrimitive::setFontFamily(const std::string& fontFamily) { impl_->setFontFamily(fontFamily); }
void TextPrimitive::setFontSize(float fontSize) { impl_->setFontSize(fontSize); }
void TextPrimitive::setFontWeight(int fontWeight) { impl_->setFontWeight(fontWeight); }
void TextPrimitive::setColor(const Color& color) { impl_->setColor(color); }
void TextPrimitive::setMaxWidth(float maxWidth) { impl_->setMaxWidth(maxWidth); }
void TextPrimitive::setWrap(bool wrap) { impl_->setWrap(wrap); }
void TextPrimitive::setHorizontalAlign(HorizontalAlign align) { impl_->setHorizontalAlign(align); }
void TextPrimitive::setVerticalAlign(VerticalAlign align) { impl_->setVerticalAlign(align); }
void TextPrimitive::setLineHeight(float lineHeight) { impl_->setLineHeight(lineHeight); }
void TextPrimitive::setStyle(const TextStyle& style) { impl_->setStyle(style); }
void TextPrimitive::setVisualScale(float originX, float originY, float scale) { impl_->setVisualScale(originX, originY, scale); }
void TextPrimitive::setTransform(const Transform& transform, const Rect& frame) { impl_->setTransform(transform, frame); }
void TextPrimitive::setTransformMatrix(const TransformMatrix& matrix) { impl_->setTransformMatrix(matrix); }
const TextStyle& TextPrimitive::style() const { return impl_->style(); }
Vec2 TextPrimitive::position() const { return impl_->position(); }
Vec2 TextPrimitive::measuredSize() { return impl_->measuredSize(); }
void TextPrimitive::prepare() { impl_->prepare(); }
float TextPrimitive::measureTextWidth(const std::string& text,
                                      const std::string& fontFamily,
                                      float fontSize,
                                      int fontWeight) {
    return Impl::measureTextWidth(text, fontFamily, fontSize, fontWeight);
}
TextPrimitive::TextMetrics TextPrimitive::measureTextMetrics(const std::string& text,
                                                             const std::string& fontFamily,
                                                             float fontSize,
                                                             int fontWeight) {
    return Impl::measureTextMetrics(text, fontFamily, fontSize, fontWeight);
}
void TextPrimitive::setDefaultFontFiles(const std::string& textFontFile, const std::string& iconFontFile) {
    Impl::setDefaultFontFiles(textFontFile, iconFontFile);
}
void TextPrimitive::render(int windowWidth, int windowHeight) { impl_->render(windowWidth, windowHeight); }

} // namespace core
