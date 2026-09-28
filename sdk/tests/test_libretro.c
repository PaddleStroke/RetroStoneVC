/*
 * Loads a <game>_libretro.so like a frontend does (dlopen), runs it with no
 * content for N frames and checks video, audio, input and save RAM.
 *   test_libretro path/to/core.so [frames]
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "libretro.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned vw, vh, frames_video, pixfmt = 99;
static size_t audio_frames;
static int failures;
static unsigned run_frame;

#define CHECK(c, ...) do { if (!(c)) { failures++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } \
                           else { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static bool env(unsigned cmd, void *data)
{
    switch (cmd) {
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: pixfmt = *(enum retro_pixel_format *)data; return true;
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: return false;
    default: return false;
    }
}
static void video(const void *d, unsigned w, unsigned h, size_t pitch) { (void)d; (void)pitch; vw = w; vh = h; frames_video++; }
static void audio1(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t audio(const int16_t *d, size_t n) { (void)d; audio_frames += n; return n; }
static void poll(void) {}
static int16_t input(unsigned port, unsigned dev, unsigned idx, unsigned id)
{
    (void)dev; (void)idx;
    /* press Start on pad 1 now and then (walks through the title menu) */
    return port == 0 && id == RETRO_DEVICE_ID_JOYPAD_START && (run_frame % 90) < 3;
}

#define SYM(name) name##_t name = (name##_t)dlsym(h, #name); if (!name) { printf("missing %s\n", #name); return 1; }
typedef void (*retro_init_t)(void);
typedef void (*retro_deinit_t)(void);
typedef void (*retro_run_t)(void);
typedef void (*retro_get_system_info_t)(struct retro_system_info *);
typedef void (*retro_get_system_av_info_t)(struct retro_system_av_info *);
typedef void (*retro_set_environment_t)(retro_environment_t);
typedef void (*retro_set_video_refresh_t)(retro_video_refresh_t);
typedef void (*retro_set_audio_sample_t)(retro_audio_sample_t);
typedef void (*retro_set_audio_sample_batch_t)(retro_audio_sample_batch_t);
typedef void (*retro_set_input_poll_t)(retro_input_poll_t);
typedef void (*retro_set_input_state_t)(retro_input_state_t);
typedef bool (*retro_load_game_t)(const struct retro_game_info *);
typedef void (*retro_unload_game_t)(void);
typedef void *(*retro_get_memory_data_t)(unsigned);
typedef size_t (*retro_get_memory_size_t)(unsigned);

int main(int argc, char **argv)
{
    if (argc < 2) { printf("usage: test_libretro core.so [frames]\n"); return 2; }
    unsigned n = argc > 2 ? (unsigned)atoi(argv[2]) : 600;
    void *h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!h) { printf("dlopen: %s\n", dlerror()); return 1; }
    SYM(retro_init) SYM(retro_deinit) SYM(retro_run) SYM(retro_get_system_info) SYM(retro_get_system_av_info)
    SYM(retro_set_environment) SYM(retro_set_video_refresh) SYM(retro_set_audio_sample)
    SYM(retro_set_audio_sample_batch) SYM(retro_set_input_poll) SYM(retro_set_input_state)
    SYM(retro_load_game) SYM(retro_unload_game) SYM(retro_get_memory_data) SYM(retro_get_memory_size)
    printf("libretro core %s\n", argv[1]);
    retro_set_environment(env);
    retro_set_video_refresh(video);
    retro_set_audio_sample(audio1);
    retro_set_audio_sample_batch(audio);
    retro_set_input_poll(poll);
    retro_set_input_state(input);
    retro_init();
    struct retro_system_info si;
    struct retro_system_av_info av;
    retro_get_system_info(&si);
    retro_get_system_av_info(&av);
    CHECK(av.geometry.base_width == 320 && av.geometry.base_height == 240, "geometry 320x240 (%s %s)", si.library_name, si.library_version);
    CHECK(av.timing.fps == 60.0 && av.timing.sample_rate == 32000.0, "60 fps, 32 kHz");
    CHECK(retro_load_game(NULL), "loads with no content");
    CHECK(pixfmt == RETRO_PIXEL_FORMAT_RGB565, "RGB565 pixel format");
    CHECK(retro_get_memory_size(RETRO_MEMORY_SAVE_RAM) == 32768 && retro_get_memory_data(RETRO_MEMORY_SAVE_RAM),
          "32 KiB save RAM exposed");
    for (run_frame = 0; run_frame < n; run_frame++) retro_run();
    CHECK(frames_video == n && vw == 320 && vh == 240, "%u video frames of %ux%u", frames_video, vw, vh);
    double expect = n * 32000.0 / 60.0;
    CHECK(audio_frames > expect - 2 && audio_frames < expect + 2, "%zu audio frames (expected %.0f)", audio_frames, expect);
    retro_unload_game();
    retro_deinit();
    dlclose(h);
    printf("libretro: %s\n", failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}
