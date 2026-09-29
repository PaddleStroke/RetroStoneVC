/*
 * Bomber Mole: level files (format in DESIGN.md).
 * All rights reserved, 8BCraft.
 */
#include "bm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const SEASON_NAMES[SEASONS] = {"spring", "summer", "autumn", "winter"};
const char *season_name(int s) { return (unsigned)s < SEASONS ? SEASON_NAMES[s] : "?"; }

static const struct { const char *name; int t; } TERRAIN_NAMES[] = {
    {"floor", TR_FLOOR}, {"stone", TR_STONE}, {"soft_dirt", TR_DIRT}, {"dirt", TR_DIRT},
    {"hard_rock", TR_ROCK}, {"rock", TR_ROCK}, {"roots", TR_ROOTS}, {"frozen_dirt", TR_FROZEN},
    {"leaves", TR_LEAVES}, {"water", TR_WATER}, {"puddle", TR_PUDDLE}, {"thin_floor", TR_THIN},
    {"hole_down", TR_HOLE_DOWN}, {"hole_up", TR_HOLE_UP}, {"ladder", TR_LADDER}, {"exit", TR_EXIT},
    {"bridge", TR_BRIDGE}, {"ice", TR_ICE}, {"thin_ice", TR_THIN_ICE}, {"mud", TR_MUD},
    {"tall_grass", TR_COVER}, {"corn", TR_COVER}, {"cover", TR_COVER}, {"burnt", TR_BURNT},
    {"gate", TR_GATE}, {"plate", TR_PLATE}, {"lever", TR_LEVER}, {"steam_vent", TR_VENT},
    {"vent", TR_VENT}, {"pipe", TR_PIPE}, {"crate", TR_CRATE}, {"sprinkler", TR_SPRINKLER},
    {"windmill", TR_WINDMILL}, {"beehive", TR_HIVE}, {"hive", TR_HIVE}, {"gas", TR_GAS},
    {"gas_pocket", TR_GAS},
};
static const char *const ITEM_NAMES[IT_COUNT] = {"none", "grub", "bomb", "fire", "speed", "remote", "heart"};
static const char *const DIR_NAMES[4] = {"up", "right", "down", "left"};

/* default legend: character -> spec */
static const struct { char c; const char *spec; } DEFAULT_LEGEND[] = {
    {'.', "floor"}, {'#', "stone"}, {'d', "soft_dirt"}, {'r', "hard_rock"}, {'t', "roots"},
    {'f', "frozen_dirt"}, {'l', "leaves"}, {'~', "water"}, {'p', "puddle"}, {'_', "thin_floor"},
    {'v', "hole_down"}, {'^', "hole_up"}, {'H', "ladder"}, {'E', "exit"},
    {'M', "floor+mole"}, {'2', "floor+p2"}, {'3', "floor+p3"}, {'4', "floor+p4"},
    {'F', "floor+ferret"}, {'C', "floor+cat"}, {'K', "floor+boss"}, {'z', "floor+ferret+asleep"},
    {'D', "floor+dog+asleep"},
    {'g', "floor+grub"}, {'G', "soft_dirt+grub"}, {'L', "leaves+grub"}, {'Q', "hard_rock+grub"},
    {'b', "floor+bomb"}, {'B', "soft_dirt+bomb"}, {'x', "floor+fire"}, {'X', "soft_dirt+fire"},
    {'s', "floor+speed"}, {'S', "soft_dirt+speed"}, {'o', "floor+remote"}, {'O', "soft_dirt+remote"},
    {'h', "floor+heart"}, {'Y', "soft_dirt+heart"},
    {'=', "bridge"}, {'&', "water+log"}, {'i', "ice"}, {'j', "thin_ice"}, {'m', "mud"},
    {'w', "tall_grass"}, {'P', "plate+chan:1"}, {'|', "gate+chan:1"}, {'/', "lever+chan:1"},
    {'V', "steam_vent"}, {'@', "pipe+chan:1"}, {'c', "crate"}, {'k', "sprinkler"}, {'W', "windmill"},
    {'<', "floor+push_left"}, {'>', "floor+push_right"}, {'n', "floor+push_up"}, {'u', "floor+push_down"},
    {'{', "water+flow_left"}, {'}', "water+flow_right"}, {'e', "beehive"}, {'*', "gas_pocket"},
};

