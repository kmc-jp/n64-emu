#pragma once

#include "memory/memory.h"
#include "mmio/vi.h"
#include "utils.h"
#include "vulkan_headers.hpp"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <algorithm>
#include <device.hpp>
#include <rdp_device.hpp>
#include <volk.h>
#include <wsi.hpp>

namespace N64 {
namespace Frontend {

constexpr int WINDOW_WIDTH = 1024;
constexpr int WINDOW_HEIGHT = 768;

class ParallelRdpWindowInfo {
  public:
    struct CoordinatePair {
        float x;
        float y;
    };
    virtual CoordinatePair get_window_size() = 0;
    virtual ~ParallelRdpWindowInfo() = default;
};

class SDLParallelRdpWindowInfo : public ParallelRdpWindowInfo {
  public:
    SDLParallelRdpWindowInfo(SDL_Window *window_) : window(window_) {}

    CoordinatePair get_window_size() {
        int width, height;
        SDL_GetWindowSize(window, &width, &height);
        return CoordinatePair{static_cast<float>(width),
                              static_cast<float>(height)};
    }

  private:
    SDL_Window *window;
};

// https://github.com/Themaister/Granite-MicroSamples/blob/master/06_wsi_sdl2.cpp
class SDL2Platform : public Vulkan::WSIPlatform {
  private:
    SDL_Window *window;
    bool is_alive = true;

  public:
    SDL2Platform(SDL_Window *window_) : window(window_) {}

    VkSurfaceKHR create_surface(VkInstance instance,
                                VkPhysicalDevice) override {
        VkSurfaceKHR surface;
        if (SDL_Vulkan_CreateSurface(window, instance, &surface))
            return surface;
        else
            return VK_NULL_HANDLE;
    }

    std::vector<const char *> get_instance_extensions() override {
        unsigned instance_ext_count = 0;
        SDL_Vulkan_GetInstanceExtensions(window, &instance_ext_count, nullptr);
        std::vector<const char *> instance_names(instance_ext_count);
        SDL_Vulkan_GetInstanceExtensions(window, &instance_ext_count,
                                         instance_names.data());
        return instance_names;
    }

    uint32_t get_surface_width() override {
        int w, h;
        SDL_Vulkan_GetDrawableSize(window, &w, &h);
        return w;
    }

    uint32_t get_surface_height() override {
        int w, h;
        SDL_Vulkan_GetDrawableSize(window, &w, &h);
        return h;
    }

    bool alive(Vulkan::WSI &) override { return is_alive; }

    void poll_input() override {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                is_alive = false;
                break;
            default:
                break;
            }
        }
    }
};

class Frontend {
  private:
    SDL_Window *window = nullptr;

    Vulkan::WSI *wsi = nullptr;
    SDL2Platform *platform = nullptr;

    VkInstance instance{};
    VkPhysicalDevice physicalDevice{};
    VkDevice device{};
    uint32_t queueFamily{uint32_t(-1)};
    VkQueue queue{};
    VkPipelineCache pipelineCache{};
    VkDescriptorPool descriptorPool{};
    VkAllocationCallbacks *allocator{};
    uint32_t minImageCount;

    Vulkan::Program *fullscreen_quad_program;

    SDLParallelRdpWindowInfo *window_info;
    RDP::CommandProcessor *command_processor;

  public:
    Frontend(const uint8_t *rdram) {
        init_sdl();
        init_parallel_rdp(rdram);
        init_imgui();

        instance = wsi->get_context().get_instance();
        physicalDevice = wsi->get_device().get_physical_device();
        device = wsi->get_device().get_device();
        queueFamily = wsi->get_context()
                          .get_queue_info()
                          .family_indices[Vulkan::QUEUE_INDEX_GRAPHICS];
        queue = wsi->get_context()
                    .get_queue_info()
                    .queues[Vulkan::QUEUE_INDEX_GRAPHICS];
        pipelineCache = nullptr;
        descriptorPool = nullptr;
        allocator = nullptr;
        minImageCount = 2;

    }

