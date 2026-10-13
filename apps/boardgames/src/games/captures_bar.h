/*
 * The pieces a player has taken, in their bar (Banqi, Makruk): small pieces in a row, kind by kind, closer together
 * the more there are; when one row would hide them under each other, two rows of smaller pieces. Every piece shows.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef CAPTURES_BAR_H
#define CAPTURES_BAR_H

#include <math.h>

#include "shell.h"

#define CBAR_KINDS 8

/* the pieces in the bar's box b, up to x = end: kinds kinds in order, n[k] of the picture pic[k]; step: the pitch
 * between two of a kind, in sizes (as close as they lie when there is room); gap: more between two kinds (units);
 * least: the closest pitch one row may take (in sizes) before two rows are used */
static inline void cbar_draw(box b, float end, SDL_Texture *const *pic, const int *n, int kinds, float step, float gap,
                             float least)
{
    float u = ui.unit, cy = b.y + b.h / 2;
    int total = 0, list[64], m = 0;
    for (int k = 0; k < kinds && k < CBAR_KINDS; k++)
        for (int i = 0; i < n[k] && m < 64; i++) list[m++] = k;
    total = m;
    if (!total) return;
    for (int rows = 1; rows <= 2; rows++) {
        float sz = rows == 1 ? 0.6f * u : fminf(0.6f * u, 0.49f * b.h), g = gap * u * sz / (0.6f * u);
        int per = (total + rows - 1) / rows;
        float pitch[2];
        int ok = 1;
        for (int r = 0; r < rows; r++) {        /* each row's pitch: as close as its pieces need */
            int a = r * per, z = a + per < total ? a + per : total, groups = 0;
            for (int i = a; i < z; i++) groups += i == a || list[i] != list[i - 1];
            pitch[r] = z - a > 1 ? fminf(step * sz, (end - b.x - sz - (groups - 1) * g) / (z - a - 1)) : 0;
            if (z - a > 1 && pitch[r] < least * sz) ok = 0;
        }
        if (!ok && rows == 1) continue;
        for (int r = 0; r < rows; r++) {
            int a = r * per, z = a + per < total ? a + per : total;
            float x = b.x + sz / 2, y = cy + (r - (rows - 1) / 2.0f) * 0.5f * b.h;
            for (int i = a; i < z; i++) {
                if (i > a) x += pitch[r] + (list[i] != list[i - 1] ? g : 0);
                ui_put_c(pic[list[i]], x, y, sz, sz);
            }
        }
        return;
    }
}

#endif
