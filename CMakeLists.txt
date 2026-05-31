cmake_minimum_required(VERSION 3.14)
project(EUI-NEO VERSION 1.0)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(EUI_NEO_TOP_LEVEL_DEFAULT ON)
else()
    set(EUI_NEO_TOP_LEVEL_DEFAULT OFF)
endif()
option(EUI_BUILD_APPS "Build EUI-NEO bundled example applications" ${EUI_NEO_TOP_LEVEL_DEFAULT})
set(EUI_WINDOW_BACKEND "glfw" CACHE STRING "Window backend: glfw or sdl2")
set_property(CACHE EUI_WINDOW_BACKEND PROPERTY STRINGS glfw sdl2)
set(EUI_RENDER_BACKEND "opengl" CACHE STRING "Render backend: opengl or vulkan")
set_property(CACHE EUI_RENDER_BACKEND PROPERTY STRINGS opengl vulkan)
if(NOT EUI_WINDOW_BACKEND MATCHES "^(glfw|sdl2)$")
    message(FATAL_ERROR "EUI_WINDOW_BACKEND must be one of: glfw, sdl2")
endif()
if(NOT EUI_RENDER_BACKEND MATCHES "^(opengl|vulkan)$")
    message(FATAL_ERROR "EUI_RENDER_BACKEND must be one of: opengl, vulkan")
endif()
if(EUI_RENDER_BACKEND STREQUAL "vulkan")
    message(FATAL_ERROR "EUI_RENDER_BACKEND=vulkan is not available yet. Finish the backend-neutral runtime, primitive, text, image, and render-cache split first.")
endif()

if(APPLE)
    enable_language(OBJC)
    set_source_files_properties(core/platform/native_bridge.c PROPERTIES LANGUAGE OBJC)
    set_source_files_properties(core/platform/tray_bridge.c PROPERTIES LANGUAGE OBJC)
    set_source_files_properties(core/platform/ime_bridge.c PROPERTIES LANGUAGE OBJC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

include("${CMAKE_CURRENT_SOURCE_DIR}/3rd/dependencies.cmake")

set(CORE_SOURCES
    core/platform/async.cpp
    core/platform/network.cpp
    core/platform/native_bridge.c
    core/platform/platform.cpp
    core/render/image.cpp
    core/render/image_facade.cpp
    core/render/text.cpp
    core/platform/tray_bridge.c
    core/platform/window_backend.cpp
)

if(EUI_RENDER_BACKEND STREQUAL "opengl")
    list(APPEND CORE_SOURCES
        core/render/opengl/opengl_backend.cpp
        core/render/opengl/opengl_primitives.cpp
    )
endif()

if(EUI_WINDOW_BACKEND STREQUAL "glfw")
    list(APPEND CORE_SOURCES core/platform/ime_bridge.c)
endif()

add_library(eui_neo STATIC ${CORE_SOURCES})
add_library(eui::neo ALIAS eui_neo)
target_include_directories(eui_neo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR} ${TRAY_INCLUDE_DIR})
target_link_libraries(eui_neo PUBLIC Threads::Threads)

if(EUI_RENDER_BACKEND STREQUAL "opengl")
    target_compile_definitions(eui_neo PUBLIC EUI_RENDER_BACKEND_OPENGL=1)
    target_link_libraries(eui_neo PUBLIC glad OpenGL::GL)
endif()

if(EUI_WINDOW_BACKEND STREQUAL "sdl2")
    if(NOT TARGET SDL2::SDL2)
        find_package(SDL2 CONFIG REQUIRED)
    endif()
    target_compile_definitions(eui_neo PUBLIC EUI_WINDOW_BACKEND_SDL2=1)
    target_link_libraries(eui_neo PUBLIC SDL2::SDL2)
    set(EUI_APP_MAIN_SOURCE core/app/sdl2_app_main.cpp)
else()
    target_link_libraries(eui_neo PUBLIC glfw)
    set(EUI_APP_MAIN_SOURCE core/app/glfw_app_main.cpp)
endif()

if(TARGET harfbuzz::harfbuzz)
    target_link_libraries(eui_neo PUBLIC harfbuzz::harfbuzz)
    target_compile_definitions(eui_neo PUBLIC EUI_HAS_HARFBUZZ=1)
elseif(TARGET PkgConfig::EUI_HARFBUZZ)
    target_link_libraries(eui_neo PUBLIC Freetype::Freetype)
    target_link_libraries(eui_neo PUBLIC PkgConfig::EUI_HARFBUZZ)
    target_compile_definitions(eui_neo PUBLIC EUI_HAS_HARFBUZZ=1)
else()
    target_link_libraries(eui_neo PUBLIC Freetype::Freetype)
endif()

if(MSVC)
    target_compile_options(eui_neo PRIVATE $<$<NOT:$<CONFIG:Debug>>:/O1> /GS- /sdl- /utf-8 /wd4819)
elseif(APPLE)
    target_compile_options(eui_neo PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Os -fno-exceptions -fno-rtti>)
else()
    target_compile_options(eui_neo PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Os -fno-exceptions -fno-rtti>)
endif()

if(WIN32)
    target_compile_definitions(eui_neo PUBLIC EUI_TRAY_WINAPI=1 NOMINMAX)
    target_link_libraries(eui_neo PUBLIC winmm urlmon shell32 user32 imm32)
elseif(APPLE)
    target_compile_definitions(eui_neo PUBLIC EUI_TRAY_APPKIT=1)
    target_link_libraries(eui_neo PUBLIC "-framework Cocoa" objc)
elseif(UNIX AND EUI_APPINDICATOR_FOUND AND EUI_GTK3_FOUND)
    target_compile_definitions(eui_neo PUBLIC EUI_TRAY_APPINDICATOR=1)
    target_include_directories(eui_neo PUBLIC ${EUI_APPINDICATOR_INCLUDE_DIRS} ${EUI_GTK3_INCLUDE_DIRS})
    target_compile_options(eui_neo PRIVATE ${EUI_APPINDICATOR_CFLAGS_OTHER} ${EUI_GTK3_CFLAGS_OTHER})
    target_link_directories(eui_neo PUBLIC ${EUI_APPINDICATOR_LIBRARY_DIRS} ${EUI_GTK3_LIBRARY_DIRS})
    target_link_libraries(eui_neo PUBLIC ${EUI_APPINDICATOR_LIBRARIES} ${EUI_GTK3_LIBRARIES})
endif()

if(TARGET CURL::libcurl)
    target_link_libraries(eui_neo PUBLIC CURL::libcurl)
    target_compile_definitions(eui_neo PUBLIC EUI_HAS_CURL=1)
endif()

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

function(eui_neo_copy_assets target_name)
    if(APP_ASSET_FILES)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${target_name}>/assets
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${APP_ASSET_FILES} $<TARGET_FILE_DIR:${target_name}>/assets
        )
    endif()
endfunction()

function(eui_neo_configure_app target_name)
    target_link_libraries(${target_name} PRIVATE eui::neo)

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

    if(WIN32 AND EUI_WINDOW_BACKEND STREQUAL "glfw")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:glfw>
            $<TARGET_FILE_DIR:${target_name}>
        )
    endif()

    eui_neo_copy_assets(${target_name})
endfunction()

if(EUI_BUILD_APPS)
    file(GLOB APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp")
    foreach(app_source ${APP_SOURCES})
        get_filename_component(app_name "${app_source}" NAME_WE)
        add_executable(${app_name} ${EUI_APP_MAIN_SOURCE} "${app_source}" ${APP_ICON_RESOURCE})
        eui_neo_configure_app(${app_name})
    endforeach()
endif()
