/*
 * The iPhone and iPad apps' platform helpers (ios_app.h): the safe area, and the program's entry point.
 * Objective-C with ARC (CLANG_ENABLE_OBJC_ARC in apps/ios/<app>/project.yml).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#import <UIKit/UIKit.h>
#include <math.h>

#include "ios_app.h"
#include <SDL_syswm.h>

void ios_safe_area(SDL_Window *win, int w, int h, SDL_Rect *safe)
{
    safe->x = safe->y = 0;
    safe->w = w;
    safe->h = h;
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!win || w <= 0 || h <= 0 || !SDL_GetWindowWMInfo(win, &info) || info.subsystem != SDL_SYSWM_UIKIT) return;
    UIWindow *window = info.info.uikit.window;
    UIView *view = window.rootViewController.view ? window.rootViewController.view : window;
    CGSize size = view.bounds.size;             /* points: the insets' unit */
    if (size.width <= 0 || size.height <= 0) return;
    UIEdgeInsets in = view.safeAreaInsets;
    double kx = w / size.width, ky = h / size.height;
    int l = (int)ceil(in.left * kx), r = (int)ceil(in.right * kx), t = (int)ceil(in.top * ky),
        b = (int)ceil(in.bottom * ky);
    if (l < 0 || r < 0 || t < 0 || b < 0 || l + r >= w / 2 || t + b >= h / 2) return;   /* nothing sensible */
    safe->x = l;
    safe->y = t;
    safe->w = w - l - r;
    safe->h = h - t - b;
}

/* the entry point: UIKit first (SDL's application delegate), then the app's main, renamed SDL_main by SDL_main.h.
 * (SDL's static library has the same main, SDL_uikit_main.c: this one makes the link independent of it.) */
#undef main
int main(int argc, char *argv[])
{
    return SDL_UIKitRunApp(argc, argv, SDL_main);
}
