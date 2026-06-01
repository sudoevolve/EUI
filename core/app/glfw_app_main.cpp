#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

#include <GLFW/glfw3.h>

#include "eui/app.h"
#include "core/app/app_runner.h"
#include "core/app/dsl_window_manager.h"
#include "core/app/dsl_window_runtime.h"
#include "core/app/main_window_runtime.h"
#include "core/input/input_state.h"
#include "core/platform/platform.h"
#include "core/window/window_backend.h"
#include "core/render/render_backend.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

struct WindowState : app::AppRunner {
    bool hideToTrayRequested = false;
    bool forceClose = false;
    GLFWwindow* modalChildWindow = nullptr;
};

struct ManagedWindow {
    GLFWwindow* window = nullptr;
    WindowState state;
    app::DslWindowRuntime content;
    std::unique_ptr<core::render::RenderBackend> renderBackend;
};

struct TimerResolutionGuard {
    TimerResolutionGuard() {
#ifdef _WIN32
        timeBeginPeriod(1);
#endif
    }

    ~TimerResolutionGuard() {
#ifdef _WIN32
        timeEndPeriod(1);
#endif
    }
};

float getDpiScale(GLFWwindow* window) {
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    glfwGetWindowContentScale(window, &scaleX, &scaleY);
    return (scaleX + scaleY) * 0.5f;
}

float getPointerScale(GLFWwindow* window) {
    int windowWidth = 0;
    int windowHeight = 0;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

    if (windowWidth <= 0 || windowHeight <= 0) {
        return 1.0f;
    }

    const float scaleX = static_cast<float>(framebufferWidth) / static_cast<float>(windowWidth);
    const float scaleY = static_cast<float>(framebufferHeight) / static_cast<float>(windowHeight);
    return (scaleX + scaleY) * 0.5f;
}

GLFWmonitor* getWindowMonitor(GLFWwindow* window) {
    if (GLFWmonitor* monitor = glfwGetWindowMonitor(window)) {
        return monitor;
    }

    int windowX = 0;
    int windowY = 0;
    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetWindowPos(window, &windowX, &windowY);
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    GLFWmonitor* bestMonitor = glfwGetPrimaryMonitor();
    int bestArea = 0;

    for (int i = 0; i < monitorCount; ++i) {
        GLFWmonitor* monitor = monitors[i];
        int monitorX = 0;
        int monitorY = 0;
        glfwGetMonitorPos(monitor, &monitorX, &monitorY);
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (!mode) {
            continue;
        }

        const int overlapLeft = std::max(windowX, monitorX);
        const int overlapTop = std::max(windowY, monitorY);
        const int overlapRight = std::min(windowX + windowWidth, monitorX + mode->width);
        const int overlapBottom = std::min(windowY + windowHeight, monitorY + mode->height);
        const int overlapWidth = std::max(0, overlapRight - overlapLeft);
        const int overlapHeight = std::max(0, overlapBottom - overlapTop);
        const int overlapArea = overlapWidth * overlapHeight;
        if (overlapArea > bestArea) {
            bestArea = overlapArea;
            bestMonitor = monitor;
        }
    }

    return bestMonitor;
}

double getWindowRefreshRate(GLFWwindow* window) {
    GLFWmonitor* monitor = getWindowMonitor(window);
    const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
    if (mode && mode->refreshRate > 0) {
        return static_cast<double>(mode->refreshRate);
    }
    return 60.0;
}

void updateFrameInterval(GLFWwindow* window, WindowState& windowState, double now, bool force = false) {
    windowState.updateFrameInterval(getWindowRefreshRate(window), now, force);
}

void waitForNextFrame(GLFWwindow* window, const WindowState& windowState) {
    while (!glfwWindowShouldClose(window)) {
        const double remaining = windowState.nextFrameTime - glfwGetTime();
        if (remaining <= 0.0) {
            break;
        }

        if (remaining > 0.002) {
            glfwWaitEventsTimeout(remaining - 0.001);
        } else {
            std::this_thread::sleep_for(std::chrono::duration<double>(remaining * 0.5));
        }
    }
}

