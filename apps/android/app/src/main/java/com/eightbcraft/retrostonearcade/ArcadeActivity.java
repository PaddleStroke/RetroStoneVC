/*
 * RetroStone Arcade: the Android activity. Everything happens in native code (libmain.so: the launcher, the games,
 * apps/launcher/src/rsb_sdl.c); SDL's SDLActivity (org.libsdl.app) runs it: the surface, touches, controllers, audio,
 * the Back button (SDL_HINT_ANDROID_TRAP_BACK_BUTTON), the app's lifecycle and SDL_OpenURL (the Play Store button).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
package com.eightbcraft.retrostonearcade;

import org.libsdl.app.SDLActivity;

public class ArcadeActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        // The native code keeps its state in static memory: when the app is closed (Back on the menu), end the
        // process so the next launch starts clean. The games' progress is already saved (rsb_suspend).
        if (isFinishing()) {
            System.exit(0);
        }
    }
}
