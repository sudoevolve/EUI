function(eui_apply_example_warnings target_name)
    if(NOT EUI_STRICT_WARNINGS)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(eui_can_fetch_git_repository repository out_can_fetch)
    set(can_fetch OFF)

    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" ls-remote --heads "${repository}"
            RESULT_VARIABLE git_result
            OUTPUT_QUIET
            ERROR_QUIET
            TIMEOUT 8
        )
        if(git_result EQUAL 0)
            set(can_fetch ON)
        endif()
    endif()

    set(${out_can_fetch} "${can_fetch}" PARENT_SCOPE)
endfunction()

function(eui_read_example_config source_file out_backend out_platform out_mode)
    file(READ "${source_file}" source_text LIMIT 4096)

    set(example_backend "")
    set(example_platform "")
    set(example_mode "DEFAULT")

    if(source_text MATCHES "#[ \t]*define[ \t]+EUI_BACKEND_VULKAN([ \t]+1)?"
       OR source_text MATCHES "#[ \t]*define[ \t]+EUI_BACKEND_VK([ \t]+1)?")
        set(example_backend "VULKAN")
        set(example_mode "PUBLIC_MACROS")
    elseif(source_text MATCHES "#[ \t]*define[ \t]+EUI_BACKEND_OPENGL([ \t]+1)?")
        set(example_backend "OPENGL")
        set(example_mode "PUBLIC_MACROS")
    endif()

    if(source_text MATCHES "#[ \t]*define[ \t]+EUI_PLATFORM_GLFW([ \t]+1)?")
        set(example_platform "GLFW")
        set(example_mode "PUBLIC_MACROS")
    elseif(source_text MATCHES "#[ \t]*define[ \t]+EUI_PLATFORM_SDL2([ \t]+1)?"
        OR source_text MATCHES "#[ \t]*define[ \t]+EUI_PLATFORM_SDL([ \t]+1)?")
        set(example_platform "SDL2")
        set(example_mode "PUBLIC_MACROS")
    endif()

    if(NOT example_platform)
        if(source_text MATCHES "#[ \t]*define[ \t]+EUI_ENABLE_GLFW_OPENGL_BACKEND([ \t]+1)?")
            set(example_backend "OPENGL")
            set(example_platform "GLFW")
            set(example_mode "LEGACY_MACROS")
        elseif(source_text MATCHES "#[ \t]*define[ \t]+EUI_ENABLE_SDL2_OPENGL_BACKEND([ \t]+1)?")
            set(example_backend "OPENGL")
            set(example_platform "SDL2")
            set(example_mode "LEGACY_MACROS")
        endif()
    endif()

    if(example_platform AND NOT example_backend)
        set(example_backend "OPENGL")
    endif()

    set(${out_backend} "${example_backend}" PARENT_SCOPE)
    set(${out_platform} "${example_platform}" PARENT_SCOPE)
    set(${out_mode} "${example_mode}" PARENT_SCOPE)
endfunction()

