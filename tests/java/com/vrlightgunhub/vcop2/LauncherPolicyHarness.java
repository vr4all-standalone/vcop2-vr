package com.vrlightgunhub.vcop2;

/** Executes the same state machine used by the Android Activity, without Android mocks. */
public final class LauncherPolicyHarness {
    private static void expect(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    private static void checked(ReadyLaunchPolicy p, boolean valid) {
        int token = p.beginCheck();
        expect(token >= 0, "foreground check can begin");
        expect(p.beginCheck() == -1, "pending checks cannot duplicate");
        expect(!p.isReady(), "checking invalidates an earlier ready result");
        expect(p.finishCheck(token, valid), "current result is accepted");
    }
    public static void main(String[] args) {
        ReadyLaunchPolicy p = new ReadyLaunchPolicy(false, false);
        expect(p.beginCheck() == -1, "no check before resume");
        p.onResume();
        checked(p, false);
        expect(!p.isReady() && !p.shouldLaunch(), "invalid files never enable Play or auto entry");
        expect(!p.requestPlay(), "manual Play also rejects invalid files");
        checked(p, true);
        expect(p.isReady() && p.shouldLaunch(), "validated files auto enter on first entry");
        expect(!p.shouldLaunch(), "auto entry is consumed before handoff");
        expect(p.beginLaunch(), "ready foreground handoff begins");
        expect(!p.beginLaunch() && !p.requestPlay(), "duplicate handoff is blocked");
        p.onNewEntry(false);
        expect(!p.shouldLaunch(), "reused intent cannot duplicate a finishing handoff");

        p = new ReadyLaunchPolicy(false, false); p.onResume();
        int token = p.beginCheck(); p.onPause();
        expect(p.finishCheck(token, true), "background completion retains validated result");
        expect(!p.shouldLaunch() && !p.beginLaunch(), "background completion cannot enter VR");
        p.onResume();
        expect(p.shouldLaunch() && p.beginLaunch(), "permission/background return can auto enter once");

        for (boolean restored : new boolean[] { false, true }) {
            p = new ReadyLaunchPolicy(!restored, restored); p.onResume(); checked(p, true);
            expect(!p.shouldLaunch(), "Quit/error return and consumed recreation stay on the panel");
            expect(p.isReady() && p.requestPlay(), "returned panel still offers manual Play");
            checked(p, true);
            expect(p.shouldLaunch() && p.beginLaunch(), "manual Play rechecks and enters");
            p.launchFailed();
            expect(!p.shouldLaunch() && p.requestPlay(), "failed handoff stays on the panel and allows retry");
        }

        p = new ReadyLaunchPolicy(false, false); p.onResume(); token = p.beginCheck();
        p.onNewEntry(true);
        expect(!p.finishCheck(token, true), "old async result cannot launch a returned/reused entry");
        checked(p, true);
        expect(!p.shouldLaunch(), "new returned entry suppresses auto launch");
        p.onNewEntry(false); checked(p, true);
        expect(p.shouldLaunch(), "fresh library entry on a reused Activity can auto launch");
        expect(p.autoStartConsumed(), "saved state records the automatic attempt");

        p = new ReadyLaunchPolicy(true, false); p.onResume(); checked(p, true);
        expect(p.requestPlay(), "manual play starts from a validated result");
        checked(p, false);
        expect(!p.shouldLaunch() && !p.isReady(), "file removed before manual Play prevents entry");
        System.out.println("PASS launcher readiness, lifecycle, return, stale results and duplicate guards");
    }
}
