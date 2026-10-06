/* Mouse Espresso: the cafe items painted into an 8-bit canvas (paint.c). */
#ifndef ME_PAINT_H
#define ME_PAINT_H
#include "game.h"
typedef struct canvas { uint8_t *p; int w,h,stride; } canvas;
/* one item at frame t; left: the world x of the canvas's column 0 */
void paint_item(const canvas *c,const item *it,const item_pose *p,int left,int t);
/* every item that reaches the canvas */
void paint_course(const canvas *c,const course *cs,int left,int t);
#endif