void hideWindowToTray(GLFWwindow* window, WindowState& windowState, core::render::RenderBackend& renderBackend) {
    if (!windowState.trayAvailable || windowState.hiddenToTray) {
        return;
    }

    core::render::ScopedRenderBackend scopedRenderBackend(renderBackend);
    app::releaseGraphicsResources();
    glfwHideWindow(window);
    windowState.hiddenToTray = true;
    windowState.hideToTrayRequested = false;
    windowState.needsRender = false;
    windowState.renderedFrames = 0;
    windowState.nextFrameTime = glfwGetTime();
}

void restoreWindowFromTray(GLFWwindow* window, WindowState& windowState) {
    if (!windowState.hiddenToTray) {
        return;
    }

    glfwRestoreWindow(window);
    glfwShowWindow(window);
    glfwFocusWindow(window);
    windowState.hiddenToTray = false;
    windowState.hideToTrayRequested = false;
    windowState.needsRender = true;
    windowState.nextFrameTime = glfwGetTime();
}

void installWindowCallbacks(GLFWwindow* window, WindowState& windowState) {
    glfwSetWindowUserPointer(window, &windowState);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* currentWindow, int w, int h) {
        (void)w;
        (void)h;
        static_cast<WindowState*>(glfwGetWindowUserPointer(currentWindow))->needsRender = true;
    });
    glfwSetWindowRefreshCallback(window, [](GLFWwindow* currentWindow) {
        static_cast<WindowState*>(glfwGetWindowUserPointer(currentWindow))->needsRender = true;
    });
    glfwSetWindowContentScaleCallback(window, [](GLFWwindow* currentWindow, float, float) {
        static_cast<WindowState*>(glfwGetWindowUserPointer(currentWindow))->needsRender = true;
    });
    glfwSetWindowFocusCallback(window, [](GLFWwindow* currentWindow, int focused) {
        WindowState* state = static_cast<WindowState*>(glfwGetWindowUserPointer(currentWindow));
        if (!state) {
            return;
        }
        state->needsRender = true;
        if (focused && state->modalChildWindow != nullptr && !glfwWindowShouldClose(state->modalChildWindow)) {
            glfwFocusWindow(state->modalChildWindow);
        }
    });
}

std::unique_ptr<ManagedWindow> createManagedWindow(const app::DslWindowRequest& request,
                                                   GLFWwindow* parentWindow,
                                                   core::render::RenderBackend& shareBackend) {
    core::window::WindowCreateRequest windowRequest;
    windowRequest.width = request.width;
    windowRequest.height = request.height;
    windowRequest.title = request.title.c_str();
    windowRequest.parent = parentWindow;
    windowRequest.renderApi = core::render::windowRenderApi();
    GLFWwindow* childWindow = static_cast<GLFWwindow*>(core::window::createWindow(windowRequest));
    if (!childWindow) {
        return {};
    }

    auto managed = std::make_unique<ManagedWindow>();
    managed->window = childWindow;
    managed->renderBackend = core::render::createRenderBackend(childWindow, &shareBackend);
    if (!managed->renderBackend) {
        core::window::destroyWindow(childWindow);
        return {};
    }
    if (!managed->renderBackend->initialize()) {
        core::window::destroyWindow(childWindow);
        return {};
    }
    managed->state.lastTitleUpdate = glfwGetTime();
    managed->state.nextFrameTime = managed->state.lastTitleUpdate;
    installWindowCallbacks(childWindow, managed->state);

    if (!managed->content.initialize(childWindow, request)) {
        managed->renderBackend.reset();
        core::releaseInputQueue(childWindow);
        core::window::destroyWindow(childWindow);
        return {};
    }

    managed->state.needsRender = true;
    if (managed->content.request().modal) {
        glfwFocusWindow(childWindow);
    }
    return managed;
}

void destroyManagedWindow(std::unique_ptr<ManagedWindow>& managed) {
    if (!managed || managed->window == nullptr) {
        managed.reset();
        return;
    }

    GLFWwindow* windowToDestroy = managed->window;
    if (managed->renderBackend) {
        managed->renderBackend->makeCurrent();
        managed->renderBackend->releaseRenderCache();
    }
    core::releaseInputQueue(windowToDestroy);
    if (managed->renderBackend) {
        core::render::ScopedRenderBackend scopedRenderBackend(*managed->renderBackend);
        managed->content.shutdown(false);
    } else {
        managed->content.shutdown(false);
    }
    managed->renderBackend.reset();
    core::window::destroyWindow(windowToDestroy);
    managed.reset();
}

