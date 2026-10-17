/* RetroStone VC SDK: helpers shared by the desktop frontends (SDL2, headless).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft). */
#ifndef RS_DESKTOP_H
#define RS_DESKTOP_H

#include "rs_host.h"
#include "rs_i18n.h"

/* Directory used for the data override (NULL = none). */
void  rsd_set_data_dir(const char *dir);
void  rsd_add_data_dir(const char *dir);   /* more directories searched after the data directory */
void *rsd_load_file(const char *path, size_t *size);
int   rsd_save_file(const char *path, const void *data, size_t size);
/* rs_file_fn for rs_host_set_file_loader: <data_dir>/<name>. */
void *rsd_data_loader(const char *name, size_t *size);
/* Loads/saves the 32 KiB save RAM. */
int   rsd_sram_load(const char *path);
int   rsd_sram_save(const char *path);
/* Widescreen: the environment variable RS_WIDTH=N asks for that width (rs_host_set_width), like --width N on the
 * desktop runners (the tests run a game's scripts at several widths this way). Returns N, or 0 when unset. */
int   rsd_width_env(void);
/* The language (docs/i18n.md): RS_LANG=fr (any locale name: fr_FR, pt-BR, zh...) asks for it (rs_host_set_lang), like
 * --lang fr on the desktop runners (the tests run a game in every language this way). Returns the RS_LANG_* or -1. */
int   rsd_lang_env(void);
int   rsd_lang_arg(const char *code);     /* --lang CODE: sets it; -1 (and a message) when it is not one of ours */
/* Writes the current framebuffer (RGB565, rs_screen_width() x rs_screen_height(): 240, or a tall screen's height) as
 * PNG, scaled by an integer. */
int   rsd_write_png(const char *path, const uint16_t *fb, int scale);
/* The same, only the screen columns x0 .. x1 - 1 (a zoomed screen's visible ones: rs_screen_visible_x0(), x1()). */
int   rsd_write_png_cols(const char *path, const uint16_t *fb, int scale, int x0, int x1);
/* Parses "key=value" and sets it as a runtime option. */
void  rsd_option(const char *kv);
/* Button name ("B", "START", "UP"...) or '+'-joined list to a bit mask. */
uint16_t rsd_buttons(const char *s);

#endif
