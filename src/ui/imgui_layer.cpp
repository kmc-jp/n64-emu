#include "ui/imgui_layer.h"
#include "command_buffer.hpp"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include "utils/log.h"
#include "video/present.h"
#include "vulkan_common.hpp"
#include <algorithm>
#include <cstdlib>
#include <string>
#include <volk.h>

namespace N64 {
namespace Ui {

namespace {

VkDescriptorPool g_pool = VK_NULL_HANDLE;
VkRenderPass g_imgui_rp = VK_NULL_HANDLE;
VkDevice g_vk_device = VK_NULL_HANDLE;
bool g_ready = false;
bool g_frame_rendered = false;
UiTheme g_theme = UiTheme::Dark;

ImVec4 rgba(unsigned hex, float a = 1.f) {
    return ImVec4(((hex >> 16) & 0xff) / 255.f, ((hex >> 8) & 0xff) / 255.f,
                  (hex & 0xff) / 255.f, a);
}

ImVec4 with_alpha(ImVec4 c, float a) {
    c.w = a;
    return c;
}

ImFont *load_ui_font(ImGuiIO &io) {
#ifdef _WIN32
    std::string path;
    if (const char *root = std::getenv("SystemRoot"))
        path = std::string(root) + "\\Fonts\\segoeui.ttf";
    else
        path = "C:\\Windows\\Fonts\\segoeui.ttf";

    ImFontConfig cfg{};
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = true;
    if (ImFont *font = io.Fonts->AddFontFromFileTTF(path.c_str(), 0.0f, &cfg))
        return font;
    Utils::warn("Failed to load {}; falling back to default ImGui font", path);
#endif
    return io.Fonts->AddFontDefaultVector();
}

void apply_ui_layout(ImGuiStyle &style) {
    style.WindowPadding = ImVec2(16.0f, 16.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(12.0f, 8.0f);
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;
}

void apply_palette(ImGuiStyle &style, UiTheme theme) {
    // Role                Light      Dark
    const ImVec4 bg = rgba(theme == UiTheme::Light ? 0xF8F9FA : 0x121212);
    const ImVec4 s1 = rgba(theme == UiTheme::Light ? 0xFFFFFF : 0x1E1E1E);
    const ImVec4 s2 = rgba(theme == UiTheme::Light ? 0xF1F3F4 : 0x2D2D2D);
    const ImVec4 tp = rgba(theme == UiTheme::Light ? 0x202124 : 0xE3E3E3);
    const ImVec4 ts = rgba(theme == UiTheme::Light ? 0x5F6368 : 0xA0A0A0);
    const ImVec4 td = rgba(theme == UiTheme::Light ? 0x9AA0A6 : 0x6E6E6E);
    const ImVec4 bd = rgba(theme == UiTheme::Light ? 0xDADCE0 : 0x383838);
    const ImVec4 ac = rgba(theme == UiTheme::Light ? 0x1A73E8 : 0x8AB4F8);
    const ImVec4 er = rgba(theme == UiTheme::Light ? 0xD93025 : 0xF28B82);

    auto &c = style.Colors;
    c[ImGuiCol_Text] = tp;
    c[ImGuiCol_TextDisabled] = td;
    c[ImGuiCol_WindowBg] = s1;
    c[ImGuiCol_ChildBg] = s1;
    c[ImGuiCol_PopupBg] = s2;
    c[ImGuiCol_Border] = bd;
    c[ImGuiCol_BorderShadow] = rgba(0x000000, 0.f);
    c[ImGuiCol_FrameBg] = s2;
    c[ImGuiCol_FrameBgHovered] = with_alpha(ac, 0.20f);
    c[ImGuiCol_FrameBgActive] = with_alpha(ac, 0.35f);
    c[ImGuiCol_TitleBg] = s2;
    c[ImGuiCol_TitleBgActive] = s2;
    c[ImGuiCol_TitleBgCollapsed] = s1;
    c[ImGuiCol_MenuBarBg] = s1;
    c[ImGuiCol_ScrollbarBg] = bg;
    c[ImGuiCol_ScrollbarGrab] = bd;
    c[ImGuiCol_ScrollbarGrabHovered] = with_alpha(ac, 0.55f);
    c[ImGuiCol_ScrollbarGrabActive] = ac;
    c[ImGuiCol_CheckMark] = ac;
    c[ImGuiCol_CheckboxSelectedBg] = with_alpha(ac, 0.35f);
    c[ImGuiCol_SliderGrab] = ac;
    c[ImGuiCol_SliderGrabActive] = ac;
    c[ImGuiCol_Button] = with_alpha(ac, 0.25f);
    c[ImGuiCol_ButtonHovered] = with_alpha(ac, 0.45f);
    c[ImGuiCol_ButtonActive] = with_alpha(ac, 0.70f);
    c[ImGuiCol_Header] = with_alpha(ac, 0.25f);
    c[ImGuiCol_HeaderHovered] = with_alpha(ac, 0.40f);
    c[ImGuiCol_HeaderActive] = with_alpha(ac, 0.55f);
    c[ImGuiCol_Separator] = bd;
    c[ImGuiCol_SeparatorHovered] = with_alpha(ac, 0.60f);
    c[ImGuiCol_SeparatorActive] = ac;
    c[ImGuiCol_ResizeGrip] = with_alpha(ac, 0.25f);
    c[ImGuiCol_ResizeGripHovered] = with_alpha(ac, 0.50f);
    c[ImGuiCol_ResizeGripActive] = with_alpha(ac, 0.75f);
    c[ImGuiCol_InputTextCursor] = ac;
    c[ImGuiCol_Tab] = s2;
    c[ImGuiCol_TabHovered] = with_alpha(ac, 0.45f);
    c[ImGuiCol_TabSelected] = with_alpha(ac, 0.30f);
    c[ImGuiCol_TabSelectedOverline] = ac;
    c[ImGuiCol_TabDimmed] = s2;
    c[ImGuiCol_TabDimmedSelected] = with_alpha(ac, 0.20f);
    c[ImGuiCol_TabDimmedSelectedOverline] = with_alpha(ac, 0.50f);
    c[ImGuiCol_PlotLines] = ac;
    c[ImGuiCol_PlotLinesHovered] = er;
    c[ImGuiCol_PlotHistogram] = ac;
    c[ImGuiCol_PlotHistogramHovered] = er;
    c[ImGuiCol_TableHeaderBg] = s2;
    c[ImGuiCol_TableBorderStrong] = bd;
    c[ImGuiCol_TableBorderLight] = bd;
    c[ImGuiCol_TableRowBg] = rgba(0x000000, 0.f);
    c[ImGuiCol_TableRowBgAlt] = with_alpha(tp, 0.03f);
    c[ImGuiCol_TextLink] = ac;
    c[ImGuiCol_TextSelectedBg] = with_alpha(ac, 0.35f);
    c[ImGuiCol_TreeLines] = ts;
    c[ImGuiCol_DragDropTarget] = ac;
    c[ImGuiCol_UnsavedMarker] = er;
    c[ImGuiCol_NavCursor] = ac;
    c[ImGuiCol_NavWindowingHighlight] = with_alpha(ac, 0.70f);
    c[ImGuiCol_NavWindowingDimBg] = rgba(0x000000, 0.40f);
    c[ImGuiCol_ModalWindowDimBg] = rgba(0x000000, 0.45f);

    Video::set_clear_color(bg.x, bg.y, bg.z, bg.w);
}

} // namespace

void imgui_apply_theme(UiTheme theme) {
    g_theme = theme;
    if (!g_ready && ImGui::GetCurrentContext() == nullptr)
        return;
    apply_palette(ImGui::GetStyle(), theme);
}

UiTheme imgui_current_theme() { return g_theme; }

bool imgui_init(SDL_Window *window, Vulkan::WSI &wsi, UiTheme theme) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    const float dpi_scale =
        std::max(1.0f, ImGui_ImplSDL2_GetContentScaleForWindow(window));
    ImGuiStyle &style = ImGui::GetStyle();
    apply_ui_layout(style);
    apply_palette(style, theme);
    g_theme = theme;
    style.ScaleAllSizes(dpi_scale);
    style.FontScaleDpi = dpi_scale;
    style.FontSizeBase = 20.0f;
    load_ui_font(io);

