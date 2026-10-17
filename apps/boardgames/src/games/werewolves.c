/*
 * Werewolves in RetroStone Board: the folk game for 5 to 12 people round one device, the device the narrator (the
 * rules: werewolves_rules.c, werewolves.h; the people and the hand-off: party.h). The roles are dealt as cards, each
 * seen alone behind the shell's cover. At night the cover names the role that wakes (a werewolf, the seer's crystal
 * ball, the witch's hat): everyone else keeps their eyes shut while it acts on the people's chips; the dawn shows the
 * dead and their cards; by day the village taps whom it votes out. The roles' names are the game's words
 * (help/<lang>/werewolves_words.txt).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/guessart.h"
#include "games/party.h"
#include "games/werewolves.h"
#include "shell.h"
#include "text.h"

#define HOLD_ROLE(r) (-10 - (r))                /* the device held by a role at night (party.h's holders) */
#define ROLE_OF(h) (-10 - (h))
enum { PIC_VILLAGE = WW_ROLES, PIC_LIFE, PIC_DEATH, PICS };

static ww_state P;
static pt_seat S;
static int pick = -1;                           /* the chip chosen, not confirmed yet */
static int save_on, poison_on;                  /* the witch's step: the life potion given, the death potion out */

static const uint8_t INKC[3] = {60, 40, 26};
static const uint8_t ROLE_BG[PICS][3] = {{206, 160, 98}, {54, 62, 98}, {92, 72, 132}, {48, 92, 72},
                                         {206, 160, 98}, {62, 138, 98}, {66, 42, 74}};

/* ---- the pictures (a unit square each) --------------------------------------------------------------------------- */
static void wolf(ga_pen *p, float lw)
{
    static const float fur[3] = {132, 132, 142}, pale[3] = {206, 204, 210}, dark[3] = {40, 38, 44},
                       eye[3] = {246, 200, 60}, ear[3] = {176, 128, 128};
    const float e1[] = {0.2f, 0.42f, 0.24f, 0.08f, 0.44f, 0.3f}, e2[] = {0.8f, 0.42f, 0.76f, 0.08f, 0.56f, 0.3f};
    ga_poly(p, ART_PART, e1, 3);
    ga_poly(p, ART_PART, e2, 3);
    ga_paint(p, fur, NULL, 0.3f, lw);
    const float i1[] = {0.27f, 0.36f, 0.28f, 0.17f, 0.4f, 0.3f}, i2[] = {0.73f, 0.36f, 0.72f, 0.17f, 0.6f, 0.3f};
    ga_poly(p, ART_PART, i1, 3);
    ga_poly(p, ART_PART, i2, 3);
    ga_paint(p, ear, NULL, 0, 0);
    const float head[] = {0.16f, 0.46f, 0.3f, 0.28f, 0.5f, 0.24f, 0.7f, 0.28f, 0.84f, 0.46f, 0.78f, 0.7f, 0.5f, 0.92f,
                          0.22f, 0.7f};
    ga_poly(p, ART_PART, head, 8);
    ga_paint(p, fur, NULL, 0.3f, lw);
    const float muzzle[] = {0.36f, 0.62f, 0.5f, 0.5f, 0.64f, 0.62f, 0.5f, 0.9f};
    ga_poly(p, ART_PART, muzzle, 4);
    ga_paint(p, pale, NULL, 0.2f, lw * 0.8f);
    ga_ell(p, ART_PART, 0.38f, 0.48f, 0.055f, 0.04f);
    ga_ell(p, ART_PART, 0.62f, 0.48f, 0.055f, 0.04f);
    ga_paint(p, eye, NULL, 0.2f, lw * 0.6f);
    ga_ell(p, ART_PART, 0.38f, 0.48f, 0.014f, 0.035f);
    ga_ell(p, ART_PART, 0.62f, 0.48f, 0.014f, 0.035f);
    ga_ell(p, ART_PART, 0.5f, 0.82f, 0.055f, 0.04f);
    ga_paint(p, dark, NULL, 0.2f, 0);
}

