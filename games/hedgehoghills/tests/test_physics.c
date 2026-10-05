#include "game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static world run(int policy,int players,int seed) {
    world w; world_init(&w,players,(uint32_t)seed);
    int input[4]={1,1,1,1}; world_step(&w,input);
    for(int t=0;t<7200 && !world_all_down(&w);t++) {
        for(int p=0;p<players;p++) input[p]=policy==0?0:policy==1?1:bot_decide(&w,p);
        world_step(&w,input);
        for(int p=0;p<players;p++) {
            assert(w.h[p].vx>=0);
            assert(w.h[p].y<=hill_y(&w,(int)(w.h[p].wx>>16))+Q16_ONE*2);
        }
    }
    return w;
}
int main(void) {
    for(int seed=1;seed<=12;seed++) {
        world coast=run(0,1,seed),hold=run(1,1,seed),skill=run(2,1,seed);
        printf("seed %d: coast=%d/%df hold=%d/%df skill=%d/%df launches=%d apples=%d combo=%d\n",
            seed,coast.h[0].score,coast.t,hold.h[0].score,hold.t,skill.h[0].score,skill.t,
            skill.h[0].launches,skill.h[0].apples,skill.h[0].combo);
        assert(skill.h[0].launches>0);
        assert(skill.h[0].score>hold.h[0].score);
        assert(skill.h[0].score>coast.h[0].score);
        world same=run(2,1,seed); assert(!memcmp(&skill,&same,sizeof skill));
    }
    world four=run(2,4,8);
    for(int p=0;p<4;p++) assert(four.h[p].score>0);
    world w;world_init(&w,1,4);
    int off[4]={0}; world_step(&w,off);assert(!w.started && !w.t);
    for(int x=1;x<10000;x++) assert(hill_y(&w,x)-hill_y(&w,x-1)<Q16_ONE*2);
    /* A rival's camera must never eliminate a player who is ahead of the frost. */
    world_init(&w,2,4); w.started=1; w.scroll=6000;
    for(int p=0;p<2;p++) {
        w.h[p].state=HS_RUN;
        w.h[p].wx=(p?500LL:6096LL)*Q16_ONE;
        w.h[p].y=hill_y(&w,(int)(w.h[p].wx>>16));
    }
    for(int t=0;t<30;t++) {
        int held[4]={bot_decide(&w,0),bot_decide(&w,1),0,0};
        world_step(&w,held);
        assert(w.h[1].state==HS_RUN && w.h[1].x<-3000);
    }
    w.frost=w.h[1].wx+100*Q16_ONE;
    world_step(&w,off);
    assert(w.h[1].state==HS_HIT && (w.events[1]&EV_HIT));
    puts("physics: all passed");
}
