/*
 * The house kit's screens in a zoomed tall screen's visible columns (docs/spec.md "Tall screen"; a phone held upright).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A zoomed game keeps ONE look in portrait: the title, the panels and the pause stay in the zoomed view of the play
 * (rs_screen_fill(1), never the full-width letterboxed picture). The kit's layouts are 40 columns wide; these helpers
 * lay the same things out inside the columns the phone shows (rs_screen_visible_x0 .. x1): the title's lines wrapped
 * and stacked, the slots closer, the banners and panels narrower, the game-over values under their labels.
 * Header-only (static inline), built on the kit's public calls: a game includes it beside house_ui.h. Every helper
 * draws in the kit's UI coordinates (tiles 0..39 x 0..29, the UI layer scrolled by -hu_ui_x()), like the kit.
 *
 * ht_narrow() says when to use them: a tall screen whose visible columns are fewer than the kit's 320 (then the game
 * keeps rs_screen_fill(1) and calls ht_*), else the kit's own calls, unchanged (landscape, 4:3: the same pixels).
 * Call them after rs_screen_fill(1) and rs_screen_focus_x() of the frame (the visible columns follow the focus).
 * Nothing here is saved in states (no statics): the helpers draw from their arguments.
 */
#ifndef HOUSE_TALL_H
#define HOUSE_TALL_H

#include <stdio.h>
#include <string.h>
#include "house_ui.h"
#include "rs_i18n.h"

/* the visible columns, in UI pixels [x0, x1) and whole UI tiles [c0, c1); 4 px in from each side (the phone may show a
 * few columns fewer than rs_screen_visible_x0 .. x1 says: it scales the height and crops the rest) */
static inline int ht_fdiv8(int v) { return v >= 0 ? v / 8 : -((7 - v) / 8); }
static inline void ht_px(int *x0, int *x1)
{
    int o = rs_screen_origin_x() + hu_ui_x();
    if (x0) *x0 = rs_screen_visible_x0() - o + 4;
    if (x1) *x1 = rs_screen_visible_x1() - o - 4;
}
static inline void ht_cols(int *c0, int *c1)
{
    int x0, x1;
    ht_px(&x0, &x1);
    int a = ht_fdiv8(x0 + 7), b = ht_fdiv8(x1);
    if (a < 0) a = 0;
    if (b > 40) b = 40;
    if (c0) *c0 = a;
    if (c1) *c1 = b;
}
static inline int ht_ncols(void) { int a, b; ht_cols(&a, &b); return b - a; }
/* a zoomed tall screen showing fewer columns than the kit's layouts need */
static inline int ht_narrow(void)
{
    return rs_screen_height() > RS_SCREEN_H && rs_screen_visible_x1() - rs_screen_visible_x0() < RS_SCREEN_W;
}
/* the column that centres w columns in the visible ones; the UI x of their centre */
static inline int ht_center_w(int w) { int a, b; ht_cols(&a, &b); return a + (b - a - w) / 2; }
static inline int ht_center(const char *s, int big) { return ht_center_w(hu_width(s, big)); }
static inline int ht_mid_x(void) { int a, b; ht_px(&a, &b); return (a + b) / 2; }

/* ---- wrapped text ------------------------------------------------------------------------------------------------ */
#define HT_LINE 128
#define HT_MAXL 4
/* splits s into lines of at most maxw columns (at spaces; CJK between any two characters); returns the count */
static inline int ht_wrap(const char *s, int maxw, char out[][HT_LINE], int maxl)
{
    int n = 0;
    while (*s == ' ') s++;
    while (*s && n < maxl) {
        char buf[HT_LINE];
        const char *p = s, *brk = NULL;
        int len = 0;
        while (*p) {                                        /* as many characters as fit */
            const char *q = p;
            uint32_t cp = rs_utf8_next(&q);
            int clen = (int)(q - p), wide = rs_glyph_wide(cp);
            if (len + clen >= HT_LINE) break;
            memcpy(buf + len, p, (size_t)clen);
            buf[len + clen] = 0;
            if (len > 0 && hu_width(buf, 0) > maxw) break;
            if (cp == ' ' || (wide && len > 0)) brk = p;    /* a break before a space or a CJK glyph ... */
            len += clen;
            p = q;
            if (wide) brk = p;                              /* ... and after a CJK glyph */
        }
        if (*p && *p != ' ' && brk && brk > s) p = brk;     /* inside a word: back to the last break */
        len = (int)(p - s);
        while (len > 0 && s[len - 1] == ' ') len--;
        memcpy(out[n], s, (size_t)len);
        out[n][len] = 0;
        n++;
        s = p;
        while (*s == ' ') s++;
    }
    return n;
}
/* the rows a line of s takes: 2 when its accents or CJK glyphs take the row above */
static inline int ht_line_rows(const char *s) { return hu_tall(s) ? 2 : 1; }
/* s centred in the visible columns from row `row` down, wrapped; box: on a panel (hu_box_text); returns the last
 * row used */