function(eui_add_example source_file opengl_target)
    get_filename_component(target_name "${source_file}" NAME_WE)

    eui_read_example_config("${source_file}" example_backend example_platform example_mode)

    set(example_link_libs "${opengl_target}")
    set(example_include_dirs "")
    set(example_compile_defs "")

    if(example_platform)
        if(NOT example_backend STREQUAL "OPENGL")
            message(STATUS
                "Skipping ${target_name}: ${source_file} requests ${example_backend}, "
                "but only OpenGL examples are implemented right now."
            )
            return()
        endif()

        if(example_platform STREQUAL "GLFW")
            if(NOT EUI_GLFW_TARGET)
                message(STATUS
                    "Skipping ${target_name}: ${source_file} requests GLFW/OpenGL, "
                    "but GLFW was not found."
                )
                return()
            endif()
            list(APPEND example_link_libs ${EUI_GLFW_TARGET})
        elseif(example_platform STREQUAL "SDL2")
            if(NOT EUI_SDL2_TARGET)
                message(STATUS
                    "Skipping ${target_name}: ${source_file} requests SDL2/OpenGL, "
                    "but SDL2 was not found."
                )
                return()
            endif()
            list(APPEND example_link_libs ${EUI_SDL2_TARGET})
            list(APPEND example_include_dirs ${EUI_SDL2_INCLUDE_DIRS})
        endif()
    else()
        list(APPEND example_link_libs ${EUI_EXAMPLE_FALLBACK_LINK_LIBS})
        list(APPEND example_include_dirs ${EUI_EXAMPLE_FALLBACK_INCLUDE_DIRS})
        list(APPEND example_compile_defs ${EUI_EXAMPLE_FALLBACK_COMPILE_DEFS})

        if(NOT EUI_EXAMPLE_FALLBACK_LINK_LIBS)
            message(STATUS
                "Skipping ${target_name}: no example backend was selected. "
                "Add EUI_BACKEND_OPENGL + EUI_PLATFORM_GLFW/SDL2 in the source file "
                "or use the fallback CMake options."
            )
            return()
        endif()
    endif()

    add_executable(${target_name} WIN32 "${source_file}")
    target_link_libraries(${target_name} PRIVATE EUI::eui ${example_link_libs})
    if(example_include_dirs)
        target_include_directories(${target_name} PRIVATE ${example_include_dirs})
    endif()
    if(example_compile_defs)
        target_compile_definitions(${target_name} PRIVATE ${example_compile_defs})
    endif()
    set_target_properties(${target_name} PROPERTIES OUTPUT_NAME "${target_name}")
    eui_apply_example_warnings(${target_name})
endfunction()

function(eui_add_compat_example_target compat_name actual_target)
    if(TARGET ${actual_target} AND NOT TARGET ${compat_name})
        add_custom_target(${compat_name} DEPENDS ${actual_target})
    endif()
endfunction()

function(eui_find_glfw_target out_target allow_fetch)
    find_package(glfw3 CONFIG QUIET)
    if(NOT TARGET glfw AND NOT TARGET glfw3::glfw AND NOT TARGET glfw3)
        find_package(glfw3 QUIET)
    endif()

    set(glfw_target "")
    if(TARGET glfw)
        set(glfw_target glfw)
    elseif(TARGET glfw3::glfw)
        set(glfw_target glfw3::glfw)
    elseif(TARGET glfw3)
        set(glfw_target glfw3)
    endif()

    if(NOT glfw_target AND allow_fetch AND EUI_FETCH_GLFW_FROM_GIT)
        eui_can_fetch_git_repository("https://github.com/glfw/glfw.git" glfw_can_fetch)
        if(glfw_can_fetch)
            message(STATUS "GLFW not found locally. Fetching from Git (${EUI_GLFW_GIT_TAG})...")

            set(GLFW_BUILD_DOCS OFF CACHE BOOL "Disable GLFW docs" FORCE)
            set(GLFW_BUILD_TESTS OFF CACHE BOOL "Disable GLFW tests" FORCE)
            set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Disable GLFW examples" FORCE)
            set(GLFW_INSTALL OFF CACHE BOOL "Disable GLFW install target" FORCE)

            FetchContent_Declare(
                glfw
                GIT_REPOSITORY https://github.com/glfw/glfw.git
                GIT_TAG ${EUI_GLFW_GIT_TAG}
                GIT_SHALLOW TRUE
            )
            FetchContent_MakeAvailable(glfw)

            if(TARGET glfw)
                set(glfw_target glfw)
            elseif(TARGET glfw3::glfw)
                set(glfw_target glfw3::glfw)
            elseif(TARGET glfw3)
                set(glfw_target glfw3)
            endif()
        else()
            message(WARNING
                "GLFW not found locally and Git fetch is unavailable right now. "
                "Skipping GLFW-backed examples."
            )
        endif()
    endif()

    set(${out_target} "${glfw_target}" PARENT_SCOPE)
endfunction()