static void seer(ga_pen *p, float lw)
{
    static const float brass[3] = {196, 150, 70}, ball[3] = {150, 128, 210}, white[3] = {250, 248, 240},
                       iris[3] = {60, 110, 190}, dark[3] = {28, 24, 34};
    const float stand[] = {0.28f, 0.92f, 0.34f, 0.76f, 0.66f, 0.76f, 0.72f, 0.92f};
    ga_poly(p, ART_PART, stand, 4);
    ga_paint(p, brass, NULL, 0.35f, lw);
    ga_ell(p, ART_PART, 0.5f, 0.48f, 0.32f, 0.32f);
    ga_paint(p, ball, NULL, 0.55f, lw);
    const float eyel[] = {0.28f, 0.48f, 0.4f, 0.4f, 0.5f, 0.38f, 0.6f, 0.4f, 0.72f, 0.48f, 0.6f, 0.56f, 0.5f, 0.58f,
                          0.4f, 0.56f};
    ga_poly(p, ART_PART, eyel, 8);
    ga_paint(p, white, NULL, 0, lw * 0.7f);
    ga_ell(p, ART_PART, 0.5f, 0.48f, 0.085f, 0.085f);
    ga_paint(p, iris, NULL, 0.2f, 0);
    ga_ell(p, ART_PART, 0.5f, 0.48f, 0.035f, 0.035f);
    ga_paint(p, dark, NULL, 0, 0);
    ga_ell(p, ART_PART, 0.4f, 0.32f, 0.06f, 0.035f);
    ga_paint(p, white, NULL, 0, 0);
}

static void witch(ga_pen *p, float lw)
{
    static const float hat[3] = {58, 40, 74}, band[3] = {150, 60, 120}, gold[3] = {236, 190, 70};
    ga_ell(p, ART_PART, 0.5f, 0.8f, 0.42f, 0.09f);
    const float cone[] = {0.26f, 0.8f, 0.44f, 0.36f, 0.56f, 0.1f, 0.74f, 0.2f, 0.62f, 0.32f, 0.74f, 0.8f};
    ga_poly(p, ART_PART, cone, 6);
    ga_paint(p, hat, NULL, 0.35f, lw);
    const float b[] = {0.3f, 0.7f, 0.7f, 0.7f, 0.72f, 0.79f, 0.28f, 0.79f};
    ga_poly(p, ART_PART, b, 4);
    ga_paint(p, band, NULL, 0.2f, lw * 0.6f);
    ga_rect(p, ART_PART, 0.44f, 0.68f, 0.56f, 0.81f);
    ga_paint(p, gold, NULL, 0.3f, lw * 0.6f);
    ga_rect(p, ART_PART, 0.475f, 0.71f, 0.525f, 0.78f);
    ga_paint(p, band, NULL, 0, 0);
}

static void villager(ga_pen *p, float lw)
{
    static const float skin[3] = {236, 196, 156}, straw[3] = {230, 196, 104}, cloth[3] = {176, 64, 52},
                       dark[3] = {60, 40, 30}, shirt[3] = {120, 150, 190};
    ga_ell(p, ART_PART, 0.5f, 1.08f, 0.38f, 0.3f);
    ga_paint(p, shirt, NULL, 0.3f, lw);
    const float scarf[] = {0.34f, 0.76f, 0.66f, 0.76f, 0.5f, 0.88f};
    ga_poly(p, ART_PART, scarf, 3);
    ga_paint(p, cloth, NULL, 0.2f, lw);
    ga_ell(p, ART_PART, 0.5f, 0.55f, 0.21f, 0.23f);
    ga_paint(p, skin, NULL, 0.25f, lw);
    ga_ell(p, ART_PART, 0.42f, 0.56f, 0.025f, 0.03f);
    ga_ell(p, ART_PART, 0.58f, 0.56f, 0.025f, 0.03f);
    ga_arc(p, ART_PART, 0.5f, 0.6f, 0.08f, 0.1f, 200, 340);
    ga_paint(p, dark, NULL, 0, 0);
    ga_ell(p, ART_PART, 0.5f, 0.38f, 0.4f, 0.08f);
    ga_ell(p, ART_PART, 0.5f, 0.3f, 0.2f, 0.12f);
    ga_paint(p, straw, NULL, 0.3f, lw);
}

