#include "renderer.h"
#include "frontend.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <parallel_rdp_wrapper.h>
#include <volk.h>

#include "mmio/vi.h"

//#include <settings.h>

// prior to 2.0.10, this was anonymous enum
#if SDL_COMPILEDVERSION < SDL_VERSIONNUM(2, 0, 10)
typedef int SDL_PixelFormatEnum;
#endif

int SCREEN_SCALE = 2;
static SDL_GLContext gl_context;
SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static uint8_t pixel_buffer[640 * 480 * 4]; // should be the largest needed

uint32_t fps_interval = 1000; // 1000ms = 1 second
uint32_t sdl_lastframe = 0;
uint32_t sdl_numframes = 0;
uint32_t sdl_fps = 0;
uint32_t game_fps = 0;
const char *APP_NAME = "n64-emu";
char sdl_wintitle[100] = "n64-emu 00 FPS";

SDL_Window *get_window_handle() { return window; }

void video_init_vulkan() {
    window = SDL_CreateWindow(
        APP_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        N64_SCREEN_X * SCREEN_SCALE, N64_SCREEN_Y * SCREEN_SCALE,
        SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (volkInitialize() != VK_SUCCESS) {
        Utils::critical("Failed to load Volk");
        exit(-1);
    }
}

void video_init_software() {
    window =
        SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_UNDEFINED,
                         SDL_WINDOWPOS_UNDEFINED, N64_SCREEN_X * SCREEN_SCALE,
                         N64_SCREEN_Y * SCREEN_SCALE, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
}

void render_init() {
    uint32_t flags = SDL_INIT_AUDIO;
    if (SDL_Init(flags) < 0) {
        Utils::critical("SDL couldn't initialize! {}", SDL_GetError());
        exit(-1);
    }
}

static uint32_t last_vi_type = 0;
static uint32_t vi_height = 0;
static uint32_t vi_width = 0;

inline void pre_scanout(SDL_PixelFormatEnum pixel_format) {
    float y_scale = (float)N64::g_vi().reg_y_scale.scale / 1024.0;
    float x_scale = (float)N64::g_vi().reg_x_scale.scale / 1024.0;

    int new_height =
        ceilf((float)((N64::g_vi().reg_v_video.end - N64::g_vi().reg_v_end.start) >> 1) *
              y_scale);
    int new_width =
        ceilf((float)((n64sys.vi.hstart.end - n64sys.vi.hstart.start) >> 0) *
              x_scale);

    bool should_recreate_texture = false;

    should_recreate_texture |= new_height != vi_height;
    should_recreate_texture |= new_width != vi_width;

    should_recreate_texture |= last_vi_type != n64sys.vi.status.type;

    if (should_recreate_texture) {
        last_vi_type = n64sys.vi.status.type;
        vi_height = new_height;
        vi_width = new_width;
        if (texture != NULL) {
            SDL_DestroyTexture(texture);
        }
        texture =
            SDL_CreateTexture(renderer, pixel_format,
                              SDL_TEXTUREACCESS_STREAMING, vi_width, vi_height);
    }
}

static void vi_scanout_16bit() {
    pre_scanout(SDL_PIXELFORMAT_RGBA5551);
    const int rdram_offset = n64sys.vi.vi_origin & (N64_RDRAM_SIZE - 1);
    for (int y = 0; y < vi_height; y++) {
        int yofs = (y * vi_width * 2);
        for (int x = 0; x < vi_width; x += 2) {
            memcpy(&pixel_buffer[yofs + x * 2 + 2],
                   &n64sys.mem.rdram[rdram_offset + yofs + x * 2 + 0],
                   sizeof(uint16_t));
            memcpy(&pixel_buffer[yofs + x * 2 + 0],
                   &n64sys.mem.rdram[rdram_offset + yofs + x * 2 + 2],
                   sizeof(uint16_t));
        }
    }
    SDL_UpdateTexture(texture, NULL, &pixel_buffer, vi_width * 2);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
}

static void vi_scanout_32bit() {
    pre_scanout(SDL_PIXELFORMAT_RGBA8888);
    int rdram_offset = n64sys.vi.vi_origin & (N64_RDRAM_SIZE - 1);
    SDL_UpdateTexture(texture, NULL, &n64sys.mem.rdram[rdram_offset],
                      vi_width * 4);
    SDL_RenderCopy(renderer, texture, NULL, NULL);

    void n64_render_screen() {
        sdl_numframes++;
        uint32_t ticks = SDL_GetTicks();
        if (sdl_lastframe < ticks - fps_interval) {
            sdl_lastframe = ticks;
            sdl_fps = sdl_numframes;
            sdl_numframes = 0;
            game_fps = n64sys.vi.swaps;
            n64sys.vi.swaps = 0;
            const char *game_name = n64sys.mem.rom.game_name_db != NULL
                                        ? n64sys.mem.rom.game_name_db
                                        : n64sys.mem.rom.game_name_cartridge;
            if (game_name == NULL || strcmp(game_name, "") == 0) {
                snprintf(sdl_wintitle, sizeof(sdl_wintitle),
                         APP_NAME " %02d emulator FPS / %02d game FPS", sdl_fps,
                         game_fps);
            } else {
                snprintf(sdl_wintitle, sizeof(sdl_wintitle),
                         N64_APP_NAME " [%s] %02d emulator FPS / %02d game FPS",
                         game_name, sdl_fps, game_fps);
            }
            SDL_SetWindowTitle(window, sdl_wintitle);
        }
    }
}

bool is_framerate_unlocked() {
    switch (n64_video_type) {
    case VULKAN_VIDEO_TYPE:
    case QT_VULKAN_VIDEO_TYPE:
        return prdp_is_framerate_unlocked();

    case UNKNOWN_VIDEO_TYPE:
    case SOFTWARE_VIDEO_TYPE:
        return false;
    }
}

void set_framerate_unlocked(bool unlocked) {
    prdp_set_framerate_unlocked(unlocked);
}