static inline int ht_text(int row, const char *s, int box)
{
    char l[HT_MAXL][HT_LINE];
    int n = ht_wrap(s, ht_ncols() - (box ? 2 : 0), l, HT_MAXL), r = row;
    for (int i = 0; i < n; i++) {
        if (i && hu_tall(l[i])) r++;
        int x = ht_center(l[i], 0);
        if (box) hu_box_text(x, r, l[i]);
        else hu_text(x, r, l[i]);
        if (i < n - 1) r++;
    }
    return r;
}
/* the rows ht_text would take (1 + the extra lines) */
static inline int ht_text_rows(const char *s, int box)
{
    char l[HT_MAXL][HT_LINE];
    int n = ht_wrap(s, ht_ncols() - (box ? 2 : 0), l, HT_MAXL), r = 1;
    for (int i = 1; i < n; i++) r += ht_line_rows(l[i]);
    return r;
}
/* a 2x line centred in the visible columns (1x when 2x does not fit) */
static inline void ht_big(int row, const char *s, int style)
{
    if (hu_width(s, 1) <= ht_ncols()) hu_big(ht_center(s, 1), row, s, style);
    else ht_text(row + 1, s, 0);
}

/* ---- banners and panels -------------------------------------------------------------------------------------- */
/* hu_banner in the visible columns: as wide as the title + 2 (at least 20 when they fit); a title too wide for 2x
 * glyphs on a panel: 1x on a 3-row panel */
static inline void ht_banner(int y, const char *title)
{
    int cols = ht_ncols(), w = hu_width(title, 1) + 2, acc = hu_tall(title);
    if (w <= cols) {
        if (w < 20) w = 20 < cols ? 20 : cols;
        hu_panel(ht_center_w(w), y - acc, w, 4 + acc);
        hu_big(ht_center(title, 1), y + 1, title, HU_BIG_BANNER);
        return;
    }
    if (w - 2 <= cols) { hu_big(ht_center(title, 1), y + 1, title, HU_BIG_FREE); return; }   /* 2x over the scene */
    w = hu_width(title, 0) + 2;
    if (w > cols) w = cols;
    hu_panel(ht_center_w(w), y - acc, w, 3 + acc);
    hu_box_text(ht_center(title, 0), y + 1, title);
}

/* the game-over panel for 1 player (the banner on row 4 above it, ht_banner): the labels SCORE, BEST (NEW BEST),
 * MEDAL each over its value (ht_gameover_pos), then `note` if not NULL (hu_gameover_note's line), wrapped; the
 * panel from row 9; returns the retry line's row (ht_retry_line_at) */
