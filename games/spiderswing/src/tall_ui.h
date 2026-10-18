/*
 * A phone held upright (docs/spec.md "Tall screen"): the house kit's title and panels laid out in the columns a zoomed
 * screen shows, so the title, the panels and play keep ONE view (rs_screen_fill(1) throughout, no blurred bands).
 * The kit (games/common) lays them out over the 40 columns of the 4:3 stage; these calls narrow them: a logo of two
 * words on two lines, the title's lines wrapped, the players' slots closer, a banner on two lines, a narrower results
 * panel. On a screen that shows every column (landscape, 4:3) the games call the kit as before: the same pixels.
 * The same header is in several games (until the kit has it); static inline, UI coordinates (the kit's: tiles
 * 0..39 x 0..29, the stage centred), the columns shown centred on the stage (rs_screen_focus_x(-1)).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef TALL_UI_H
#define TALL_UI_H

#include <stdio.h>
#include <string.h>
#include "rs.h"
#include "house_ui.h"

/* a zoomed tall screen: not every column shown */
static inline int tu_on(void)
{
    return rs_screen_height() > RS_SCREEN_H && rs_screen_visible_x1() - rs_screen_visible_x0() < rs_screen_width();
}

/* the UI tile columns wholly shown: c0 .. c1 - 1 (all 40 when every column shows); the UI centred on the columns
 * shown (the stage's middle shown, or the UI layer scrolled there: tu_oam_shift for the kit's sprites) */
static inline void tu_cols(int *c0, int *c1)
{
    int v = rs_screen_visible_x1() - rs_screen_visible_x0(), x0 = RS_SCREEN_W / 2 - v / 2, x1 = x0 + v;
    int a = (x0 + 7) >> 3, b = x1 >> 3;
    if (!tu_on() || a < 0) a = 0;
    if (!tu_on() || b > 40) b = 40;
    if (c0) *c0 = a;
    if (c1) *c1 = b;
}

/* the widest centred line (columns) that stays in the columns shown */
static inline int tu_room(void)
{
    int c0, c1;
    tu_cols(&c0, &c1);
    int l = 20 - c0, r = c1 - 20;
    return 2 * (l < r ? l : r);
}

/* the sprites drawn since OAM slot first moved dx pixels across (the UI scrolled onto the columns shown) */
static inline void tu_oam_shift(int first, int dx)
{
    for (int i = first; dx && i < rs_oam_next(); i++) rs_oam(i)->x = (int16_t)(rs_oam(i)->x + dx);
}

#define TU_LINE 128
/* s cut into lines of at most w columns (2x text: big), at spaces (CJK: between any two characters), at " - " first
 * (the dash dropped: "A: RELEASE - L/R: STEER" on two lines); a word longer than w is cut. Returns the lines. */
static inline int tu_wrap(const char *s, int w, int big, char out[][TU_LINE], int max)
{
    int n = 0;
    const char *p = s;
    while (*p && n < max) {
        while (*p == ' ') p++;
        if (!*p) break;
        const char *q = p, *brk = NULL, *dash = NULL;
        int cw = 0;
        while (*q) {
            const char *nx = q;
            uint32_t cp = rs_utf8_next(&nx);
            char ch[8];
            int len = (int)(nx - q);
            if (len < 1 || len > 6) len = 1;
            memcpy(ch, q, (size_t)len);
            ch[len] = 0;
            int gw = hu_width(ch, big);
            if (cp == ' ') brk = q;
            if (cp == ' ' && q[1] == '-' && q[2] == ' ' && q > p) dash = q;
            if (cw + gw > w) break;
            cw += gw;
            q += len;
            if (cp >= 128 && rs_glyph_wide(cp)) brk = q;
        }
        const char *end = !*q ? q : dash ? dash : brk && brk > p ? brk : q > p ? q : q + 1;
        int len = (int)(end - p);
        if (len >= TU_LINE) len = TU_LINE - 1;
        memcpy(out[n], p, (size_t)len);
        while (len > 0 && out[n][len - 1] == ' ') len--;
        out[n][len] = 0;
        n++;
        p = end == dash ? end + 3 : end;
    }
    return n;
}

/* s centred on row y, wrapped in w columns (0: the room shown), in the panel font (box) or over the scene; the rows
 * it takes (a line with accents or CJK takes the row above it too). blank: only clears its rows (a blink). */
static inline int tu_text(int y, const char *s, int w, int box, int blank)
{
    char l[4][TU_LINE];
    if (w <= 0) w = tu_room();
    int n = tu_wrap(s, w, 0, l, 4), row = y;
    for (int i = 0; i < n; i++) {
        if (i > 0) row += hu_tall(l[i]) ? 2 : 1;
        if (blank) {
            if (box) {
                char b[TU_LINE];
                int k = hu_width(l[i], 0);
                memset(b, ' ', (size_t)k);
                b[k] = 0;
                hu_box_text(hu_center(l[i], 0), row, b);
            } else hu_clear_rows(row, 1);
        } else if (box) hu_box_text(hu_center(l[i], 0), row, l[i]);
        else hu_text(hu_center(l[i], 0), row, l[i]);
    }
    return row - y + 1;
}

