/*
 * RetroStone Board's launcher icon: the app's own pictures, drawn by its own code (art.c, cards.c) at the icon's
 * resolution, for apps/android/tools/make_icon.py (ICON_STYLE=board), which lays them on the green table in the gold
 * frame. Built and run by apps/android/build.sh board (make -f apps/boardgames/Makefile icon-art).
 *
 *     icon_art <dir>      writes <dir>/knight.png, die.png, card.png (RGBA, straight alpha)
 *
 * Linked with the whole app (its main renamed board_main: -Dmain=board_main on shell.c), so the pictures are always
 * the app's current ones. MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "art.h"
#include "cards.h"
#include "games/chess.h"

static int save(const char *dir, const char *name, int w, int h, const uint8_t *rgba)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    if (!stbi_write_png(path, w, h, 4, rgba, w * 4)) {
        fprintf(stderr, "icon_art: cannot write %s\n", path);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: icon_art <dir>\n");
        return 2;
    }
    const char *dir = argv[1];
    enum { PIECE = 320, DIE = 150, CARD_W = 200 };
    int card_h = (int)(CARD_W * cards_aspect(DECK_FRENCH) + 0.5f);
    uint8_t *b = calloc((size_t)PIECE * PIECE * 4 + (size_t)CARD_W * card_h * 4, 1);
    if (!b) return 1;
    int err = 0;
    art_piece(KNIGHT, WHITE, PIECE, b);                     /* the chess family: the white knight */
    err |= save(dir, "knight.png", PIECE, PIECE, b);
    art_die(5, 1, DIE, b);                                  /* dice & race: an ebony die showing five */
    err |= save(dir, "die.png", DIE, DIE, b);
    cards_paint(DECK_FRENCH, CARD(SUIT_HEARTS, RANK_ACE), CARD_W, card_h, b);   /* cards: the ace of hearts */
    err |= save(dir, "card.png", CARD_W, card_h, b);
    free(b);
    return err;
}
