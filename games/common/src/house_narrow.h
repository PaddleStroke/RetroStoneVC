/*
 * The house UI kit in a narrow view: a zoomed tall screen (docs/spec.md "Tall screen", rs_screen_zoom) shows only the
 * middle columns of the 320-px UI area, so the title, the banners, the ranking and PAUSED are laid out again inside
 * them (stacked logo words, wrapped lines, narrower slots and ranking), the scene kept zoomed behind them.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Header-only (static inline), on top of house_ui.h's public calls: a game includes it where it draws its UI.
 * Every call takes w, the UI columns shown (hn_cols()), centred on UI column 20: the game centres its view on the
 * stage's middle (rs_screen_focus_x(-1)) while these show, so the kit's own sprites (digits, medals) land right.
 * With w >= 40 (every column: landscape, 4:3, a full-width screen) each call is exactly the kit's: the same pixels.
 * Deterministic and stateless (nothing to save in a state).
 */
#ifndef HOUSE_NARROW_H
#define HOUSE_NARROW_H

#include <stdio.h>
#include <string.h>
#include "rs.h"
#include "rs_i18n.h"
#include "house_ui.h"

/* the UI columns shown, an even count centred on column 20 (40: every column) */
static inline int hn_cols(void)
{
    int v = rs_screen_visible_x1() - rs_screen_visible_x0();
    return v >= RS_SCREEN_W ? 40 : 2 * (v / 16);
}

/* ---- wrapped text ----------------------------------------------------------------------------------------------------
 * s cut at its spaces into lines of at most w columns (at most maxl lines: the last one takes the rest), each centred
 * on column 20 from row `row` on; a line step of 2 rows when a line uses the row above (accents, CJK). Returns the rows
 * used (the last line's row - row + 1). draw 0: only measures. */
#define HN_LINES 4
static inline int hn_split(const char *s, int w, int maxl, char out[HN_LINES][160], int big)
{
    int n = 0;
    char buf[160];
    out[0][0] = 0;
    if (maxl > HN_LINES) maxl = HN_LINES;
    const char *p = s;
    while (*p) {
        const char *e = strchr(p, ' ');
        int len = e ? (int)(e - p) : (int)strlen(p);
        snprintf(buf, sizeof buf, "%s%s%.*s", out[n], out[n][0] ? " " : "", len, p);
        if (out[n][0] && hu_width(buf, big) > w && n < maxl - 1) {
            n++;
            snprintf(out[n], 160, "%.*s", len, p);
        } else {
            snprintf(out[n], 160, "%s", buf);
        }
        p = e ? e + 1 : p + len;
    }
    return out[0][0] ? n + 1 : 0;
}
/* the same, the lines balanced: the narrowest width that keeps their count */
static inline int hn_split_bal(const char *s, int w, int maxl, char out[HN_LINES][160], int big)
{
    int n = hn_split(s, w, HN_LINES, out, big), b = w;
    if (n < 2 || n > maxl) return hn_split(s, w, maxl, out, big);
    while (b > 1 && hn_split(s, b - 1, HN_LINES, out, big) == n) b--;
    return hn_split(s, b, maxl, out, big);
}
static inline int hn_step(char l[HN_LINES][160], int n)
{
    for (int i = 1; i < n; i++)
        if (hu_tall(l[i])) return 2;
    return 1;
}
static inline int hn_wrap(int row, const char *s, int box, int w, int maxl, int draw)
{
    char l[HN_LINES][160];
    int n = hn_split_bal(s, w, maxl, l, 0), st = hn_step(l, n);
    for (int i = 0; draw && i < n; i++) {
        if (box) hu_box_text(hu_center(l[i], 0), row + i * st, l[i]);
        else hu_text(hu_center(l[i], 0), row + i * st, l[i]);
    }
    return n ? (n - 1) * st + 1 : 0;
}

/* ---- the logo -------------------------------------------------------------------------------------------------------
 * hu_logo, then, when it is wider than the view, its words stacked (as many per line as fit), each line centred and
 * 5 rows below the one above: a two-line logo takes the rows ty .. ty + 12. Returns the lines. */