bool updateManagedWindow(ManagedWindow& managed, float deltaSeconds, bool externalReady) {
    if (managed.window == nullptr || glfwWindowShouldClose(managed.window)) {
        return false;
    }

    managed.renderBackend->makeCurrent();

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(managed.window, &framebufferWidth, &framebufferHeight);
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        managed.state.needsRender = true;
        return true;
    }

    const float dpiScale = getDpiScale(managed.window);
    const float pointerScale = getPointerScale(managed.window);
    const float logicalWidth = static_cast<float>(framebufferWidth) / dpiScale;
    const float logicalHeight = static_cast<float>(framebufferHeight) / dpiScale;

    if (managed.content.update(managed.window, deltaSeconds, logicalWidth, logicalHeight, pointerScale, dpiScale, externalReady)) {
        managed.state.needsRender = true;
    }

    if (managed.state.needsRender || managed.content.needsRender()) {
        managed.renderBackend->beginFrame({
            managed.window,
            core::window::nativeWindowInfo(managed.window),
            framebufferWidth,
            framebufferHeight,
            dpiScale
        });
        managed.content.render(*managed.renderBackend, framebufferWidth, framebufferHeight, dpiScale);
        managed.renderBackend->present();
        managed.state.needsRender = false;
        ++managed.state.renderedFrames;
    }
    return true;
}

bool isManagedWindowClosed(const ManagedWindow& managed) {
    return managed.window == nullptr || glfwWindowShouldClose(managed.window);
}

void pruneClosedWindows(app::DslWindowManager<ManagedWindow>& windows) {
    windows.pruneClosed(isManagedWindowClosed, destroyManagedWindow);
}

void createRequestedWindows(app::DslWindowManager<ManagedWindow>& windows,
                            GLFWwindow* shareWindow,
                            core::render::RenderBackend& shareBackend,
                            const std::vector<app::DslWindowRequest>& requests) {
    windows.createPending(requests, [&](const app::DslWindowRequest& request) {
        return createManagedWindow(request, shareWindow, shareBackend);
    });
    shareBackend.makeCurrent();
}

GLFWwindow* findModalChildWindow(app::DslWindowManager<ManagedWindow>& windows) {
    ManagedWindow* managed = windows.modalWindow(isManagedWindowClosed);
    return managed != nullptr ? managed->window : nullptr;
}

