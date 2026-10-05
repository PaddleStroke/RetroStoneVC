/* Pure, fixed-point hill physics. MIT, (c) 2026 Pierre-Louis Boyer (8BCraft). */
#include "game.h"
#include "curve.h"
#include <string.h>
#include <stdlib.h>
static uint32_t mix(uint32_t n) {
    n ^= n >> 16; n *= 0x7feb352du; n ^= n >> 15; n *= 0x846ca68bu; return n ^ (n >> 16);
}
static int crest(const world *w, int n) { return 118 + (int)(mix((uint32_t)n ^ w->seed) % 29); }
int32_t hill_y(const world *w, int x) {
    if (x < 0) x = 0;
    int n=x/HILL_SPAN, u=x%HILL_SPAN, half=HILL_SPAN/2;
    int valley=198+(int)(mix((uint32_t)n+w->seed+933u)%23);
    int a=u<half?crest(w,n):valley, b=u<half?valley:crest(w,n+1);
    int phase=(u%half)*256/half, frac=(u%half)*256%half;
    int c=hh_curve[phase]+(hh_curve[phase+1]-hh_curve[phase])*frac/half;
    return a*Q16_ONE+(b-a)*c;
}
int32_t hill_slope(const world *w, int x) { return (hill_y(w,x+3)-hill_y(w,x-3))/6; }
static int32_t surface(const world *w, int64_t wx) {
    int x=(int)(wx>>16),f=(int)(wx&65535);
    int32_t a=hill_y(w,x),b=hill_y(w,x+1);
    return a+(int32_t)((int64_t)(b-a)*f/Q16_ONE);
}
void apple_pos(const world *w, int id, int *x, int *y) {
    int n=id/3, k=id%3;
    *x=n*HILL_SPAN+(k==0?135:k==1?245:345);
    *y=(hill_y(w,*x)>>16)-(k==2?48:16);
}
int hero_on_ground(const hero *h) { return h->grounded; }
void world_init(world *w, int players, uint32_t seed) {
    memset(w,0,sizeof *w); w->seed=seed; w->players=players<1?1:players>4?4:players;
    w->frost=-360LL*Q16_ONE;
    for(int p=0;p<MAX_PLAYERS;p++) {
        hero *h=&w->h[p]; h->state=p<w->players?HS_READY:HS_OFF;
        h->wx=(88-p*14)*Q16_ONE; h->x=(int)(h->wx>>16);
        h->y=hill_y(w,h->x); h->vx=4*Q16_ONE; h->grounded=1;
        h->air_t=h->land_t=99; h->apple_id=-1;
    }
}
int world_running(const world *w) {
    for(int p=0;p<w->players;p++) if(w->h[p].state==HS_RUN) return 1;
    return 0;
}
int world_all_down(const world *w) {
    for(int p=0;p<w->players;p++) if(w->h[p].state!=HS_DOWN) return 0;
    return 1;
}
int bot_decide(const world *w, int p) {
    const hero *h=&w->h[p];
    int x=(int)(h->wx>>16);
    if(h->state==HS_READY) return 1;
    /* Look slightly ahead: release just before the valley. In flight, dive toward a downhill landing. */
    int look=x+8+p*3;
    if(h->grounded) return hill_slope(w,look)>Q16_ONE/14;
    int d=(hill_y(w,x)-h->y)>>16;
    return h->vy>0 && hill_slope(w,x+18)>Q16_ONE/8 && d>15;
}
void world_step(world *w, const int held[MAX_PLAYERS]) {
    memset(w->events,0,sizeof w->events);
    if(!w->started) {
        int start=0;
        for(int p=0;p<w->players;p++) start|=held[p];
        if(!start) return;
        w->started=1;
        for(int p=0;p<w->players;p++) { w->h[p].state=HS_RUN; w->events[p]=EV_START; }
    }
    w->t++;
    if(world_running(w)) {
        int speed=Q16_ONE*48/10+(w->t<5000?w->t:5000)*42;
        if(speed>Q16_ONE*80/10) speed=Q16_ONE*80/10;
        w->frost+=speed;
    }
    int lead=w->scroll+HERO_X;
    for(int p=0;p<w->players;p++) {
        hero *h=&w->h[p]; h->t++; h->air_t++; h->land_t++;
        if(h->smooth_t>0) h->smooth_t--;
        if(h->state==HS_HIT) {
            int32_t floor=surface(w,h->wx);
            h->vy+=Q16_ONE/2; h->y+=h->vy;
            if(h->y>=floor) { h->y=floor; h->vy=0; h->grounded=1; }
            if(h->t>=26 && h->grounded) { h->state=HS_DOWN; h->t=0; }
            continue;
        }
        if(h->state!=HS_RUN) continue;
        h->held=held[p]!=0;
        int oldx=(int)(h->wx>>16);
        int32_t slope=hill_slope(w,oldx);
        if(h->grounded) {
            h->vx+=(int32_t)((int64_t)slope*(h->held?52:24)/100)+Q16_ONE/100;
            /* Curling while climbing costs momentum; releasing preserves the launch. */
            if(h->held && slope<0) h->vx=h->vx*94/100;
            else h->vx=h->vx*999/1000;
            if(h->vx<Q16_ONE*18/10) h->vx=Q16_ONE*18/10;
            if(h->vx>Q16_ONE*135/10) h->vx=Q16_ONE*135/10;
            h->wx+=h->vx;
            int32_t floor=surface(w,h->wx);
            int32_t dy=floor-h->y;
            if(!h->held && h->land_t>8 && slope < -Q16_ONE/5 && dy>h->vy+Q16_ONE/10) {
                h->grounded=0; h->air_t=0; h->launches++; h->y+=h->vy;
                w->events[p]|=EV_JUMP;
            } else { h->y=floor; h->vy=dy; }
        } else {
            h->wx+=h->vx;
            h->vy+=h->held?Q16_ONE*65/100:Q16_ONE*16/100;
            if(h->vy>Q16_ONE*14) h->vy=Q16_ONE*14;
            h->y+=h->vy;
            int nx=(int)(h->wx>>16);
            int32_t floor=surface(w,h->wx);
            if(h->y>=floor) {
                int32_t tangent=(int32_t)((int64_t)hill_slope(w,nx)*h->vx/Q16_ONE);
                int32_t impact=h->vy-tangent;
                h->grounded=1; h->y=floor; h->vy=tangent; h->land_t=0;
                w->events[p]|=EV_LAND;
                if(h->air_t>10 && hill_slope(w,nx)>Q16_ONE/12 && impact<Q16_ONE*6) {
                    h->combo++; if(h->combo>9) h->combo=9;
                    h->vx+=Q16_ONE*9/10; h->bonus+=h->combo*3; h->smooth_t=75;
                    w->events[p]|=EV_SCORE;
                } else if(h->air_t>10) {
                    h->combo=0; h->vx=h->vx*72/100; h->smooth_t=-1;
                    h->spill=h->apples<3?h->apples:3;
                    h->apples-=h->spill; h->bonus-=h->spill*5;
                }
            }
        }
        int x=(int)(h->wx>>16);
        int first=(x/HILL_SPAN)*3;
        for(int a=first;a<first+3;a++) {
            int ax,ay; apple_pos(w,a,&ax,&ay);
            if(a>h->apple_id && abs(ax-x)<13 && abs(ay-((h->y>>16)-12))<16) {
                h->apple_id=a; h->apples++; h->bonus+=5; w->events[p]|=EV_APPLE;
            }
        }
        h->score=(x-88)/16+h->bonus;
        if(h->score<0) h->score=0;
        if(h->wx<=w->frost) {
            h->state=HS_HIT; h->t=0; h->combo=0; w->events[p]|=EV_HIT;
        } else if(x>lead) lead=x;
    }
    if(lead-HERO_X>w->scroll) w->scroll=lead-HERO_X;
    for(int p=0;p<w->players;p++) w->h[p].x=(int)(w->h[p].wx>>16)-w->scroll;
}
