# Otter Rapids art maintenance

All artwork is code drawn. Otter sprites, player hues, golden fish and title lettering use the house kit through tools/make_art.py and tools/build_assets.py. The top-down river, rocks, logs, whirlpool spirals, reeds and currents are drawn in src/draw.c with the same material palettes.

Review native-size and four-player screenshots after art changes. Characters keep their full size, outline, eyes and shaded silhouettes. Moving foam and shadows remain attached to their hazard; the water covers the full screen. The title displays the frozen actual starting otters with identifying player labels.