function(eui_find_sdl2_target out_target out_include_dirs allow_fetch)
    set(sdl2_target "")
    set(sdl2_include_dirs "")

    find_package(SDL2 CONFIG QUIET)
    if(TARGET SDL2::SDL2)
        set(sdl2_target SDL2::SDL2)
    elseif(TARGET SDL2::SDL2-static)
        set(sdl2_target SDL2::SDL2-static)
    else()
        find_package(SDL2 QUIET)
        if(SDL2_FOUND)
            set(sdl2_target ${SDL2_LIBRARIES})
            set(sdl2_include_dirs ${SDL2_INCLUDE_DIRS})
        endif()
    endif()

    if(NOT sdl2_target AND allow_fetch AND EUI_FETCH_SDL2_FROM_GIT)
        eui_can_fetch_git_repository("https://github.com/libsdl-org/SDL.git" sdl2_can_fetch)
        if(sdl2_can_fetch)
            message(STATUS "SDL2 not found locally. Fetching from Git (${EUI_SDL2_GIT_TAG})...")

            set(SDL_SHARED OFF CACHE BOOL "Disable shared SDL2 target" FORCE)
            set(SDL_STATIC ON CACHE BOOL "Enable static SDL2 target" FORCE)
            set(SDL_TEST_LIBRARY OFF CACHE BOOL "Disable SDL2 test library" FORCE)
            set(SDL_TESTS OFF CACHE BOOL "Disable SDL2 tests" FORCE)

            FetchContent_Declare(
                sdl2
                GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
                GIT_TAG ${EUI_SDL2_GIT_TAG}
                GIT_SHALLOW TRUE
            )
            FetchContent_MakeAvailable(sdl2)

            if(TARGET SDL2::SDL2)
                set(sdl2_target SDL2::SDL2)
            elseif(TARGET SDL2::SDL2-static)
                set(sdl2_target SDL2::SDL2-static)
            elseif(TARGET SDL2-static)
                set(sdl2_target SDL2-static)
            elseif(TARGET SDL2)
                set(sdl2_target SDL2)
            endif()
        else()
            message(WARNING
                "SDL2 not found locally and Git fetch is unavailable right now. "
                "Skipping SDL2-backed examples."
            )
        endif()
    endif()

    set(${out_target} "${sdl2_target}" PARENT_SCOPE)
    set(${out_include_dirs} "${sdl2_include_dirs}" PARENT_SCOPE)
endfunction()

function(eui_resolve_window_backends out_glfw_target out_sdl2_target out_sdl2_include_dirs out_mode_label)
    set(use_legacy_flags OFF)
    if(NOT EUI_ENABLE_GLFW_WINDOW_BACKEND OR NOT EUI_ENABLE_SDL2_WINDOW_BACKEND)
        set(use_legacy_flags ON)
    endif()

    if(use_legacy_flags)
        set(glfw_target "")
        set(sdl2_target "")
        set(sdl2_include_dirs "")
        if(EUI_ENABLE_GLFW_WINDOW_BACKEND)
            eui_find_glfw_target(glfw_target TRUE)
        endif()
        if(EUI_ENABLE_SDL2_WINDOW_BACKEND)
            eui_find_sdl2_target(sdl2_target sdl2_include_dirs TRUE)
        endif()
        set(${out_glfw_target} "${glfw_target}" PARENT_SCOPE)
        set(${out_sdl2_target} "${sdl2_target}" PARENT_SCOPE)
        set(${out_sdl2_include_dirs} "${sdl2_include_dirs}" PARENT_SCOPE)
        set(${out_mode_label} "LEGACY_FLAGS" PARENT_SCOPE)
        return()
    endif()

    set(window_backend_mode "${EUI_WINDOW_BACKEND}")
    if(window_backend_mode STREQUAL "")
        set(window_backend_mode "AUTO")
    endif()
    string(TOUPPER "${window_backend_mode}" window_backend_mode)

    set(valid_modes AUTO GLFW SDL2 ALL)
    list(FIND valid_modes "${window_backend_mode}" backend_mode_index)
    if(backend_mode_index EQUAL -1)
        message(FATAL_ERROR
            "Invalid EUI_WINDOW_BACKEND='${EUI_WINDOW_BACKEND}'. "
            "Expected one of: AUTO, GLFW, SDL2, ALL."
        )
    endif()

    set(glfw_target "")
    set(sdl2_target "")
    set(sdl2_include_dirs "")
    set(mode_label "${window_backend_mode}")

    if(window_backend_mode STREQUAL "GLFW")
        eui_find_glfw_target(glfw_target TRUE)
    elseif(window_backend_mode STREQUAL "SDL2")
        eui_find_sdl2_target(sdl2_target sdl2_include_dirs TRUE)
    elseif(window_backend_mode STREQUAL "ALL")
        eui_find_glfw_target(glfw_target TRUE)
        eui_find_sdl2_target(sdl2_target sdl2_include_dirs TRUE)
    elseif(window_backend_mode STREQUAL "AUTO")
        eui_find_sdl2_target(sdl2_target sdl2_include_dirs FALSE)
        if(sdl2_target)
            set(mode_label "AUTO->SDL2(local)")
        else()
            eui_find_glfw_target(glfw_target FALSE)
            if(glfw_target)
                set(mode_label "AUTO->GLFW(local)")
            else()
                eui_find_glfw_target(glfw_target TRUE)
                if(glfw_target)
                    set(mode_label "AUTO->GLFW(fetch)")
                else()
                    eui_find_sdl2_target(sdl2_target sdl2_include_dirs TRUE)
                    if(sdl2_target)
                        set(mode_label "AUTO->SDL2(fetch)")
                    else()
                        set(mode_label "AUTO->none")
                    endif()
                endif()
            endif()
        endif()
    endif()

    set(${out_glfw_target} "${glfw_target}" PARENT_SCOPE)
    set(${out_sdl2_target} "${sdl2_target}" PARENT_SCOPE)
    set(${out_sdl2_include_dirs} "${sdl2_include_dirs}" PARENT_SCOPE)
    set(${out_mode_label} "${mode_label}" PARENT_SCOPE)
