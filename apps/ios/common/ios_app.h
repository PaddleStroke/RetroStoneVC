/*
 * The iPhone and iPad apps' platform helpers (apps/ios/common/ios_app.m), for both apps (RetroStone Board, RetroStone
 * Arcade). Their C code includes this only on iOS (#ifdef __IPHONEOS__, SDL's platform macro).
 * ios_app.m also holds the program's entry point: main() starts UIKit (SDL_UIKitRunApp), which calls the app's own
 * main (renamed SDL_main by SDL_main.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef IOS_APP_H
#define IOS_APP_H

#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the part of the window clear of the notch, the Dynamic Island, the rounded corners and the home indicator
 * (UIKit's safeAreaInsets), in the pixels of the renderer's output (w x h: SDL_GetRendererOutputSize); the whole
 * w x h when UIKit gives nothing usable. Cheap: call it once a frame (it follows the device's turns). */
void ios_safe_area(SDL_Window *win, int w, int h, SDL_Rect *safe);

#ifdef __cplusplus
}
#endif

#endif
