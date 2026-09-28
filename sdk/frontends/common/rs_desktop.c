/* RetroStone VC SDK: desktop helpers. MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft). */
#include "rs_desktop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8
#include "stb_image_write.h"

static char g_data_dir[512];

void rsd_set_data_dir(const char *dir)
{
    if (dir) snprintf(g_data_dir, sizeof g_data_dir, "%s", dir);
    else g_data_dir[0] = 0;
}

void *rsd_load_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    void *d = malloc((size_t)n + 1);
    if (!d) { fclose(f); return NULL; }
    if (fread(d, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(d); return NULL; }
    ((char *)d)[n] = 0;
    fclose(f);
    if (size) *size = (size_t)n;
    return d;
}

int rsd_save_file(const char *path, const void *data, size_t size)
{
    char tmp[600];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    size_t w = fwrite(data, 1, size, f);
    if (fclose(f) != 0 || w != size) { remove(tmp); return -1; }
    remove(path);
    return rename(tmp, path);
}

void *rsd_data_loader(const char *name, size_t *size)
{
    char p[1024];
    if (!g_data_dir[0] || strstr(name, "..")) return NULL;
    snprintf(p, sizeof p, "%s/%s", g_data_dir, name);
    return rsd_load_file(p, size);
}

int rsd_sram_load(const char *path)
{
    size_t n = 0;
    void *d = rsd_load_file(path, &n);
    if (!d) return -1;
    memcpy(rs_host_sram(), d, n < RS_SRAM_SIZE ? n : RS_SRAM_SIZE);
    free(d);
    return 0;
}

int rsd_sram_save(const char *path) { return rsd_save_file(path, rs_host_sram(), RS_SRAM_SIZE); }

int rsd_write_png(const char *path, const uint16_t *fb, int scale)
{
    if (scale < 1) scale = 1;
    int w = RS_SCREEN_W * scale, h = RS_SCREEN_H * scale;
    uint8_t *rgb = malloc((size_t)w * h * 3);
    if (!rgb) return -1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint16_t c = fb[(y / scale) * RS_SCREEN_W + x / scale];
            uint8_t *p = rgb + ((size_t)y * w + x) * 3;
            unsigned r = c >> 11, g = (c >> 5) & 63, b = c & 31;
            p[0] = (uint8_t)((r << 3) | (r >> 2));
            p[1] = (uint8_t)((g << 2) | (g >> 4));
            p[2] = (uint8_t)((b << 3) | (b >> 2));
        }
    int ok = stbi_write_png(path, w, h, 3, rgb, w * 3);
    free(rgb);
    return ok ? 0 : -1;
}

void rsd_option(const char *kv)
{
    char k[64];
    const char *eq = strchr(kv, '=');
    if (!eq) { rs_host_set_option(kv, "1"); return; }
    size_t n = (size_t)(eq - kv);
    if (n >= sizeof k) n = sizeof k - 1;
    memcpy(k, kv, n);
    k[n] = 0;
    rs_host_set_option(k, eq + 1);
}

uint16_t rsd_buttons(const char *s)
{
    static const struct { const char *n; uint16_t b; } names[] = {
        {"B", RS_BTN_B}, {"Y", RS_BTN_Y}, {"SELECT", RS_BTN_SELECT}, {"START", RS_BTN_START},
        {"UP", RS_BTN_UP}, {"DOWN", RS_BTN_DOWN}, {"LEFT", RS_BTN_LEFT}, {"RIGHT", RS_BTN_RIGHT},
        {"A", RS_BTN_A}, {"X", RS_BTN_X}, {"L", RS_BTN_L}, {"R", RS_BTN_R},
    };
    uint16_t m = 0;
    char buf[128];
    snprintf(buf, sizeof buf, "%s", s);
    for (char *t = strtok(buf, "+,|"); t; t = strtok(NULL, "+,|"))
        for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++)
            if (!strcmp(t, names[i].n)) m |= names[i].b;
    return m;
}