endfunction()

function(eui_configure_examples)
    include(FetchContent)

    set(EUI_OPENGL_TARGET "")
    find_package(OpenGL QUIET)
    if(TARGET OpenGL::GL)
        set(EUI_OPENGL_TARGET OpenGL::GL)
    elseif(WIN32)
        # On Windows OpenGL comes from the system SDK.
        set(EUI_OPENGL_TARGET opengl32)
    elseif(APPLE)
        find_library(EUI_OPENGL_FRAMEWORK OpenGL)
        if(EUI_OPENGL_FRAMEWORK)
            set(EUI_OPENGL_TARGET ${EUI_OPENGL_FRAMEWORK})
        endif()
    endif()

    file(GLOB EUI_EXAMPLE_SOURCE_PATHS CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/examples/*.cpp"
    )
    list(SORT EUI_EXAMPLE_SOURCE_PATHS)

    set(EUI_ANY_SOURCE_GLFW OFF)
    set(EUI_ANY_SOURCE_SDL2 OFF)
    set(EUI_ANY_SOURCE_DEFAULT OFF)
    foreach(example_source_path IN LISTS EUI_EXAMPLE_SOURCE_PATHS)
        eui_read_example_config("${example_source_path}" example_backend example_platform example_mode)
        if(example_platform STREQUAL "GLFW")
            set(EUI_ANY_SOURCE_GLFW ON)
        elseif(example_platform STREQUAL "SDL2")
            set(EUI_ANY_SOURCE_SDL2 ON)
        else()
            set(EUI_ANY_SOURCE_DEFAULT ON)
        endif()
    endforeach()

    set(EUI_FALLBACK_GLFW_TARGET "")
    set(EUI_FALLBACK_SDL2_TARGET "")
    set(EUI_FALLBACK_SDL2_INCLUDE_DIRS "")
    set(EUI_WINDOW_BACKEND_MODE_LABEL "SOURCE_MACROS_ONLY")
    if(EUI_ANY_SOURCE_DEFAULT)
        eui_resolve_window_backends(EUI_FALLBACK_GLFW_TARGET EUI_FALLBACK_SDL2_TARGET
                                    EUI_FALLBACK_SDL2_INCLUDE_DIRS EUI_WINDOW_BACKEND_MODE_LABEL)
    endif()

    set(EUI_GLFW_TARGET "${EUI_FALLBACK_GLFW_TARGET}")
    set(EUI_SDL2_TARGET "${EUI_FALLBACK_SDL2_TARGET}")
    set(EUI_SDL2_INCLUDE_DIRS "${EUI_FALLBACK_SDL2_INCLUDE_DIRS}")

    if(EUI_ANY_SOURCE_GLFW AND NOT EUI_GLFW_TARGET)
        eui_find_glfw_target(EUI_GLFW_TARGET TRUE)
    endif()
    if(EUI_ANY_SOURCE_SDL2 AND NOT EUI_SDL2_TARGET)
        eui_find_sdl2_target(EUI_SDL2_TARGET EUI_SDL2_INCLUDE_DIRS TRUE)
    endif()

    set(EUI_EXAMPLE_FALLBACK_LINK_LIBS "")
    set(EUI_EXAMPLE_FALLBACK_INCLUDE_DIRS "")
    set(EUI_EXAMPLE_FALLBACK_COMPILE_DEFS "")

    if(EUI_ENABLE_GLFW_WINDOW_BACKEND AND EUI_FALLBACK_GLFW_TARGET)
        list(APPEND EUI_EXAMPLE_FALLBACK_LINK_LIBS ${EUI_FALLBACK_GLFW_TARGET})
        list(APPEND EUI_EXAMPLE_FALLBACK_COMPILE_DEFS EUI_ENABLE_GLFW_OPENGL_BACKEND=1)
    endif()
    if(EUI_ENABLE_SDL2_WINDOW_BACKEND AND EUI_FALLBACK_SDL2_TARGET)
        list(APPEND EUI_EXAMPLE_FALLBACK_LINK_LIBS ${EUI_FALLBACK_SDL2_TARGET})
        list(APPEND EUI_EXAMPLE_FALLBACK_COMPILE_DEFS EUI_ENABLE_SDL2_OPENGL_BACKEND=1 SDL_MAIN_HANDLED=1)
        list(APPEND EUI_EXAMPLE_FALLBACK_INCLUDE_DIRS ${EUI_FALLBACK_SDL2_INCLUDE_DIRS})
    endif()

    set(EUI_HAS_ANY_EXAMPLE_BACKEND OFF)
    if(EUI_GLFW_TARGET OR EUI_SDL2_TARGET OR EUI_EXAMPLE_FALLBACK_LINK_LIBS)
        set(EUI_HAS_ANY_EXAMPLE_BACKEND ON)
    endif()

    if(EUI_OPENGL_TARGET AND EUI_HAS_ANY_EXAMPLE_BACKEND)
        message(STATUS "EUI window backend selection: ${EUI_WINDOW_BACKEND_MODE_LABEL}")

        set(EUI_EXAMPLE_TARGETS "")
        foreach(example_source_path IN LISTS EUI_EXAMPLE_SOURCE_PATHS)
            get_filename_component(example_target "${example_source_path}" NAME_WE)
            eui_add_example("${example_source_path}" ${EUI_OPENGL_TARGET})
            if(TARGET ${example_target})
                list(APPEND EUI_EXAMPLE_TARGETS "${example_target}")
            endif()
        endforeach()

        # Legacy command aliases are kept only as CMake compatibility targets.
        # Public example target/output names always follow examples/*.cpp.
        set(EUI_LEGACY_EXAMPLE_TARGET_MAP
            "eui_demo=basic_widgets_demo"
            "eui_minimal_demo=anchor_layout_demo"
            "eui_layout_examples_demo=anchor_layout_demo"
            "eui_quick_syntax_demo=basic_widgets_demo"
            "eui_sidebar_navigation_demo=sidebar_navigation_demo"
            "eui_calculator_demo=calculator_demo"
            "eui_reference_dashboard_demo=reference_dashboard_demo"
        )
        foreach(legacy_alias IN LISTS EUI_LEGACY_EXAMPLE_TARGET_MAP)
            string(REPLACE "=" ";" legacy_parts "${legacy_alias}")
            list(GET legacy_parts 0 legacy_target)
            list(GET legacy_parts 1 actual_target)
            eui_add_compat_example_target(${legacy_target} ${actual_target})
        endforeach()

        if(EUI_EXAMPLE_TARGETS)
            install(TARGETS ${EUI_EXAMPLE_TARGETS}
                RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            )
        endif()
    elseif(NOT EUI_OPENGL_TARGET)
        message(WARNING
            "Skipping examples: OpenGL library not found on this platform."
        )
    else()
        message(WARNING
            "Skipping examples: no enabled window backend was found. "
            "Use EUI_WINDOW_BACKEND=AUTO|GLFW|SDL2|ALL, install the backend locally, "
            "or enable EUI_FETCH_GLFW_FROM_GIT / EUI_FETCH_SDL2_FROM_GIT."
        )
    endif()
endfunction()