    if (!ImGui_ImplSDL2_InitForVulkan(window)) {
        Utils::critical("ImGui_ImplSDL2_InitForVulkan failed");
        return false;
    }

    auto &device = wsi.get_device();
    auto &ctx = wsi.get_context();
    const auto &qinfo = device.get_queue_info();
    const VkFormat swap_format = device.get_swapchain_view().get_format();

    // Compatible color-only render pass for ImGui (Granite's request_render_pass is private).
    VkAttachmentDescription attachment{};
    attachment.format = swap_format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rp_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp_info.attachmentCount = 1;
    rp_info.pAttachments = &attachment;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies = &dependency;

    if (vkCreateRenderPass(device.get_device(), &rp_info, nullptr, &g_imgui_rp) !=
        VK_SUCCESS) {
        Utils::critical("Failed to create ImGui render pass");
        return false;
    }

    ImGui_ImplVulkan_InitInfo init{};
    init.ApiVersion = VK_API_VERSION_1_1;
    init.Instance = ctx.get_instance();
    init.PhysicalDevice = ctx.get_gpu();
    init.Device = device.get_device();
    init.QueueFamily = qinfo.family_indices[Vulkan::QUEUE_INDEX_GRAPHICS];
    init.Queue = qinfo.queues[Vulkan::QUEUE_INDEX_GRAPHICS];
    init.DescriptorPoolSize = 64;
    init.MinImageCount = 2;
    init.ImageCount = std::max(2u, device.get_num_swapchain_images());
    init.PipelineInfoMain.RenderPass = g_imgui_rp;
    init.PipelineInfoMain.Subpass = 0;
    init.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    if (!ImGui_ImplVulkan_Init(&init)) {
        Utils::critical("ImGui_ImplVulkan_Init failed");
        return false;
    }

