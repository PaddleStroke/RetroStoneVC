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
/* The file loader tells where the file it just returned came from (a folder), for rs_asset_origin(). */
void rs_host_set_file_origin(const char *where);
void rs_host_set_option(const char *key, const char *value);

/* Widescreen (docs/spec.md "Widescreen"). The frontend asks for a width (any value: it is clamped to
 * RS_SCREEN_W_MIN..RS_SCREEN_W_MAX) before rs_host_init(); the game gets the smaller of it and its
 * rs_game.max_width (RS_SCREEN_W when the game has not opted in: the frontend pillarboxes the picture).
 * rs_host_width_for() tells the width a game would get (the libretro geometry, before the game starts). */
void rs_host_set_width(int width);
int  rs_host_width_for(const rs_game *game, int width);
/* The width of a running game, live: saves a state, restarts the game at the new width and loads the state (the
 * battery save stays in memory). Returns the width in use: unchanged when the game has not opted in, has no save
 * states, or the state could not be saved or loaded at the new width (then the game runs on at the old width). */
int  rs_host_change_width(int width);
/* The desktop and frontend helper: the width for a picture of aspect num:den at 240 lines (16:9 -> 427). */
int  rs_host_width_for_aspect(int num, int den);

/* Starts the runtime and calls the game's init(). */
int  rs_host_init(const rs_game *game);
void rs_host_shutdown(void);
/* The runtime without a game (unit tests): video/audio state only. */
void rs_host_reset(void);

void rs_host_set_pad(int port, uint16_t buttons, int connected);
/* What drives the port (RS_DEVICE_PAD, RS_DEVICE_KEYBOARD, RS_DEVICE_KEYBOARD2): rs_pad_device(). */
void rs_host_set_pad_device(int port, int device);
/* Development function key for the next frame (1..10 = F1..F10, 0 = none): rs_dev_key(). */
void rs_host_set_dev_key(int key);
/* Touch (docs/spec.md "Touch"): the fingers down now, player 1, in SCREEN pixels of rs_screen_width() x 240 (the
 * frontend maps its display through its scaling and borders; values outside are clamped). id: any number that stays
 * the same while the finger is down. Kept until the next call (like the pads): call it before each rs_host_frame();
 * n = 0 lifts them all. The runtime gives the fingers their slots and works out the edges, taps, swipes and holds at
 * the start of the frame. At most RS_TOUCH_MAX fingers (the others are ignored until a slot is free). */
typedef struct rs_host_touch { long id; int x, y; } rs_host_touch;
void rs_host_set_touches(int n, const rs_host_touch *touches);

/* Runs one 1/60 s frame: update(), draw(), render, audio. */
void rs_host_frame(void);
/* Renders the current PPU state only (tests). */
void rs_host_render(void);

const uint16_t *rs_host_framebuffer(void);  /* RGB565, rs_screen_width() x 240, pitch = the width (320 by default) */
/* Audio produced by the last frame (533 or 534 stereo frames). */
const int16_t  *rs_host_audio(int *frames);

/* Save RAM: the frontend loads it before init and saves it when dirty. */
uint8_t *rs_host_sram(void);
int      rs_host_sram_dirty(int clear);

/* Save states (docs/spec.md "Save states"), between two frames. The size is a fixed maximum for a game build
 * (libretro requires it) and can be asked before rs_host_init(); 0 = the game has no save states.
 * save: the bytes used (the rest of the buffer is zeroed), 0 on failure (the log says why).
 * load: 0 = loaded; otherwise nothing was changed (wrong game, version or build, truncated, corrupted). */
size_t rs_host_state_size(const rs_game *game);
size_t rs_host_state_save(void *buf, size_t size);
int    rs_host_state_load(const void *buf, size_t size);
/* What the game registered (kind "var", "ptr", "ref" or "raster"; tests and tools/state_audit.py). */
void   rs_host_state_list(void (*fn)(const char *kind, const char *name, size_t size));

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