/* the title logo: on one line when it fits the columns shown, else a word a line (the second 6 rows lower, in the
 * second ramp). The kit draws one line: the second word's tiles go after the first word's (its config's logo tile
 * moved for that call, then put back). Returns the rows the logo's letters end on (ty + 5 or ty + 11). */
static inline int tu_logo(const char *title, const rs_color (*ramps)[3], int nramps, int ty, int rivets)
{
    int len = (int)strlen(title), sc = len * 22 - 2 <= 304 ? 4 : 3, px = len * (5 * sc + 2) - 2;
    const char *sp = strchr(title, ' ');
    if (!tu_on() || !sp || px <= tu_room() * 8 - 8) {
        hu_logo(title, ramps, nramps, ty, rivets);
        return ty + 5;
    }
    char a[48], b[48];
    int k = (int)(sp - title);
    if (k > 40) k = 40;
    memcpy(a, title, (size_t)k);
    a[k] = 0;
    snprintf(b, sizeof b, " %s ", sp + 1);              /* a space each side: the second word, centred, in ramp 2 */
    int used = hu_logo(a, ramps, nramps, ty, rivets);
    hu_config *c = (hu_config *)hu_cfg();
    int keep = c->logo_tile;
    c->logo_tile += used;
    hu_logo(b, ramps, nramps, ty + 6, 0);
    c->logo_tile = keep;
    return ty + 11;
}

/* a banner (the kit's hu_banner) whose title is too wide for the columns shown: its words on two lines of 2x text in
 * a taller panel ending where the one-line banner would (rows y - 3 .. y + 3), or the small font when a word alone
 * is too wide */
static inline void tu_banner(int y, const char *title)
{
    int room = tu_room();
    if (!tu_on() || hu_width(title, 1) + 2 <= room) {
        hu_banner(y, title);
        return;
    }
    char l[3][TU_LINE];
    int n = tu_wrap(title, room - 2, 1, l, 3), w = 0, ok = n == 2, pos = 0;
    for (int i = 0; i < n; i++) {
        if (hu_width(l[i], 1) > room - 2) ok = 0;
        while (title[pos] == ' ') pos++;          /* a word cut (no space, no CJK at the cut): the small font */
        pos += (int)strlen(l[i]);
        const char *c = title + pos;
        uint32_t cp = rs_utf8_next(&c);
        if (i < n - 1 && title[pos] != ' ' && !(cp >= 128 && rs_glyph_wide(cp))) ok = 0;
        if (hu_width(l[i], 1) + 2 > w) w = hu_width(l[i], 1) + 2;
    }
    if (ok) {
        int acc = hu_tall(l[0]), top = y - 3 - acc;
        if (top < 0) top = 0;
        if (w < 20 && room >= 20) w = 20;
        hu_panel((40 - w) / 2, top, w, 7 + acc);
        hu_big(hu_center(l[0], 1), top + acc + 1, l[0], HU_BIG_BANNER);
        hu_big(hu_center(l[1], 1), top + acc + 4, l[1], HU_BIG_BANNER);
        return;
    }
    n = tu_wrap(title, room - 2, 0, l, 2);
    hu_panel((40 - room) / 2, y, room, 4);
    tu_text(n > 1 ? y + 1 : y + 2, title, room - 2, 1, 0);
}

/* the pause line (the kit's hu_pause): 2x text, or the small font when the word is too wide for the columns shown */
static inline void tu_pause(int paused, int y)
{
    hu_pause(paused, y);
    if (paused && tu_on() && hu_width(hu_str(HS_PAUSED), 1) > tu_room()) {
        hu_clear_rows(y, 2);
        tu_text(y + 1, hu_str(HS_PAUSED), 0, 0, 0);
    }
}

/* ---- the title (hu_title_draw / hu_title_sprites in the columns shown) ----------------------------------------------
 * The rows: the players' slots (row slot_row), PRESS A TO <VERB> from row 21, BEST, the join line, the credits: each
 * line wrapped, the next one below it (the credits on their two lines at the bottom). */
typedef struct tu_title_t { int16_t glyph_x, glyph_y; } tu_title_t;
static tu_title_t tu_title_;

static inline void tu_slot_pos(int p, int maxp, int slot_row, int *cx, int *cy)
{
    int col = 20 - 5 * maxp / 2 + 5 * (p & 3);
    if (cx) *cx = col * 8 + 28;
    if (cy) *cy = slot_row * 8 + 4;
}