static inline int hn_logo(const char *title, const rs_color (*ramps)[3], int nramps, int ty, int rivets, int w)
{
    hu_logo(title, ramps, nramps, ty, rivets);
    int len = (int)strlen(title), sc = (len * 22 - 2 <= 304) ? 4 : 3, adv = 5 * sc + 2, total = len * adv - 2;
    int x0 = (320 - total) / 2;
    if (total + 8 <= w * 8) return 1;
    /* the words' tile columns: each cut in the middle of the space before the next word */
    int cut[12], nw = 0;
    cut[nw++] = 0;
    for (int i = 0; i < len && nw < 11; i++)
        if (title[i] == ' ') cut[nw++] = (x0 + i * adv + adv / 2) / 8;
    cut[nw] = 40;
    int layer = hu_cfg()->layer;
    uint16_t save[8][40], blank = rs_bg_get(layer, 0, ty);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 40; x++) {
            save[y][x] = rs_bg_get(layer, x, ty + y);
            rs_bg_put(layer, x, ty + y, blank);
        }
    /* the words' extents (their non-blank columns) and the lines */
    int a[12], b[12];
    for (int k = 0; k < nw; k++) {
        a[k] = cut[k + 1];
        b[k] = cut[k];
        for (int x = cut[k]; x < cut[k + 1]; x++)
            for (int y = 0; y < 8; y++)
                if (save[y][x] != blank) {
                    if (x < a[k]) a[k] = x;
                    if (x + 1 > b[k]) b[k] = x + 1;
                }
    }
    int line = 0, k = 0;
    while (k < nw) {
        int e = k + 1;
        while (e < nw && b[e] - a[k] <= w) e++;
        int dx = 20 - (a[k] + b[e - 1]) / 2;
        for (int y = 0; y < 8; y++) {
            int ry = ty + y + line * 5;
            if (ry >= 32) continue;
            for (int x = a[k]; x < b[e - 1]; x++)
                if (save[y][x] != blank) {
                    int X = x + dx;
                    if (X >= 0 && X < 40) rs_bg_put(layer, X, ry, save[y][x]);
                }
        }
        line++;
        k = e;
    }
    return line;
}

/* ---- the title ------------------------------------------------------------------------------------------------------
 * hu_title_draw / hu_title_sprites in w columns: the slots closer (w / 4 columns apart), the prompt wrapped (the A glyph left of
 * its first line), BEST, the join line wrapped and the credits on two lines (rows 28 and 29). max_players and
 * slot_row as given to hu_title_setup (slot_row 0: HU_ROW_SLOTS); verb as given there (NULL: PLAY); best < 0: the game
 * draws its own best line (row 24). dx (sprites): the UI layer's shift right of the middle (a view panned). */
