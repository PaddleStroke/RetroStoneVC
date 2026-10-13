/*
 * RetroStone Board's store graphics: the games' own menu pictures (each game's thumb(), the menu card's picture),
 * drawn by the app's code at any size with their transparency, for make_store_art_board.py (the feature graphic).
 *
 *     board_thumbs <dir> <size> <id>...     writes <dir>/<id>.png (size x size, RGBA, straight alpha)
 *
 * Linked with the whole app like apps/boardgames/android/icon_art.c (shell.c's main renamed board_main), and its
 * game_register renamed too (-Dgame_register=shell_game_register on shell.c): the games register here instead, so
 * their pictures can be asked for by id. Built by make_store_art_board.py. MIT licence, (c) 2026 Pierre-Louis Boyer
 * (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "shell.h"
#include "text.h"

static const game *reg[256];
static int nreg;

void game_register(const game *g)
{
    if (nreg < (int)(sizeof reg / sizeof reg[0])) reg[nreg++] = g;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: board_thumbs <dir> <size> <id>...\n");
        return 2;
    }
    int size = atoi(argv[2]), err = 0;
    uint8_t *b = malloc((size_t)size * size * 4);
    if (!b || size < 16) return 1;
    /* as the picture mode starts (shell.c, shot()): the texts (some pictures write digits) and a software renderer */
    ui.w = 720;
    ui.h = 1280;
    text_init("en");
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, ui.w, ui.h, 32, SDL_PIXELFORMAT_RGBA32);
    ui.r = surf ? SDL_CreateSoftwareRenderer(surf) : NULL;
    if (!ui.r) {
        fprintf(stderr, "board_thumbs: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderDrawBlendMode(ui.r, SDL_BLENDMODE_BLEND);
    for (int a = 3; a < argc; a++) {
        const game *g = NULL;
        for (int i = 0; i < nreg; i++)
            if (!strcmp(reg[i]->id, argv[a])) g = reg[i];
        if (!g || !g->thumb) {
            fprintf(stderr, "board_thumbs: no picture for %s\n", argv[a]);
            err = 1;
            continue;
        }
        memset(b, 0, (size_t)size * size * 4);
        g->thumb(-1, size, b);
        char path[1024];
        snprintf(path, sizeof path, "%s/%s.png", argv[1], argv[a]);
        if (!stbi_write_png(path, size, size, 4, b, size * 4)) {
            fprintf(stderr, "board_thumbs: cannot write %s\n", path);
            err = 1;
        }
    }
    free(b);
    return err;
}
