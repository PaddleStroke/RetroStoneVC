# Goat Gorge: the rooms

`ch1.txt` .. `ch5.txt`: each chapter's rooms in order. A room is a line `room [settings]` then 15 lines of 20
characters (one per 16-px cell). Lines before the first room and between rooms are comments. The rooms are built into
the game's asset pack; a copy in the frontend's data folder (`data/levels/chN.txt`) wins, so they can be tried without
rebuilding.

| Character | Cell |
|---|---|
| `.` | air |
| `#` | rock (autotiled: the chapter's cap on its tops) |
| `-` | a ledge: a wooden plank, solid from above only |
| `=` | a crumbling block: it shakes when stood on (or slid down), falls, comes back; a run of them falls together |
| `^` `v` | spikes pointing up (on a floor), down (under a ceiling) |
| `<` `>` | spikes pointing left (on a wall at their right), right (on a wall at their left) |
| `o` | a spring (a bouncy mushroom); rock must be under it |
| `*` | a crystal: it gives the dash back in the air, comes back after 150 frames |
| `B` | the berry (at most one): optional, counted when the room is left with it |
| `S` | the start (exactly one): the kid's feet on the cell's bottom |
| `H` / `V` | a raft (a run of them in a row: one raft) moving sideways / up and down |
| `x` | where a raft's path ends: in its row (`H`) or its column (`V`, the run's first cell) |
| `d` | the chapter's decoration (a flowering bush, mushrooms on a stump, crystals, a wind banner, a cairn), standing on the rock under it; not solid |

The way out: `exit=right`, `exit=left` or `exit=top` on the room line; then every other open side is sky (the kid meets an
unseen wall at the screen's sides, and may jump above the screen: a marker shows where, it comes back down). Without
`exit=`, the room must open on exactly one side (a gap in the left or the right column, else the top row). A
gap in the bottom row is a fall (a death).

Settings on the `room` line: `hint=N` shows a tip the first seconds (1 jump, 2 wall jump, 3 dash, 4 spring, 5 crystal,
6 crumbling, 7 raft, 8 wind, 9 berry); `wind=X,Y` a wind (1/256 px per frame: 192 pushes 0.75 px/f); `gust=ON,OFF`
the wind blows ON frames, then rests OFF frames.

After changing a room: `make goatgorge-solve` (tests/solver.c finds and records each room's routes in
`solutions.txt`; it fails on a room it cannot finish), then `make goatgorge-check`.
