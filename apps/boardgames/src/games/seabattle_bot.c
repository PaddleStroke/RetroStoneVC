/*
 * Sea Battle's bot (bots.h): it places its fleet, then fires, seeing of the other sea only what its own shots told it:
 * the misses, the hits, and the ships sunk (shown, as to a player). The other fleet's ships still afloat are wiped
 * from the copy it reads.
 *   placing: easy takes the fleet it was dealt (random); medium a random fleet of its own whose ships never touch;
 *            hard the one of a few such fleets that keeps off the squares a hunter tries first (the middle of the sea).
 *            The paper fleet (ten ships, too long to write in one move) is taken as dealt: its ships never touch
 *            anyway.
 *   firing:  easy hunts at random and, after a hit, tries the squares round it, but now and then forgets the hit;
 *            medium hunts on diagonals as far apart as the smallest ship afloat (every ship covers one of their
 *            squares) and finishes a ship it hit along the line its hits make; hard draws whole fleets that agree
 *            with all it has seen (every hit not sunk explained, no ship on a miss or a sunk ship, nor touching one
 *            in the paper fleet) and fires where most of them put a ship. In a salvo it takes the best squares, each
 *            fleet's ship counted by the chance the shot sinks it (a ship sunk is a shot less coming back, whatever
 *            its size); medium and easy pick a salvo's squares one after the other.
 * Runs on the worker thread: it copies the game at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation: placing, "0=a1h 1=c3v ... ok" or "ok"; firing, "e5" or a salvo "a1+b2+c3";
 * played by seabattle_show().
 */
#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/seabattle.h"

enum { OPEN, MISSED, HIT, SUNK, BLOCKED };      /* a square of the other sea as the bot knows it (BLOCKED: not shot, but
                                                 * no ship can be there: next to a sunk ship or a hit's corner, in the
                                                 * paper fleet) */

#define HARD_FLEETS 1500                        /* hard: the fleets it draws for a shot (about 10 ms on a PC) */

typedef struct {
    uint8_t sq[SB_CELLS];
    uint8_t len[SB_MAXSHIPS], n;                /* the ships still afloat */
    uint8_t apart;
} sea;

/* the other sea from the shots fired at it; p must be the seat's view (sunk ships known, the others wiped) */
static void read_sea(const sbpos *p, int them, const uint8_t *sunk, sea *s)
{
    memset(s, 0, sizeof *s);
    s->apart = p->apart;
    for (int c = 0; c < SB_CELLS; c++)
        s->sq[c] = p->shot[them][c] == SHOT_MISS ? MISSED : p->shot[them][c] == SHOT_HIT ? HIT : OPEN;
    for (int i = 0; i < p->nships; i++) {
        if (!sunk[i]) { s->len[s->n++] = p->len[i]; continue; }
        int cells[SB_N], k = sb_cells(&p->ship[them][i], cells);
        for (int j = 0; j < k; j++) {
            s->sq[cells[j]] = SUNK;
            if (!s->apart) continue;
            int x = cells[j] % SB_N, y = cells[j] / SB_N;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && ny >= 0 && nx < SB_N && ny < SB_N && s->sq[ny * SB_N + nx] == OPEN)
                        s->sq[ny * SB_N + nx] = BLOCKED;
                }
        }
    }
    if (!s->apart) return;
    for (int c = 0; c < SB_CELLS; c++) {        /* the paper fleet: a hit's corners are empty (ships are straight and
                                                 * never touch) */
        if (s->sq[c] != HIT) continue;
        int x = c % SB_N, y = c / SB_N;
        for (int dy = -1; dy <= 1; dy += 2)
            for (int dx = -1; dx <= 1; dx += 2) {
                int nx = x + dx, ny = y + dy;
                if (nx >= 0 && ny >= 0 && nx < SB_N && ny < SB_N && s->sq[ny * SB_N + nx] == OPEN)
                    s->sq[ny * SB_N + nx] = BLOCKED;
            }
    }
}

static int open_hits(const sea *s)
{
    int n = 0;
    for (int c = 0; c < SB_CELLS; c++) n += s->sq[c] == HIT;
    return n;
}

/* the ways the ships afloat can lie across each open square (those through hits weigh 40 a hit): hard's fallback
 * when no fleet could be drawn, and the heat map its fleet keeps off */