static void village(ga_pen *p, float lw)
{
    static const float wall[3] = {236, 218, 180}, roof[3] = {176, 72, 52}, door[3] = {110, 72, 44},
                       roof2[3] = {120, 80, 60};
    ga_rect(p, ART_PART, 0.1f, 0.56f, 0.4f, 0.86f);
    ga_rect(p, ART_PART, 0.56f, 0.5f, 0.9f, 0.86f);
    ga_rect(p, ART_PART, 0.36f, 0.42f, 0.62f, 0.86f);
    ga_paint(p, wall, NULL, 0.2f, lw);
    const float r1[] = {0.06f, 0.58f, 0.25f, 0.4f, 0.44f, 0.58f}, r2[] = {0.52f, 0.52f, 0.73f, 0.32f, 0.94f, 0.52f};
    ga_poly(p, ART_PART, r1, 3);
    ga_poly(p, ART_PART, r2, 3);
    ga_paint(p, roof, NULL, 0.25f, lw);
    const float r3[] = {0.32f, 0.44f, 0.49f, 0.14f, 0.66f, 0.44f};
    ga_poly(p, ART_PART, r3, 3);
    ga_paint(p, roof2, NULL, 0.25f, lw);
    ga_rect(p, ART_PART, 0.45f, 0.66f, 0.53f, 0.86f);
    ga_rect(p, ART_PART, 0.18f, 0.66f, 0.26f, 0.74f);
    ga_rect(p, ART_PART, 0.68f, 0.62f, 0.78f, 0.72f);
    ga_paint(p, door, NULL, 0, 0);
}

/* a potion: a round flask, its liquid, a cork; life a heart on it, death a skull */
static void potion(ga_pen *p, float lw, int death)
{
    static const float glass[3] = {214, 230, 236}, cork[3] = {170, 120, 70}, white[3] = {250, 248, 240},
                       life[3] = {214, 70, 92}, dark[3] = {30, 26, 34};
    static const float liq[2][3] = {{78, 188, 120}, {120, 70, 150}};
    ga_rect(p, ART_PART, 0.42f, 0.16f, 0.58f, 0.4f);
    ga_ell(p, ART_PART, 0.5f, 0.62f, 0.3f, 0.3f);
    ga_paint(p, glass, NULL, 0.4f, lw);
    ga_ell(p, ART_PART, 0.5f, 0.64f, 0.25f, 0.25f);
    ga_paint(p, liq[death], NULL, 0.4f, 0);
    ga_rect(p, ART_PART, 0.4f, 0.08f, 0.6f, 0.2f);
    ga_paint(p, cork, NULL, 0.3f, lw * 0.7f);
    if (!death) {
        ga_ell(p, ART_PART, 0.45f, 0.6f, 0.06f, 0.06f);
        ga_ell(p, ART_PART, 0.55f, 0.6f, 0.06f, 0.06f);
        const float h[] = {0.39f, 0.62f, 0.61f, 0.62f, 0.5f, 0.76f};
        ga_poly(p, ART_PART, h, 3);
        ga_paint(p, white, NULL, 0, 0);
        ga_ell(p, ART_PART, 0.45f, 0.61f, 0.035f, 0.035f);
        ga_ell(p, ART_PART, 0.55f, 0.61f, 0.035f, 0.035f);
        const float k[] = {0.415f, 0.625f, 0.585f, 0.625f, 0.5f, 0.72f};
        ga_poly(p, ART_PART, k, 3);
        ga_paint(p, life, NULL, 0, 0);
    } else {
        ga_ell(p, ART_PART, 0.5f, 0.6f, 0.1f, 0.09f);
        ga_rect(p, ART_PART, 0.45f, 0.64f, 0.55f, 0.74f);
        ga_paint(p, white, NULL, 0, 0);
        ga_ell(p, ART_PART, 0.465f, 0.6f, 0.025f, 0.025f);
        ga_ell(p, ART_PART, 0.535f, 0.6f, 0.025f, 0.025f);
        ga_paint(p, dark, NULL, 0, 0);
    }
}