int main() {
    core::render::initializeRenderBackendLoader();
    if (!glfwInit()) {
        return -1;
    }
    TimerResolutionGuard timerResolution;

    core::window::WindowCreateRequest windowRequest;
    windowRequest.width = app::initialWindowWidth();
    windowRequest.height = app::initialWindowHeight();
    windowRequest.title = app::windowTitle();
    windowRequest.renderApi = core::render::windowRenderApi();
    GLFWwindow* window = static_cast<GLFWwindow*>(core::window::createWindow(windowRequest));
    if (!window) {
        glfwTerminate();
        return -1;
    }

    WindowState windowState;
    windowState.resetTiming(glfwGetTime());
    updateFrameInterval(window, windowState, windowState.lastTitleUpdate, true);
    if (app::showFrameCountInTitle()) {
        char title[128];
        std::snprintf(title, sizeof(title), "%s - 0 FPS", app::windowTitle());
        glfwSetWindowTitle(window, title);
    }
    installWindowCallbacks(window, windowState);

    const auto cleanupMainWindow = [&] {
        core::releaseInputQueue(window);
        core::window::destroyWindow(window);
        glfwTerminate();
    };

    auto renderBackend = core::render::createRenderBackend(window);
    if (!renderBackend) {
        cleanupMainWindow();
        return -1;
    }
    if (!renderBackend->initialize()) {
        cleanupMainWindow();
        return -1;
    }

    if (!app::initialize(window)) {
        app::shutdown();
        renderBackend.reset();
        cleanupMainWindow();
        return -1;
    }
    app::MainWindowRuntime mainWindowRuntime(windowState);
    windowState.initializeTray();
    glfwSetWindowCloseCallback(window, [](GLFWwindow* currentWindow) {
        WindowState* state = static_cast<WindowState*>(glfwGetWindowUserPointer(currentWindow));
        if (state && state->modalChildWindow != nullptr && !glfwWindowShouldClose(state->modalChildWindow)) {
            glfwFocusWindow(state->modalChildWindow);
            glfwSetWindowShouldClose(currentWindow, GLFW_FALSE);
            return;
        }
        if (state && state->trayAvailable && !state->forceClose) {
            state->hideToTrayRequested = true;
            glfwSetWindowShouldClose(currentWindow, GLFW_FALSE);
        }
    });
    glfwSetWindowIconifyCallback(window, [](GLFWwindow* currentWindow, int iconified) {
        WindowState* state = static_cast<WindowState*>(glfwGetWindowUserPointer(currentWindow));
        if (state && state->trayAvailable && iconified && !state->forceClose) {
            state->hideToTrayRequested = true;
        }
    });

    app::DslWindowManager<ManagedWindow> childWindows;

    while (!glfwWindowShouldClose(window)) {
        renderBackend->makeCurrent();
        windowState.pollTray(false);
        if (windowState.consumeTrayExitRequested()) {
            windowState.forceClose = true;
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        }
        if (windowState.consumeTrayShowRequested()) {
            restoreWindowFromTray(window, windowState);
        }
        pruneClosedWindows(childWindows);
        windowState.modalChildWindow = findModalChildWindow(childWindows);
        if (windowState.hideToTrayRequested && !childWindows.empty()) {
            windowState.hideToTrayRequested = false;
        }
        if (windowState.hideToTrayRequested) {
            renderBackend->releaseRenderCache();
            hideWindowToTray(window, windowState, *renderBackend);
        }
        if (windowState.hiddenToTray) {
            glfwWaitEventsTimeout(0.10);
            windowState.resetTiming(glfwGetTime());
            continue;
        }

        if (windowState.anyAnimating(childWindows.anyAnimating())) {
            waitForNextFrame(window, windowState);
        }

        const double currentFrameTime = glfwGetTime();

        if (windowState.modalChildWindow == nullptr && glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            if (windowState.trayAvailable) {
                windowState.hideToTrayRequested = true;
            } else {
                glfwSetWindowShouldClose(window, 1);
                break;
            }
        }

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (framebufferWidth <= 0 || framebufferHeight <= 0) {
            glfwWaitEvents();
            mainWindowRuntime.markUnavailableFrame(glfwGetTime());
            continue;
        }

        const float dpiScale = getDpiScale(window);
        const float pointerScale = getPointerScale(window);
        const bool mainInputEnabled = windowState.modalChildWindow == nullptr;

        mainWindowRuntime.runFrame(
            window,
            *renderBackend,
            {framebufferWidth, framebufferHeight, dpiScale, pointerScale},
            currentFrameTime,
            getWindowRefreshRate(window),
            mainInputEnabled,
            [&] {
                createRequestedWindows(childWindows, window, *renderBackend, app::consumeWindowRequests());
                pruneClosedWindows(childWindows);
                windowState.modalChildWindow = findModalChildWindow(childWindows);
            },
            [&](float frameDelta, bool frameExternalReady) {
                childWindows.updateAll([&](ManagedWindow& managed) {
                    updateManagedWindow(managed, frameDelta, frameExternalReady);
                });

                createRequestedWindows(childWindows, window, *renderBackend, app::consumeWindowRequests());
                pruneClosedWindows(childWindows);
                windowState.modalChildWindow = findModalChildWindow(childWindows);
            },
            [&](const char* title) {
                glfwSetWindowTitle(window, title);
            },
            [&] {
                return childWindows.anyAnimating();
            });

        const bool anyAnimating = windowState.anyAnimating(childWindows.anyAnimating());
        if (anyAnimating) {
            glfwPollEvents();
        } else {
            glfwWaitEvents();
        }
    }

    childWindows.destroyAll(destroyManagedWindow);
    core::releaseInputQueue(window);
    renderBackend->makeCurrent();
    renderBackend->releaseRenderCache();
    core::platform::shutdownTray();
    {
        core::render::ScopedRenderBackend scopedRenderBackend(*renderBackend);
        app::shutdown();
    }
    renderBackend.reset();
    glfwTerminate();
    return 0;
}
