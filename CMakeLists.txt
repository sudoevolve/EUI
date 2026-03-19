cmake_minimum_required(VERSION 3.20)
project(EUI VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(EUI_BUILD_EXAMPLES "Build example applications when dependencies exist" ON)
option(EUI_STRICT_WARNINGS "Enable strict warnings on local targets" ON)
set(EUI_WINDOW_BACKEND "AUTO" CACHE STRING "Convenience window backend mode: AUTO, GLFW, SDL2, or ALL")
set_property(CACHE EUI_WINDOW_BACKEND PROPERTY STRINGS AUTO GLFW SDL2 ALL)
option(EUI_FETCH_GLFW_FROM_GIT "Auto-fetch GLFW from Git if it is not available locally" OFF)
option(EUI_FETCH_SDL2_FROM_GIT "Auto-fetch SDL2 from Git if it is not available locally" OFF)
option(EUI_ENABLE_GLFW_WINDOW_BACKEND "Enable GLFW window backend for app/examples when available" ON)
option(EUI_ENABLE_SDL2_WINDOW_BACKEND "Enable SDL2 window backend for app/examples when available" ON)
set(EUI_GLFW_GIT_TAG "3.4" CACHE STRING "GLFW Git tag used by FetchContent")
set(EUI_SDL2_GIT_TAG "SDL2" CACHE STRING "SDL2 Git ref used by FetchContent")

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(eui INTERFACE)
add_library(EUI::eui ALIAS eui)
target_include_directories(eui INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(eui INTERFACE cxx_std_17)

if(EUI_STRICT_WARNINGS)
    if(MSVC)
        target_compile_options(eui INTERFACE /W4)
    else()
        target_compile_options(eui INTERFACE -Wall -Wextra -Wpedantic)
    endif()
endif()

set(EUI_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/EUI")

set(EUI_DOC_FILES "")
foreach(eui_doc_candidate
    "${CMAKE_CURRENT_SOURCE_DIR}/readme.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/readme.zh-CN.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/README.zh-CN.md"
)
    if(EXISTS "${eui_doc_candidate}")
        list(APPEND EUI_DOC_FILES "${eui_doc_candidate}")
    endif()
endforeach()

install(TARGETS eui EXPORT EUITargets)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
if(EUI_DOC_FILES)
    install(FILES ${EUI_DOC_FILES} DESTINATION "${CMAKE_INSTALL_DOCDIR}")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/docs")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/docs/" DESTINATION "${CMAKE_INSTALL_DOCDIR}/docs")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/examples/" DESTINATION "${CMAKE_INSTALL_DOCDIR}/examples")
endif()

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EUIConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/EUIConfig.cmake"
    INSTALL_DESTINATION "${EUI_INSTALL_CMAKEDIR}"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/EUIConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)
install(EXPORT EUITargets
    FILE EUIConfigTargets.cmake
    NAMESPACE EUI::
    DESTINATION "${EUI_INSTALL_CMAKEDIR}"
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/EUIConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/EUIConfigVersion.cmake"
    DESTINATION "${EUI_INSTALL_CMAKEDIR}"
)

if(EUI_BUILD_EXAMPLES)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/EUIExamples.cmake")
    eui_configure_examples()
endif()

set(CPACK_PACKAGE_NAME "EUI")
set(CPACK_PACKAGE_VENDOR "EUI")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Header-only immediate-mode C++ UI toolkit")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "EUI")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(EUI_CPACK_README "")
foreach(eui_doc_candidate IN LISTS EUI_DOC_FILES)
    set(EUI_CPACK_README "${eui_doc_candidate}")
    break()
endforeach()
if(EUI_CPACK_README)
    set(CPACK_RESOURCE_FILE_README "${EUI_CPACK_README}")
endif()
if(NOT DEFINED CPACK_PACKAGE_FILE_NAME)
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
endif()
include(CPack)
