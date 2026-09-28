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

#endif
