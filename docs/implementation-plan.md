# Virtua Cop 2 VR Implementation Plan

> For agentic workers: use superpowers:executing-plans for inline execution.

**Goal:** Port arcade Virtua Cop 2 with the accepted ports complete feature set.
**Architecture:** Isolated MAME simulation/Model 2 capture feeds immutable scenes to an independently paced shared OpenXR host. Establish game-specific inputs/state/weapons from real evidence.
**Tech stack:** C++20, MAME 0.289, OpenXR/OpenGL/GLES, Android NDK/Gradle, Python.
**Spec:** docs/design.md

## Global constraints

Preserve media and existing edits; independent app/state identities. No game-data downloads or guessed state. Preserve every startup/player/help/service/weapon/styling requirement. Keep large output in the D: workspace cache because C: was full at startup. Execute directly as requested.

## Review focus

Missing board ROMs; corrupt payloads with correct filenames; held inputs across mode changes; native clipping/window/tilemap semantics; unknown gun identity and player acknowledgement.

## Task 1: Original baseline

- [x] Add failing tests for payload hashes/sizes, duplicate/unsafe members, dependency reporting and source preservation. Implement and run green.
- [x] Generate metadata from pinned game, board and device definitions; audit supplied copies and stage only a verified private set.
- [x] Build isolated minimal Model 2 engine with proven platform build fixes.
- [x] Run bounded boot/attract/P1/P2/service captures; record actual speed and faults.

## Task 2: Stereo and controls

- [x] Capture owned polygons/textures/window/layer/lens data without changing baseline output; compare neutral replay.
- [x] Test stereo transforms and gun intersections against measured projection and native 10-bit/offscreen inputs.
- [x] Establish native player/weapon/accepted-shot evidence; extract original gun privately.
- [x] Integrate splash/controller labels/How to play/player ownership/purple menus/service/settings; verify held-input/focus boundaries.

## Task 3: Platforms and delivery

- [x] Build ARM64 and Windows hosts; separate package/state; validate media; local signing; software-only packaging.
- [x] Verify APK/portable components, source correspondence, licenses and checksums.
- [ ] Complete worn-headset startup, players, service, audio and weapon play tests.
- [x] Add installer profile after working artifacts; independent review and measured validation.

## Execution ledger

2026-10-27: User requests direct implementation and all prior-port features. Work in a new isolated project and compiler tree because existing accepted ports are uncommitted. Preserve parent main branch and sibling work; no unrelated staging/commits.

Task 1 evidence: 30 game/board payloads validated; the native device audit additionally required epr-18022.ic2. Found and verified the user's local segabill.zip; canonical private native ZIP contains all 31 payloads. Clean Windows engine compiled. Corrected original-game diagnostic runs show playable 3D stages, both player HUDs after P1/P2 Start, and native Gun Setting. PC diagnostic speed was 319.51% over 59 native seconds (not a Quest performance result). Script failures are detected even if MAME exits zero. Review findings fixed: default bundled diagnostic script, exported Android SDL/ARM64 patches.

Task 2 in progress: production cabinet I/O and owned-scene collector tests pass. Opt-in source-pinned capture records raw polygons before clipping, native focus/matrix/object IDs and owned texture/palette/luma/gamma snapshots. These are diagnostics, not an implemented VR renderer.

Final foundation verification: 22 Python tests and both native regression programs pass. The corrected capture engine builds; a fresh 40-native-second gameplay run completes without Lua/capture errors. All seven complete scene/raster sets pass exact foreground/native compositing with zero mismatched pixels. The RGB palette-index capture defect was reproduced, fixed, recaptured and verified. Render resources now freeze at the actual draw point, and incomplete captures retain metadata.partial. Task 2 and Task 3 remain open.

Final capture-lifecycle correction: added a third native regression that reproduced premature metadata commit when background output was missing. Completion now requires all three raster success flags. The regression passes; a fresh guarded 40-second run and all seven capture sets pass. Final total: 22 Python tests and three native programs. Independent focused review reports no further material defect in the guard.

2026-10-28 delivery implementation: continuous native capture, original material replay, matching stereo/aim view lens, full player/reload policy, shared controller/UI/service presentation and original pickup extraction are built. Seven native and31 Python regressions pass. Real Windows host integration verifies both native joins and repeated reload/shooting. Signed ARM64 APK exists and is installed on Quest3S; physical play is pending device unlock. Current evidence supersedes the historical foundation-only ledger above.

Performance verification: batched material attributes preserve all35 replay rasters byte-for-byte while the actual Quest GPU tests improve to5.34/7.32ms per stereo frame at1832x1920 per eye. Removing duplicate software3D raster work leaves five native workRAM snapshots byte-identical across46-second original-game reference/VR runs. The renderer/GLES/presentation gates are now seven CTests.
