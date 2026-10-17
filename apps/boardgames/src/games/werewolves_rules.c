/*
 * Werewolves: the rules (werewolves.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/werewolves.h"

static uint32_t rnd(ww_state *s, uint32_t n)
{
    s->rng ^= s->rng << 13;
    s->rng ^= s->rng >> 17;
    s->rng ^= s->rng << 5;
    return s->rng % n;
}

int ww_wolves_for(int n) { return n >= 10 ? 3 : n >= 7 ? 2 : 1; }
int ww_witch_for(int n) { return n >= 7; }

int ww_alive(const ww_state *s, int role)
{
    int k = 0;
    for (int i = 0; i < s->n; i++) k += s->alive[i] && (role < 0 || s->role[i] == role);
    return k;
}

static void night(ww_state *s)
{
    s->phase = WP_WOLVES;
    s->night++;
    s->victim = s->seen = s->poisoned = -1;
    s->saved = 0;
    s->ndead = 0;
    s->out = -1;
}

void ww_new(ww_state *s, int n, uint32_t seed)
{
    memset(s, 0, sizeof *s);
    s->n = n < WW_MINP ? WW_MINP : n > WW_MAXP ? WW_MAXP : n;
    s->rng = seed * 2654435761u + 0x6a09e667u;
    if (!s->rng) s->rng = 1;
    int k = 0, w = ww_wolves_for(s->n);
    for (int i = 0; i < w; i++) s->role[k++] = WW_WOLF;
    s->role[k++] = WW_SEER;
    if (ww_witch_for(s->n)) s->role[k++] = WW_WITCH;
    while (k < s->n) s->role[k++] = WW_VILLAGER;
    for (int i = s->n - 1; i > 0; i--) {
        int j = (int)rnd(s, (uint32_t)i + 1);
        uint8_t t = s->role[i];
        s->role[i] = s->role[j];
        s->role[j] = t;
    }
    for (int i = 0; i < s->n; i++) s->alive[i] = 1;
    s->heal = s->poison = ww_witch_for(s->n);
    s->phase = WP_DEAL;
    s->victim = s->seen = s->poisoned = s->out = -1;
    s->dead[0] = s->dead[1] = -1;
}

/* the game over? (the village: no wolf left; the wolves: as many as the others) */
static int settle(ww_state *s)
{
    int w = ww_alive(s, WW_WOLF), o = ww_alive(s, -1) - w;
    if (w && w < o) return 0;
    s->winner = w ? WW_WOLVES_WIN : WW_VILLAGE_WINS;
    s->phase = WP_OVER;
    return 1;
}

static int alive_role(const ww_state *s, int role) { return ww_alive(s, role) > 0; }

/* the dawn: the victim (unless saved) and the poisoned die */
static void dawn(ww_state *s)
{
    s->ndead = 0;
    s->dead[0] = s->dead[1] = -1;
    if (s->victim >= 0 && !s->saved) s->dead[s->ndead++] = s->victim;
    if (s->poisoned >= 0 && s->poisoned != (s->ndead ? s->dead[0] : -1)) s->dead[s->ndead++] = s->poisoned;
    for (int i = 0; i < s->ndead; i++) s->alive[s->dead[i]] = 0;
    s->phase = WP_DAWN;
    settle(s);
}

static void after_seer(ww_state *s)
{
    if (alive_role(s, WW_WITCH)) s->phase = WP_WITCH;
    else dawn(s);
}

int ww_looked(ww_state *s)
{
    if (s->phase != WP_DEAL) return 0;
    if (++s->looker >= s->n) night(s);
    return 1;
}

int ww_can_kill(const ww_state *s, int p)
{
    return s->phase == WP_WOLVES && p >= 0 && p < s->n && s->alive[p] && s->role[p] != WW_WOLF;
}

int ww_kill(ww_state *s, int p)
{
    if (!ww_can_kill(s, p)) return 0;
    s->victim = p;
    if (alive_role(s, WW_SEER)) s->phase = WP_SEER;
    else after_seer(s);
    return 1;
}

int ww_can_look(const ww_state *s, int p)
{
    return s->phase == WP_SEER && p >= 0 && p < s->n && s->alive[p] && s->role[p] != WW_SEER;
}

int ww_look(ww_state *s, int p)
{
    if (!ww_can_look(s, p)) return 0;
    s->seen = p;
    after_seer(s);
    return 1;
}

int ww_can_poison(const ww_state *s, int p)
{
    return s->phase == WP_WITCH && s->poison && p >= 0 && p < s->n && s->alive[p] && s->role[p] != WW_WITCH;
}

int ww_witch(ww_state *s, int save, int poison)
{
    if (s->phase != WP_WITCH || (save && (!s->heal || s->victim < 0)) || (poison >= 0 && !ww_can_poison(s, poison)))
        return 0;
    if (save) { s->saved = 1; s->heal = 0; }
    if (poison >= 0) { s->poisoned = poison; s->poison = 0; }
    dawn(s);
    return 1;
}

int ww_next(ww_state *s)
{
    if (s->phase == WP_DAWN) { s->phase = WP_VOTE; s->out = -1; return 1; }
    if (s->phase == WP_OUT) { night(s); return 1; }
    return 0;
}

int ww_vote(ww_state *s, int p)
{
    if (s->phase != WP_VOTE || p < 0 || p >= s->n || !s->alive[p]) return 0;
    s->out = p;
    s->alive[p] = 0;
    s->phase = WP_OUT;
    settle(s);
    return 1;
}

int ww_holder_role(const ww_state *s)
{
    return s->phase == WP_WOLVES ? WW_WOLF : s->phase == WP_SEER ? WW_SEER : s->phase == WP_WITCH ? WW_WITCH : -1;
}

int ww_script(ww_state *s, const char *p)
{
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        char c = *p++;
        int ok;
        if (c == 'w') {
            int save = 0, poison = -1;
            if (*p == 'h') { save = 1; p++; }
            if (*p == 'p') poison = (int)strtol(p + 1, (char **)&p, 10);
            ok = ww_witch(s, save, poison);
        } else {
            int k = *p >= '0' && *p <= '9' ? (int)strtol(p, (char **)&p, 10) : -1;
            ok = c == 'l' ? ww_looked(s) : c == 'k' ? ww_kill(s, k) : c == 'e' ? ww_look(s, k)
               : c == 'n' ? ww_next(s) : c == 'v' ? ww_vote(s, k) : 0;
        }
        if (!ok) return 0;
        while (*p && *p != ' ') p++;
    }
    return 1;
}
