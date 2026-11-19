#include "gui.hpp"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>

#include <windows.h>
#include <GL/gl.h>

#include <chrono>
#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace pitwall::app {
namespace {

void enable_process_dpi() {
    // Do not set only the UI thread: WGL/driver initialization must share the
    // process pixel coordinate system. The embedded manifest sets this at startup.
    using SetProcessContext = BOOL (WINAPI*)(HANDLE);
    const auto address = GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext");
    if (address) {
        const auto set_context = std::bit_cast<SetProcessContext>(address);
        (void)set_context(reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-4)));
        // ERROR_ACCESS_DENIED is expected when the startup manifest already set it.
    } else {
        (void)SetProcessDPIAware();
    }
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam)) { return 1; }
    if (message == WM_SYSCOMMAND && (wparam & 0xfff0) == SC_KEYMENU) { return 0; }
    if (message == WM_GETMINMAXINFO) {
        auto* bounds = reinterpret_cast<MINMAXINFO*>(lparam);
        bounds->ptMinTrackSize = {900, 620};
        return 0;
    }
    if (message == WM_DPICHANGED) {
        const auto* suggested = reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(window, nullptr, suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    if (message == WM_CLOSE || message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, wparam, lparam);
}

class GlWindow {
public:
    HWND handle{};
    HDC dc{};
    HGLRC context{};
    HINSTANCE instance{GetModuleHandleW(nullptr)};
    bool registered{};

    explicit GlWindow(bool hidden) {
        try {
            WNDCLASSEXW descriptor{};
            descriptor.cbSize = sizeof(descriptor);
            descriptor.style = CS_OWNDC;
            descriptor.lpfnWndProc = window_proc;
            descriptor.hInstance = instance;
            descriptor.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
            descriptor.lpszClassName = L"PitWallDrivingWindow";
            registered = RegisterClassExW(&descriptor) != 0;
            if (!registered) { throw std::runtime_error("Cannot register the window class"); }
            RECT area{0, 0, 1280, 800};
            AdjustWindowRect(&area, WS_OVERLAPPEDWINDOW, FALSE);
            MONITORINFO monitor{};
            monitor.cbSize = sizeof(monitor);
            GetMonitorInfoW(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY), &monitor);
            const int outer_width = std::min(static_cast<int>(area.right - area.left),
                static_cast<int>(monitor.rcWork.right - monitor.rcWork.left) - 32);
            const int outer_height = std::min(static_cast<int>(area.bottom - area.top),
                static_cast<int>(monitor.rcWork.bottom - monitor.rcWork.top) - 32);
            handle = CreateWindowExW(0, descriptor.lpszClassName, L"PitWall - Racing Simulator",
                WS_OVERLAPPEDWINDOW, monitor.rcWork.left + 16, monitor.rcWork.top + 16,
                outer_width, outer_height, nullptr, nullptr, instance, nullptr);
            if (!handle) { throw std::runtime_error("Cannot create the simulator window"); }
            dc = GetDC(handle);
            PIXELFORMATDESCRIPTOR format{};
            format.nSize = sizeof(format);
            format.nVersion = 1;
            format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
            format.iPixelType = PFD_TYPE_RGBA;
            format.cColorBits = 32;
            format.cDepthBits = 24;
            format.iLayerType = PFD_MAIN_PLANE;
            const int selected = ChoosePixelFormat(dc, &format);
            if (!selected || !SetPixelFormat(dc, selected, &format)) {
                throw std::runtime_error("Cannot select an OpenGL pixel format");
            }
            context = wglCreateContext(dc);
            if (!context || !wglMakeCurrent(dc, context)) {
                throw std::runtime_error("Cannot create/activate an OpenGL context");
            }
            const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
            if (!version || version[0] < '3') {
                throw std::runtime_error("OpenGL 3.0+ required; install/update the GPU driver");
            }
            std::cout << "OpenGL " << version << '\n';
            if (!hidden) { ShowWindow(handle, SW_SHOW); UpdateWindow(handle); }
        } catch (...) { cleanup(); throw; }
    }

    ~GlWindow() { cleanup(); }
    GlWindow(const GlWindow&) = delete;
    GlWindow& operator=(const GlWindow&) = delete;

private:
    void cleanup() noexcept {
        if (context) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(context); context = nullptr; }
        if (dc && handle) { ReleaseDC(handle, dc); dc = nullptr; }
        if (handle) { DestroyWindow(handle); handle = nullptr; }
        if (registered) { UnregisterClassW(L"PitWallDrivingWindow", instance); registered = false; }
    }
};

