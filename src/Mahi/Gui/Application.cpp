#include <Mahi/Gui/Application.hpp>
#include <Mahi/Gui/Icons/IconsFontAwesome5.hpp>
#include <Mahi/Gui/Icons/IconsFontAwesome5Brands.hpp>
#include <Mahi/Gui/imgui_custom.hpp>
#include <Mahi/Util/System.hpp>
#include <Mahi/Util/Timing/Clock.hpp>
#include "Fonts/Fonts.hpp"

#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg_gl.h"
#include "imgui_internal.h"
#include "examples/imgui_impl_glfw.h"
#include "examples/imgui_impl_opengl3.h"
#include <stdio.h>
#include <iostream>
#include <stdexcept>
#include <thread>

///////////////////////////////////////////////////////////////////////////////
// FORWARDS
///////////////////////////////////////////////////////////////////////////////

namespace mahi {
namespace gui {
    
namespace
{
// GLFW
static void glfw_context_version();
static void glfw_setup_window_callbacks(GLFWwindow* window, void* userPointer);
static void glfw_error_callback(int error, const char *description);
static void glfw_pos_callback(GLFWwindow* window, int xpos, int ypos);
static void glfw_size_callback(GLFWwindow* window, int width, int height);
static void glfw_close_callback(GLFWwindow* window);
static void glfw_key_callback(GLFWwindow*,int key, int scancode, int action, int mods);
static void glfw_drop_callback(GLFWwindow *window, int count, const char **paths);
// IMGUI
static void configureImGui(GLFWwindow *window);
} // namespace

///////////////////////////////////////////////////////////////////////////////
// APPLICATION
///////////////////////////////////////////////////////////////////////////////

util::Event<void(int, const std::string&)> Application::on_error;

Application::Application() : window(nullptr), background_color({0.5,0.5,0.5,1})
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW!");
    glfw_context_version();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    window = glfwCreateWindow(100, 100, "", NULL, NULL);
    if (window == NULL)
        throw std::runtime_error("Failed to create Window!");
    // Setup OpenGL context
    glfwMakeContextCurrent(window);
    set_vsync(true);
    // Setup GLFW Callbacks
    glfw_setup_window_callbacks(window, this);
    // Initialize OpenGL loader
    if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
        throw std::runtime_error("Failed to initialize OpenGL loader!");
    // initialize NanoVg
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);  
    vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES); // | NVG_DEBUG
    if (vg == NULL)
        throw std::runtime_error("Failed to create NanoVG context!");
    // Setup ImGui
    configureImGui(window);
}

Application::Application(const std::string & title, int monitorIdx) : window(nullptr), background_color({0.5,0.5,0.5,1})
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW!");
    GLFWmonitor *monitor = nullptr;
    if (monitorIdx == 0) 
        monitor = glfwGetPrimaryMonitor();
    else
    {
        int count;
        auto monitors = glfwGetMonitors(&count);
        if (monitorIdx < count)
            monitor = monitors[monitorIdx];
        else
            monitor = glfwGetPrimaryMonitor();
    }
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    if (!mode)
        throw std::runtime_error("Failed to get Video Mode!");
   
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    glfwWindowHint(GLFW_SAMPLES, 4);

    glfw_context_version();
    window = glfwCreateWindow(mode->width, mode->height, title.c_str(), monitor, NULL);
    if (window == NULL)
        throw std::runtime_error("Failed to create Window!");
    // Setup OpenGL context
    glfwMakeContextCurrent(window);
    set_vsync(true);
    // Setup GLFW Callbacks
    glfw_setup_window_callbacks(window, this);
    // Initialize OpenGL loader
    if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
        throw std::runtime_error("Failed to initialize OpenGL loader (GLAD)!");
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);  
    // initialize NanoVg
    vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES); // | NVG_DEBUG
    if (vg == NULL)
        throw std::runtime_error("Failed to create NanoVG context!");
    // Setup ImGui
    configureImGui(window);
}

Application::Application(int width, int height, const std::string& title, bool resizable, int monitor) : window(nullptr), background_color({0.5,0.5,0.5,1})
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW!");
    glfw_context_version();
    if (!resizable)
        glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    window = glfwCreateWindow(width, height, title.c_str(), NULL, NULL);
    if (window == NULL)
        throw std::runtime_error("Failed to create Window!");
    // Setup OpenGL context
    glfwMakeContextCurrent(window);
    set_vsync(true);
    // Center window
    center_window(monitor);
    // Setup GLFW Callbacks
    glfw_setup_window_callbacks(window, this);
    // Initialize OpenGL loader
    if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0)
        throw std::runtime_error("Failed to initialize OpenGL loader!");
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);  
    // initialize NanoVg
    vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES); // | NVG_DEBUG
    if (vg == NULL)
        throw std::runtime_error("Failed to create NanoVG context!");
    // Setup ImGui
    configureImGui(window);
}