static inline void hn_slot_pos(int p, int max_players, int slot_row, int w, int *cx, int *cy)
{
    if (w >= 40) { hu_slot_pos(p, cx, cy); return; }
    int sp = w / 4 < 9 ? w / 4 : 9, col = 20 - sp * max_players / 2 + sp * (p & 3);
    if (cx) *cx = col * 8 + 28;
    if (cy) *cy = (slot_row ? slot_row : HU_ROW_SLOTS) * 8 + 4;
}
static inline void hn_prompt_text(const char *verb, char *s, int size)
{
    rs_strf(s, (size_t)size, hu_str(HS_PRESS_TO), verb ? verb : hu_str(HS_PLAY));
}
static inline void hn_title_draw(int t, int best, const char *verb, int max_players, int slot_row, int w)
{
    if (w >= 40) { hu_title_draw(t, best); return; }
    char s[160], who[64];
    int sr = slot_row ? slot_row : HU_ROW_SLOTS;
    int n = hu_players();
    if (max_players > 1) {
        hu_clear_rows(sr, 1);
        for (int p = 0; p < n; p++) {
            rs_strf(s, sizeof s, hu_str(HS_PLAYER_TAG), p + 1);
            int cx;
            hn_slot_pos(p, max_players, sr, w, &cx, NULL);
            hu_text((cx - 28) / 8, sr, s);
        }
        if (n < max_players) {
            who[0] = 0;
            for (int p = n; p < max_players; p++) {
                size_t k = strlen(who);
                if (p > n) k += (size_t)snprintf(who + k, sizeof who - k, " / ");
                if (k < sizeof who) rs_strf(who + k, sizeof who - k, hu_str(HS_PLAYER_TAG), p + 1);
            }
            rs_strf(s, sizeof s, hu_str(n > 1 ? HS_JOIN_LEAVE : HS_JOIN), who);
        } else {
            rs_strf(s, sizeof s, hu_strn(HS_FULL, n), n);
        }
    }
    /* from the bottom: the credits (28, 29), the join line ending on row 26, BEST two rows above it */
    int jr = 26;
    if (max_players > 1) {
        int rows = hn_wrap(0, s, 0, w - 1, 2, 0);
        jr = 27 - rows;
        hu_clear_rows(25, 3);
        hn_wrap(jr, s, 0, w - 1, 2, 1);
    }
    if (best >= 0) hu_clear_rows(jr - 2, 2);
    if (best > 0) {
        rs_strf(s, sizeof s, hu_str(HS_BEST_N), best);
        hu_text(hu_center(s, 0), jr - 2, s);
    }
    hu_clear_rows(28, 2);
    hu_text(hu_center("(C) 2026 8BCRAFT", 0), 28, "(C) 2026 8BCRAFT");
    hn_prompt_text(verb, s, sizeof s);              /* last: over the rows cleared above */
    hu_clear_rows(HU_ROW_PROMPT, hn_wrap(HU_ROW_PROMPT, s, 0, w - 5, 2, 0));
    if (hu_blink(t)) hn_wrap(HU_ROW_PROMPT, s, 0, w - 5, 2, 1);
    hu_text(hu_center("RETROSTONE VC", 0), 29, "RETROSTONE VC");
}
static inline void hn_title_sprites(int t, hu_icon_fn icon, void *user, const char *verb, int max_players,
                                    int slot_row, int w, int dx)
{
    if (w >= 40) { hu_title_sprites(t, icon, user); return; }
    char s[160], l[HN_LINES][160];
    hn_prompt_text(verb, s, sizeof s);
    if (hn_split_bal(s, w - 5, 2, l, 0)) {
        int x = hu_center(l[0], 0) * 8 - 20;
        hu_glyph(hu_title_glyph(NULL, NULL) == HU_BTN_DPAD ? HU_BTN_DPAD : HU_BTN_A, x + dx, HU_ROW_PROMPT * 8 - 4, (t / 30) % 2, 3);
    }
    if (max_players < 2) return;
    for (int p = 0; p < hu_players(); p++) {
        int cx, cy, a = hu_player_age(p);
        hn_slot_pos(p, max_players, slot_row, w, &cx, &cy);
        cx += dx;
        if (a < 24) {
            hu_sparkle(cx + 8, cy - 14, (a / 6) & 1, 3);
            hu_sparkle(cx - 14, cy - 2, (a / 6 + 1) & 1, 3);
        }
        if (icon) icon(p, cx, cy + (a < 16 ? 12 - hu_ease_back(a, 16, 12) : 0), t, user);
    }
}

/* ---- banners and PAUSED --------------------------------------------------------------------------------------------
 * hu_banner in w columns: the title's words on 2x lines that fit (the panel grows upwards from row y + 3), or, when a
 * word alone is too wide, the title in the small font on the panel. */
