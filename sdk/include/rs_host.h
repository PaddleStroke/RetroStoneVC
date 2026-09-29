/*
 * RetroStone VC SDK: the frontend (host) side of the runtime.
 * Frontends (libretro core, SDL2 runner, headless runner, tests) drive the
 * console through this API; games never call it.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef RS_HOST_H
#define RS_HOST_H

#include "rs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*rs_log_fn)(const char *line);
/* Optional file loader for the data-directory override (NULL = none).
 * Returns malloc'ed data or NULL. */
typedef void *(*rs_file_fn)(const char *name, size_t *size);

void rs_host_set_log(rs_log_fn fn);
void rs_host_set_file_loader(rs_file_fn fn);
void rs_host_set_option(const char *key, const char *value);

/* Starts the runtime and calls the game's init(). */
int  rs_host_init(const rs_game *game);
void rs_host_shutdown(void);
/* The runtime without a game (unit tests): video/audio state only. */
void rs_host_reset(void);

void rs_host_set_pad(int port, uint16_t buttons, int connected);
/* Development function key for the next frame (1..10 = F1..F10, 0 = none): rs_dev_key(). */
void rs_host_set_dev_key(int key);

/* Runs one 1/60 s frame: update(), draw(), render, audio. */
void rs_host_frame(void);
/* Renders the current PPU state only (tests). */
void rs_host_render(void);

const uint16_t *rs_host_framebuffer(void);  /* RGB565, 320x240, pitch 320 */
/* Audio produced by the last frame (533 or 534 stereo frames). */
const int16_t  *rs_host_audio(int *frames);

/* Save RAM: the frontend loads it before init and saves it when dirty. */
uint8_t *rs_host_sram(void);
int      rs_host_sram_dirty(int clear);

/* Profiling of the last frame, in microseconds (host clock). */
typedef struct rs_host_stats {
    uint32_t update_us, render_us, audio_us;
    int sprites, max_sprites_line;
} rs_host_stats;
const rs_host_stats *rs_host_stats_last(void);
uint64_t rs_host_time_us(void);

#ifdef __cplusplus
}
#endif
#endif
