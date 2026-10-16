/*
 * Janggi (Korean chess) in RetroStone Board: the board with its two palaces (games/palace.h, xiangqi's board without
 * the river), the octagonal ivory pieces sized by rank with their characters, tap or drag, the pass button (the rules:
 * janggi_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The script's notation (the screenshots, the tests): a move is its two points, files a..i from Cho's left, ranks
 * 0..9 from Cho's side; "pass" a pass: "e3f3 c6d6 pass i6i5" (Cho's middle soldier aside, Han's soldier in, Cho
 * passes, ...).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/janggi.h"
#include "games/palace.h"
#include "shell.h"
#include "text.h"

#define ANIM_MS 150

static jgame G;
static jmove legal[JG_MAX_MOVES];
static int nlegal, state = GS_PLAY;
static palace B;                                /* the board on the screen */
static int made = -1, made_small = -1;
static SDL_Texture *t_piece[2][8], *t_small[2][8], *t_board;
static int sel = -1;
static ui_trays T;                              /* the pieces each player took, by their side */
static struct { int on, from, moved; float x, y; } drag;
static struct { int on; jmove m; Uint32 t0; } anim;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    nlegal = jn_legal(jn_pos(&G), legal);
    switch (jn_status(&G)) {
    case JG_PLAY: state = GS_PLAY; break;
    case JG_MATE: state = jn_pos(&G)->side ? GS_WIN0 : GS_WIN1; break;   /* mated: that player lost */
    default: state = GS_DRAW;
    }
    sel = -1;
    drag.on = 0;
}

static void start(int variant)
{
    (void)variant;
    jn_new(&G);
    anim.on = 0;
    refresh();
}

static int status(void) { return state; }
static int turn(void) { return jn_pos(&G)->side; }

