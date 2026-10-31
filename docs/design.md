# Virtua Cop 2 VR design

The user authorizes starting Model 2 games one at a time, choosing the first title, and using all features of the accepted VR4ALL ports. First title: original arcade Virtua Cop 2 (vcop2, Model 2A). Preserve existing ports and installer edits. Model 3 is a later project.

## Required behavior

- Independently paced Quest 3/3S and Windows OpenXR with native stereo world geometry, head tracking, tracked guns, original audio and game speed. Diagnostic panels do not establish VR completion.
- Three seconds of focused, presented VR4ALL startup splash, then both controller models with button-anchored instructions. Suppress Start/fire during splash and until native readiness. How to play restores instructions.
- A/X start/continue P1 or join P2 with native credits and native acknowledgement. Solo supports either-hand gun transfer; dual play has reversible P1-left/P1-right assignments. Preserve held Start ownership and suppress held firing across focus/menu/layout changes.
- Triggers aim/fire; B/Y reload. Button, shoot-outside and point-outside modes. Use actual Model 2A serial gun ranges, offscreen detection and active-low cabinet bits, never CarnEvil defaults.
- Purple VR4ALL settings: Resume, Recenter, Gun, Gun angle/size, Reload, Two players, Laser, four Haptics levels, World size, Panel distance, How to play, Service and two-step Quit. Persist settings. Depth/wider-view choices need renderer evidence.
- Extract the original game gun and pickups from supplied geometry/textures privately. Offer only In-game gun, Arcade and Zapper; In-game follows each player's actual native equipped weapon. Anchor each authored grip to the physical controller grip and mirror the left controller's gun. Do not label fallback models as the original gun.
- Laser, shot coordinates and stereo surfaces agree. Accepted-shot recoil/haptics/muzzle effects require measured native events; no guessed ammo/shot addresses.
- Service uses actual Test/Service/Start/trigger/gun inputs, a distinct gun-calibration mode and reliable exit. Keep NVRAM across updates and separate all state from media.
- Independent Android package com.vrlightgunhub.vcop2, library and settings identities. Deliver software-only local APK/PCVR packages and checksums after verified builds. Publishing is outside this request.

## Implementation choices

Pin MAME 0.289 at f34f02505e32c1993c6a782b6814232cbfc74e36. Virtua Cop 2 uses i960 plus TGP, Model 2A rasterizer and SCSP audio. Capture polygons/texture state before clipping; preserve window order, tilemap priority and diagnostics. Reuse proven OpenXR/UI modules with attribution; do not copy game-specific RAM fingerprints or Voodoo assumptions.

Compiler output and private media/captures live on D: within the existing workspace cache. C: was full at startup, then space became available; the small source files now live normally in ports/vcop2-vr. Only build/private/research are junctions to this task's D: directories. Existing engine trees remain read-only references. Supplied media is read-only. Check real ROM SHA1/CRC32 including CPU/video board tables and the separately defined billboard device firmware. Name missing dependencies and do not download game data.

## Evidence gates

1. Verify supplied media; establish original boot, attract, start/join and service.
2. Capture actual scenes, recover lens, compare neutral replay, then verify stereo and aim.
3. Integrate shared controls/UI and measured native player/weapon/shot state.
4. Build/verify Quest and PCVR packages; record physical headset testing separately.
