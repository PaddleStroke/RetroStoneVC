# Biscuit Bandit

Endless bakery catching: Left/Right moves a raccoon with smooth D-pad acceleration.
A starts the actual frozen title scene. Every caught cookie immediately earns
10points and accumulates on the plate; there is no capacity limit, basket,
delivery action or emptying pause. One missed/spilled cookie or caught cup
immediately eliminates that player. Multiplayer survivors continue.

The plate shows a compact overlapping heap, with nine representative cookies
retaining Q16 inertia and tilt. Further catches settle into that heap rather
than growing above the screen. Total collected cookies and score keep increasing
independently of visible heap size. Integer counters safely saturate at INT_MAX.
Spilled cookies leave the heap and fall out naturally during loss feedback.

The conveyor already holds the first visible cookie behind the title. New items
arrive every80frames initially, decreasing gradually to40frames over two minutes.
They spend72frames moving visibly along the belt before falling from the actual
visible position. More items enter while earlier cookies are still moving/falling;
there is always a visible stream, without a delivery break. Belt slats animate
with world time and freeze on the title, pause and final results.

Drop positions travel between seeded outer zones across x44..276, reaching both
sides instead of clustering in the middle. Consecutive steps are limited to
104pixels initially, shrinking to56pixels at the fastest cadence; destinations
and step lengths vary by seed. Reaching an outer destination starts a journey
toward the opposite side. Every fifth item is a recognizable cup. The next
journey follows the inward56pixel dodge away from the cup, making avoidance
compatible with catching every cookie even at the fastest interval. No destination
chutes, landing marks or prediction guides are drawn. Falling speed still increases
gradually from2.37to2.73pixels/frame. Both speed and spacing have fixed caps.

One full320x240 cafe, full-size raccoons, independent claims on the same items,
no player collision or priority penalty. Scores are immediate and independent.
The title preserves the exact world/RNG arrangement when A starts. Joining only
rebuilds the frozen start. Pause freezes world and animation. Versioned checked
uint32 SRAM records remain outside save states; the run-best snapshot is registered.

Code-drawn house art: striped tails/masks, colored aprons, wide trays, cookies,
cups, moving conveyor and muted cafe windows, shelves and tiled floor.

Validation covers a1000-cookie capacity fixture, immediate scoring, overflow,
independent multiplayer catches/failures, D-pad inertia, visible spills and
100seeded five-minute four-player runs at maximum stream pace, plus30complete
opening-to-cap runs. These require
all players to survive, catch over300cookies, never miss and never see an idle
stream. Distribution checks require repeated catches in both outer zones and
large movements away from the center, and enforce the56pixel step bound at cap.
Another20cap courses allow an eight-frame reaction pause after each catch;
all four players still collect every cookie and avoid every cup.
Full lobby/input/pause/SRAM/rendering/libretro/save-state checks run too.