static int target(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

static void play(jmove m, int animate)
{
    jn_play(&G, m);
    anim.on = animate;
    anim.m = m;
    anim.t0 = ui_ms();
    refresh();
    ui_sound(m.cap ? SND_CAPTURE : SND_MOVE);
    if (state == GS_PLAY && jn_in_check(jn_pos(&G), jn_pos(&G)->side)) ui_sound(SND_CHECK);
}

static int can_pass(void) { return state == GS_PLAY && jn_can_pass(jn_pos(&G)); }

static void pass(void)
{
    jn_play(&G, (jmove){JG_PASS, JG_PASS, 0});
    anim.on = 0;
    refresh();
}

static int undo(void)
{
    if (!jn_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "e3f3 c6d6 pass": the screenshots' and the tests' moves */
static int script(const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        if (!strncmp(s, "pass", 4)) {
            if (!can_pass()) return 0;
            pass();
            s += 4;
            continue;
        }
        int k = jn_parse(legal, nlegal, s);
        if (k < 0) return 0;
        play(legal[k], 0);
        s += 4;
    }
    return 1;
}

/* ---- the bot (janggi_bot.c) -------------------------------------------------------------------------------------- */
const jgame *janggi_live(void) { return &G; }

/* one move ("e3f3", or "pass"), played as if tapped: it slides */
int janggi_show(const char *move)
{
    if (!move || state != GS_PLAY) return 0;
    if (!strcmp(move, "pass")) {
        if (!can_pass()) return 0;
        pass();
        return 1;
    }
    int k = strlen(move) == 4 ? jn_parse(legal, nlegal, move) : -1;
    if (k < 0) return 0;
    play(legal[k], 1);
    return 1;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the characters, JG_GENERAL..JG_SOLDIER: Cho's set, Han's set (help/glyphs.txt keeps them in the fonts) */
static const int CHARS[2][8] = {
    {0, 0x695A, 0x58EB, 0x8C61, 0x99AC, 0x8ECA, 0x5305, 0x5352},   /* 楚士象馬車包卒 */
    {0, 0x6F22, 0x58EB, 0x8C61, 0x99AC, 0x8ECA, 0x5305, 0x5175},   /* 漢士象馬車包兵 */
};
static const uint8_t INK[2][3] = {{12, 84, 88}, PALACE_RED_INK};   /* Cho's blue-green ink, Han's red */
/* the pieces' sizes by rank: the general largest, the guards and soldiers smallest */
static const float SIZE[8] = {0, 1.04f, 0.76f, 0.88f, 0.88f, 0.88f, 0.88f, 0.76f};

/* a piece: an ivory octagon, an engraved line round it and the character in its player's ink, size x size */
static void piece_pic(int type, int player, int size, uint8_t *rgba)
{
    palace_piece(8, SIZE[type], CHARS[player][type], INK[player], size, rgba);
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
/* the board as big as the room lets it be (its width in portrait, its height in landscape); the pieces each player
 * took in a tray by their side, in the room it leaves (above and below it, or beside it) */
static void layout(box b)
{
    palace_fit(&B, b);
    int sq = B.sq;
    float x, y;
    ui_trays_place(&T, b, (float)(PALACE_W * sq + 2 * B.fr), (float)(PALACE_H * sq + 2 * B.fr), 15, (float)sq,
                0.7f * ui.unit, &x, &y);
    B.bx = x + B.fr;
    B.by = y + B.fr;
    if (made != sq) {
        made = sq;
        for (int c = 0; c < 2; c++)
            for (int t = JG_GENERAL; t <= JG_SOLDIER; t++) {
                ui_free(&t_piece[c][t]);
                piece_pic(t, c, sq, ui_buf(sq, sq));
                t_piece[c][t] = ui_tex(sq, sq, ui_buf(sq, sq));
            }
        ui_free(&t_board);
        t_board = palace_board_tex(&B, 0, NULL, 0);
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int c = 0; c < 2; c++)
            for (int t = JG_GENERAL; t <= JG_SOLDIER; t++) {
                ui_free(&t_small[c][t]);
                piece_pic(t, c, small, ui_buf(small, small));
                t_small[c][t] = ui_tex(small, small, ui_buf(small, small));
            }
    }
}

/* a piece centred on (x, y) with its shadow, size: a point's square (the piece is smaller by rank), turned round for
 * the top player face to face: its character faces them */
static void piece_at(int pc, float x, float y, float size, int shadow)
{
    float k = SIZE[JG_TYPE(pc)] * 0.9f * size;  /* the piece's own width */
    if (shadow) { ui_shadow_flat(x, y, k, k, 1); return; }
    SDL_Texture *t = t_piece[JG_OWNER(pc)][JG_TYPE(pc)];
    if (ui_upside(JG_OWNER(pc))) ui_put_rot(t, x, y, size, size, 180);
    else ui_put_c(t, x, y, size, size);
}

/* the pieces the player took: how many of each type (landed: the one just taken once the move has landed); their
 * number */
static int taken(int player, int gone[8], int landed)
{
    static const int start_n[8] = {0, 1, 2, 2, 2, 2, 2, 5};
    const jpos *p = jn_pos(&G);
    int c = !player, n = 0;
    for (int t = 0; t < 8; t++) gone[t] = start_n[t];
    for (int s = 0; s < JG_PTS; s++)
        if (p->sq[s] && JG_OWNER(p->sq[s]) == c) gone[JG_TYPE(p->sq[s])]--;
    if (landed && anim.on && anim.m.cap && JG_OWNER(anim.m.cap) == c) gone[JG_TYPE(anim.m.cap)]--;
    for (int t = 0; t < 8; t++) n += gone[t] > 0 ? gone[t] : 0;
    return n;
}

static const int ORDER[6] = {JG_CHARIOT, JG_CANNON, JG_HORSE, JG_ELEPHANT, JG_GUARD, JG_SOLDIER};

/* the pieces each player took, in the tray by their side, the strongest first, their characters facing them */
static void draw_trays(void)
{
    if (!ui_trays_on(&T)) return;
    for (int c = 0; c < 2; c++) {
        int gone[8], i = 0;
        taken(c, gone, 1);
        for (int k = 0; k < 6; k++)
            for (int j = 0; j < gone[ORDER[k]]; j++) {
                float x, y, d = T.pitch, w = SIZE[ORDER[k]] * 0.9f * d;
                SDL_Texture *t = t_piece[!c][ORDER[k]];
                ui_trays_slot(&T, c, i++, &x, &y);
                ui_shadow_flat(x, y, w, w, 1);
                if (ui_upside(c)) ui_put_rot(t, x, y, d, d, 180);
                else ui_put_c(t, x, y, d, d);
            }
    }
}

static void draw(void)
{
    const jpos *p = jn_pos(&G);
    int sq = B.sq;
    float x, y, h = sq / 2.0f;
    palace_board(&B, t_board);
    draw_trays();
    if (G.ply && G.move[G.ply - 1].from != JG_PASS) {
        jmove m = G.move[G.ply - 1];
        palace_xy(&B, m.from, &x, &y);
        ui_mark_last(x - h, y - h, sq, sq);
        palace_xy(&B, m.to, &x, &y);
        ui_mark_last(x - h, y - h, sq, sq);
    }
    if (sel >= 0) {
        palace_xy(&B, sel, &x, &y);
        ui_mark_select(x - h, y - h, sq, sq);
    }
    if (state != GS_DRAW && jn_in_check(p, p->side)) {
        palace_xy(&B, jn_general(p, p->side), &x, &y);
        ui_mark_glow(x, y, 1.6f * sq);            /* round the general, which fills its point */
    }
    float t = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)ANIM_MS;
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
        t = 1 - (1 - t) * (1 - t) * (1 - t);
    }
    for (int shade = 1; shade >= 0; shade--)    /* the shadows first: a shadow falls on the wood only */
        for (int s = 0; s < JG_PTS; s++) {
            if (!p->sq[s] || (drag.on && drag.moved && s == drag.from) || (anim.on && s == anim.m.to)) continue;
            palace_xy(&B, s, &x, &y);
            piece_at(p->sq[s], x, y, (float)sq, shade);
        }
    if (sel >= 0) {                             /* where the selected piece can go */
        for (int i = 0; i < nlegal; i++) {
            if (legal[i].from != sel) continue;
            palace_xy(&B, legal[i].to, &x, &y);
            if (legal[i].cap) ui_mark_ring(x, y, sq);
            else ui_mark_dot(x, y, 0.32f * sq);
        }
    }
    if (anim.on) {
        float x0, y0, x1, y1;
        palace_xy(&B, anim.m.from, &x0, &y0);
        palace_xy(&B, anim.m.to, &x1, &y1);
        x = x0 + (x1 - x0) * t;
        y = y0 + (y1 - y0) * t;
        piece_at(p->sq[anim.m.to], x, y, (float)sq, 1);
        piece_at(p->sq[anim.m.to], x, y, (float)sq, 0);
    }
    if (drag.on && drag.moved) {                /* the dragged piece, a little bigger, beside the finger */
        float s = 1.18f * sq, off = ui_upside(p->side) ? 0.25f : -0.25f;
        x = drag.x;
        y = drag.y + s * off;
        piece_at(p->sq[drag.from], x, y, s, 1);
        piece_at(p->sq[drag.from], x, y, s, 0);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (state != GS_PLAY) return;
    int s = palace_pt(&B, x, y);
    if (s < 0) { sel = -1; return; }
    if (sel >= 0 && target(sel, s) >= 0) {
        play(legal[target(sel, s)], 1);
        return;
    }
    const jpos *p = jn_pos(&G);
    if (p->sq[s] && JG_OWNER(p->sq[s]) == p->side) {
        sel = s;
        drag.on = 1;
        drag.from = s;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
        return;
    }
    if (sel >= 0) ui_sound(SND_DENY);
    sel = -1;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (hypotf(x - drag.x, y - drag.y) > B.sq * 0.12f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) return;                    /* a tap: the piece stays selected */
    int s = palace_pt(&B, x, y), k = s < 0 ? -1 : target(drag.from, s);
    if (k >= 0) play(legal[k], 0);
    else if (s != drag.from) ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the pieces a player took from the other: small octagons, the strongest first (when there are no trays) */
static void bar(int player, box b)
{
    if (ui_trays_on(&T)) return;
    const int *order = ORDER;
    int c = !player, gone[8], total = taken(player, gone, 0), groups = 0;
    for (int k = 0; k < 6; k++) groups += gone[order[k]] > 0;
    if (!total) return;
    float sz = 0.6f * ui.unit, gap = 0.12f * ui.unit, x = b.x + sz / 2, cy = b.y + b.h / 2;
    float step = total > 1 ? fminf(0.86f * sz, (b.w - sz - (groups - 1) * gap) / (total - 1)) : 0;
    for (int k = 0; k < 6; k++) {
        int t = order[k];
        if (gone[t] <= 0) continue;
        for (int i = 0; i < gone[t]; i++, x += step) ui_put_c(t_small[c][t], x, cy, sz, sz);
        x += gap;
    }
}

static void token(int player, int size, uint8_t *rgba) { piece_pic(JG_GENERAL, player & 1, size, rgba); }

/* the menu's card: Cho's palace, its general in the middle, its guards, two soldiers, Han's chariot and cannon */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    palace_pic c = palace_thumb(2, 0, 5, 0, NULL, 0, size, rgba);
    static const int place[][3] = {{2, 3, JG_GENERAL}, {1, 4, JG_GUARD}, {3, 4, JG_GUARD}, {0, 1, JG_SOLDIER},
                                   {2, 1, JG_SOLDIER}, {4, 2, 8 + JG_CHARIOT}, {3, 0, 8 + JG_CANNON}};
    uint8_t *one = calloc((size_t)c.s * c.s, 4);
    if (!one) return;
    for (unsigned i = 0; i < sizeof place / sizeof place[0]; i++) {
        memset(one, 0, (size_t)c.s * c.s * 4);
        piece_pic(place[i][2] & 7, place[i][2] >> 3, c.s, one);
        art_blend(rgba, size, size, one, c.s, c.s, c.x + place[i][0] * c.s, c.y + place[i][1] * c.s);
    }
    free(one);
}

static int busy(void) { return anim.on; }     /* the end waits for the last piece to arrive */

static const game janggi_game = {
    .id = "janggi", .order = 180, .variants = 1, .fill = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script, .busy = busy,
};
REGISTER_GAME(janggi_game)
