/*
 * Map Quiz in RetroStone Board: a map, a place's name, find it (mapquiz.c draws and plays; this file and
 * mapquiz_rules.c the maps and the quiz, without SDL: the tests link them). The maps come from Natural Earth (public
 * domain), made by tools/mapquiz_gen.py into mapquiz_data.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A map: a grid (w x h units, 16000 at most), its regions (a country, a state; kind: asked, other land, disputed), its
 * arcs (the lines between two regions or along a coast, each once) and its rings (a region's outlines, made of arcs).
 * Loaded, it holds every ring's points, every region's fill as triangles (ear clipping), and its arcs' points.
 */
#ifndef MAPQUIZ_H
#define MAPQUIZ_H

#include <stdint.h>

typedef struct {
    const char *code;                           /* "FRA", "US-CA", "FR-BRETAGNE", "D_KASHMIR" */
    int16_t name;                               /* its line in mapquiz_words.txt after the prompt (-1: none) */
    uint8_t kind, tone, flags;                  /* MQ_ASK..; a parchment tone (neighbours differ); MQ_IN_INSET */
    int16_t lx, ly;                             /* a point well inside it (its marker) */
    float area;                                 /* grid units squared */
} mq_region_def;

typedef struct {
    const char *id;                             /* "world", "usa" */
    int w, h, nregions;
    const mq_region_def *regions;
    int npts, nbytes;
    const int8_t *pts;                          /* the arcs' points, as steps (mapquiz_gen.py) */
    int narcs;
    const uint16_t *arcs;                       /* each arc's count of points */
    const uint8_t *arck;                        /* each arc's kind: MQ_COAST.. | MQ_ARC_INSET */
    int nring;
    const int16_t *rings;                       /* region, outer (1) or hole (0), n, n arcs (~i: arc i backwards) */
    int nframes;
    const int16_t *frames;                      /* the insets' frames: x0, y0, x1, y1 */
} mq_map_def;

typedef struct {
    uint8_t chapter, map;
    const char *codes;                          /* the regions asked, between spaces; "*": the map's set */
} mq_level_def;

enum { MQ_ASK, MQ_OTHER, MQ_DISPUTED };
enum { MQ_IN_INSET = 1 };
enum { MQ_COAST, MQ_INNER, MQ_BORDER, MQ_EDGE, MQ_ARC_INSET = 4 };

#include "games/mapquiz_count.h"            /* MQ_MAPS, MQ_LEVELS, MQ_CHAPTERS, MQ_NAMES (mapquiz_gen.py) */

/* the levels' chapters: Europe, Asia, Africa, the Americas, Oceania, the world, then the countries' states */
#define MQ_VARIANTS MQ_MAPS                     /* the random tab: a card a map */
#define MQ_MAX_REG 256                          /* the most regions a map has */

typedef struct {
    int first, count;                           /* its points in m->rp (x, y pairs) */
    int region, outer;
} mq_ring;

typedef struct {
    const char *code;
    int name, kind, tone, flags;
    int ring0, nring;                           /* its rings (consecutive in m->ring) */
    int i0, ni;                                 /* its triangles' corners in m->fi (points of m->rp) */
    float x0, y0, x1, y1;                       /* its bounding box */
    float lx, ly, area;
} mq_reg;

typedef struct {
    int map, w, h;
    int nreg;
    mq_reg reg[MQ_MAX_REG];
    int nring;
    mq_ring *ring;
    float *rp;                                  /* the rings' points */
    int nrp;
    int *fi;                                    /* the fills' triangles */
    int nfi;
    float *ap;                                  /* the arcs' points */
    int nap;
    int narc;
    int *as;                                    /* arc i: points as[i] .. as[i + 1] - 1 of ap */
    uint8_t *ak;
    float *ab;                                  /* each arc's bounding box: x0, y0, x1, y1 */
    int order[MQ_MAX_REG];                      /* the regions as drawn: the biggest first (enclaves over them) */
} mq_map;