static void density(const sea *s, double *d)
{
    memset(d, 0, sizeof(double) * SB_CELLS);
    for (int i = 0; i < s->n; i++) {
        int L = s->len[i];
        if (i && s->len[i] == s->len[i - 1]) continue;   /* the same length again: counted with the first */
        int same = 1;
        while (i + same < s->n && s->len[i + same] == L) same++;
        for (int vert = 0; vert < 2; vert++)
            for (int y = 0; y < SB_N - (vert ? L - 1 : 0); y++)
                for (int x = 0; x < SB_N - (vert ? 0 : L - 1); x++) {
                    double w = same;
                    int ok = 1;
                    for (int k = 0; k < L && ok; k++) {
                        int c = (y + (vert ? k : 0)) * SB_N + x + (vert ? 0 : k);
                        if (s->sq[c] != OPEN && s->sq[c] != HIT) ok = 0;
                        else if (s->sq[c] == HIT) w *= 40;
                    }
                    if (!ok) continue;
                    for (int k = 0; k < L; k++) {
                        int c = (y + (vert ? k : 0)) * SB_N + x + (vert ? 0 : k);
                        if (s->sq[c] == OPEN) d[c] += w;
                    }
                }
    }
}

/* hard: whole fleets that agree with everything seen, drawn at random (each hit not yet sunk taken by a ship first,
 * then the other ships anywhere they may lie); freq[c]: the fleets with a ship on c (salvo: each counted as the
 * chance a shot there sinks it, one over its squares not hit: in a salvo, each ship sunk is a shot less coming
 * back, whatever its size). Returns the fleets drawn. */
static int sample(const sea *s, ai_rng *r, int tries, int salvo, double *freq)
{
    memset(freq, 0, sizeof(double) * SB_CELLS);
    int drawn = 0;
    for (int t = 0; t < tries; t++) {
        if ((t & 255) == 255 && ai_cancelled()) break;
        uint8_t occ[SB_CELLS] = {0};            /* 1 a ship, 2 next to one (the paper fleet) */
        double wt[SB_CELLS];
        int left[SB_MAXSHIPS], nl = s->n, ok = 1;
        for (int i = 0; i < nl; i++) left[i] = i;
        while (ok && nl) {
            int h = -1, nh = 0;               /* a hit no ship covers yet, at random */
            for (int c = 0; c < SB_CELLS; c++)
                if (s->sq[c] == HIT && occ[c] != 1 && ai_rand_n(r, ++nh) == 0) h = c;
            /* the ways: (ship, square, across or down), through h when there is one */
            int way[SB_MAXSHIPS * SB_CELLS * 2], nw = 0;
            for (int j = 0; j < nl; j++) {
                int L = s->len[left[j]];
                for (int vert = 0; vert < 2; vert++)
                    for (int y = 0; y < SB_N - (vert ? L - 1 : 0); y++)
                        for (int x = 0; x < SB_N - (vert ? 0 : L - 1); x++) {
                            int fit = 1, through = h < 0;
                            for (int k = 0; k < L && fit; k++) {
                                int c = (y + (vert ? k : 0)) * SB_N + x + (vert ? 0 : k);
                                fit = (s->sq[c] == OPEN || s->sq[c] == HIT) && !occ[c];
                                through |= c == h;
                            }
                            if (fit && through) way[nw++] = (j * SB_CELLS + y * SB_N + x) * 2 + vert;
                        }
                if (h < 0) break;               /* no hit to cover: this ship (the first left) anywhere */
            }
            if (!nw) { ok = 0; break; }
            int w = way[ai_rand_n(r, nw)], vert = w & 1, j = w / 2 / SB_CELLS, c0 = w / 2 % SB_CELLS;
            int L = s->len[left[j]], hit = 0;
            for (int k = 0; k < L; k++) hit += s->sq[c0 + (vert ? k * SB_N : k)] == HIT;
            for (int k = 0; k < L; k++) {
                int c = c0 + (vert ? k * SB_N : k);
                occ[c] = 1;
                wt[c] = salvo ? 1.0 / (L - hit > 0 ? L - hit : 1) : 1;
                if (!s->apart) continue;
                int x = c % SB_N, y = c / SB_N;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int nx = x + dx, ny = y + dy;
                        if (nx >= 0 && ny >= 0 && nx < SB_N && ny < SB_N && !occ[ny * SB_N + nx])
                            occ[ny * SB_N + nx] = 2;
                    }
            }
            memmove(left + j, left + j + 1, sizeof left[0] * (size_t)(--nl - j));   /* the longest left first */
        }
        for (int c = 0; c < SB_CELLS && ok; c++) ok = s->sq[c] != HIT || occ[c] == 1;   /* every hit explained */
        if (!ok) continue;
        drawn++;
        for (int c = 0; c < SB_CELLS; c++)
            if (occ[c] == 1 && s->sq[c] == OPEN) freq[c] += wt[c];
    }
    return drawn;
}

