// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.signin;

import android.accounts.Account;
import android.content.Context;
import android.preference.PreferenceManager;
import android.util.Log;

import com.google.common.annotations.VisibleForTesting;

import org.chromium.base.ObserverList;

public class ChromeSigninController {
    public interface Listener {
        /**
         * Called when the user signs out of Chrome.
         */
        void onClearSignedInUser();
    }

    public static final String TAG = "ChromeSigninController";

    @VisibleForTesting
    public static final String SIGNED_IN_ACCOUNT_KEY = "google.services.username";

    private static final Object LOCK = new Object();

    private static ChromeSigninController sChromeSigninController;

    private final Context mApplicationContext;

    private final ObserverList<Listener> mListeners = new ObserverList<Listener>();

    private ChromeSigninController(Context context) {
        mApplicationContext = context.getApplicationContext();
    }

    /**
     * A factory method for the ChromeSigninController.
     *
     * @param context the ApplicationContext is retrieved from the context used as an argument.
     * @return a singleton instance of the ChromeSigninController
     */
    public static ChromeSigninController get(Context context) {
        synchronized (LOCK) {
            if (sChromeSigninController == null) {
                sChromeSigninController = new ChromeSigninController(context);
            }
        }
        return sChromeSigninController;
    }

    public Account getSignedInUser() {
        String syncAccountName = getSignedInAccountName();
        if (syncAccountName == null) {
            return null;
        }
        return AccountManagerHelper.createAccountFromName(syncAccountName);
    }

    public boolean isSignedIn() {
        return getSignedInAccountName() != null;
    }

    public void setSignedInAccountName(String accountName) {
        PreferenceManager.getDefaultSharedPreferences(mApplicationContext).edit()
                .putString(SIGNED_IN_ACCOUNT_KEY, accountName)
                .apply();
    }

    public void clearSignedInUser() {
        Log.d(TAG, "Clearing user signed in to Chrome");
        setSignedInAccountName(null);

        for (Listener listener : mListeners) {
            listener.onClearSignedInUser();
        }
    }

    public String getSignedInAccountName() {
        return PreferenceManager.getDefaultSharedPreferences(mApplicationContext)
                .getString(SIGNED_IN_ACCOUNT_KEY, null);
    }

    /**
     * Adds a Listener.
     * @param listener Listener to add.
     */
    public void addListener(Listener listener) {
        mListeners.addObserver(listener);
    }

    /**
     * Removes a Listener.
     * @param listener Listener to remove from the list.
     */
    public void removeListener(Listener listener) {
        mListeners.removeObserver(listener);
    }
}