static inline int ht_over_w(int new_best, const char *note)
{
    int l = hu_width(hu_str(HS_SCORE), 0), cols = ht_ncols();
    if (hu_width(hu_str(new_best ? HS_NEW_BEST : HS_BEST), 0) > l) l = hu_width(hu_str(new_best ? HS_NEW_BEST : HS_BEST), 0);
    if (hu_width(hu_str(HS_MEDAL), 0) > l) l = hu_width(hu_str(HS_MEDAL), 0);
    if (note && hu_width(note, 0) > l) l = hu_width(note, 0);
    int w = l + 4 > 16 ? l + 4 : 16;
    return w > cols ? cols : w;
}
/* the labels' rows: 4 apart (5, from row 11, when a label's accents or CJK glyphs take the row above) */
static inline int ht_over_tall(void)
{
    return hu_tall(hu_str(HS_SCORE)) || hu_tall(hu_str(HS_BEST)) || hu_tall(hu_str(HS_NEW_BEST)) || hu_tall(hu_str(HS_MEDAL));
}
static inline int ht_over_label_row(int i) { return ht_over_tall() ? 11 + 5 * i : 10 + 4 * i; }
static inline int ht_gameover_retry_row(const char *note)
{
    int r = ht_over_label_row(2) + 5;
    return note ? r + 1 + ht_text_rows(note, 1) : r;
}
static inline int ht_gameover_panel(int new_best, const char *note, const char *retry)
{
    int w = ht_over_w(new_best, note), x = ht_center_w(w), r = ht_gameover_retry_row(note);
    int bottom = r + (retry ? ht_text_rows(retry, 1) : 1) + 1;
    hu_panel(x, 9, w, bottom - 9 + 1);
    const char *lab[3] = {hu_str(HS_SCORE), hu_str(new_best ? HS_NEW_BEST : HS_BEST), hu_str(HS_MEDAL)};
    for (int i = 0; i < 3; i++) hu_box_text(ht_center(lab[i], 0), ht_over_label_row(i), lab[i]);
    if (note) ht_text(ht_over_label_row(2) + 5, note, 1);
    return r;
}
/* the value of label i (0 score, 1 best, 2 medal): the digits' centre x and top y (UI pixels, before the slide) */
static inline void ht_gameover_pos(int i, int *cx, int *y)
{
    int r = ht_over_label_row(i);
    if (cx) *cx = ht_mid_x();
    if (y) *y = i < 2 ? (r + 2) * 8 - 4 : (r + 1) * 8;
}
static inline void ht_gameover_sprites(int score, int best, int medal, int t, int slide_px)
{
    int cx, y;
    ht_gameover_pos(0, &cx, &y);
    hu_number(score, cx, y + slide_px, 3);
    ht_gameover_pos(1, &cx, &y);
    hu_number(best, cx, y + slide_px, 3);
    ht_gameover_pos(2, &cx, &y);
    if (medal) {
        hu_medal(medal, cx - 12, y + slide_px, 3);
        if ((t / 20) % 3 == 0) hu_sparkle(cx + 14, y + slide_px, (t / 10) % 2, 3);
    } else {
        hu_box_text((cx - 4) / 8, y / 8 + 1, "-");
    }
}
/* the blinking retry line (hu_retry_line_at) centred in the visible columns, wrapped on the panel */
static inline void ht_retry_line_at(int row, int t, int lock, const char *text)
{
    if (t < lock) return;
    if (hu_blink(t)) { ht_text(row, text, 1); return; }
    char l[HT_MAXL][HT_LINE], blank[HT_LINE];
    int n = ht_wrap(text, ht_ncols() - 2, l, HT_MAXL), r = row;
    for (int i = 0; i < n; i++) {
        if (i && hu_tall(l[i])) r++;
        int k = hu_width(l[i], 0);
        if (k >= HT_LINE) k = HT_LINE - 1;
        memset(blank, ' ', (size_t)k);
        blank[k] = 0;
        hu_box_text(ht_center(l[i], 0), r, blank);
        r++;
    }
}

/* the ranking for 2-4 players (hu_results_*): the banner on row 3, the panel from row 8, one row per place 3 rows
 * apart: the rank (when it fits) and the tag, the hero icon, the value, the medal */