static int neighbour(int c, int dir)            /* 0 left, 1 right, 2 up, 3 down; -1: off the sea */
{
    int x = c % SB_N, y = c / SB_N;
    x += dir == 0 ? -1 : dir == 1 ? 1 : 0;
    y += dir == 2 ? -1 : dir == 3 ? 1 : 0;
    return x < 0 || y < 0 || x >= SB_N || y >= SB_N ? -1 : y * SB_N + x;
}

/* medium and easy: the squares worth trying next to the hits (medium: along a line of two hits first) */
static int targets(const sea *s, int line, int *out)
{
    int n = 0, best = 0;
    for (int c = 0; c < SB_CELLS; c++) {
        if (s->sq[c] != HIT) continue;
        for (int dir = 0; dir < 4; dir++) {
            int t = neighbour(c, dir);
            if (t < 0 || s->sq[t] != OPEN) continue;
            int back = neighbour(c, dir ^ 1), score = 1;
            if (line && back >= 0 && s->sq[back] == HIT) score = 2;   /* the line of hits goes on */
            if (score > best) { best = score; n = 0; }
            if (score == best) {
                int dup = 0;
                for (int i = 0; i < n; i++) dup |= out[i] == t;
                if (!dup) out[n++] = t;
            }
        }
    }
    return n;
}

/* one square to fire at, for a level */
static int aim(const sea *s, int level, ai_rng *r)
{
    int cand[SB_CELLS], n = 0;
    if (level >= AI_HARD) {
        double d[SB_CELLS], best = 0;
        density(s, d);
        for (int c = 0; c < SB_CELLS; c++) {
            if (s->sq[c] != OPEN) continue;
            if (d[c] > best * 1.000001) { best = d[c]; n = 0; }
            if (d[c] >= best * 0.999999 && d[c] > 0) cand[n++] = c;
        }
        if (n) return cand[ai_rand_n(r, n)];
    } else if (open_hits(s) && (level >= AI_MEDIUM || ai_rand_n(r, 100) < 70)) {
        n = targets(s, level >= AI_MEDIUM, cand);
        if (n) return cand[ai_rand_n(r, n)];
    }
    if (level >= AI_MEDIUM) {                   /* the hunt on diagonals as far apart as the smallest ship afloat: any
                                                 * ship covers one of their squares */
        int small = SB_N;
        for (int i = 0; i < s->n; i++) small = s->len[i] < small ? s->len[i] : small;
        for (int c = 0; c < SB_CELLS; c++)
            if (s->sq[c] == OPEN && (small < 2 || (c % SB_N + c / SB_N) % small == 0)) cand[n++] = c;
        if (!n)
            for (int c = 0; c < SB_CELLS; c++)
                if (s->sq[c] == OPEN) cand[n++] = c;
    } else {                                    /* easy: anywhere not shot (a miss's corner too: it does not know) */
        for (int c = 0; c < SB_CELLS; c++)
            if (s->sq[c] == OPEN || s->sq[c] == BLOCKED) cand[n++] = c;
    }
    return n ? cand[ai_rand_n(r, n)] : -1;
}

/* ---- placing ----------------------------------------------------------------------------------------------------- */
static int touching(const sbpos *p, const sbship *f)
{
    sbpos t = *p;
    t.apart = 1;
    return !sb_valid(&t, f);
}