typedef struct legend_entry {
    int used;
    cell c;
    int actor, asleep, player, log, etype, harv;
} legend_entry;

static int parse_spec(const char *spec, legend_entry *e, char *err, size_t errn)
{
    char buf[128];
    memset(e, 0, sizeof *e);
    e->used = 1;
    e->actor = AK_NONE;
    e->etype = -1;
    snprintf(buf, sizeof buf, "%s", spec);
    for (char *tok = strtok(buf, "+ \t"); tok; tok = strtok(NULL, "+ \t")) {
        int ok = 0;
        for (unsigned i = 0; i < sizeof TERRAIN_NAMES / sizeof TERRAIN_NAMES[0]; i++)
            if (!strcmp(tok, TERRAIN_NAMES[i].name)) { e->c.t = (uint8_t)TERRAIN_NAMES[i].t; ok = 1; }
        for (int i = 1; i < IT_COUNT && !ok; i++)
            if (!strcmp(tok, ITEM_NAMES[i])) { e->c.item = (uint8_t)i; ok = 1; }
        if (ok) continue;
        if (!strcmp(tok, "mole")) { e->actor = AK_MOLE; e->player = 0; }
        else if (tok[0] == 'p' && tok[1] >= '2' && tok[1] <= '4' && !tok[2]) { e->actor = AK_MOLE; e->player = tok[1] - '1'; }
        else if (!strcmp(tok, "ferret")) e->actor = AK_FERRET;
        else if (!strncmp(tok, "ferret:", 7)) { e->actor = AK_FERRET; e->etype = (int8_t)enemy_type_for(AK_FERRET, atoi(tok + 7)); }
        else if (!strncmp(tok, "cat:", 4)) { e->actor = AK_CAT; e->etype = (int8_t)enemy_type_for(AK_CAT, atoi(tok + 4)); }
        else if (!strcmp(tok, "cat")) e->actor = AK_CAT;
        else if (!strcmp(tok, "boss")) e->actor = AK_BOSS;
        else if (!strcmp(tok, "dog")) e->actor = AK_DOG;
        else if (!strcmp(tok, "asleep")) e->asleep = 1;
        else if (!strcmp(tok, "log")) e->log = 1;
        else if (!strncmp(tok, "chan:", 5)) e->c.chan = (uint8_t)clampi(atoi(tok + 5), 0, NCHAN - 1);
        else if (!strncmp(tok, "timed:", 6)) e->c.timed = (uint8_t)clampi(atoi(tok + 6), 1, 255);
        else if (!strncmp(tok, "harvester:", 10)) {     /* harvester parked here, first sweep that way */
            for (int d = 0; d < 4; d++)
                if (!strcmp(tok + 10, DIR_NAMES[d])) { e->harv = d + 1; ok = 1; }
            if (!ok) { snprintf(err, errn, "bad direction in '%s'", tok); return -1; }
        }
        else if (!strncmp(tok, "blow_", 5)) {           /* windmill: the direction the wind blows */
            for (int d = 0; d < 4; d++)
                if (!strcmp(tok + 5, DIR_NAMES[d])) { e->c.blow = (uint8_t)(d + 1); ok = 1; }
            if (!ok) { snprintf(err, errn, "bad direction in '%s'", tok); return -1; }
        }
        else if (!strncmp(tok, "push_", 5) || !strncmp(tok, "flow_", 5)) {
            for (int d = 0; d < 4; d++)
                if (!strcmp(tok + 5, DIR_NAMES[d])) {
                    e->c.pushdir = (uint8_t)(d + 1);
                    e->c.pushkind = tok[0] == 'p' ? PUSH_WIND : PUSH_FLOW;
                    ok = 1;
                }
            if (!ok) { snprintf(err, errn, "bad direction in '%s'", tok); return -1; }
        }
        else if (!ok) {
            for (int t = 0; t < ET_COUNT && !ok; t++)
                if (!strcmp(tok, ENEMY_TYPES[t].name)) {
                    e->actor = ENEMY_TYPES[t].kind;
                    e->etype = t;
                    ok = 1;
                }
            if (!ok) {
                snprintf(err, errn, "unknown legend word '%s'", tok);
                return -1;
            }
        }
    }
    if (e->c.t == TR_GATE || e->c.t == TR_PLATE || e->c.t == TR_LEVER || e->c.t == TR_PIPE)
        if (!e->c.chan) e->c.chan = 1;
    return 0;
}

