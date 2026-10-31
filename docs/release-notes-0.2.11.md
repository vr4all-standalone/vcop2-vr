# Virtua Cop 2 VR 0.2.11

Public release: Quest, PCVR and complete corresponding source downloads are available on the project page.

Prepared Quest and Windows PCVR update; Android code 13. The Quest setup screen now uses the wide purple VR4ALL launch panel, with file/access status, help and a manual **Play** button. It enters VR automatically once the real game-file checks pass; returning from Quit or an error leaves Play available without a duplicate automatic launch.

The Arcade gun's body and complete grip follow logical player identity across hand swaps: P1 red / P2 blue. Grey screws, metal and lens details keep their original materials; other gun choices retain their appearance. Displayed **Gun angle** is -30..30 degrees in one-degree steps. New displayed 0 uses the old physical -10-degree pose consistently for the model, muzzle, laser, aiming and menu pointer. Every legacy saved angle resets to displayed 0 once; a persistent migration marker preserves later adjustments and other saved preferences.

This update has fresh software tests and Windows/Quest builds. Its APK was installed in place and matched the candidate hash; the new setup, colors and angle behavior have not been physically observed in a headset. Earlier accepted gameplay features remain separate historical evidence.

Retains Model 2A scene/HUD/sky, grip-aligned mirrored weapons, native pickup switching, combat optimizations and grip-toggle zoom feedback.

Install over the existing app with the same signing identity to retain game data, calibration, native saves and other preferences. Bring your own supported media; none is bundled. PCVR headset profiles, comfort, full game routes and this update's physical launcher/material/angle behavior remain untested. GPU regression checks and compilation are software results.

Downloads use the canonical Quest APK, PCVR ZIP, complete source ZIP and SHA256SUMS.txt names documented in README. Source includes full MAME 0.289 bundle/patches and exact SDL/OpenXR snapshots. Publication is handled separately; this document does not assert that it has happened.
