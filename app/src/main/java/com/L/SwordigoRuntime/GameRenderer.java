package com.L.SwordigoRuntime;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.util.Log;
import android.view.Display;
import android.view.DisplayCutout;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowManager;
import com.touchfoo.swordigo.Native;

import net.kiwi.launcher.MainActivity;
import net.kiwi.launcher.SettingsFragment;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class GameRenderer implements GLSurfaceView.Renderer {
    private static final String TAG = "GameRenderer";

    private boolean applicationSetup = false;
    private double prevTime = 0;

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        try {
            Log.i(TAG, "GL_VERSION: " + gl.glGetString(GL10.GL_VERSION));
            Log.i(TAG, "GL_RENDERER: " + gl.glGetString(GL10.GL_RENDERER));
            Log.i(TAG, "GL_EXTENSIONS: " + gl.glGetString(GL10.GL_EXTENSIONS));
            requestRefreshRate();

            if (!applicationSetup) {
                Native.setupNativeInterface();
                Native.setupApplication();
                applicationSetup = true;
            } else {
                Native.reloadContext();
            }
        } catch (Throwable t) {
            Log.e(TAG, "onSurfaceCreated failed", t);
        }
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        try {
            boolean isPad = isTablet();
            int horizontalInset = getHorizontalCutoutInset();
            Native.setApplicationViewSize(width, height, isPad, horizontalInset, 0);
        } catch (Throwable t) {
            Log.e(TAG, "onSurfaceChanged failed", t);
        }
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        try {
            int targetFps = SettingsFragment.getFps();
            double targetFrameTime = 1.0 / targetFps;

            double now = System.nanoTime() / 1_000_000_000.0;
            if (prevTime != 0 && targetFps <= 60) {
                double elapsed = now - prevTime;
                if (elapsed < targetFrameTime) {
                    long sleepMs = (long) ((targetFrameTime - elapsed) * 1000.0);
                    if (sleepMs > 0) {
                        Thread.sleep(sleepMs);
                        now = System.nanoTime() / 1_000_000_000.0;
                    }
                }
            }

            float dt;
            if (prevTime == 0) {
                dt = 1f / targetFps;
            } else {
                dt = (float) (now - prevTime);
                if (dt > 0.0667f) {
                    dt = 0.0667f;
                }
            }
            prevTime = now;

            Native.updateApplication(dt);
            Native.drawApplication();
        } catch (Throwable t) {
            Log.e(TAG, "onDrawFrame failed", t);
        }
    }

    private void requestRefreshRate() {
        Activity activity = (Activity) Native.mainActivity;
        if (activity == null || activity.getWindow() == null) return;
        int targetFps = SettingsFragment.getFps();
        activity.runOnUiThread(() -> {
            try {
                Window window = activity.getWindow();
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                    WindowManager.LayoutParams params = window.getAttributes();
                    params.preferredRefreshRate = targetFps;
                    window.setAttributes(params);
                }
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    try {
                        if (GameView.instance != null && GameView.instance.getHolder() != null && GameView.instance.getHolder().getSurface() != null) {
                            GameView.instance.getHolder().getSurface().setFrameRate((float) targetFps, android.view.Surface.FRAME_RATE_COMPATIBILITY_DEFAULT);
                        }
                    } catch (Throwable ignored) {}
                }
                Display display = window.getWindowManager().getDefaultDisplay();
                Display.Mode currentMode = display.getMode();
                Display.Mode[] modes = display.getSupportedModes();
                if (modes == null || modes.length == 0) return;
                int curWidth  = currentMode.getPhysicalWidth();
                int curHeight = currentMode.getPhysicalHeight();
                Display.Mode best = null;
                float minDiff = Float.MAX_VALUE;
                for (Display.Mode m : modes) {
                    if (m.getPhysicalWidth() == curWidth && m.getPhysicalHeight() == curHeight) {
                        float diff = Math.abs(m.getRefreshRate() - targetFps);
                        if (diff < minDiff) {
                            best = m;
                            minDiff = diff;
                        }
                    }
                }
                if (best != null) {
                    WindowManager.LayoutParams params = window.getAttributes();
                    params.preferredDisplayModeId = best.getModeId();
                    window.setAttributes(params);
                }
            } catch (Throwable t) {
                Log.e(TAG, "Failed to request refresh rate", t);
            }
        });
    }

    private boolean isTablet() {
        Activity activity = (Activity) Native.mainActivity;
        if (activity == null) return false;
        return (activity.getResources().getConfiguration().screenLayout
        & Configuration.SCREENLAYOUT_SIZE_MASK) >= Configuration.SCREENLAYOUT_SIZE_LARGE;
    }

    private int getHorizontalCutoutInset() {
        Activity activity = (Activity) Native.mainActivity;
        if (activity == null || Build.VERSION.SDK_INT < 28) return 0;

        WindowInsets insets = activity.getWindow().getDecorView().getRootWindowInsets();
        if (insets == null) return 0;

        DisplayCutout cutout = insets.getDisplayCutout();
        if (cutout == null) return 0;

        return Math.max(cutout.getSafeInsetLeft(), cutout.getSafeInsetRight());
    }
}