    g_vk_device = device.get_device();
    g_ready = true;
    return true;
}

void imgui_shutdown() {
    if (!g_ready)
        return;
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    if (g_imgui_rp != VK_NULL_HANDLE && g_vk_device != VK_NULL_HANDLE) {
        vkDestroyRenderPass(g_vk_device, g_imgui_rp, nullptr);
        g_imgui_rp = VK_NULL_HANDLE;
    }
    g_pool = VK_NULL_HANDLE;
    g_vk_device = VK_NULL_HANDLE;
    g_ready = false;
}

bool imgui_set_sdl_window(SDL_Window *window) {
    if (!g_ready || !window)
        return false;
    ImGui_ImplSDL2_Shutdown();
    if (!ImGui_ImplSDL2_InitForVulkan(window)) {
        Utils::critical("ImGui_ImplSDL2_InitForVulkan failed (window rebind)");
        return false;
    }
    return true;
}

void imgui_new_frame() {
    if (!g_ready)
        return;
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    g_frame_rendered = false;
}

void imgui_render(Vulkan::CommandBuffer &cmd) {
    if (!g_ready)
        return;
    if (!g_frame_rendered) {
        ImGui::Render();
        g_frame_rendered = true;
    }
    ImDrawData *draw = ImGui::GetDrawData();
    if (draw)
        ImGui_ImplVulkan_RenderDrawData(draw, cmd.get_command_buffer());
}

bool imgui_process_event(const SDL_Event &e) {
    if (!g_ready)
        return false;
    return ImGui_ImplSDL2_ProcessEvent(&e);
}

bool imgui_want_capture_keyboard() {
    return g_ready && ImGui::GetIO().WantCaptureKeyboard;
}

bool imgui_want_capture_mouse() {
    return g_ready && ImGui::GetIO().WantCaptureMouse;
}

} // namespace Ui
} // namespace N64
