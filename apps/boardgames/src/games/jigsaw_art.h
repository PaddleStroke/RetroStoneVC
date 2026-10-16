/*
 * Jigsaw's paintings as bytes: art/jigsaw/<name>.jpg (public-domain paintings from Wikimedia Commons, about 1024 px,
 * tools/jigsaw_paintings.txt), embedded by tools/gen_art.py into the build's art_data.c, sorted by name. The game
 * finds a picture of jigsaw_pictures.h by its name.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef JIGSAW_ART_H
#define JIGSAW_ART_H

typedef struct { const char *name; const unsigned char *jpg; int size; } jigsaw_jpeg;
extern const jigsaw_jpeg jigsaw_art[];
extern const int jigsaw_art_n;

#endif
