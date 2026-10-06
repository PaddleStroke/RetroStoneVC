/*
 * RetroStone Arcade: the Android activity. Everything happens in native code (libmain.so: the launcher, the games,
 * apps/launcher/src/rsb_sdl.c); SDL's SDLActivity (org.libsdl.app) runs it: the surface, touches, controllers, audio,
 * the Back button (SDL_HINT_ANDROID_TRAP_BACK_BUTTON), the app's lifecycle and SDL_OpenURL (the Play Store button).
 * Here only: the end of the process, and the Google Play review prompt (requestReview, called by the native code when
 * apps/launcher/src/rsb_review.h says so).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
package com.eightbcraft.retrostonearcade;

import android.content.pm.ApplicationInfo;
import android.util.Log;

import com.google.android.play.core.review.ReviewInfo;
import com.google.android.play.core.review.ReviewManager;
import com.google.android.play.core.review.ReviewManagerFactory;
import com.google.android.play.core.review.testing.FakeReviewManager;

import org.libsdl.app.SDLActivity;

public class ArcadeActivity extends SDLActivity {
    private static final String TAG = "RetroStoneReview";

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

    /* Called from the native thread (rsb_sdl.c ask_review), the player being back in the menu. Google Play decides
     * whether its dialog shows (its quotas; only for an app installed from Play); nothing tells whether the player
     * rated. A debug build not installed by Play uses FakeReviewManager (no dialog: the flow is only logged). Every
     * error is logged and ignored. */
    @SuppressWarnings("deprecation")
    public void requestReview() {
        runOnUiThread(() -> {
            try {
                boolean debug = (getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;
                boolean fromPlay = "com.android.vending".equals(getPackageManager().getInstallerPackageName(getPackageName()));
                final boolean fake = debug && !fromPlay;
                final ReviewManager manager = fake ? new FakeReviewManager(this) : ReviewManagerFactory.create(this);
                Log.i(TAG, "requestReviewFlow" + (fake ? " (FakeReviewManager: a debug build not installed by Play)" : ""));
                manager.requestReviewFlow().addOnCompleteListener(request -> {
                    if (!request.isSuccessful()) {
                        Log.w(TAG, "requestReviewFlow failed", request.getException());
                        return;
                    }
                    ReviewInfo info = request.getResult();
                    manager.launchReviewFlow(this, info).addOnCompleteListener(flow ->
                        Log.i(TAG, "launchReviewFlow done" + (flow.isSuccessful() ? "" : " (failed)")));
                });
            } catch (RuntimeException e) {
                Log.w(TAG, "review prompt", e);
            }
        });
    }
}
