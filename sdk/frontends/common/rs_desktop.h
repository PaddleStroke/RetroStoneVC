/* RetroStone VC SDK: helpers shared by the desktop frontends (SDL2, headless).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft). */
#ifndef RS_DESKTOP_H
#define RS_DESKTOP_H

#include "rs_host.h"

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
/* Writes the current framebuffer (RGB565) as PNG, scaled by an integer. */
int   rsd_write_png(const char *path, const uint16_t *fb, int scale);
/* Parses "key=value" and sets it as a runtime option. */
void  rsd_option(const char *kv);
/* Button name ("B", "START", "UP"...) or '+'-joined list to a bit mask. */
uint16_t rsd_buttons(const char *s);

#endif