int  mq_map_load(mq_map *m, int map);           /* 0: no memory */
int  mq_map_load_ex(mq_map *m, int map, int fills);   /* fills 0: no triangles (a picture painted, a hit found) */
void mq_map_free(mq_map *m);
int  mq_find(const mq_map *m, const char *code);   /* a region by its code, -1 */
/* the region under a grid point: inside one (the smallest if several), else the nearest edge within slack grid
 * units; -1 none. Inside an inset's frame only that inset's regions count. */
int  mq_hit(const mq_map *m, float x, float y, float slack);
int  mq_inside(const mq_map *m, int region, float x, float y);
/* a ring (n points of p from first, x, y pairs) cut into triangles: their corners (indices into p) in out, 3 (n - 2)
 * of them; returns how many */
int  mq_earclip(const float *p, int first, int n, int *out);
/* the map painted into a picture (w x h, rgba): grid point (x, y) at (ox + x * s, oy + y * s); colour(region, ctx,
 * rgba[4]) gives each region's colour (alpha 0: not painted), coverage smoothed */
typedef void (*mq_colour)(const mq_map *m, int region, void *ctx, uint8_t rgba[4]);
void mq_paint(const mq_map *m, float s, float ox, float oy, int w, int h, uint8_t *rgba, mq_colour colour, void *ctx);

/* ---- the quiz ---------------------------------------------------------------------------------------------------- */
#define MQ_TRIES 3                              /* misses on a prompt before its answer is shown */
enum { MQ_NONE, MQ_FOUND, MQ_MISSED };          /* a region's state */
enum { MQ_RIGHT, MQ_WRONG, MQ_GIVEN, MQ_OUT, MQ_NOTHING };   /* what a tap did */

typedef struct {
    int map;
    int n;                                      /* regions asked */
    int16_t ask[MQ_MAX_REG];                    /* in the order asked */
    uint8_t in[MQ_MAX_REG];                     /* 1: in this quiz */
    uint8_t state[MQ_MAX_REG];                  /* MQ_NONE, MQ_FOUND, MQ_MISSED */
    int cur;                                    /* the prompt: ask[cur]; n: done */
    int tries;                                  /* misses on this prompt */
    int misses;                                 /* wrong taps in all */
    int given;                                  /* prompts whose answer was shown */
} mq_quiz;

/* the regions of a level (or of a map's set: codes "*" or NULL) into out; returns how many (0: a bad code) */
int  mq_set(const mq_map *m, const char *codes, int16_t *out);
/* a new quiz of these regions in an order from seed */
void mq_quiz_start(mq_quiz *q, const mq_map *m, const int16_t *regions, int n, uint32_t seed);
int  mq_target(const mq_quiz *q);               /* the region asked now, -1: done */
int  mq_done(const mq_quiz *q);
/* a tap on a region (-1: on the sea): MQ_RIGHT (found), MQ_WRONG (a miss), MQ_GIVEN (the last miss: the answer is
 * shown and marked missed, the next prompt comes), MQ_OUT (land not in the quiz: nothing counted), MQ_NOTHING */
int  mq_tap(mq_quiz *q, const mq_map *m, int region);
/* the stars: misses at most a tenth of the regions and within the time (8 s a region and 15 s): 3; misses at most a
 * quarter of them and one: 2; else 1 */
int  mq_stars(const mq_quiz *q, uint32_t ms);
uint32_t mq_time_limit(int n);

/* the generated tables (mapquiz_data.h is compiled in mapquiz_rules.c only) */
const mq_map_def *mq_def(int map);
const mq_level_def *mq_level(int level);
int  mq_chapter_of(int level);
int  mq_chapter_map(int chapter);               /* the map a chapter's icon shows */
const char *mq_map_set(int map);                /* a map's set: the continent's countries, "*": its states */

#endif
