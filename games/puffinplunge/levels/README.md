# Puffin Plunge: the chunks

`chunks.txt` holds the hand-made pieces of the well. A level (src/world.c `world_start_level`) piles up 9 + its
number of them (9 for level 1-1, 20 for 4-3), picked at random among its zone's, never the same twice in a row, each
maybe mirrored, between a landing ledge at the top and the bottom's floor (the boss's cave on the last level).

```
chunk NAME ZONES [WEIGHT]
```
then 12 rows of 10 characters: the shaft's inner columns (its walls are added on both sides). ZONES: the zones that use
the chunk (`1`..`4`, several digits, or `*` for all); WEIGHT 1..9 (default 1): how often it comes.

| | |
|---|---|
| `.` | empty |
| `#` | rock |
| `?` | rock or empty (55% rock) |
| `c` | a crystal block: two bubbles break it, gems often inside |
| `g` `G` | a gem, a big gem (5) |
| `w` | a walker (zone 1 snail, 2 crab, 3 beetle, 4 woolly grub): settles on the rock below |
| `f` | a flyer (bat, fish, moth, ice bat) |
| `x` | a spiky one (an urchin: crystal, sea, thorny, ice): never stomp it |
| `j` | a floater (zone 1 a bat, 2 and 3 a jellyfish, 4 a frost wisp) |
| `t` | the zone's special one (snail, pufferfish, puffcap on the rock below, icicle under the rock above) |
| `D` | a side room's door: in the first or last column, on rock (`#`); 70% of them open (a shop or a gift) |

A critter letter brings its critter more often the deeper the level (58% on level 1-1, 92% at most). Lines starting
with `# ` (a hash and a space) are comments; a row of rock starting with `#` is a row.

The rules: the top and bottom rows clear; from every column of the top row the puffin can reach the bottom row by
falling, walking and hopping up to two cells, without shooting (crystals and `?` count as rock).
`tests/test_world.c` checks every chunk (`make puffinplunge-check`).
