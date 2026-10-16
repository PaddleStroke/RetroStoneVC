/*
 * The Arcade tab's cards (arcade.h): their pictures, the tab's joystick, the shell's stand-in games. Without ARCADE
 * (every edition but the RetroStone app): no card, and arcade_play.c's functions stand in here.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "arcade.h"

#include <string.h>

int arcade_of(const game *g)
{
    for (int a = 0; a < arcade_ncards; a++)
        if (arcade_cards[a].card == g) return a;
    return -1;
}

/* the picture at a whole scale (nearest: crisp pixels), cut to the square from its middle, its corners rounded */
void arcade_thumb(int a, int size, uint8_t *o)
{
    if (a < 0 || a >= arcade_ncards || size < 1) return;
    const arcade_card *c = &arcade_cards[a];
    int k = (size + ARCADE_SIDE - 1) / ARCADE_SIDE, off = (k * ARCADE_SIDE - size) / 2;
    float r = size * 0.09f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            int sx = (x + off) / k, sy = (y + off) / k;
            int i = c->pix[sy * ARCADE_SIDE + sx];
            uint8_t *p = o + 4 * (y * size + x);
            const uint8_t *rgb = c->pal + 3 * (i < c->ncol ? i : 0);
            p[0] = rgb[0], p[1] = rgb[1], p[2] = rgb[2];
            /* the rounded corners, soft */
            float dx = x + 0.5f < r ? r - x - 0.5f : x + 0.5f > size - r ? x + 0.5f - (size - r) : 0;
            float dy = y + 0.5f < r ? r - y - 0.5f : y + 0.5f > size - r ? y + 0.5f - (size - r) : 0;
            float d = sqrtf(dx * dx + dy * dy) - r + 0.5f;
            p[3] = d <= 0 ? 255 : d >= 1 ? 0 : (uint8_t)(255 * (1 - d));
        }
}

/* a joystick: the ebony base, the ivory stick, its red lacquer ball, a red button beside it */
void arcade_icon(int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.8f, 0.42f, 0.13f);
    art_rect(f, ART_PART, 0.08f, 0.68f, 0.92f, 0.8f);
    art_ellipse(f, ART_PART, 0.5f, 0.68f, 0.42f, 0.13f);
    art_fig_blend(o, size, size, f, &art_player[1], 0, 0, size, 1);
    art_fig_free(f);
    f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.68f, 0.13f, 0.05f);
    art_band(f, ART_PART, 0.5f, 0.68f, 0.56f, 0.3f, 0.09f);
    art_fig_blend(o, size, size, f, &art_player[0], 0, 0, size, 1);
    art_fig_free(f);
    f = art_fig_new();
    art_ellipse(f, ART_PART, 0.565f, 0.25f, 0.17f, 0.17f);
    art_ellipse(f, ART_PART, 0.22f, 0.66f, 0.08f, 0.045f);
    art_fig_blend(o, size, size, f, &art_player[2], 0, 0, size, 1);
    art_fig_free(f);
}

/* the stand-in game: nothing to play in the shell itself */
void arcade_token(int player, int size, uint8_t *rgba) { (void)player, (void)size, (void)rgba; }
void arcade_start(int variant) { (void)variant; }
void arcade_layout(box board) { (void)board; }
void arcade_draw(void) {}
void arcade_xy(float x, float y) { (void)x, (void)y; }
int  arcade_zero(void) { return 0; }

#ifndef ARCADE                                  /* not the RetroStone app: no Arcade */
const arcade_card arcade_cards[1] = {{0}};
const int arcade_ncards = 0;
int arcade_play(const char *id, SDL_Renderer *r) { (void)id, (void)r; return 0; }
int arcade_shot(const char *id, SDL_Renderer *r, SDL_Surface *surf, const char *out, int argc, char **argv)
{
    (void)id, (void)r, (void)surf, (void)out, (void)argc, (void)argv;
    return 1;
}
#endif
