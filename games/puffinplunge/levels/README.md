# Puffin Plunge: the chunks

`chunks.txt` holds the hand-made pieces of the endless well (src/world.c `gen_chunk`): chunk after chunk, each picked
at random among its zone's (from the chunk's number and the dive's seed alone), maybe mirrored; between them the gates
of the zones, the rest stops and the walrus's caves (DESIGN.md "The endless well").

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

A critter letter brings its critter more often the deeper the dive (56% in the first 180 m, 95% at most); a `?` is
rock less often deeper down. Lines starting
with `# ` (a hash and a space) are comments; a row of rock starting with `#` is a row.

The rules: the top and bottom rows clear; from every column of the top row the puffin can reach the bottom row by
falling, walking and hopping up to two cells, without shooting (crystals and `?` count as rock).
`tests/test_world.c` checks every chunk, and the well as it is made down to 4800 m (`make puffinplunge-check`).