static inline int ht_results_w(void) { int c = ht_ncols(); return c < 24 ? c : 24; }
static inline int ht_results_rank(void) { return ht_results_w() >= 21; }
static inline void ht_results_panel(const hu_standing *s, const char *title)
{
    char t[96];
    if (!title) {
        int w = hu_winner(s);
        if (w >= 0) rs_strf(t, sizeof t, hu_str(HS_WINS), w + 1);
        else snprintf(t, sizeof t, "%s", hu_str(HS_DRAW));
        title = t;
    }
    ht_banner(3, title);
    int w = ht_results_w(), x = ht_center_w(w), rk = ht_results_rank();
    hu_panel(x, 8, w, 3 * s->n + 4);
    for (int i = 0; i < s->n; i++) {
        char tag[16];
        int row = hu_results_row(s, i);
        if (rk) hu_box_text(x + 1, row, hu_str(HS_RANK_1 + (s->rank[i] & 3)));
        rs_strf(tag, sizeof tag, hu_str(HS_PLAYER_TAG), (s->order[i] & 3) + 1);
        hu_box_text(x + (rk ? 5 : 1), row, tag);
    }
}
/* place i's icon centre, value centre x, medal x (UI pixels) */
static inline void ht_results_cols(int *icon_cx, int *value_cx, int *medal_x)
{
    int w = ht_results_w(), x = ht_center_w(w) * 8, rk = ht_results_rank();
    int ic = x + (rk ? 8 : 4) * 8 + 4, mx = x + w * 8 - 8 - 26;
    if (icon_cx) *icon_cx = ic;
    if (medal_x) *medal_x = mx;
    if (value_cx) *value_cx = (ic + 8 + mx) / 2 + 2;
}
static inline void ht_results_icon_pos(const hu_standing *s, int place, int *cx, int *cy)
{
    ht_results_cols(cx, NULL, NULL);
    if (cy) *cy = hu_results_row(s, place) * 8 + 4;
}
static inline void ht_results_sprites(const hu_standing *s, int t, int slide_px)
{
    int vx, mx;
    ht_results_cols(NULL, &vx, &mx);
    for (int i = 0; i < s->n; i++) {
        int y = hu_results_row(s, i) * 8 + slide_px, m = hu_results_medal(s, i);
        hu_number(s->value[s->order[i]], vx, y - 4, 3);
        if (m) {
            hu_medal(m, mx, y - 8, 3);
            if (s->rank[i] == 0 && (t / 20) % 3 == 0) hu_sparkle(mx + 18, y - 8, (t / 10) % 2, 3);
        }
    }
}

/* ---- the title ----------------------------------------------------------------------------------------------------- */
/* The title's logo on two lines (one word each, the kit's logo font at 4x), the first at row ty, the second 6 rows
 * lower: the second word's tiles follow the first's (the kit's logo area, HU_LOGO_TILES in all). ramps as hu_logo
 * (one per word). A one-word title: one line. Returns the rows the logo takes (its glyphs: ty + 1 .. ty + 5 or 11). */
static inline int ht_logo(const char *title, const rs_color (*ramps)[3], int nramps, int ty, int rivets)
{
    char w1[64];
    const char *sp = strchr(title, ' ');
    int len = (int)strlen(title), lw = len * 22 - 2 <= 304 ? len * 22 - 2 : len * 17 - 2;   /* hu_logo: 4x or 3x */
    if (!sp || lw <= ht_ncols() * 8) { hu_logo(title, ramps, nramps, ty, rivets); return 6; }
    size_t n = (size_t)(sp - title);
    if (n >= sizeof w1) n = sizeof w1 - 1;
    memcpy(w1, title, n);
    w1[n] = 0;
    hu_config *c = (hu_config *)hu_cfg();               /* the second word after the first's tiles */
    int base = c->logo_tile;
    int used = hu_logo(w1, ramps, nramps, ty, rivets);
    c->logo_tile = base + used;
    hu_logo(sp + 1, nramps > 1 ? ramps + 1 : ramps, nramps > 1 ? nramps - 1 : 1, ty + 6, 0);
    c->logo_tile = base;
    return 12;
}

/* What the title shows below the logo (hu_title_draw), laid out from row `top` down to row 29 in the visible
 * columns: the slots (2-4 players, their icons closer), the prompt PRESS A TO <VERB> (the A glyph left of it, the
 * line wrapped), BEST n, the join line (wrapped), the credits on two lines. Rows: slots on `top`, then the rest
 * spread over the rows left, bottom-aligned on row 29. */
