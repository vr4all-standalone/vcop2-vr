# Virtua Cop 2 VR 0.2.14

Local Quest and Windows PCVR candidate; Android version code16. GitHub publication awaits approval.

Fresh settings and **Reset to defaults** use the user's captured profile: Arcade gun, angle0, **Shoot off-screen** reload, P1left/P2right, Laser On, Custom reticle with minimum20%, maximum35% and shrink distance7.0m, Medium haptics, world1.000, panel1.80m, HUD100%, gun1.00, Wider view On and view width110degrees. Game zoom starts Off each session.

The visible VC2 capture originally showed Point off-screen; the user's later instruction explicitly chose Shoot off-screen for both Virtua Cop games. Existing explicitly saved alternatives are preserved, including In-game gun/Zapper, Point off-screen reload, Laser Off and other valid tuning. Missing or invalid settings use the new defaults. Reset is an explicit menu action and saves this complete profile.

Retains the current reticle minimum/maximum controls, HUD sizing and Reset behavior, native game logic, input ownership, native pickups, camera, renderer and performance changes. The generic reticle helper's25%/6m safety baseline and supported setting ranges are unchanged; actual settings supply the captured profile.

Software regression tests exercise defaults, malformed/missing settings, explicit alternate save/load, the actual menu Reset action, GPU reticles, hand transitions and existing native input behavior. Quest and PCVR packages require separate headset launch/play acceptance. No device install or play, public upload or downloaded-release verification is claimed by this candidate.

Canonical release names are `vcop2-vr-quest.apk`, `vcop2-vr-pcvr.zip`, `vcop2-vr-source.zip` and `SHA256SUMS.txt`; preparation uses versioned APK/PCVR filenames. Complete corresponding source packaging remains a separate release-preparation step. No game data is bundled; install over the current application with the same signing identity to retain saved data.