Application::~Application()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    nvgDeleteGL3(vg);
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Application::run()
{
    ImGuiIO &io = ImGui::GetIO();
    util::Clock clock;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        // Clear frame, setup dendering
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(background_color.r, background_color.g, background_color.b, background_color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // nanovg
        int winWidth, winHeight;
        glfwGetWindowSize(window, &winWidth, &winHeight);
        float pxRatio = static_cast<float>(fbWidth) / static_cast<float>(winWidth);
        nvgBeginFrame(vg, static_cast<float>(winWidth), static_cast<float>(winHeight), pxRatio);
        // update
        update();
#ifdef MAHI_COROUTINES
        // resume coroutines
        if (!m_coroutines.empty())
        {
            std::vector<util::Enumerator> temp;
            temp.swap(m_coroutines);
            for (auto &coro : temp)
            {
                if (coro.moveNext())
                    m_coroutines.push_back(std::move(coro));
            }
        }
#endif 
        nvgEndFrame(vg);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow *backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
        if (!m_vsync && m_frame_time != util::Time::Inf) {
            
            util::sleep(m_frame_time - clock.get_elapsed_time());       
            clock.restart();     
        }
        glfwSwapBuffers(window);
    }
    on_application_quit.emit();
}

void Application::quit() {
    glfwSetWindowShouldClose(window, 1);
}

util::Time Application::time() const {
    return util::seconds(glfwGetTime());
}

void Application::set_window_title(const std::string& title) {
    glfwSetWindowTitle(window, title.c_str());
}

void Application::set_window_pos(int xpos, int ypos) {
    glfwSetWindowPos(window, xpos, ypos);
}

std::pair<int,int> Application::get_window_pos() const {
    int xpos, ypos;
    glfwGetWindowPos(window, &xpos, &ypos);
    return {xpos, ypos};
}

void Application::set_window_size(int width, int height) {
    glfwSetWindowSize(window, width, height);
}

std::pair<int,int> Application::get_window_size() const {
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    return {width, height};
}

void Application::set_window_size_limits(int min_width, int min_height, int max_width, int max_height) {
    glfwSetWindowSizeLimits(window, min_width, min_height, max_width, max_height);
}

void Application::center_window(int monitorIdx) {
    GLFWmonitor *monitor = nullptr;
    if (monitorIdx == 0) 
        monitor = glfwGetPrimaryMonitor();
    else
    {
        int count;
        auto monitors = glfwGetMonitors(&count);
        if (monitorIdx < count)
            monitor = monitors[monitorIdx];
        else
            monitor = glfwGetPrimaryMonitor();
    }
    if (!monitor)
        throw std::runtime_error("Failed to get Monitor!");
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    if (!mode)
        throw std::runtime_error("Failed to get Video Mode!");
    int monitorX, monitorY;
    glfwGetMonitorPos(monitor, &monitorX, &monitorY);
    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwSetWindowPos(window, monitorX + (mode->width - windowWidth) / 2, monitorY + (mode->height - windowHeight) / 2);
}

void Application::minimize_window() {
    glfwIconifyWindow(window);
}

void Application::maximize_window() {
    glfwMaximizeWindow(window);
}

void Application::restore_window() {
    glfwRestoreWindow(window);
}

void Application::hide_window() {
    glfwHideWindow(window);
}

void Application::show_window() {
    glfwShowWindow(window);
}

void Application::focus_window() {
    glfwFocusWindow(window);
}

void Application::request_window_attention() {
    glfwRequestWindowAttention(window);
}

void Application::set_vsync(bool enabled) {
    m_vsync = enabled;
    if (m_vsync)
        glfwSwapInterval(1); // Enable vsync
    else
        glfwSwapInterval(0); // Disable vsync
}

void Application::set_frame_limit(util::Frequency freq) {
    set_vsync(false);
    m_frame_time = freq.to_time();
}

std::pair<float,float> Application::get_mouse_pos() const {
    double x,y;
    glfwGetCursorPos(window, &x, &y);
    return {(float)x,(float)y};
}


void Application::update()
{
    // do nothing by default (user implemented)
}

#ifdef MAHI_COROUTINES

std::shared_ptr<util::Coroutine> Application::startCoroutine(util::Enumerator &&e)
{
    auto h = e.getCoroutine();
    m_coroutines.push_back(std::move(e));
    return h;
}

void Application::stopCoroutine(std::shared_ptr<util::Coroutine> routine)
{
    if (routine)
        routine->stop();
}

void Application::stopCoroutines()
{
    m_coroutines.clear();
}

int Application::coroutineCount() const
{
    return static_cast<int>(m_coroutines.size());
}

#endif

