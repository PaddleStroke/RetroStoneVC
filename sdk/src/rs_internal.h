/* RetroStone VC SDK internals. MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft). */
#ifndef RS_INTERNAL_H
#define RS_INTERNAL_H

#include "rs_host.h"

#if defined(__GNUC__)
#define RS_INLINE static inline __attribute__((always_inline))
#define RS_LIKELY(x)   __builtin_expect(!!(x), 1)
#define RS_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define RS_INLINE static inline
#define RS_LIKELY(x)   (x)
#define RS_UNLIKELY(x) (x)
#endif

/* PPU */
void ppu_reset(void);
void ppu_render(uint16_t *fb);          /* 320x240 RGB565 */
int  ppu_last_sprites(void);
int  ppu_last_max_line(void);

/* APU */
void apu_reset(void);
void apu_shutdown(void);
void apu_render(int16_t *out, int frames); /* interleaved stereo */

/* Text */
void text_reset(void);

/* Core */
void rs_logv_internal(const char *prefix, const char *fmt, void *ap);
const rs_game *core_game(void);
const char *core_asset_name(const void *data);                 /* the asset holding this data, or NULL */
const void *core_asset_find(const char *name, size_t *size);   /* loaded or embedded, no file access */

/* ---- save states (rs_state.c) -------------------------------------------------------------------------
 * Little-endian writer / reader over a buffer. An overflow sets err and further calls do nothing. */
typedef struct rs_wr { uint8_t *p; size_t n, cap; int err; } rs_wr;
typedef struct rs_rd { const uint8_t *p; size_t n, pos; int err; } rs_rd;
void     wr_bytes(rs_wr *w, const void *d, size_t n);
void     wr_u8(rs_wr *w, uint8_t v);
void     wr_u16(rs_wr *w, uint16_t v);
void     wr_u32(rs_wr *w, uint32_t v);
#define  wr_i32(w, v) wr_u32(w, (uint32_t)(int32_t)(v))
void     rd_bytes(rs_rd *r, void *d, size_t n);     /* d == NULL: skip */
uint8_t  rd_u8(rs_rd *r);
uint16_t rd_u16(rs_rd *r);
uint32_t rd_u32(rs_rd *r);
#define  rd_i32(r) ((int32_t)rd_u32(r))
/* Pointers the runtime holds for the game (line-scroll tables, raster user data): saved as (object, offset) of
 * the game's registered objects. need = bytes that must follow the pointer. put: -1 = not in a registered object
 * (w->err is set); get: -1 = invalid. */
int state_ptr_put(rs_wr *w, const void *p, size_t need, const char *what);
int state_ptr_get(rs_rd *r, const void **p, size_t need);
int state_raster_put(rs_wr *w, rs_raster_fn fn);
int state_raster_get(rs_rd *r, rs_raster_fn *fn);
/* Each unit saves its registers and memory; load(apply = 0) only reads and checks, load(apply = 1) applies.
 * Both return 0 when the data is valid. */
void core_state_save(rs_wr *w);
int  core_state_load(rs_rd *r, int apply);
void ppu_state_save(rs_wr *w);
int  ppu_state_load(rs_rd *r, int apply);
void apu_state_save(rs_wr *w);
int  apu_state_load(rs_rd *r, int apply);
void music_state_save(rs_wr *w);
int  music_state_load(rs_rd *r, int apply);
void text_state_save(rs_wr *w);
int  text_state_load(rs_rd *r, int apply);
/* fixed maximum sizes of those sections (retro_serialize_size must not change) */
#define STATE_CORE_MAX  (8 + RS_PAD_MAX * 5)
#define STATE_PPU_MAX   (RS_TILE_MAX * 32 + RS_TILE_MAX / 8 + 4 + 256 * 2 + RS_OAM_MAX * 11 + RS_OAM_MAX / 8 + 64 + \
                         RS_BG_COUNT * (80 + 128 * 128 * 2) + 8 + RS_VIEW_MAX * 32)
#define STATE_APU_MAX   (40 + RS_VOICE_MAX * 60 + (RS_AUDIO_RATE * 240 / 1000) * 4 + RS_SAMPLE_MAX * 8)
#define STATE_MUSIC_MAX (4 + 64 + 8)
#define STATE_TEXT_MAX  (6 * 4)

#endif