static int season_of(const char *s)
{
    for (int i = 0; i < SEASONS; i++)
        if (!strcmp(s, SEASON_NAMES[i])) return i;
    return -1;
}

static int boss_of(const char *s)
{
    static const char *const names[] = {"none", "barncat", "farmer", "badger", "fox", "owl"};
    for (int i = 0; i < BOSS_KINDS; i++)
        if (!strcmp(s, names[i])) return i;
    return -1;
}

static void trim(char *s)
{
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) *--e = 0;
}

int level_parse(level_def *L, const char *text, size_t len, const char *fname)
{
    static legend_entry legend[128];
    char line[256];
    int section = -1, row = 0, rows[NDEPTH] = {0, 0, 0}, in_legend = 0, lineno = 0;
    memset(L, 0, sizeof *L);
    snprintf(L->file, sizeof L->file, "%s", fname ? fname : "?");
    L->bombs = 1;
    L->range = 2;
    L->harvest_period = 480;
    L->harvest_warn = 90;
    L->gust_period = 240;
    L->gust_active = 100;
    L->gust_step = 16;
    L->vent_period = 240;
    L->vent_active = 40;
    L->season = -1;
    L->boss = -1;
    memset(legend, 0, sizeof legend);
    for (unsigned i = 0; i < sizeof DEFAULT_LEGEND / sizeof DEFAULT_LEGEND[0]; i++)
        parse_spec(DEFAULT_LEGEND[i].spec, &legend[(unsigned char)DEFAULT_LEGEND[i].c], line, sizeof line);

    size_t pos = 0;
    while (pos < len) {
        size_t n = 0;
        while (pos < len && text[pos] != '\n') {
            if (n < sizeof line - 1) line[n++] = text[pos];
            pos++;
        }
        pos++;
        line[n] = 0;
        lineno++;
        trim(line);
        char *s = line;
        if (section >= 0 && row < GH) {          /* grid line: raw */
            if (!*s) continue;
            if ((int)strlen(s) != GW) {
                snprintf(L->error, sizeof L->error, "%s:%d: grid line must be %d chars (got %d)", L->file, lineno, GW, (int)strlen(s));
                return -1;
            }
            for (int x = 0; x < GW; x++) {
                legend_entry *e = &legend[(unsigned char)s[x]];
                if (!e->used) {
                    snprintf(L->error, sizeof L->error, "%s:%d: '%c' is not in the legend", L->file, lineno, s[x]);
                    return -1;
                }
                L->g[section][row][x] = e->c;
                if (e->actor != AK_NONE && L->nsp < 64) {
                    spawn *sp = &L->sp[L->nsp++];
                    sp->kind = (uint8_t)e->actor;
                    sp->depth = (uint8_t)section;
                    sp->x = (uint8_t)x;
                    sp->y = (uint8_t)row;
                    sp->asleep = (uint8_t)e->asleep;
                    sp->player = (uint8_t)e->player;
                    sp->etype = (int8_t)e->etype;
                }
                if (e->harv && L->nharv < MAX_HARV) {
                    L->harv[L->nharv].depth = (uint8_t)section;
                    L->harv[L->nharv].x = (uint8_t)x;
                    L->harv[L->nharv].y = (uint8_t)row;
                    L->harv[L->nharv].dir = (uint8_t)(e->harv - 1);
                    L->nharv++;
                }
                if (e->log && L->nlogs < MAX_LOGS) {
                    L->logs[L->nlogs].depth = (uint8_t)section;
                    L->logs[L->nlogs].x = (uint8_t)x;
                    L->logs[L->nlogs].y = (uint8_t)row;
                    L->nlogs++;
                }
            }
            rows[section] = ++row;
            continue;
        }
        while (*s == ' ' || *s == '\t') s++;
        if (!*s || *s == '#') continue;
        if (!strcmp(s, "surface:") || !strcmp(s, "under1:") || !strcmp(s, "under2:")) {
            section = s[0] == 's' ? 0 : s[5] - '0';
            row = 0;
            in_legend = 0;
            continue;
        }
        if (!strcmp(s, "legend:")) { in_legend = 1; continue; }
        if (in_legend) {
            /* "Z = soft_dirt + heart" */
            char *eq = strchr(s, '=');
            if (!eq || eq == s) {
                snprintf(L->error, sizeof L->error, "%s:%d: legend lines are 'C = spec'", L->file, lineno);
                return -1;
            }
            unsigned char c = (unsigned char)s[0];
            if (parse_spec(eq + 1, &legend[c], L->error, sizeof L->error)) return -1;
            continue;
        }
        char *colon = strchr(s, ':');
        if (!colon) {
            snprintf(L->error, sizeof L->error, "%s:%d: expected 'key: value'", L->file, lineno);
            return -1;
        }
        *colon = 0;
        char *v = colon + 1;
        while (*v == ' ') v++;
        char *hash = strstr(v, "  #");
        if (hash) { *hash = 0; trim(v); }
        if (!strcmp(s, "name")) snprintf(L->name, sizeof L->name, "%s", v);
        else if (!strcmp(s, "hint")) snprintf(L->hint, sizeof L->hint, "%s", v);
        else if (!strcmp(s, "music")) snprintf(L->music, sizeof L->music, "%s", v);
        else if (!strcmp(s, "season")) L->season = season_of(v);
        else if (!strcmp(s, "arc")) L->arc = atoi(v);
        else if (!strcmp(s, "level")) L->num = atoi(v);
        else if (!strcmp(s, "bombs")) L->bombs = clampi(atoi(v), 1, 8);
        else if (!strcmp(s, "range")) L->range = clampi(atoi(v), 1, 8);
        else if (!strcmp(s, "speed")) L->speed = clampi(atoi(v), 0, 3);
        else if (!strcmp(s, "status")) L->stub = !strcmp(v, "stub");
        else if (!strcmp(s, "dark")) L->dark = atoi(v);
        else if (!strcmp(s, "tier")) L->tier = clampi(atoi(v), 1, 4);
        else if (!strcmp(s, "boss")) L->boss = boss_of(v);
        else if (!strcmp(s, "gust")) sscanf(v, "%d %d %d", &L->gust_period, &L->gust_active, &L->gust_step);
        else if (!strcmp(s, "harvest")) sscanf(v, "%d %d", &L->harvest_period, &L->harvest_warn);
        else if (!strcmp(s, "vent")) sscanf(v, "%d %d", &L->vent_period, &L->vent_active);
        /* unknown keys are ignored: data hooks for later gimmicks */
    }
    if (L->season < 0) { snprintf(L->error, sizeof L->error, "%s: missing or bad 'season'", L->file); return -1; }
    for (int d = 0; d < NDEPTH; d++)
        if (rows[d] != GH) {
            snprintf(L->error, sizeof L->error, "%s: depth %d has %d rows (need %d)", L->file, d, rows[d], GH);
            return -1;
        }
    if (!L->tier) {                             /* the difficulty curve (DESIGN.md) */
        static const int by_season[SEASONS] = {1, 2, 3, 4};
        L->tier = L->season == SEASON_SPRING && L->num >= 5 ? 2 : by_season[L->season];
    }
    for (int i = 0; i < L->nsp; i++)
        if ((L->sp[i].kind == AK_FERRET || L->sp[i].kind == AK_CAT) && L->sp[i].etype < 0)
            L->sp[i].etype = (int8_t)enemy_type_for(L->sp[i].kind, L->tier);
    if (L->boss < 0) {
        static const int by_season[SEASONS] = {BOSS_CAT, BOSS_FARMER, BOSS_FOX, BOSS_OWL};
        L->boss = by_season[L->season];
    }
    /* checks: one mole, an exit on the surface, holes that line up, grubs */
    int moles = 0, exits = 0, grubs = 0, boss = 0;
    for (int i = 0; i < L->nsp; i++) {
        moles += L->sp[i].kind == AK_MOLE && L->sp[i].player == 0;
        boss += L->sp[i].kind == AK_BOSS;
    }
    for (int d = 0; d < NDEPTH; d++)
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++) {
                const cell *c = &L->g[d][y][x];
                grubs += c->item == IT_GRUB;
                if (c->t == TR_EXIT) exits += d == 0 ? 1 : 100;
                if ((c->t == TR_HOLE_DOWN || c->t == TR_THIN) && d == NDEPTH - 1) {
                    snprintf(L->error, sizeof L->error, "%s: hole or thin floor on the deepest level at %d,%d", L->file, x, y);
                    return -1;
                }
                if (c->t == TR_HOLE_DOWN) {
                    int b = L->g[d + 1][y][x].t;
                    if (b != TR_HOLE_UP && b != TR_LADDER) {
                        snprintf(L->error, sizeof L->error, "%s: hole down at %d,%d (depth %d) needs a hole up or ladder below", L->file, x, y, d);
                        return -1;
                    }
                }
                if ((c->t == TR_HOLE_UP || c->t == TR_LADDER) && (d == 0 || L->g[d - 1][y][x].t != TR_HOLE_DOWN)) {
                    snprintf(L->error, sizeof L->error, "%s: hole up/ladder at %d,%d (depth %d) needs a hole down above", L->file, x, y, d);
                    return -1;
                }
            }
    if (moles != 1) { snprintf(L->error, sizeof L->error, "%s: needs exactly one mole start (M), found %d", L->file, moles); return -1; }
    if (exits != 1) { snprintf(L->error, sizeof L->error, "%s: needs exactly one exit, on the surface", L->file); return -1; }
    if (grubs + boss < 1) { snprintf(L->error, sizeof L->error, "%s: needs at least one grub", L->file); return -1; }
    /* puddles make mud on the depth below */
    for (int d = 0; d < NDEPTH - 1; d++)
        for (int y = 0; y < GH; y++)
            for (int x = 0; x < GW; x++)
                if (L->g[d][y][x].t == TR_PUDDLE && L->g[d + 1][y][x].t == TR_FLOOR)
                    L->g[d + 1][y][x].t = TR_MUD;
    if (!L->name[0]) snprintf(L->name, sizeof L->name, "%s %d", season_name(L->season), L->num);
    if (!L->music[0]) snprintf(L->music, sizeof L->music, "%s", L->boss && boss ? "boss" : season_name(L->season));
    return 0;
}

int level_load(level_def *L, int arc, int num)
{
    char name[64];
    size_t n = 0;
    snprintf(name, sizeof name, "levels/%s-%d.txt", season_name(arc), num);
    const char *t = rs_asset(name, &n);
    if (!t) {
        memset(L, 0, sizeof *L);
        snprintf(L->error, sizeof L->error, "%s not found", name);
        return -1;
    }
    if (level_parse(L, t, n, name + 7)) {
        rs_log("level error: %s", L->error);
        return -1;
    }
    return 0;
}
