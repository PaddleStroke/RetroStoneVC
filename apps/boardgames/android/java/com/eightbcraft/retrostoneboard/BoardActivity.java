/*
 * RetroStone Board: the Android activity. Everything happens in native code (libmain.so: apps/boardgames/src, built
 * by make -f apps/boardgames/Makefile android); SDL's SDLActivity (org.libsdl.app) runs it: the surface, the touches
 * (each finger, and the first one as the mouse), audio, the Back key (SDLK_AC_BACK: the rules page closes, a game
 * goes home, the menu quits), the orientation (the app's SDL_HINT_ORIENTATIONS) and the app's lifecycle.
 * Built into the app by apps/android/build.sh board (apps/android/app-board.conf).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
package com.eightbcraft.retrostoneboard;

import org.libsdl.app.SDLActivity;

public class BoardActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        // The native code keeps its state in static memory (the games registered once, the quit flag set when Back
        // leaves the menu): when the app is closed, end the process so the next launch starts clean.
        if (isFinishing()) {
            System.exit(0);
        }
    }
}