namespace
{

///////////////////////////////////////////////////////////////////////////////
// GLFW
///////////////////////////////////////////////////////////////////////////////

static void glfw_context_version()
{
    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 + GLSL 150
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // Required on Mac
#else
    // GL 3.0 + GLSL 130
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif
}

void glfw_setup_window_callbacks(GLFWwindow* window, void* userPointer) {
    glfwSetWindowUserPointer(window, userPointer);
    glfwSetWindowPosCallback(window, glfw_pos_callback);
    glfwSetWindowSizeCallback(window, glfw_size_callback);
    glfwSetWindowCloseCallback(window, glfw_close_callback);
    glfwSetKeyCallback(window, glfw_key_callback);
    glfwSetDropCallback(window, glfw_drop_callback);
}

static void glfw_error_callback(int error, const char *description)
{
    static std::string dsc = description;
    Application::on_error.emit(error, dsc);
}

static void glfw_pos_callback(GLFWwindow* window, int xpos, int ypos) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    app->on_window_moved.emit(xpos, ypos);
}

static void glfw_size_callback(GLFWwindow* window, int width, int height) {
    Application *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
    app->on_window_resized.emit(width, height);
}

static void glfw_close_callback(GLFWwindow* window) {
    Application *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
    auto close = app->on_window_closed.emit();
    if (!close) 
        glfwSetWindowShouldClose(window, GLFW_FALSE);
}

static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    Application *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
    app->on_keyboard.emit(key,scancode,action,mods);
}

static void glfw_drop_callback(GLFWwindow *window, int count, const char **paths)
{
    static std::vector<std::string> pathsVec;
    pathsVec.resize(count);
    for (int i = 0; i < count; ++i)
        pathsVec[i] = paths[i];
    Application *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
    app->on_file_drop.emit(pathsVec);
}

///////////////////////////////////////////////////////////////////////////////
// IMGUI
///////////////////////////////////////////////////////////////////////////////
static void configureImGui(GLFWwindow *window)
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // we are doing 4X MSAA with GLFW
    // ImGui::GetStyle().AntiAliasedFill = false;
    // ImGui::GetStyle().AntiAliasedLines = false;
    
    // add fonts
    io.Fonts->Clear();
    ImFontConfig font_cfg;
    font_cfg.PixelSnapH = true;
    font_cfg.OversampleH = 1;
    font_cfg.OversampleV = 1;
    strcpy(font_cfg.Name, "Roboto Mono Bold");
    unsigned char *fontCopy1 = new unsigned char[RobotoMono_Bold_ttf_len];
    std::memcpy(fontCopy1, &RobotoMono_Bold_ttf, RobotoMono_Bold_ttf_len);
    io.Fonts->AddFontFromMemoryTTF(fontCopy1, RobotoMono_Bold_ttf_len, 15.0f, &font_cfg);

    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.GlyphMinAdvanceX = 14.0f;
    icons_config.GlyphOffset = ImVec2(0, 0);
    icons_config.OversampleH = 1;
    icons_config.OversampleV = 1;

    // merge in icons from font awesome 5
    static const ImWchar fa_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
    unsigned char *fontCopy2 = new unsigned char[fa_solid_900_ttf_len];
    std::memcpy(fontCopy2, &fa_solid_900_ttf, fa_solid_900_ttf_len);
    io.Fonts->AddFontFromMemoryTTF(fontCopy2, fa_solid_900_ttf_len, 14.0f, &icons_config, fa_ranges);

    // merge in icons from font awesome 5 brands
    static const ImWchar fab_ranges[] = {ICON_MIN_FAB, ICON_MAX_FAB, 0};
    unsigned char *fontCopy3 = new unsigned char[fa_brands_400_ttf_len];
    std::memcpy(fontCopy3, &fa_brands_400_ttf, fa_brands_400_ttf_len);
    io.Fonts->AddFontFromMemoryTTF(fontCopy3, fa_brands_400_ttf_len, 14, &icons_config, fab_ranges);   

    ImGuiStyle *imStyle = &ImGui::GetStyle();

    // Main
    imStyle->WindowPadding = ImVec2(8, 8);
    imStyle->FramePadding = ImVec2(3, 2);
    imStyle->ItemSpacing = ImVec2(4, 4);
    imStyle->ItemInnerSpacing = ImVec2(4, 4);
    imStyle->IndentSpacing = 20.0f;
    imStyle->ScrollbarSize = 15.0f;
    imStyle->GrabMinSize = 5.0f;

    // Rounding
    imStyle->WindowRounding = 2.0f;
    imStyle->ChildRounding = 2.0f;
    imStyle->FrameRounding = 2.0f;
    imStyle->PopupRounding = 2.0f;
    imStyle->ScrollbarRounding = 10.0f;
    imStyle->GrabRounding = 2.0f;
    imStyle->TabRounding = 2.0f;

    // Alignment
    imStyle->WindowMenuButtonPosition = ImGuiDir_Right;

    // Setup Dear ImGui style
    ImGui::StyleColorsMahiDark4();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);

    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 + GLSL 150
    const char *glsl_version = "#version 150";
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // // create device objects and set font  (Evan added this so it may break things)
    // ImGui_ImplOpenGL3_CreateDeviceObjects();
    // ImGui::SetCurrentFont(ImGui::GetDefaultFont());
}

} // private namespace

} // namespace gui
} // namespace mahi