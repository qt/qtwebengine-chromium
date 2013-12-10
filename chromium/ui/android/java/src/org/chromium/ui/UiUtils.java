// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.util.Log;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;

/**
 * Utility functions for common Android UI tasks.
 * This class is not supposed to be instantiated.
 */
public class UiUtils {
    private static final String TAG = "UiUtils";

    /**
     * Guards this class from being instantiated.
     */
    private UiUtils() {
    }

    /** The minimum size of the bottom margin below the app to detect a keyboard. */
    private static float KEYBOARD_DETECT_BOTTOM_THRESHOLD_DP = 100;

    /**
     * Shows the software keyboard if necessary.
     * @param view The currently focused {@link View}, which would receive soft keyboard input.
     */
    public static void showKeyboard(View view) {
        InputMethodManager imm =
                (InputMethodManager) view.getContext().getSystemService(
                        Context.INPUT_METHOD_SERVICE);
        // Only shows soft keyboard if there isn't an open physical keyboard.
        imm.showSoftInput(view, InputMethodManager.SHOW_IMPLICIT);
    }

    /**
     * Hides the keyboard.
     * @param view The {@link View} that is currently accepting input.
     * @return Whether the keyboard was visible before.
     */
    public static boolean hideKeyboard(View view) {
        InputMethodManager imm =
                (InputMethodManager) view.getContext().getSystemService(
                        Context.INPUT_METHOD_SERVICE);
        return imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
    }

    public static boolean isKeyboardShowing(Context context, View view) {
        View rootView = view.getRootView();
        if (rootView == null) return false;
        Rect appRect = new Rect();
        rootView.getWindowVisibleDisplayFrame(appRect);
        final float screenHeight = context.getResources().getDisplayMetrics().heightPixels;
        final float bottomMargin = Math.abs(appRect.bottom - screenHeight);
        final float density = context.getResources().getDisplayMetrics().density;
        return bottomMargin > KEYBOARD_DETECT_BOTTOM_THRESHOLD_DP * density;
    }

    /**
     * Inserts a {@link View} into a {@link ViewGroup} after directly before a given {@View}.
     * @param container The {@link View} to add newView to.
     * @param newView The new {@link View} to add.
     * @param existingView The {@link View} to insert the newView before.
     * @return The index where newView was inserted, or -1 if it was not inserted.
     */
    public static int insertBefore(ViewGroup container, View newView, View existingView) {
        return insertView(container, newView, existingView, false);
    }

    /**
     * Inserts a {@link View} into a {@link ViewGroup} after directly after a given {@View}.
     * @param container The {@link View} to add newView to.
     * @param newView The new {@link View} to add.
     * @param existingView The {@link View} to insert the newView after.
     * @return The index where newView was inserted, or -1 if it was not inserted.
     */
    public static int insertAfter(ViewGroup container, View newView, View existingView) {
        return insertView(container, newView, existingView, true);
    }

    private static int insertView(
            ViewGroup container, View newView, View existingView, boolean after) {
        // See if the view has already been added.
        int index = container.indexOfChild(newView);
        if (index >= 0) return index;

        // Find the location of the existing view.
        index = container.indexOfChild(existingView);
        if (index < 0) return -1;

        // Add the view.
        if (after) index++;
        container.addView(newView, index);
        return index;
    }

    /**
     * Generates a scaled screenshot of the given view.  The maximum size of the screenshot is
     * determined by maximumDimension.
     *
     * @param currentView      The view to generate a screenshot of.
     * @param maximumDimension The maximum width or height of the generated screenshot.  The bitmap
     *                         will be scaled to ensure the maximum width or height is equal to or
     *                         less than this.  Any value <= 0, will result in no scaling.
     * @param bitmapConfig     Bitmap config for the generated screenshot (ARGB_8888 or RGB_565).
     * @return The screen bitmap of the view or null if a problem was encountered.
     */
    public static Bitmap generateScaledScreenshot(
            View currentView, int maximumDimension, Bitmap.Config bitmapConfig) {
        Bitmap screenshot = null;
        boolean drawingCacheEnabled = currentView.isDrawingCacheEnabled();
        try {
            prepareViewHierarchyForScreenshot(currentView, true);
            if (!drawingCacheEnabled) currentView.setDrawingCacheEnabled(true);
            // Android has a maximum drawing cache size and if the drawing cache is bigger
            // than that, getDrawingCache() returns null.
            Bitmap originalBitmap = currentView.getDrawingCache();
            if (originalBitmap != null) {
                double originalHeight = originalBitmap.getHeight();
                double originalWidth = originalBitmap.getWidth();
                int newWidth = (int) originalWidth;
                int newHeight = (int) originalHeight;
                if (maximumDimension > 0) {
                    double scale = maximumDimension / Math.max(originalWidth, originalHeight);
                    newWidth = (int) Math.round(originalWidth * scale);
                    newHeight = (int) Math.round(originalHeight * scale);
                }
                Bitmap scaledScreenshot =
                        Bitmap.createScaledBitmap(originalBitmap, newWidth, newHeight, true);
                if (scaledScreenshot.getConfig() != bitmapConfig) {
                    screenshot = scaledScreenshot.copy(bitmapConfig, false);
                    scaledScreenshot.recycle();
                    scaledScreenshot = null;
                } else {
                    screenshot = scaledScreenshot;
                }
            } else if (currentView.getMeasuredHeight() > 0 && currentView.getMeasuredWidth() > 0) {
                double originalHeight = currentView.getMeasuredHeight();
                double originalWidth = currentView.getMeasuredWidth();
                int newWidth = (int) originalWidth;
                int newHeight = (int) originalHeight;
                if (maximumDimension > 0) {
                    double scale = maximumDimension / Math.max(originalWidth, originalHeight);
                    newWidth = (int) Math.round(originalWidth * scale);
                    newHeight = (int) Math.round(originalHeight * scale);
                }
                Bitmap bitmap = Bitmap.createBitmap(newWidth, newHeight, bitmapConfig);
                Canvas canvas = new Canvas(bitmap);
                canvas.scale((float) (newWidth / originalWidth),
                        (float) (newHeight / originalHeight));
                currentView.draw(canvas);
                screenshot = bitmap;
            }
        } catch (OutOfMemoryError e) {
            Log.d(TAG, "Unable to capture screenshot and scale it down." + e.getMessage());
        } finally {
            if (!drawingCacheEnabled) currentView.setDrawingCacheEnabled(false);
            prepareViewHierarchyForScreenshot(currentView, false);
        }
        return screenshot;
    }

    private static void prepareViewHierarchyForScreenshot(View view, boolean takingScreenshot) {
        if (view instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) view;
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                prepareViewHierarchyForScreenshot(viewGroup.getChildAt(i), takingScreenshot);
            }
        } else if (view instanceof SurfaceView) {
            view.setWillNotDraw(!takingScreenshot);
        }
    }
}