/* picture k (a role, the village, a potion) in a square of side s at (ox, oy) of a picture w x h */
static void paint_pic_in(int k, uint8_t *rgba, int w, int h, float s, float ox, float oy)
{
    ga_pen p;
    ga_begin(&p, rgba, w, h, s, ox, oy);
    float lw = fmaxf(0.8f, s * 0.012f);
    switch (k) {
    case WW_WOLF: wolf(&p, lw); break;
    case WW_SEER: seer(&p, lw); break;
    case WW_WITCH: witch(&p, lw); break;
    case WW_VILLAGER: villager(&p, lw); break;
    case PIC_VILLAGE: village(&p, lw); break;
    default: potion(&p, lw, k == PIC_DEATH); break;
    }
    ga_end(&p);
}

static void paint_pic(int k, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    paint_pic_in(k, rgba, size, size, size, 0, 0);
}

/* the picture on a disc of its colour (the cover's tokens, the badges) */
static void paint_disc(int k, int size, uint8_t *rgba)
{
    static const uint8_t rim[3] = {150, 118, 70};
    art_disc(size, rim, 1, 0, rgba);
    int in = (int)(size * 0.9f);
    uint8_t *d = calloc((size_t)in * in, 4);
    if (!d) return;
    art_disc(in, ROLE_BG[k], 1, 0, d);
    art_blend(rgba, size, size, d, in, in, (size - in) / 2, (size - in) / 2);
    free(d);
    paint_pic_in(k, rgba, size, size, size * 0.72f, size * 0.14f, size * 0.12f);
}

/* the role cards: the picture on a disc of its colour, the role's name */
static void paint_card(int role, int w, int h, uint8_t *rgba, int words)
{
    pt_card(w, h, rgba);
    int d = (int)(w * 0.8f);
    uint8_t *t = calloc((size_t)d * d, 4);
    if (t) {
        paint_disc(role, d, t);
        art_blend(rgba, w, h, t, d, d, (w - d) / 2, (int)(h * 0.1f));
        free(t);
    }
    char s[96];
    if (words && text_word("werewolves_words", role, s, sizeof s))
        pt_text(rgba, w, h, s, w * 0.12f, 1, INKC, w / 2.0f, h * 0.8f, w * 0.86f);
}

static void paint_face(int card, int w, int h, uint8_t *rgba) { paint_card(card, w, h, rgba, 1); }

static int deck(void)
{
    static const cards_deck_def def = {1.4f, paint_face, NULL, NULL};
    static int d = -1;
    if (d < 0) d = cards_deck_add(&def);
    return d;
}

/* ---- the screen -------------------------------------------------------------------------------------------------- */
static box Tb;
static pt_split L;
static pt_look K;
static float cd, cx[WW_MAXP], cy[WW_MAXP];

static void layout(box b)
{
    Tb = b;
    pt_split_layout(&L, b);
    pt_look_layout(&K, b, cards_aspect(deck()));
    cd = pt_grid(L.mid, P.n, 1.6f * ui.unit, cx, cy);
}

/* ---- the moves --------------------------------------------------------------------------------------------------- */
static int holder(void)
{
    if (P.phase == WP_DEAL) return P.looker;
    int r = ww_holder_role(&P);
    return r >= 0 ? HOLD_ROLE(r) : PT_ALL;
}

static void sync(void)
{
    int h = holder();
    if (P.phase != WP_OVER && h != S.who[S.slot]) pt_pass(&S, h);
    if (P.phase == WP_OVER) ui_tokens_changed();
    pick = -1;
    save_on = poison_on = 0;
}

static void start(int v)
{
    ww_new(&P, WW_MINP + (v < 0 ? 0 : v), (uint32_t)ui_random(1 << 30));
    pt_seat_reset(&S, holder());
    pick = -1;
    save_on = poison_on = 0;
    K.key = -1;
    if (Tb.w > 0) layout(Tb);
}