/* row: the prompt's first row (0: HU_ROW_PROMPT); each next line one free row (the accents' row) below the last */
static inline void tu_title_draw(int t, int best, const char *verb, int maxp, int slot_row, int row)
{
    char s[160], l[4][TU_LINE];
    int room = tu_room();
    if (!slot_row) slot_row = HU_ROW_SLOTS;
    if (!row) row = HU_ROW_PROMPT;
    int row0 = row;                                     /* the join line at least 4 rows below (the credits' 2 rows) */
    hu_clear_rows(slot_row, 1);
    for (int y = row - 1; y < 30; y++) hu_clear_rows(y, 1);
    if (maxp > 1)
        for (int p = 0; p < hu_players(); p++) {
            int cx;
            tu_slot_pos(p, maxp, slot_row, &cx, NULL);
            rs_strf(s, sizeof s, hu_str(HS_PLAYER_TAG), p + 1);
            hu_text((cx - 28) / 8, slot_row, s);
        }
    /* PRESS A TO <VERB>, the A glyph left of its first line */
    rs_strf(s, sizeof s, hu_str(HS_PRESS_TO), verb ? verb : hu_str(HS_PLAY));
    int n = tu_wrap(s, room - 2, 0, l, 2);
    for (int i = 0; i < n; i++) {
        if (i > 0) row += hu_tall(l[i]) ? 2 : 1;
        int x = hu_center(l[i], 0) + 1;
        if (i == 0) {
            tu_title_.glyph_x = (int16_t)(x * 8 - 20);
            tu_title_.glyph_y = (int16_t)(row * 8 - 4);
        }
        if (hu_blink(t)) hu_text(x, row, l[i]);
    }
    row += 2;
    if (best > 0) {
        rs_strf(s, sizeof s, hu_str(HS_BEST_N), best);
        row += tu_text(row, s, 0, 0, 0) + 1;
    }
    if (maxp > 1) {
        if (hu_players() < maxp) {
            char who[64] = "";
            for (int p = hu_players(); p < maxp; p++) {
                size_t k = strlen(who);
                if (p > hu_players()) k += (size_t)snprintf(who + k, sizeof who - k, " / ");
                if (k < sizeof who) rs_strf(who + k, sizeof who - k, hu_str(HS_PLAYER_TAG), p + 1);
            }
            rs_strf(s, sizeof s, hu_str(hu_players() > 1 ? HS_JOIN_LEAVE : HS_JOIN), who);
        } else {
            rs_strf(s, sizeof s, hu_strn(HS_FULL, hu_players()), hu_players());
        }
        if (row < row0 + 4) row = row0 + 4;
        row += tu_text(row, s, 0, 0, 0) + 1;
    }
    /* the credits: on two lines at the bottom, one when the lines above take the room */
    const char *c1 = "(C) 2026 8BCRAFT", *c2 = row <= 28 ? "RETROSTONE VC" : c1;
    if (row <= 28) hu_text(hu_center(c1, 0), 28, c1);
    if (row <= 29) hu_text(hu_center(c2, 0), 29, c2);
}

/* the A glyph and the joined players' icons, popping in (hu_title_sprites' look) */
static inline void tu_title_sprites(int t, int maxp, int slot_row, hu_icon_fn icon, void *user)
{
    int glyph = hu_title_glyph(NULL, NULL);
    if (!slot_row) slot_row = HU_ROW_SLOTS;
    if (glyph >= 0) hu_glyph(glyph, tu_title_.glyph_x, tu_title_.glyph_y, (t / 30) % 2, 3);
    if (maxp < 2) return;
    for (int p = 0; p < hu_players(); p++) {
        int cx, cy, a = hu_player_age(p);
        tu_slot_pos(p, maxp, slot_row, &cx, &cy);
        if (a < 24) {
            hu_sparkle(cx + 8, cy - 14, (a / 6) & 1, 3);
            hu_sparkle(cx - 14, cy - 2, (a / 6 + 1) & 1, 3);
        }
        if (icon) icon(p, cx, cy + (a < 16 ? 12 - hu_ease_back(a, 16, 12) : 0), t, user);
    }
}

/* ---- the results (hu_results_panel): the panel 22 columns wide (9 .. 30), the same rows and sprites -------------- */
static inline void tu_results_panel(const hu_standing *s, const char *title)
{
    if (!tu_on()) {
        hu_results_panel(s, title);
        return;
    }
    char t[96];
    if (!title) {
        int w = hu_winner(s);
        if (w >= 0) rs_strf(t, sizeof t, hu_str(HS_WINS), w + 1);
        else snprintf(t, sizeof t, "%s", hu_str(HS_DRAW));
        title = t;
    }
    tu_banner(3, title);
    hu_panel(9, 8, 22, 3 * s->n + 4);
    for (int i = 0; i < s->n; i++) {
        char tag[16];
        int row = hu_results_row(s, i);
        hu_box_text(10, row, hu_str(HS_RANK_1 + (s->rank[i] & 3)));
        rs_strf(tag, sizeof tag, hu_str(HS_PLAYER_TAG), (s->order[i] & 3) + 1);
        hu_box_text(14, row, tag);
    }
}

#endif