static inline void hn_banner(int y, const char *title, int w)
{
    if (w >= 40 || hu_width(title, 1) + 2 <= w) { hu_banner(y, title); return; }
    char l[HN_LINES][160];
    int n = hn_split_bal(title, w - 2, 2, l, 1), big = 1;
    for (int i = 0; i < n; i++)
        if (hu_width(l[i], 1) + 2 > w) big = 0;
    if (!big) {
        int rows = hn_wrap(0, title, 1, w - 2, 2, 0);
        hu_panel(20 - w / 2, y, w, rows + 2 < 4 ? 4 : rows + 2);
        hn_wrap(y + (rows == 1 ? 2 : 1), title, 1, w - 2, 2, 1);
        return;
    }
    int acc = 0;
    for (int i = 0; i < n; i++) acc |= hu_tall(l[i]);
    int lh = 2 + acc, first = y + 1 - (n - 1) * lh, top = first - 1 - acc;
    int bottom = y + 4;
    if (top < 2) { bottom += 2 - top; first += 2 - top; top = 2; }   /* rows 0-1: often a HUD pinned at the top */
    int pw = 20;
    for (int i = 0; i < n; i++)
        if (hu_width(l[i], 1) + 2 > pw) pw = hu_width(l[i], 1) + 2;
    hu_panel((40 - pw) / 2, top, pw, bottom - top);
    for (int i = 0; i < n; i++) hu_big(hu_center(l[i], 1), first + i * lh, l[i], HU_BIG_BANNER);
}
/* hu_pause, PAUSED in the small font when its 2x letters would not fit */
static inline void hn_pause(int paused, int y, int w)
{
    hu_pause(paused, y);
    if (paused && w < 40 && hu_width(hu_str(HS_PAUSED), 1) > w) {
        hu_clear_rows(y, 2);
        hu_text(hu_center(hu_str(HS_PAUSED), 0), y, hu_str(HS_PAUSED));
    }
}

/* ---- the ranking -----------------------------------------------------------------------------------------------------
 * hu_results_* in w columns (the kit's panel is 24 wide): below 24, a panel of w columns, the rank, the tag, the hero
 * icon, the value and the medal closer together. */
static inline int hn_results_x0(int w) { return w >= 24 ? 8 : 20 - w / 2; }
static inline void hn_results_panel(const hu_standing *s, const char *title, int w)
{
    if (w >= 40) { hu_results_panel(s, title); return; }
    char t[96];
    if (!title) {
        int win = hu_winner(s);
        if (win >= 0) rs_strf(t, sizeof t, hu_str(HS_WINS), win + 1);
        else snprintf(t, sizeof t, "%s", hu_str(HS_DRAW));
        title = t;
    }
    hn_banner(3, title, w);
    int x0 = hn_results_x0(w), kit = w >= 24;      /* 24 columns or more: the kit's panel */
    hu_panel(x0, 8, kit ? 24 : w, 3 * s->n + 4);
    for (int i = 0; i < s->n; i++) {
        char tag[16];
        int row = hu_results_row(s, i);
        hu_box_text(x0 + 2 - !kit, row, hu_str(HS_RANK_1 + (s->rank[i] & 3)));
        rs_strf(tag, sizeof tag, hu_str(HS_PLAYER_TAG), (s->order[i] & 3) + 1);
        hu_box_text(x0 + 6 - !kit, row, tag);
    }
}
static inline void hn_results_icon_pos(const hu_standing *s, int place, int w, int *cx, int *cy)
{
    hu_results_icon_pos(s, place, cx, cy);
    if (w < 24 && cx) *cx = hn_results_x0(w) * 8 + 68 + (w - 20) * 2;
}
static inline void hn_results_sprites(const hu_standing *s, int t, int slide_px, int w)
{
    if (w >= 24) { hu_results_sprites(s, t, slide_px); return; }
    int x0 = hn_results_x0(w) * 8, e = (w - 20) * 4;
    for (int i = 0; i < s->n; i++) {
        int y = hu_results_row(s, i) * 8 + slide_px, m = hu_results_medal(s, i);
        hu_number(s->value[s->order[i]], x0 + 104 + e, y - 4, 3);
        if (m) {
            hu_medal(m, x0 + 130 + e * 2, y - 8, 3);
            if (s->rank[i] == 0 && (t / 20) % 3 == 0) hu_sparkle(x0 + 148 + e * 2, y - 8, (t / 10) % 2, 3);
        }
    }
}

#endif
