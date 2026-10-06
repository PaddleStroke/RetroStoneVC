/*
 * Penguin Slalom: the polar bears' tuning (DESIGN.md "Polar bears"). Distances are world pixels (the slope), times
 * are frames at 60 Hz. A bear is warned by a marker at the very spot it lands on, then slides straight down the
 * slope BEAR_SLIDE px per frame faster than the penguins.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/penguinslalom/LICENSE.
 */
#ifndef PS_TUNING_H
#define PS_TUNING_H

/* ---- the warning ------------------------------------------------------------------------------------------------ */
#define BEAR_WARN_EARLY   60        /* frames between the marker and the landing, at the start */
#define BEAR_WARN_LATE    50        /* ... once the course is fully ramped (BEAR_RAMP) */
#define BEAR_WARN_MIN     40        /* never shorter (the tests check every bear) */

/* ---- where a bear lands and how it slides ----------------------------------------------------------------------- */
#define BEAR_SLIDE        3         /* px per frame faster than the penguins (1.8 screen px per frame) */
#define BEAR_LAND_GAP     75        /* lands this far uphill of the uphill-most penguin (1 player: screen y 67) */
#define BEAR_LAND_TOP     (-115)    /* never further uphill of the camera than this (screen y 43: its sign under the HUD) */
#define BEAR_X_MIN        40        /* its centre: anywhere across the course (24..296), its body inside it */
#define BEAR_X_MAX        280
#define BEAR_RETIRE       280       /* gone once this far below the camera */

/* ---- difficulty: ramps with the distance, like the speed -------------------------------------------------------- */
#define BEAR_RAMP         24000     /* world px from the start to the full ramp */
#define BEAR_FIRST        300       /* frames before the first warning of a run */
#define BEAR_GAP_EARLY    330       /* frames between two warnings at the start, + 0..BEAR_GAP_RAND_EARLY */
#define BEAR_GAP_RAND_EARLY 150
#define BEAR_GAP_LATE     170       /* ... at the full ramp, + 0..BEAR_GAP_RAND_LATE */
#define BEAR_GAP_RAND_LATE 80
#define BEAR_LINE_EARLY   30        /* % of the bears aimed at the racing line (the flags' line) at the start */
#define BEAR_LINE_LATE    85        /* ... at the full ramp; the others land anywhere across the course */
#define BEAR_SPREAD_EARLY 40        /* an aimed bear lands within this many px of the racing line at the start */
#define BEAR_SPREAD_LATE  12        /* ... at the full ramp */
#define BEAR_TRIES        12        /* seeded spots tried per warning; the first fair one is taken */
#define BEAR_STEP_OUT     12        /* an aimed bear's next spot, further out from the racing line */
#define BEAR_RETRY        20        /* frames before trying again when none is fair */

/* ---- fairness (play.c bear_fair; proved on the exact steering by tests/test_fair.c) ----------------------------- */
#define BEAR_REACT        16        /* frames the player needs to react to the marker */
#define FAIR_CELL         4         /* the check's course columns (px) */
#define FAIR_STEP         4         /* ... and its steps (frames): one steering decision each; so a speed of one column
                                       per step is 1 px per frame */
#define FAIR_EASY         3         /* top speed (columns per step) of a "comfortable" skier: 3/4 of the penguin's 4 */
#define FAIR_DODGE        4         /* top speed to dodge a bear: the penguin's */
#define FAIR_ACCEL        2         /* speed change per step, both (the penguin: 2.4 px per frame in 4 frames) */
#define FAIR_TOP          4         /* the largest of the two speeds */
#define FAIR_MARGIN       4         /* px of clearance from the course and the bears already warned (both sets) */
#define FAIR_BEAR_MARGIN  14        /* px the dodge keeps from the new bear (more than the 8 the proof asks for: the
                                       check's columns are 4 px wide) */
#define FAIR_BEAR_STEPS   2         /* ... and that many steps longer at both ends */
#define FAIR_AFTER        100       /* frames of course still checked after the bear has passed */
#define FAIR_FRAMES       256       /* the longest window the check looks at */

#endif
