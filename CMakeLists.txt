cmake_minimum_required(VERSION 3.14)
project(EUI-NEO VERSION 1.0)

if(APPLE)
    enable_language(OBJC)
    set_source_files_properties(core/tray_bridge.c PROPERTIES LANGUAGE OBJC)
    set_source_files_properties(core/ime_bridge.c PROPERTIES LANGUAGE OBJC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

set(EUI_DEPS_MODE "auto" CACHE STRING "Third-party dependency mode: bundled, auto, or fetch")
set_property(CACHE EUI_DEPS_MODE PROPERTY STRINGS bundled auto fetch)
if(NOT EUI_DEPS_MODE MATCHES "^(bundled|auto|fetch)$")
    message(FATAL_ERROR "EUI_DEPS_MODE must be one of: bundled, auto, fetch")
endif()
option(EUI_ENABLE_HARFBUZZ "Enable HarfBuzz shaping for complex text" ON)

set(EUI_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rd")
list(PREPEND CMAKE_MODULE_PATH "${EUI_THIRD_PARTY_DIR}")

function(eui_use_bundled_dependency out_var dep_name source_dir marker_file)
    if(EUI_DEPS_MODE STREQUAL "fetch")
        set(${out_var} FALSE PARENT_SCOPE)
        return()
    endif()

    if(EXISTS "${source_dir}/${marker_file}")
        set(${out_var} TRUE PARENT_SCOPE)
        return()
    endif()

    if(EUI_DEPS_MODE STREQUAL "bundled")
        message(FATAL_ERROR
            "Bundled dependency '${dep_name}' is missing at ${source_dir}. "
            "Restore the vendored source, or configure with -DEUI_DEPS_MODE=auto/fetch to allow network fetches."
        )
    endif()

    set(${out_var} FALSE PARENT_SCOPE)
endfunction()

function(eui_fetch_dependency source_dir_var content_name source_url)
    FetchContent_Declare(
        ${content_name}
        URL "${source_url}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        TIMEOUT 30
    )
    FetchContent_GetProperties(${content_name})
    if(NOT ${content_name}_POPULATED)
        if(POLICY CMP0169)
            cmake_policy(PUSH)
            cmake_policy(SET CMP0169 OLD)
        endif()
        FetchContent_Populate(${content_name})
        if(POLICY CMP0169)
            cmake_policy(POP)
        endif()
    endif()
    set(${source_dir_var} "${${content_name}_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target" FORCE)

include(FetchContent)

eui_use_bundled_dependency(
    EUI_USE_BUNDLED_GLFW
    "GLFW"
    "${EUI_THIRD_PARTY_DIR}/glfw"
    "CMakeLists.txt"
)

set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)
if(EUI_USE_BUNDLED_GLFW)
    add_subdirectory("${EUI_THIRD_PARTY_DIR}/glfw" "${CMAKE_CURRENT_BINARY_DIR}/_deps/glfw-bundled-build" EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(
        glfw
        URL https://github.com/glfw/glfw/archive/refs/tags/3.4.zip
        URL_HASH SHA256=a133ddc3d3c66143eba9035621db8e0bcf34dba1ee9514a9e23e96afd39fd57a
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        TIMEOUT 30
    )
    FetchContent_MakeAvailable(glfw)
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
eui_use_bundled_dependency(
    EUI_USE_BUNDLED_GLAD
    "glad"
    "${EUI_THIRD_PARTY_DIR}/glad"
    "src/glad.c"
)

if(EUI_USE_BUNDLED_GLAD)
    set(glad_SOURCE_DIR "${EUI_THIRD_PARTY_DIR}/glad")
else()
    FetchContent_Declare(
        glad
        URL https://github.com/libigl/libigl-glad/archive/651a425101365aa6e8504988ef9bb363d066c5ee.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        TIMEOUT 30
    )
    FetchContent_GetProperties(glad)
    if(NOT glad_POPULATED)
        if(POLICY CMP0169)
            cmake_policy(PUSH)
            cmake_policy(SET CMP0169 OLD)
        endif()
        FetchContent_Populate(glad)
        if(POLICY CMP0169)
            cmake_policy(POP)
        endif()
    endif()
endif()
add_library(glad "${glad_SOURCE_DIR}/src/glad.c")
target_include_directories(glad PUBLIC "${glad_SOURCE_DIR}/include")
if(NOT WIN32)
    target_link_libraries(glad PUBLIC ${CMAKE_DL_LIBS})
endif()
set_target_properties(glad PROPERTIES POSITION_INDEPENDENT_CODE ON)

eui_use_bundled_dependency(
    EUI_USE_BUNDLED_TRAY
    "tray"
    "${EUI_THIRD_PARTY_DIR}/tray"
    "tray.h"
)

if(EUI_USE_BUNDLED_TRAY)
    set(TRAY_INCLUDE_DIR "${EUI_THIRD_PARTY_DIR}/tray")
else()
    FetchContent_Declare(
        tray
        URL https://github.com/zserge/tray/archive/8dd1358b92562faf7c032cf5362fa97cbc7e13e9.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        TIMEOUT 30
    )
    FetchContent_MakeAvailable(tray)
    set(TRAY_INCLUDE_DIR "${tray_SOURCE_DIR}")
endif()

eui_use_bundled_dependency(
    EUI_USE_BUNDLED_FREETYPE
    "FreeType"
    "${EUI_THIRD_PARTY_DIR}/freetype"
    "CMakeLists.txt"
)

eui_use_bundled_dependency(
    EUI_USE_BUNDLED_ZLIB
    "zlib"
    "${EUI_THIRD_PARTY_DIR}/zlib-1.3.1"
    "zlib.h"
)

eui_use_bundled_dependency(
    EUI_USE_BUNDLED_LIBPNG
    "libpng"
    "${EUI_THIRD_PARTY_DIR}/libpng-1.6.43"
    "png.h"
)

if(EUI_ENABLE_HARFBUZZ)
    eui_use_bundled_dependency(
        EUI_USE_BUNDLED_HARFBUZZ
        "HarfBuzz"
        "${EUI_THIRD_PARTY_DIR}/harfbuzz"
        "CMakeLists.txt"
    )
endif()

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

if(EUI_USE_BUNDLED_ZLIB)
    set(EUI_ZLIB_DIR "${EUI_THIRD_PARTY_DIR}/zlib-1.3.1")
else()
    eui_fetch_dependency(
        EUI_ZLIB_DIR
        eui_zlib
        "https://github.com/madler/zlib/archive/refs/tags/v1.3.1.zip"
    )
endif()

add_library(eui_zlib STATIC
    "${EUI_ZLIB_DIR}/adler32.c"
    "${EUI_ZLIB_DIR}/compress.c"
    "${EUI_ZLIB_DIR}/crc32.c"
    "${EUI_ZLIB_DIR}/deflate.c"
    "${EUI_ZLIB_DIR}/gzclose.c"
    "${EUI_ZLIB_DIR}/gzlib.c"
    "${EUI_ZLIB_DIR}/gzread.c"
    "${EUI_ZLIB_DIR}/gzwrite.c"
    "${EUI_ZLIB_DIR}/inflate.c"
    "${EUI_ZLIB_DIR}/infback.c"
    "${EUI_ZLIB_DIR}/inftrees.c"
    "${EUI_ZLIB_DIR}/inffast.c"
    "${EUI_ZLIB_DIR}/trees.c"
    "${EUI_ZLIB_DIR}/uncompr.c"
    "${EUI_ZLIB_DIR}/zutil.c"
)
target_include_directories(eui_zlib PUBLIC "${EUI_ZLIB_DIR}")
if(UNIX)
    target_compile_definitions(eui_zlib PRIVATE HAVE_UNISTD_H)
endif()
set_target_properties(eui_zlib PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS eui_zlib)
endif()

if(EUI_USE_BUNDLED_LIBPNG)
    set(EUI_LIBPNG_DIR "${EUI_THIRD_PARTY_DIR}/libpng-1.6.43")
else()
    eui_fetch_dependency(
        EUI_LIBPNG_DIR
        eui_libpng
        "https://github.com/pnggroup/libpng/archive/refs/tags/v1.6.43.zip"
    )
endif()

set(PNG_SHARED OFF CACHE BOOL "Build libpng as a shared library" FORCE)
set(PNG_STATIC ON CACHE BOOL "Build libpng as a static library" FORCE)
set(PNG_FRAMEWORK OFF CACHE BOOL "Build libpng as a framework bundle" FORCE)
set(PNG_TESTS OFF CACHE BOOL "Build the libpng tests" FORCE)
set(PNG_TOOLS OFF CACHE BOOL "Build the libpng tools" FORCE)
set(PNG_EXECUTABLES OFF CACHE BOOL "Build the libpng executables" FORCE)
set(PNG_BUILD_ZLIB OFF CACHE BOOL "Use find_package(ZLIB) for libpng" FORCE)
set(PNG_HARDWARE_OPTIMIZATIONS OFF CACHE BOOL "Disable libpng hardware optimizations for portable bundled builds" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "Disable install rules for bundled libpng" FORCE)
add_subdirectory("${EUI_LIBPNG_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/libpng-bundled-build" EXCLUDE_FROM_ALL)
if(TARGET png_static)
    target_include_directories(png_static PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/_deps/libpng-bundled-build>")
endif()
if(TARGET png_static AND NOT TARGET PNG::PNG)
    add_library(PNG::PNG ALIAS png_static)
endif()

set(FT_DISABLE_ZLIB ON CACHE BOOL "Disable zlib support in bundled FreeType" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "Disable bzip2 support in bundled FreeType" FORCE)
set(FT_DISABLE_PNG OFF CACHE BOOL "Disable png support in bundled FreeType" FORCE)
set(FT_REQUIRE_PNG ON CACHE BOOL "Require png support in bundled FreeType" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "Disable HarfBuzz support in bundled FreeType" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "Disable brotli support in bundled FreeType" FORCE)
if(EUI_USE_BUNDLED_FREETYPE)
    set(EUI_FREETYPE_DIR "${EUI_THIRD_PARTY_DIR}/freetype")
else()
    eui_fetch_dependency(
        EUI_FREETYPE_DIR
        eui_freetype
        "https://download.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.xz"
    )
endif()

add_subdirectory("${EUI_FREETYPE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/freetype-bundled-build" EXCLUDE_FROM_ALL)
if(TARGET freetype AND NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif()
if(MSVC AND TARGET freetype)
    target_compile_options(freetype PRIVATE /utf-8 /wd4819 /wd4244 /wd4267)
endif()

if(NOT WIN32)
    find_package(CURL REQUIRED)
else()
    find_package(CURL QUIET)
endif()

if(EUI_ENABLE_HARFBUZZ)
    find_package(PkgConfig QUIET)
    if(EUI_USE_BUNDLED_HARFBUZZ)
        set(EUI_HARFBUZZ_DIR "${EUI_THIRD_PARTY_DIR}/harfbuzz")
    else()
        eui_fetch_dependency(
            EUI_HARFBUZZ_DIR
            eui_harfbuzz
            "https://github.com/harfbuzz/harfbuzz/archive/refs/tags/8.5.0.zip"
        )
    endif()

    set(HB_HAVE_CAIRO OFF CACHE BOOL "Disable Cairo support in bundled HarfBuzz" FORCE)
    set(HB_HAVE_GRAPHITE2 OFF CACHE BOOL "Disable Graphite2 support in bundled HarfBuzz" FORCE)
    set(HB_HAVE_GLIB OFF CACHE BOOL "Disable GLib support in bundled HarfBuzz" FORCE)
    set(HB_HAVE_GOBJECT OFF CACHE BOOL "Disable GObject support in bundled HarfBuzz" FORCE)
    set(HB_HAVE_ICU OFF CACHE BOOL "Disable ICU support in bundled HarfBuzz" FORCE)
    set(HB_HAVE_INTROSPECTION OFF CACHE BOOL "Disable introspection support in bundled HarfBuzz" FORCE)
    set(HB_BUILD_UTILS OFF CACHE BOOL "Disable HarfBuzz utility tools" FORCE)
    set(HB_BUILD_SUBSET OFF CACHE BOOL "Disable HarfBuzz subset library" FORCE)
    add_subdirectory("${EUI_HARFBUZZ_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/harfbuzz-bundled-build" EXCLUDE_FROM_ALL)
    if(TARGET harfbuzz AND NOT TARGET harfbuzz::harfbuzz)
        add_library(harfbuzz::harfbuzz ALIAS harfbuzz)
    endif()
    if(MSVC AND TARGET harfbuzz)
        target_compile_options(harfbuzz PRIVATE /utf-8 /wd4819 /wd4244 /wd4267)
    endif()
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
else()
    find_package(PkgConfig QUIET)
endif()

if(UNIX AND NOT APPLE)
    if(PkgConfig_FOUND)
        pkg_check_modules(EUI_APPINDICATOR QUIET appindicator3-0.1)
        pkg_check_modules(EUI_GTK3 QUIET gtk+-3.0)
    endif()
endif()

set(CORE_SOURCES
    core/async.cpp
    core/image.cpp
    core/ime_bridge.c
    core/network.cpp
    core/platform.cpp
    core/tray_bridge.c
    core/text.cpp
)

set(APP_ASSET_FILES)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    file(GLOB APP_ASSET_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
endif()

set(APP_ICON_RESOURCE)
if(WIN32)
    set(APP_ICON_ICO "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico")
    set(APP_ICON_RC "${CMAKE_CURRENT_BINARY_DIR}/eui_app_icon.rc")
    file(TO_NATIVE_PATH "${APP_ICON_ICO}" APP_ICON_ICO_NATIVE)
    string(REPLACE "\\" "\\\\" APP_ICON_ICO_ESCAPED "${APP_ICON_ICO_NATIVE}")
    file(WRITE "${APP_ICON_RC}" "IDI_APP_ICON ICON \"${APP_ICON_ICO_ESCAPED}\"\n")
    set(APP_ICON_RESOURCE "${APP_ICON_RC}")
endif()

function(configure_opengl_app target_name)
    target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${TRAY_INCLUDE_DIR})
    target_link_libraries(${target_name} PRIVATE glfw glad OpenGL::GL Threads::Threads Freetype::Freetype)

    if(TARGET harfbuzz::harfbuzz)
        target_link_libraries(${target_name} PRIVATE harfbuzz::harfbuzz)
        target_compile_definitions(${target_name} PRIVATE EUI_HAS_HARFBUZZ=1)
    elseif(TARGET PkgConfig::EUI_HARFBUZZ)
        target_link_libraries(${target_name} PRIVATE PkgConfig::EUI_HARFBUZZ)
        target_compile_definitions(${target_name} PRIVATE EUI_HAS_HARFBUZZ=1)
    endif()

    if(MSVC)
        set_target_properties(${target_name} PROPERTIES VS_GLOBAL_VcpkgXUseBuiltInApplocalDeps true)
        target_compile_options(${target_name} PRIVATE $<$<NOT:$<CONFIG:Debug>>:/O1> /GS- /sdl- /utf-8 /wd4819)
        target_link_options(${target_name} PRIVATE /OPT:REF /OPT:ICF /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWS)
    elseif(APPLE)
        target_compile_options(${target_name} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Os -fno-exceptions -fno-rtti>)
        target_link_options(${target_name} PRIVATE -Wl,-dead_strip)
    else()
        target_compile_options(${target_name} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Os -fno-exceptions -fno-rtti>)
        target_link_options(${target_name} PRIVATE -Wl,--gc-sections -s)
    endif()

    if(WIN32)
        target_compile_definitions(${target_name} PRIVATE EUI_TRAY_WINAPI=1)
        target_link_libraries(${target_name} PRIVATE winmm urlmon shell32 user32 imm32)

        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:glfw>
            $<TARGET_FILE_DIR:${target_name}>
        )
    elseif(APPLE)
        target_compile_definitions(${target_name} PRIVATE EUI_TRAY_APPKIT=1)
        target_link_libraries(${target_name} PRIVATE "-framework Cocoa" objc)
    elseif(UNIX AND EUI_APPINDICATOR_FOUND AND EUI_GTK3_FOUND)
        target_compile_definitions(${target_name} PRIVATE EUI_TRAY_APPINDICATOR=1)
        target_include_directories(${target_name} PRIVATE ${EUI_APPINDICATOR_INCLUDE_DIRS} ${EUI_GTK3_INCLUDE_DIRS})
        target_compile_options(${target_name} PRIVATE ${EUI_APPINDICATOR_CFLAGS_OTHER} ${EUI_GTK3_CFLAGS_OTHER})
        target_link_directories(${target_name} PRIVATE ${EUI_APPINDICATOR_LIBRARY_DIRS} ${EUI_GTK3_LIBRARY_DIRS})
        target_link_libraries(${target_name} PRIVATE ${EUI_APPINDICATOR_LIBRARIES} ${EUI_GTK3_LIBRARIES})
    endif()

    if(TARGET CURL::libcurl)
        target_link_libraries(${target_name} PRIVATE CURL::libcurl)
        target_compile_definitions(${target_name} PRIVATE EUI_HAS_CURL=1)
    endif()

    if(APP_ASSET_FILES)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${target_name}>/assets
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${APP_ASSET_FILES} $<TARGET_FILE_DIR:${target_name}>/assets
        )
    endif()
endfunction()

file(GLOB APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp")
foreach(app_source ${APP_SOURCES})
    get_filename_component(app_name "${app_source}" NAME_WE)
    add_executable(${app_name} main.cpp "${app_source}" ${CORE_SOURCES} ${APP_ICON_RESOURCE})
    configure_opengl_app(${app_name})
endforeach()
