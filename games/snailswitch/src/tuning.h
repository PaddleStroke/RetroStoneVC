/*
 * Snail Switch: the numbers that shape a run (DESIGN.md "The course"). The rules (play.c) read the hit boxes and the
 * curl; the director (course.c) the segments' spacings, which are written in FRAMES at the course speed of the place
 * (px = frames * speed): the speed ramp and the difficulty ramp then add up instead of fighting.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/snailswitch/LICENSE.
 */
#ifndef SNAIL_TUNING_H
#define SNAIL_TUNING_H

/* ---- the rules ---------------------------------------------------------------------------------------------------- */
#define FLIP_FRAMES   22        /* the visible curl from one surface to the other */
#define HAZ_HALF      21        /* a hazard hits within this many px of its centre (|dx| < 21) ... */
#define HAZ_EXPOSED   6         /* ... a snail whose offset from the vine is beyond 6 px on its side */
#define FRUIT_HALF    15        /* a fruit is caught within |dx| < 15 ... */
#define FRUIT_REACH   12        /* ... by a snail beyond 12 px on its side; passed by (dx > 15) uncaught: out */
#define PATROL        18        /* a caterpillar wanders 18 px either side of its anchor ... */
#define PATROL_T      240       /* ... over 240 frames */
#define SPEED_DIST    24000     /* the speed rises from 2.4 to 3.8 px/frame over this many px */

/* ---- the course buffer (world.it): generated ahead, dropped behind; never looks at the screen's width -------------- */
#define ITEM_MAX      48
#define SEG_MAX_ITEMS 12        /* the most things one segment writes (a sap stretch of 5 with 2 fruits: 7) */
#define FILL_AHEAD    800       /* the director writes the course this far past the scroll (a 560-px screen + margin) */
#define KEEP_BEHIND   160       /* ... and kept this far behind it (the last snail is 81 px behind the first; a wide
                                 * screen shows up to 80 px behind the scroll) */

/* ---- the director (course.c) -------------------------------------------------------------------------------------- */
#define FIRST_X_MIN   450       /* the first segment's first thing: world x 450..510, 334..394 px ahead of the */
#define FIRST_X_MAX   510       /* leading snail (x 116) at every width: just past a 4:3 screen's edge */
#define DIFF_DIST     24000     /* the difficulty (0..256) rises linearly with distance up to here, with the speed */
#define PAIR_MEMORY   12        /* no pair of segment kinds repeats within the last 12 transitions */
#define RECENT_MEMORY 3         /* a kind met in the last 3 segments is 4x rarer (never twice in a row) */
#define GREEDY_COOL   7         /* at least 7 segments between two greedy fruits */
#define BREATH_LOAD_EASY 7      /* a breather after about this many things (easy) ... */
#define BREATH_LOAD_HARD 14     /* ... to this many (hard), +- a few */

/* Spacings, in frames at the course speed: {easy lo, easy hi, hard lo, hard hi}; the bounds slide with the difficulty
 * and each gap is drawn at random between them. A caterpillar adds its patrol (18 px) to the gaps on both its sides. */
#define GAP_LINK      {36, 60, 25, 38}  /* between two segments of a phrase ... */
#define GAP_PHRASE    {70, 110, 44, 72} /* ... and after a phrase (2 to 4 segments) */
#define GAP_BREATH    {80, 130, 60, 100}/* a breather: nothing at all */
#define GAP_TRAIL     {14, 22, 10, 16}  /* between the fruits of a trail along one side */
#define GAP_ZIGZAG    {44, 58, 27, 36}  /* between alternating fruits (pairs, trios: quick double flips) */
#define GAP_SLALOM    {48, 62, 29, 38}  /* between hazards on alternating sides */
#define GAP_AFTER     {30, 40, 19, 26}  /* a hazard, then a fruit just after it on the same side: flip back for it */
#define GAP_CLASSIC   {34, 50, 18, 28}  /* a fruit, then a hazard on its side: leave after it */
#define GAP_SAP       {13, 18, 12, 16}  /* between the drops of a sap stretch */
#define GAP_CATER     {50, 62, 40, 50}  /* between the caterpillars of a cluster */
#define GAP_GREEDY    {28, 34, 21, 26}  /* a hazard, the greedy fruit, a hazard (all on one side) */
#define GAP_FORK      {0, 6, 0, 4}      /* a hazard and a fruit on the other side of the vine, nearly level */

#endif