static int script(const char *s)
{
    int ok = ww_script(&P, s);
    sync();
    K.key = -1;
    return ok;
}

static int undo(void) { return 0; }
static int turn(void) { return S.slot; }
static int status(void)
{
    return P.phase != WP_OVER ? GS_PLAY : P.winner == WW_VILLAGE_WINS ? GS_WIN0 : GS_WIN1;
}
static int busy(void) { return pt_look_busy(&K); }

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void badge(int k, float x, float y, float d) { ui_put_c(ct_picture(paint_disc, k, (int)d), x, y, d, d); }

/* everyone's chip: the dead faint with their role; marks: the chosen one, the wolves (at their step) */
static void draw_people(int wolves_marked)
{
    for (int i = 0; i < P.n; i++) {
        int dead = !P.alive[i];
        if (i == pick) {
            if (P.phase == WP_SEER) ui_mark_gold(cx[i], cy[i], cd * 1.55f);
            else ui_mark_glow(cx[i], cy[i], cd * 1.6f);
        }
        if (P.phase == WP_WITCH && i == P.victim) ui_mark_glow(cx[i], cy[i], cd * 1.6f);
        pt_chip_at(i, cx[i], cy[i], cd, dead ? 0.35f : 1);
        float bx = cx[i] + cd * 0.36f, by = cy[i] - cd * 0.36f, bd = cd * 0.46f;
        if (dead) badge(P.role[i], bx, by, bd);
        else if (wolves_marked && P.role[i] == WW_WOLF) badge(WW_WOLF, bx, by, bd);
        else if (P.phase == WP_SEER && i == pick) badge(P.role[i], cx[i], cy[i] + cd * 0.05f, cd * 0.9f);
        else if (P.phase == WP_WITCH && i == P.victim) badge(WW_WOLF, bx, by, bd);
        if (P.phase == WP_WITCH && i == pick) badge(PIC_DEATH, cx[i] - cd * 0.36f, by, bd);
        if (P.phase == WP_WITCH && i == P.victim && save_on) badge(PIC_LIFE, cx[i] - cd * 0.36f, by, bd);
    }
}

/* the moon, the sun: pale gold on the dark table, deeper on the light one */
static SDL_Texture *sky(int icon, int size)
{
    return pt_dark() ? ct_icon(icon, size, 246, 226, 150) : ct_icon(icon, size, 196, 140, 40);
}

static void top_picture(int k, int moon)
{
    box t = L.top;
    float d = fminf(1.6f * ui.unit, t.h * 0.7f), x = t.x + t.w / 2, y = t.y + t.h / 2;
    if (moon) {
        float m = d * 0.7f;
        ui_put_c(sky(moon > 0 ? ICON_MOON : ICON_SUN, (int)m), x - 1.1f * d, y, m, m);
    }
    badge(k, x, y, d);
}

/* the dead (dawn, the vote's result): their chips on their cards */
static void draw_dead(const int *who, int n)
{
    box m = L.mid;
    float u = ui.unit, a = cards_aspect(deck());
    float w = floorf(fminf(m.h * 0.7f / a, fminf(3.2f * u, m.w / (n + 0.6f) / 1.1f)));
    for (int i = 0; i < n; i++) {
        float x = m.x + m.w / 2 + (i - (n - 1) / 2.0f) * w * 1.15f, y = m.y + m.h / 2;
        cards_draw(deck(), P.role[who[i]], x, y, w, 0, 0);
        pt_chip_at(who[i], x - w * 0.4f, y - w * a * 0.42f, w * 0.42f, 1);
    }
    if (!n) {                                   /* nobody: the sun alone */
        float d = fminf(2.4f * u, m.h * 0.5f);
        ui_put_c(sky(ICON_SUN, (int)d), m.x + m.w / 2, m.y + m.h / 2, d, d);
    }
}