class ImGuiContextOwner {
public:
    bool platform{};
    bool renderer{};
    explicit ImGuiContextOwner(HWND window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        try {
            auto& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;
            ImFontConfig font;
            font.SizePixels = 16.0f;
            io.Fonts->AddFontDefault(&font);
            ImGui::StyleColorsDark();
            auto& style = ImGui::GetStyle();
            style.WindowPadding = {16, 16};
            style.ItemSpacing = {10, 10};
            style.FrameRounding = 5;
            style.ChildRounding = 8;
            style.Colors[ImGuiCol_WindowBg] = {0.045f, 0.065f, 0.09f, 1};
            style.Colors[ImGuiCol_ChildBg] = {0.065f, 0.09f, 0.12f, 1};
            style.Colors[ImGuiCol_Button] = {0.14f, 0.23f, 0.27f, 1};
            platform = ImGui_ImplWin32_InitForOpenGL(window);
            if (!platform) { throw std::runtime_error("ImGui Win32 initialization failed"); }
            renderer = ImGui_ImplOpenGL3_Init("#version 130");
            if (!renderer) { throw std::runtime_error("ImGui OpenGL initialization failed"); }
        } catch (...) { cleanup(); throw; }
    }
    ~ImGuiContextOwner() { cleanup(); }
    ImGuiContextOwner(const ImGuiContextOwner&) = delete;
    ImGuiContextOwner& operator=(const ImGuiContextOwner&) = delete;
private:
    void cleanup() noexcept {
        if (renderer) { ImGui_ImplOpenGL3_Shutdown(); renderer = false; }
        if (platform) { ImGui_ImplWin32_Shutdown(); platform = false; }
        if (ImGui::GetCurrentContext()) { ImGui::DestroyContext(); }
    }
};

// Hidden/occluded Windows drawables have undefined pixel ownership outside the
// visible desktop region. Render diagnostics to an FBO, not a hidden swapchain.
class DiagnosticFramebuffer {
    using Gen = void (APIENTRY*)(GLsizei, GLuint*);
    using Bind = void (APIENTRY*)(GLenum, GLuint);
    using Attach = void (APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
    using Check = GLenum (APIENTRY*)(GLenum);
    using Delete = void (APIENTRY*)(GLsizei, const GLuint*);
    static constexpr GLenum framebuffer = 0x8d40;
    static constexpr GLenum color_attachment = 0x8ce0;
    GLuint fbo_{};
    GLuint texture_{};
    Bind bind_{};
    Delete delete_{};

    template <typename Function> static Function load(const char* name) {
        const auto address = wglGetProcAddress(name);
        if (!address) { throw std::runtime_error("OpenGL framebuffer API unavailable"); }
        return std::bit_cast<Function>(address);
    }
public:
    explicit DiagnosticFramebuffer(bool enabled) {
        if (!enabled) { return; }
        bind_ = load<Bind>("glBindFramebuffer");
        delete_ = load<Delete>("glDeleteFramebuffers");
        const auto generate = load<Gen>("glGenFramebuffers");
        const auto attach = load<Attach>("glFramebufferTexture2D");
        const auto check = load<Check>("glCheckFramebufferStatus");
        try {
            glGenTextures(1, &texture_);
            glBindTexture(GL_TEXTURE_2D, texture_);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1280, 800, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            generate(1, &fbo_);
            bind_(framebuffer, fbo_);
            attach(framebuffer, color_attachment, GL_TEXTURE_2D, texture_, 0);
            if (check(framebuffer) != 0x8cd5) { throw std::runtime_error("Diagnostic FBO incomplete"); }
        } catch (...) { cleanup(); throw; }
    }
    ~DiagnosticFramebuffer() { cleanup(); }
    DiagnosticFramebuffer(const DiagnosticFramebuffer&) = delete;
    DiagnosticFramebuffer& operator=(const DiagnosticFramebuffer&) = delete;
private:
    void cleanup() noexcept {
        if (fbo_) { bind_(framebuffer, 0); delete_(1, &fbo_); fbo_ = 0; }
        if (texture_) { glDeleteTextures(1, &texture_); texture_ = 0; }
    }
};

void save_frame(const char* path, int width, int height, bool offscreen) {
    if (width < 1 || height < 1 || width > 8192 || height > 8192) {
        throw std::runtime_error("Invalid diagnostic framebuffer dimensions");
    }
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(offscreen ? 0x8ce0 : GL_BACK);
    glFinish();
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    if (glGetError() != GL_NO_ERROR) { throw std::runtime_error("Framebuffer readback failed"); }
    const int stride = (width * 3 + 3) & ~3;
    std::vector<std::uint8_t> bgr(static_cast<std::size_t>(stride) * static_cast<std::size_t>(height));
    bool varied = false;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto source = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                  static_cast<std::size_t>(x)) * 3;
            const auto dest = static_cast<std::size_t>(y) * static_cast<std::size_t>(stride) +
                                static_cast<std::size_t>(x) * 3;
            bgr[dest] = rgb[source + 2]; bgr[dest + 1] = rgb[source + 1]; bgr[dest + 2] = rgb[source];
            varied = varied || rgb[source] != rgb[0] || rgb[source + 1] != rgb[1];
        }
    }
    if (!varied) { throw std::runtime_error("Diagnostic frame is blank"); }
    BITMAPFILEHEADER file{};
    BITMAPINFOHEADER info{};
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + static_cast<DWORD>(bgr.size());
    info.biSize = sizeof(info);
    info.biWidth = width; info.biHeight = height;
    info.biPlanes = 1; info.biBitCount = 24; info.biCompression = BI_RGB;
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(&file), sizeof(file));
    output.write(reinterpret_cast<const char*>(&info), sizeof(info));
    output.write(reinterpret_cast<const char*>(bgr.data()), static_cast<std::streamsize>(bgr.size()));
    if (!output) { throw std::runtime_error("Cannot write diagnostic BMP"); }
}

} // namespace