typedef struct ht_title {
    const char *verb;           /* the hu_title_cfg verb (NULL: PLAY) */
    int max_players;            /* the hu_title_cfg max_players */
    int top;                    /* the slots' row (or the first row when max_players is 1) */
    int prompt_row;             /* 0: placed with the rest; else the prompt's own row */
} ht_title;
typedef struct ht_title_out {
    int glyph_x, glyph_y;       /* the prompt's A glyph (UI pixels) */
    int slot_cx[HU_MAX_PLAYERS], slot_cy;   /* the slots' icon centres */
} ht_title_out;
static inline void ht_slots(const ht_title *c, ht_title_out *o)
{
    int x0, x1, m = c->max_players < 1 ? 1 : c->max_players;
    ht_px(&x0, &x1);
    int sp = (x1 - x0) / m;
    for (int p = 0; p < HU_MAX_PLAYERS; p++) o->slot_cx[p] = x0 + sp * (2 * (p % m) + 1) / 2 + (sp >= 44 ? 10 : 0);
    o->slot_cy = c->top * 8 + 4;
}
static inline void ht_title_draw(const ht_title *c, int t, int best, ht_title_out *o)
{
    char s[160], prompt[96], who[64];
    int m = c->max_players, n = hu_players(), top = c->top;
    memset(o, 0, sizeof *o);
    ht_slots(c, o);
    rs_strf(prompt, sizeof prompt, hu_str(HS_PRESS_TO), c->verb ? c->verb : hu_str(HS_PLAY));
    hu_clear_rows(top, 30 - top);
    int x0, x1;
    ht_px(&x0, &x1);
    if (m > 1 && (x1 - x0) / m >= 44)                   /* the slot tags, when there is room beside the icons */
        for (int p = 0; p < n; p++) {
            rs_strf(s, sizeof s, hu_str(HS_PLAYER_TAG), p + 1);
            hu_text(ht_fdiv8(o->slot_cx[p] - 28), top, s);
        }
    /* the rows from the bottom: the credits (2), the join line, BEST, the prompt */
    const char *cr1 = "(C) 2026 8BCRAFT", *cr2 = "RETROSTONE VC";
    int r = 29;
    hu_text(ht_center(cr2, 0), r, cr2);
    hu_text(ht_center(cr1, 0), r - 1, cr1);
    r -= 3;
    if (m > 1) {
        if (n < m) {
            who[0] = 0;
            for (int p = n; p < m; p++) {
                size_t k = strlen(who);
                if (p > n) k += (size_t)snprintf(who + k, sizeof who - k, " / ");
                if (k < sizeof who) rs_strf(who + k, sizeof who - k, hu_str(HS_PLAYER_TAG), p + 1);
            }
            rs_strf(s, sizeof s, hu_str(n > 1 ? HS_JOIN_LEAVE : HS_JOIN), who);
        } else {
            rs_strf(s, sizeof s, hu_strn(HS_FULL, n), n);
        }
        int k = ht_text_rows(s, 0);
        ht_text(r - k + 1, s, 0);
        r -= k + (hu_tall(s) ? 2 : 1);
    }
    if (best > 0) {
        rs_strf(s, sizeof s, hu_str(HS_BEST_N), best);
        hu_text(ht_center(s, 0), r, s);
        r -= 1 + (hu_tall(s) ? 2 : 1);
    }
    /* the prompt: the glyph (16 px) and a space left of the text */
    char l[HT_MAXL][HT_LINE];
    int cols = ht_ncols(), nl = ht_wrap(prompt, cols - 3, l, HT_MAXL), k = 1;
    for (int i = 1; i < nl; i++) k += ht_line_rows(l[i]);
    int pr = c->prompt_row ? c->prompt_row : r - k + 1;
    if (pr <= top) pr = top + 1;
    int pw = hu_width(l[0], 0) + 2, px = ht_center_w(pw);
    o->glyph_x = px * 8 - 4;                            /* as the kit: the glyph's right edge 4 px left of the text */
    o->glyph_y = pr * 8 - 4;
    if (hu_blink(t)) {
        int rr = pr;
        for (int i = 0; i < nl; i++) {
            if (i && hu_tall(l[i])) rr++;
            hu_text(i ? ht_center(l[i], 0) : px + 2, rr, l[i]);
            rr++;
        }
    }
}
/* the title's sprites (hu_title_sprites): the A glyph, each joined player's icon at its slot, popping in */
static inline void ht_title_sprites(const ht_title *c, const ht_title_out *o, int t, hu_icon_fn icon, void *user)
{
    int gx, gy, g = hu_title_glyph(&gx, &gy);
    if (g >= 0) hu_glyph(g, o->glyph_x, o->glyph_y, (t / 30) % 2, 3);
    if (c->max_players < 2) return;
    for (int p = 0; p < hu_players(); p++) {
        int cx = o->slot_cx[p], cy = o->slot_cy, a = hu_player_age(p);
        if (a < 24) {
            hu_sparkle(cx + 8, cy - 14, (a / 6) & 1, 3);
            hu_sparkle(cx - 14, cy - 2, (a / 6 + 1) & 1, 3);
        }
        if (icon) icon(p, cx, cy + (a < 16 ? 12 - hu_ease_back(a, 16, 12) : 0), t, user);
    }
}

#endif