static void draw_roles(box r)                   /* the end: every chip and its role */
{
    float x[WW_MAXP], y[WW_MAXP];
    float d = pt_grid(r, P.n, 1.5f * ui.unit, x, y);
    for (int i = 0; i < P.n; i++) {
        int won = (P.role[i] == WW_WOLF) == (P.winner == WW_WOLVES_WIN);
        if (won) ui_mark_gold(x[i], y[i], d * 1.4f);
        pt_chip_at(i, x[i], y[i], d * 0.85f, P.alive[i] ? 1 : 0.45f);
        badge(P.role[i], x[i] + d * 0.34f, y[i] - d * 0.34f, d * 0.48f);
    }
}

static void draw(void)
{
    float mx = L.bx[1];
    switch (P.phase) {
    case WP_DEAL:
        pt_look_draw(&K, deck(), P.role[P.looker], P.looker, ui.viewer >= 0 && S.who[ui.viewer] == P.looker);
        break;
    case WP_WOLVES:
        top_picture(WW_WOLF, 1);
        draw_people(1);
        ui_button(mx, L.by, L.bd, ICON_TICK, pick >= 0 ? UI_LIT : UI_DIM);
        break;
    case WP_SEER:
        top_picture(WW_SEER, 1);
        draw_people(0);
        ui_button(mx, L.by, L.bd, ICON_TICK, pick >= 0 ? UI_LIT : UI_DIM);
        break;
    case WP_WITCH:
        top_picture(WW_WITCH, 1);
        draw_people(0);
        ui_button_paint(L.bx[0], L.by, L.bd, paint_pic, PIC_LIFE, 0.78f,
                        UI_IVORY | (!P.heal ? UI_DIM : save_on ? UI_ON : 0));
        ui_button(mx, L.by, L.bd, ICON_TICK, UI_LIT);
        ui_button_paint(L.bx[2], L.by, L.bd, paint_pic, PIC_DEATH, 0.78f,
                        UI_IVORY | (!P.poison ? UI_DIM : poison_on ? UI_ON : 0));
        break;
    case WP_DAWN:
        top_picture(PIC_VILLAGE, -1);
        draw_dead(P.dead, P.ndead);
        ui_button(mx, L.by, L.bd, ICON_ARROW, UI_LIT);
        break;
    case WP_VOTE:
        top_picture(PIC_VILLAGE, -1);
        draw_people(0);
        ui_button(mx, L.by, L.bd, ICON_TICK, pick >= 0 ? UI_LIT : UI_DIM);
        break;
    case WP_OUT:
        top_picture(PIC_VILLAGE, -1);
        draw_dead(&P.out, 1);
        ui_button(mx, L.by, L.bd, ICON_MOON, UI_LIT);
        break;
    case WP_OVER:
        if (ui_over()) {
            float u = ui.unit;
            box r = ui_end_room(fminf(Tb.w, 7.5f * u), fminf(Tb.h * 0.6f, (P.n > 6 ? 4.6f : 2.8f) * u));
            if (r.w > 0) draw_roles(r);
        } else draw_roles(L.mid);
        break;
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    float mx = L.bx[1];
    int c = pt_grid_hit(P.n, cx, cy, cd, x, y), tick = ui_button_hit(mx, L.by, L.bd, x, y);
    switch (P.phase) {
    case WP_DEAL:
        if (!pt_look_hit(&K, x, y)) return;
        if (busy()) { ui_sound(SND_DENY); return; }
        ww_looked(&P);
        ui_sound(SND_TAP);
        sync();
        return;
    case WP_WOLVES:
        if (c >= 0) {
            if (!ww_can_kill(&P, c)) { ui_sound(SND_DENY); return; }
            pick = c;
            ui_sound(SND_TAP);
        } else if (tick) {
            if (pick < 0) { ui_sound(SND_DENY); return; }
            ww_kill(&P, pick);
            ui_sound(SND_PLACE);
            sync();
        }
        return;
    case WP_SEER:
        if (c >= 0) {                           /* one look a night: the first chip tapped */
            if (pick >= 0 || !ww_can_look(&P, c)) { ui_sound(SND_DENY); return; }
            pick = c;
            ui_sound(SND_MOVE);
        } else if (tick) {
            if (pick < 0) { ui_sound(SND_DENY); return; }
            ww_look(&P, pick);
            ui_sound(SND_PLACE);
            sync();
        }
        return;
    case WP_WITCH:
        if (ui_button_hit(L.bx[0], L.by, L.bd, x, y)) {
            if (!P.heal || P.victim < 0) { ui_sound(SND_DENY); return; }
            save_on = !save_on;
            ui_sound(SND_TAP);
        } else if (ui_button_hit(L.bx[2], L.by, L.bd, x, y)) {
            if (!P.poison) { ui_sound(SND_DENY); return; }
            poison_on = !poison_on;
            if (!poison_on) pick = -1;
            ui_sound(SND_TAP);
        } else if (c >= 0) {
            if (!poison_on || !ww_can_poison(&P, c)) { ui_sound(SND_DENY); return; }
            pick = pick == c ? -1 : c;
            ui_sound(SND_TAP);
        } else if (tick) {
            ww_witch(&P, save_on, poison_on ? pick : -1);
            ui_sound(SND_PLACE);
            sync();
        }
        return;
    case WP_VOTE:
        if (c >= 0) {
            if (!P.alive[c]) { ui_sound(SND_DENY); return; }
            pick = c;
            ui_sound(SND_TAP);
        } else if (tick) {
            if (pick < 0) { ui_sound(SND_DENY); return; }
            ww_vote(&P, pick);
            ui_sound(SND_CAPTURE);
            sync();
        }
        return;
    case WP_DAWN:
    case WP_OUT:
        if (!tick) return;
        ww_next(&P);
        ui_sound(SND_TAP);
        sync();
        return;
    }
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the holder's bar: nothing more; the other's: the nights, the werewolves and the people still in play */
static void bar(int player, box b)
{
    if (player == S.slot) return;
    int s = (int)(0.6f * ui.unit);
    ui_count c[3] = {{sky(ICON_MOON, s), P.night}, {ct_picture(paint_pic, WW_WOLF, s),
                     ww_alive(&P, WW_WOLF)}, {pt_icon(ICON_PERSON, s), ww_alive(&P, -1)}};
    ui_bar_fit(b, c, 3, 0);
}

static void token(int player, int size, uint8_t *rgba)
{
    if (P.phase == WP_OVER) {                   /* the end card: the village, the werewolves */
        paint_disc(player ? WW_WOLF : PIC_VILLAGE, size, rgba);
        return;
    }
    int who = S.who[player & 1];
    if (who >= 0) pt_chip(who, size, rgba);
    else if (who == PT_ALL) pt_group(size, rgba);
    else if (who != PT_NONE) paint_disc(ROLE_OF(who), size, rgba);
}

/* the menu's card: a werewolf under the moon, before the village; the picker's: the count of people */
static void thumb(int v, int size, uint8_t *rgba)
{
    if (v >= 0) { pt_count_thumb(WW_MINP + v, size, rgba); return; }
    memset(rgba, 0, (size_t)size * size * 4);
    int d = (int)(size * 0.8f);
    uint8_t *t = calloc((size_t)d * d, 4);
    if (!t) return;
    art_disc(d, ROLE_BG[WW_WOLF], 1, 0, t);
    art_blend(rgba, size, size, t, d, d, (size - d) / 2, (size - d) / 2);
    memset(t, 0, (size_t)d * d * 4);
    art_icon(ICON_MOON, d / 3, t);
    for (int i = 0; i < (d / 3) * (d / 3); i++) t[4 * i] = 246, t[4 * i + 1] = 226, t[4 * i + 2] = 150;
    art_blend(rgba, size, size, t, d / 3, d / 3, (int)(size * 0.56f), (int)(size * 0.14f));
    free(t);
    paint_pic_in(PIC_VILLAGE, rgba, size, size, size * 0.42f, size * 0.5f, size * 0.46f);
    paint_pic_in(WW_WOLF, rgba, size, size, size * 0.56f, size * 0.12f, size * 0.26f);
}

static const game werewolves_game = {
    .id = "werewolves", .order = 392, .variants = WW_VARIANTS, .variant_first = 3,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .view = VIEW_HIDDEN,
};
REGISTER_GAME(werewolves_game)
