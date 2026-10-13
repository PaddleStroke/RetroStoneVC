/*
 * RetroStone Board's bots: the registry (bots.h). No SDL: the tests link it.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "bots.h"

#include <stdio.h>
#include <string.h>

static struct { const char *id; const bot *b; } reg[BOT_MAX];
static int nreg;

/* before main(): REGISTER_BOT's constructors (nothing else may run yet: no malloc, no order between files) */
void bot_register(const char *id, const bot *b)
{
    for (int i = 0; i < nreg; i++)
        if (!strcmp(reg[i].id, id)) { reg[i].b = b; return; }
    if (nreg >= BOT_MAX) { fprintf(stderr, "bots: too many (%s)\n", id); return; }
    reg[nreg].id = id;
    reg[nreg++].b = b;
}

const bot *bot_find(const char *id)
{
    for (int i = 0; id && i < nreg; i++)
        if (!strcmp(reg[i].id, id)) return reg[i].b;
    return NULL;
}

int bot_count(void) { return nreg; }
const char *bot_id(int i) { return i >= 0 && i < nreg ? reg[i].id : NULL; }

static unsigned seats;                          /* the main thread's only (the shell, or a test) */
unsigned bot_seats(void) { return seats; }
void bot_seats_set(unsigned mask) { seats = mask; }
