/*
 * RetroStone Arcade: the glue around a module (apps/launcher/tools/modulize.py compiles it twice per module).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   -DRSB_GLUE=1  the first object of the module: the markers at the START of its writable data and bss
 *   -DRSB_GLUE=2  the last object: the markers at the END, and the module's descriptor rsb_module_<RSB_ID>
 *   -DRSB_ID=<id> -DRSB_GAME=1 (a game: the descriptor points at its rs_game_main) or 0 (the house kit)
 *
 * modulize.py renames the game's .data / .data.rel / .data.rel.local into RSB_DATA and its .bss into RSB_BSS, then
 * links glue 1, the game's objects and glue 2 into one relocatable object (ld -r keeps the input order inside a
 * section), so the markers bound exactly the game's writable data. On Windows (PE/COFF) the linker sorts the
 * ".data$<suffix>" sections by suffix: each module gets its own suffixes _0 (start), _1 (the game), _2 (end).
 */
#include "rsb.h"
#include "rs_i18n.h"

#define RSB_STR2(x) #x
#define RSB_STR(x) RSB_STR2(x)
#define RSB_CAT2(a, b) a##b
#define RSB_CAT(a, b) RSB_CAT2(a, b)

#if defined(_WIN32)
#define RSB_DATA(n) ".data$rsg_" RSB_STR(RSB_ID) "_" #n
/* an uninitialised COFF section needs the assembler ("b" flag): a C variable would make it initialised data */
#define RSB_BSS_MARK(n, sym) \
    __asm__(".section .bss$rsg_" RSB_STR(RSB_ID) "_" #n ",\"bw\"\n\t.globl " sym "\n" sym ":\n\t.space 1\n\t.text\n")
#else
#define RSB_DATA(n) ".data.rsgame"
#endif

#if defined(__APPLE__)
/* Mach-O (the iOS app, modulize.py --flavor macho): no tool renames sections there, so the module's writable data is
 * compiled straight into sections of its own, __DATA,__rsgd<RSB_SECT> and __DATA,__rsgb<RSB_SECT> (a clang pragma,
 * -include'd by apps/ios/arcade/ios.mk), and the linker gives their bounds (section$start / section$end). Glue 1
 * makes sure both sections exist. */
#define RSB_MACHO(kind) "__DATA,__rsg" kind RSB_STR(RSB_SECT)
#define RSB_BOUND(edge, kind) __asm__("section$" edge "$__DATA$__rsg" kind RSB_STR(RSB_SECT))
#if RSB_GLUE == 1
__attribute__((section(RSB_MACHO("d")), used)) static char rsb_glue_data_mark[1] = {1};
__attribute__((section(RSB_MACHO("b")), used)) static char rsb_glue_bss_mark[1];
#else
extern char rsb_glue_data_begin[] RSB_BOUND("start", "d"), rsb_glue_data_end[] RSB_BOUND("end", "d");
extern char rsb_glue_bss_begin[] RSB_BOUND("start", "b"), rsb_glue_bss_end[] RSB_BOUND("end", "b");
#endif

#elif RSB_GLUE == 1
__attribute__((section(RSB_DATA(0)), used)) char rsb_glue_data_begin[1] = {1};
#if defined(_WIN32)
RSB_BSS_MARK(0, "rsb_glue_bss_begin");
#else
__attribute__((section(".bss.rsgame"), used)) char rsb_glue_bss_begin[1];
#endif

#else
extern char rsb_glue_data_begin[], rsb_glue_bss_begin[];
__attribute__((section(RSB_DATA(2)), used)) char rsb_glue_data_end[1] = {1};
#if defined(_WIN32)
RSB_BSS_MARK(2, "rsb_glue_bss_end");
extern char rsb_glue_bss_end[];
#else
__attribute__((section(".bss.rsgame"), used)) char rsb_glue_bss_end[1];
#endif
#endif

#if RSB_GLUE == 2
#if RSB_GAME
const rs_game *rs_game_main(void);
#define RSB_MAIN rs_game_main
/* The game's translated text (build/gen/<id>/game_text.c) registers its pack from a constructor, rs_i18n_register():
 * modulize.py renames that call to this one, so the runtime keeps each game's pack and installs it when the game
 * starts (in one program, the constructors of all the games run at load). */
void rsb_i18n_capture(const char *id, const struct rs_i18n_pack *pack);
void rsb_glue_i18n_register(const struct rs_i18n_pack *pack);
void rsb_glue_i18n_register(const struct rs_i18n_pack *pack) { rsb_i18n_capture(RSB_STR(RSB_ID), pack); }
#else
#define RSB_MAIN NULL
#endif

const rsb_module RSB_CAT(rsb_module_, RSB_ID) = {
    RSB_STR(RSB_ID), RSB_MAIN, rsb_glue_data_begin, rsb_glue_data_end, rsb_glue_bss_begin, rsb_glue_bss_end};
#endif