/* a fleet of the bot's own (the modern fleet: five ships); 0: keep the one dealt */
static int place(const sbpos *p, int level, ai_rng *r, sbship *out)
{
    if (level <= AI_EASY || p->nships > 5) return 0;
    double heat[SB_CELLS], best = -1;
    sea empty = {{0}, {0}, 0, p->apart};
    for (int i = 0; i < p->nships; i++) empty.len[empty.n++] = p->len[i];
    density(&empty, heat);
    for (int k = 0; k < (level >= AI_HARD ? 12 : 1); k++) {
        sbship f[SB_MAXSHIPS];
        uint32_t seed = ai_rand(r) | 1;
        for (int tries = 0; tries < 50; tries++) {
            sb_random(p, &seed, f);
            if (!touching(p, f)) break;
        }
        double h = 0;
        for (int i = 0; i < p->nships; i++) {
            int c[SB_N], m = sb_cells(&f[i], c);
            for (int j = 0; j < m; j++) h += heat[c[j]];
        }
        if (best < 0 || h < best) { best = h; memcpy(out, f, sizeof f); }
    }
    return 1;
}

/* ---- the move ---------------------------------------------------------------------------------------------------- */
static void square(int c, char *s) { sprintf(s, "%c%d", 'a' + c % SB_N, c / SB_N + 1); }

/* the move for the seat to play in p (its own view: sunk[i] the other fleet's ships sunk), in the notation */
int sbb_move(const sbpos *p, const uint8_t *sunk, int level, uint64_t seed, char *move, int cap)
{
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)p->turn * 131 + p->phase));
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    char buf[160] = "", w[16];
    if (p->phase == SP_PLACE) {
        sbship f[SB_MAXSHIPS];
        if (place(p, level, &r, f))
            for (int i = 0; i < p->nships; i++) {
                square(f[i].y * SB_N + f[i].x, w);
                snprintf(buf + strlen(buf), sizeof buf - strlen(buf), "%d=%s%c ", i, w, f[i].vert ? 'v' : 'h');
            }
        strcat(buf, "ok");
    } else if (p->phase == SP_SHOOT) {
        sea s;
        read_sea(p, !p->turn, sunk, &s);
        int n = sb_shots(p), drawn = 0;
        double freq[SB_CELLS];
        if (level >= AI_HARD) drawn = sample(&s, &r, HARD_FLEETS, p->salvo, freq);
        for (int k = 0; k < n; k++) {
            int c = -1;
            if (drawn) {                        /* hard: the square most of the fleets drawn put a ship on */
                double best = 0;
                int ties = 0;
                for (int q = 0; q < SB_CELLS; q++) {
                    if (s.sq[q] != OPEN || freq[q] <= 0 || freq[q] < best) continue;
                    if (freq[q] > best) best = freq[q], ties = 0;
                    if (ai_rand_n(&r, ++ties) == 0) c = q;
                }
            }
            if (c < 0) c = aim(&s, level, &r);
            if (c < 0)                          /* (never: n is at most the squares not shot) */
                for (c = 0; c < SB_CELLS && (p->shot[!p->turn][c] != SHOT_NONE || s.sq[c] == MISSED); c++) {}
            if (c >= SB_CELLS) return 0;
            s.sq[c] = MISSED;                   /* the next shot of the salvo goes elsewhere */
            square(c, w);
            if (k) strcat(buf, "+");
            strcat(buf, w);
        }
    } else {
        return 0;
    }
    if ((int)strlen(buf) >= cap) return 0;
    strcpy(move, buf);
    return 1;
}

/* the seat's view of the game: the other fleet's ships still afloat wiped (all of it while placing) */
void sbb_view(const sbpos *live, int seat, sbpos *p, uint8_t *sunk)
{
    *p = *live;
    int them = !seat;
    for (int i = 0; i < p->nships; i++) {
        sunk[i] = p->phase != SP_PLACE && sb_sunk(live, them, i);
        if (!sunk[i]) p->ship[them][i] = (sbship){0, 0, p->ship[them][i].len, 0};
    }
    p->rng = 0;                                 /* the game's dice (the fleets were dealt with them) */
}

static int think(int seat, int level, char *move, int cap)
{
    sbpos all = seabattle_live()->pos, p;       /* the copy, at entry: nothing else is read */
    uint8_t sunk[SB_MAXSHIPS];
    sbb_view(&all, seat, &p, sunk);             /* then only what the seat may see */
    if (p.phase == SP_OVER || p.turn != seat) return 0;
    int ok = sbb_move(&p, sunk, level, ai_session_seed(), move, cap);
    return ok && !ai_cancelled();
}

static const bot seabattle_bot = {.think = think, .play = seabattle_show};
REGISTER_BOT(seabattle, &seabattle_bot)
