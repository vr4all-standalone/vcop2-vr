// SPDX-License-Identifier: GPL-2.0-or-later
package com.vrlightgunhub.vcop2;

/** UI-thread state for validated, once-only automatic entry and explicit Play. */
final class ReadyLaunchPolicy {
    private boolean resumed, consumed, launching, checking, ready, manual;
    private int generation;

    ReadyLaunchPolicy(boolean suppressed, boolean restoredConsumed) {
        consumed = suppressed || restoredConsumed;
    }

    void onResume() { resumed = true; }
    void onPause() { resumed = false; manual = false; }
    void onNewEntry(boolean suppressed) {
        if (launching) return;
        consumed = suppressed;
        manual = ready = false;
        generation++;
    }
    boolean autoStartConsumed() { return consumed; }
    boolean isReady() { return ready && !checking && !launching; }
    int beginCheck() {
        if (!resumed || checking || launching) return -1;
        checking = true;
        ready = false;
        return generation;
    }
    boolean finishCheck(int token, boolean valid) {
        checking = false;
        if (token != generation) return false;
        ready = valid;
        if (!valid) manual = false;
        return true;
    }
    boolean requestPlay() {
        if (!resumed || !isReady()) return false;
        manual = true;
        return true;
    }
    boolean shouldLaunch() {
        if (!resumed || !isReady() || (consumed && !manual)) return false;
        consumed = true;
        manual = false;
        return true;
    }
    boolean beginLaunch() {
        if (!resumed || !isReady()) return false;
        consumed = launching = true;
        return true;
    }
    void launchFailed() { launching = false; consumed = true; }
}