    ~Frontend() {
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void init_sdl() {
        SDL_Init(SDL_INIT_EVERYTHING);
        window = SDL_CreateWindow("n64-emu", SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
                                  WINDOW_HEIGHT,
                                  SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
                                      SDL_WINDOW_ALLOW_HIGHDPI);
        if (!window) {
            Utils::critical("Failed to create window");
            exit(-1);
        }

        if (volkInitialize() != VK_SUCCESS) {
            Utils::critical("Failed to initialize Vulkan");
            exit(-1);
        } else {
            Utils::info("Vulkan initialized");
        }
    }

    void init_imgui() {}

    void load_wsi_platform() {
        wsi = new Vulkan::WSI();
        wsi->set_backbuffer_srgb(false);
        Vulkan::WSIPlatform *p = new SDL2Platform(window);
        wsi->set_platform(p);
        wsi->set_present_mode(Vulkan::PresentMode::SyncToVBlank);
        Vulkan::Context::SystemHandles handles;
        if (!wsi->init_simple(1, handles)) {
            Utils::critical("Failed to initialize WSI");
            exit(-1);
        }
    }

    void load_parallel_rdp() {}

    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/rdp/parallel_rdp_wrapper.cpp#L93
    void init_parallel_rdp(const uint8_t *rdram) {

        Vulkan::ResourceLayout vert_layout;
        Vulkan::ResourceLayout frag_layout;

        vert_layout.input_mask = 1;
        vert_layout.output_mask = 1;

        frag_layout.input_mask = 1;
        frag_layout.output_mask = 1;
        frag_layout.spec_constant_mask = 1;
        frag_layout.push_constant_size = 4 * sizeof(float);

        frag_layout.sets[0].sampled_image_mask = 1;
        frag_layout.sets[0].fp_mask = 1;
        frag_layout.sets[0].array_size[0] = 1;

        // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/rdp/parallel_rdp_wrapper.cpp#L72
        std::vector<uint8_t> vert_spv =
            Utils::read_binary_file("../resources/vert.spv");
        std::vector<uint8_t> frag_spv =
            Utils::read_binary_file("../resources/frag.spv");

        fullscreen_quad_program = wsi->get_device().request_program(
            reinterpret_cast<uint32_t *>(vert_spv.data()), vert_spv.size(),
            reinterpret_cast<uint32_t *>(frag_spv.data()), frag_spv.size(),
            &vert_layout, &frag_layout);

        auto aligned_rdram = reinterpret_cast<uintptr_t>(rdram);
        uintptr_t offset = 0;
        if (wsi->get_device()
                .get_device_features()
                .supports_external_memory_host) {
            size_t align =
                wsi->get_device()
                    .get_device_features()
                    .host_memory_properties.minImportedHostPointerAlignment;
            offset = aligned_rdram & (align - 1);
            aligned_rdram -= offset;
        }

        RDP::CommandProcessorFlags flags = 0;
        command_processor = new RDP::CommandProcessor(
            wsi->get_device(), reinterpret_cast<void *>(aligned_rdram), offset,
            8 * 1024 * 1024, 4 * 1024 * 1024, flags);

        if (!command_processor->device_is_supported()) {
            Utils::critical(
                "This device probably does not support 8/16-bit "
                "storage. Make sure you're using up-to-date drivers!");
            exit(-1);
        }

        window_info = new SDLParallelRdpWindowInfo(window);
    }

    void DrawFullscreenTexturedQuad(Vulkan::ImageHandle image,
                                    Vulkan::CommandBufferHandle cmd) {
        cmd->set_texture(0, 0, image->get_view(),
                         Vulkan::StockSampler::LinearClamp);
        cmd->set_program(fullscreen_quad_program);
        cmd->set_quad_state();
        auto data = static_cast<float *>(
            cmd->allocate_vertex_data(0, 6 * sizeof(float), 2 * sizeof(float)));
        data[0] = -1.0f;
        data[1] = -3.0f;
        data[2] = -1.0f;
        data[3] = +1.0f;
        data[4] = +3.0f;
        data[5] = +1.0f;

        auto window_size = window_info->get_window_size();

        float zoom =
            std::min(window_size.x / wsi->get_platform().get_surface_width(),
                     window_size.y / wsi->get_platform().get_surface_height());

        float width =
            (wsi->get_platform().get_surface_width() / window_size.x) * zoom;
        float height =
            (wsi->get_platform().get_surface_height() / window_size.y) * zoom;

        float uniform_data[] = {// Size
                                width, height,
                                // Offset
                                (1.0f - width) * 0.5f, (1.0f - height) * 0.5f};

        cmd->push_constants(uniform_data, 0, sizeof(uniform_data));

        cmd->set_vertex_attrib(0, 0, VK_FORMAT_R32G32_SFLOAT, 0);
        cmd->set_depth_test(false, false);
        cmd->set_depth_compare(VK_COMPARE_OP_ALWAYS);
        cmd->set_primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        cmd->draw(3, 1);
    }

    // What is difference?
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/rdp/parallel_rdp_wrapper.cpp#L183
    // https://github.com/Dillonb/n64/blob/6502f7d2f163c3f14da5bff8cd6d5ccc47143156/src/rdp/parallel_rdp_wrapper.cpp#L220
    void update_screen(Vulkan::ImageHandle image) {
        wsi->begin_frame();

        if (!image) {
            auto info = Vulkan::ImageCreateInfo::immutable_2d_image(
                800, 600, VK_FORMAT_R8G8B8A8_UNORM);
            info.usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            info.misc = Vulkan::IMAGE_MISC_MUTABLE_SRGB_BIT;
            info.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            image = wsi->get_device().create_image(info);

            auto cmd = wsi->get_device().request_command_buffer();

            cmd->image_barrier(*image, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_ACCESS_TRANSFER_WRITE_BIT);
            cmd->clear_image(*image, {});
            cmd->image_barrier(*image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                               VK_ACCESS_SHADER_READ_BIT);
            wsi->get_device().submit(cmd);
        }

        auto cmd = wsi->get_device().request_command_buffer();

        cmd->begin_render_pass(wsi->get_device().get_swapchain_render_pass(
            Vulkan::SwapchainRenderPass::ColorOnly));
        // ImDrawData *drawData = imguiWindow.Present(core);

        DrawFullscreenTexturedQuad(image, cmd);
        // ImGui_ImplVulkan_RenderDrawData(drawData, cmd->get_command_buffer());

        cmd->end_render_pass();
        wsi->get_device().submit(cmd);
        wsi->end_frame();
    }

    void update_screeen_parallel_rdp(N64::Mmio::VI::VI &vi) {
        command_processor->set_vi_register(RDP::VIRegister::Control,
                                           vi.reg_status);
        command_processor->set_vi_register(RDP::VIRegister::Origin,
                                           vi.reg_origin);
        command_processor->set_vi_register(RDP::VIRegister::Width,
                                           vi.reg_width);
        command_processor->set_vi_register(RDP::VIRegister::Intr, vi.reg_intr);
        command_processor->set_vi_register(RDP::VIRegister::VCurrentLine,
                                           vi.reg_current);
        command_processor->set_vi_register(RDP::VIRegister::Timing,
                                           vi.reg_burst);
        command_processor->set_vi_register(RDP::VIRegister::VSync,
                                           vi.reg_vsync);
        command_processor->set_vi_register(RDP::VIRegister::HSync,
                                           vi.reg_hsync);
        command_processor->set_vi_register(RDP::VIRegister::Leap,
                                           vi.reg_hsync_leap);
        command_processor->set_vi_register(RDP::VIRegister::HStart,
                                           vi.reg_h_video);
        command_processor->set_vi_register(RDP::VIRegister::VStart,
                                           vi.reg_v_video);
        command_processor->set_vi_register(RDP::VIRegister::VBurst,
                                           vi.reg_v_burst);
        command_processor->set_vi_register(RDP::VIRegister::XScale,
                                           vi.reg_x_scale);
        command_processor->set_vi_register(RDP::VIRegister::YScale,
                                           vi.reg_y_scale);

        RDP::ScanoutOptions opts;
        opts.persist_frame_on_invalid_input = true;
        opts.vi.aa = true;
        opts.vi.scale = true;
        opts.vi.dither_filter = true;
        opts.vi.divot_filter = true;
        opts.vi.gamma_dither = true;
        opts.downscale_steps = true;
        opts.crop_overscan_pixels = true;
        auto image = command_processor->scanout(opts);
        update_screen(image);
        command_processor->begin_frame_context();
    }
};

} // namespace Frontend
} // namespace N64
