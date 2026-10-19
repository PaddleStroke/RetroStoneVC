/*
 * RetroStone Board: the Android activity. Everything happens in native code (libmain.so: apps/boardgames/src, built
 * by make -f apps/boardgames/Makefile android); SDL's SDLActivity (org.libsdl.app) runs it: the surface, the touches
 * (each finger, and the first one as the mouse), audio, the Back key (SDLK_AC_BACK: the rules page closes, a game
 * goes home, the menu quits), the orientation (the app's SDL_HINT_ORIENTATIONS), the app's lifecycle and SDL_OpenURL
 * (the free edition's "full version" button: market://details?id=<the paid app>, an ACTION_VIEW intent; it fails
 * without a Play Store, and the native code then opens the store's web page, as the Arcade does).
 * Here also: the app kept between the system bars, and the bars' colours (setBarColors).
 * Built into the RetroStone app and both Board editions: apps/android/build.sh retrostone, board (app-board.conf) and
 * boardfree (app-boardfree.conf, another package id, the same class).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
package com.eightbcraft.retrostoneboard;

import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Insets;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

public class BoardActivity extends SDLActivity {
    /* The system bars. The app's own screens keep them (portrait and landscape); an Arcade game hides them in
     * landscape (SDL's fullscreen window: immersive sticky, mFullscreenModeActive). Shown, they must not cover the app:
     * Android 15 and later draw an app that targets API 35+ (this one: 36) edge to edge, under the bars, whatever its
     * window flags. So the content view (SDL's layout, its surface inside) is padded by the bars' insets (and a
     * notch's) while SDL is not full screen: the surface, and the window size SDL reports, are the room between the
     * bars; a turn, or the bars shown or hidden, gives new insets, a new padding, a new surface size, so an
     * SDL_WINDOWEVENT_SIZE_CHANGED. Before Android 15 the window already fits between shown bars (the decor's own
     * fitting): the insets come here consumed (0) and nothing changes. Full screen: no padding (as before: the picture
     * to every edge, the notch included). The padding's ground (the bars are transparent from Android 15) and the
     * bars' colours (before it) are the native code's: setBarColors, the colours at the top and the bottom of its
     * screen. */
    private View content;
    private int barTop = 0xff131b18, barBottom = 0xff131b18;   /* until the app says: the dark theme's page */
    private int padBottom;

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (mBrokenLibraries) return;
        content = findViewById(android.R.id.content);
        if (content == null) return;
        content.setBackground(new BarsGround());
        content.setOnApplyWindowInsetsListener((v, insets) -> {
            fit(insets);
            return insets;
        });
        bars();
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

    /* the bars shown or hidden (SDL's window style changed): the padding again (mFullscreenModeActive is set by then) */
    @Override
    public void onSystemUiVisibilityChange(int visibility) {
        super.onSystemUiVisibilityChange(visibility);
        if (content != null) content.requestApplyInsets();
    }

    /* the content's padding: the shown bars' (and a notch's) insets, none while full screen */
    @SuppressWarnings("deprecation")
    private void fit(WindowInsets in) {
        int l = 0, t = 0, r = 0, b = 0;
        if (in != null && !mFullscreenModeActive) {
            if (Build.VERSION.SDK_INT >= 30) {
                Insets i = in.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
                l = i.left;
                t = i.top;
                r = i.right;
                b = i.bottom;
            } else {
                l = in.getSystemWindowInsetLeft();
                t = in.getSystemWindowInsetTop();
                r = in.getSystemWindowInsetRight();
                b = in.getSystemWindowInsetBottom();
            }
        }
        padBottom = b;
        if (content.getPaddingLeft() != l || content.getPaddingTop() != t || content.getPaddingRight() != r ||
            content.getPaddingBottom() != b) {
            content.setPadding(l, t, r, b);
            content.invalidate();
        }
        if (Build.VERSION.SDK_INT < 30) lightIcons();      /* (SDL's window style sets the flags afresh) */
    }

    /* Called from the native thread (shell.c shell_system_bars) when the colours change: the screen's top row's and
     * bottom row's, 0xRRGGBB (the table's green-black or ivory, the menu's ribbon, an Arcade game's black). */
    public void setBarColors(final int top, final int bottom) {
        runOnUiThread(() -> {
            barTop = 0xff000000 | top;
            barBottom = 0xff000000 | bottom;
            bars();
        });
    }

    private static boolean light(int c) {
        int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
        return r * 299 + g * 587 + b * 114 > 128 * 1000;       /* a light ground: dark icons */
    }

    @SuppressWarnings("deprecation")
    private void bars() {
        Window w = getWindow();
        if (w == null) return;
        /* before Android 15: the bars' own grounds (from 15 they are transparent: the padding's ground shows) */
        w.clearFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS |
                     WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION);
        w.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        w.setStatusBarColor(barTop);
        w.setNavigationBarColor(barBottom);
        if (Build.VERSION.SDK_INT >= 29) {               /* the colour as it is, no scrim behind the buttons */
            w.setStatusBarContrastEnforced(false);
            w.setNavigationBarContrastEnforced(false);
        }
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController c = w.getInsetsController();
            int mask = WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS |
                       WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS;
            int want = (light(barTop) ? WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS : 0) |
                       (light(barBottom) ? WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS : 0);
            if (c != null && (c.getSystemBarsAppearance() & mask) != want) c.setSystemBarsAppearance(want, mask);
        } else {
            lightIcons();
        }
        if (content != null) content.invalidate();
    }

    /* Android 6 to 10: dark icons on a light ground, through the legacy flags (which SDL's window style resets) */
    @SuppressWarnings("deprecation")
    private void lightIcons() {
        if (Build.VERSION.SDK_INT < 23 || Build.VERSION.SDK_INT >= 30 || mFullscreenModeActive) return;
        Window w = getWindow();
        if (w == null) return;
        View d = w.getDecorView();
        int flags = View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
        if (Build.VERSION.SDK_INT >= 26) flags |= View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
        int v = d.getSystemUiVisibility(), want = v & ~flags;
        if (light(barTop)) want |= View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
        if (Build.VERSION.SDK_INT >= 26 && light(barBottom)) want |= View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
        if (want != v) d.setSystemUiVisibility(want);
    }

    /* the padding's ground: the top colour, the bottom colour under the navigation bar */
    private final class BarsGround extends Drawable {
        private final Paint paint = new Paint();

        @Override
        public void draw(Canvas c) {
            Rect b = getBounds();
            paint.setColor(barTop);
            c.drawRect(b, paint);
            if (padBottom > 0) {
                paint.setColor(barBottom);
                c.drawRect(b.left, b.bottom - padBottom, b.right, b.bottom, paint);
            }
        }

        @Override
        public void setAlpha(int alpha) {}

        @Override
        public void setColorFilter(ColorFilter cf) {}

        @Override
        @SuppressWarnings("deprecation")
        public int getOpacity() {
            return PixelFormat.OPAQUE;
        }
    }
}