int run_gui(const char* smoke_path, bool native_smoke) {
    const bool smoke = smoke_path != nullptr;
    enable_process_dpi();
    GlWindow window{smoke && !native_smoke};
    ImGuiContextOwner ui{window.handle};
    DiagnosticFramebuffer diagnostic{smoke && !native_smoke};
    if (native_smoke) {
        SetWindowPos(window.handle, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }
    DrivingSession session;
    if (smoke) {
        for (int i = 0; i < 180; ++i) {
            (void)session.advance(std::chrono::milliseconds{10}, {0.35, 0.0, 0.2});
        }
    }
    auto last_frame = std::chrono::steady_clock::now();
    int frames = 0;
    bool done = false;
    while (!done) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message); DispatchMessageW(&message);
            if (message.message == WM_QUIT) { done = true; }
        }
        if (done) { break; }
        if (!smoke && IsIconic(window.handle)) {
            session.set_paused(true);
            WaitMessage();
            last_frame = std::chrono::steady_clock::now();
            continue;
        }
        const auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - last_frame);
        last_frame = now;
        if (!smoke && (GetForegroundWindow() != window.handle || elapsed > std::chrono::seconds{60})) {
            session.set_paused(true);
        }
        if (elapsed > std::chrono::seconds{60}) { elapsed = {}; }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        RECT client{};
        GetClientRect(window.handle, &client);
        if (smoke && !native_smoke) {
            ImGui::GetIO().DisplaySize = {1280, 800};
        } else {
            // The per-monitor aware client rectangle is in physical pixels.
            ImGui::GetIO().DisplaySize = {static_cast<float>(client.right), static_cast<float>(client.bottom)};
        }
        ImGui::GetIO().DisplayFramebufferScale = {1, 1};
        ImGui::NewFrame();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) { done = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_P, false)) { session.set_paused(!session.paused()); }
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) { session.restart(); }
        simulation::ControlInput controls{};
        if (!session.paused() && !ImGui::GetIO().WantTextInput) {
            controls.throttle = ImGui::IsKeyDown(ImGuiKey_W) ? 1.0 : 0.0;
            controls.brake = ImGui::IsKeyDown(ImGuiKey_S) ? 1.0 : 0.0;
            controls.steering = (ImGui::IsKeyDown(ImGuiKey_A) ? 1.0 : 0.0) -
                                (ImGui::IsKeyDown(ImGuiKey_D) ? 1.0 : 0.0);
        }
        if (smoke) { controls = {0.35, 0.0, 0.2}; elapsed = std::chrono::milliseconds{20}; }
        (void)session.advance(elapsed, controls);
        draw_simulator_ui(session, controls);
        ImGui::Render();
        const auto display = ImGui::GetIO().DisplaySize;
        const int width = static_cast<int>(display.x);
        const int height = static_cast<int>(display.y);
        glViewport(0, 0, width, height);
        glClearColor(0.04f, 0.06f, 0.09f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ++frames;
        if (smoke && frames == 5) {
            save_frame(smoke_path, width, height, !native_smoke);
            std::cout << "gui smoke passed: frames=" << frames
                      << ", ticks=" << session.simulation().tick_count()
                      << ", surface=" << (native_smoke ? "native window" : "FBO")
                      << ", size=" << width << 'x' << height << '\n';
            done = true;
        }
        SwapBuffers(window.dc);
        // Cap the uncoupled render loop without busy-spinning. Physics is still 100 Hz.
        if (!smoke) { Sleep(8); }
    }
    return 0;
}

} // namespace pitwall::app
