# Virtua Cop 2 VR

<!-- overseer-20261030-defaults -->
Prepared local candidate **0.2.14**, Android code **16**. Fresh settings and **Reset to defaults** use the user's captured profile: **Arcade**, gun angle0, **Shoot off-screen** reload, P1left/P2right, **Laser On**, **Custom reticle20% minimum /35% maximum /7.0m shrink**, Medium haptics, world1.000, panel1.80m, HUD100%, gun1.00, Wider view On and view width110degrees. Game zoom starts Off per session. Explicit saved alternatives persist.

The user's later Shoot off-screen instruction overrides the Point off-screen value visible in the VC2 screenshot. Missing or invalid preferences use the new defaults; Reset explicitly saves the full profile. Current HUD sizing, reticle controls, native logic and performance work are retained.

See [this candidate's release notes](docs/release-notes-0.2.14.md) and [build instructions](docs/BUILD.md). Fresh Quest/PCVR packages and software verification are recorded in the overseer defaults evidence; device acceptance and GitHub publication are separate. Complete corresponding source packaging is pending release preparation.
<!-- /overseer-20261030-defaults -->

Version 0.2.11 (Android code13) includes the earlier correction to the **Grip - Toggle zoom** line endpoint to the grip button, retaining the inward text position. **Zoom enabled** or **Zoom disabled** still appears in the lower HUD for one second after a toggle. Tap either grip to toggle Game zoom; every launch starts with zoom **Off**. Deep zoom remains continuous and the full-screen white shooting/reload strobe stays removed. The original HUD, grip-aligned guns, mirrored left hand, corrected sky, startup credits and combat optimizations remain available.

Experimental port of the original Model 2A arcade game for Quest 3/3S and Windows OpenXR. The native game supplies the scenery, actors, audio and game logic. The VR host adds stereo views, head tracking, tracked guns and the shared purple VR4ALL interface.

This experimental VR port is available as Quest, PCVR and complete-source downloads. Canonical release files are `vcop2-vr-quest.apk`, `vcop2-vr-pcvr.zip`, `vcop2-vr-source.zip` and `SHA256SUMS.txt`. Install the Quest APK with VR4ALL or ADB, then open Virtua Cop 2 VR under Unknown Sources. For PCVR, extract the portable ZIP, enable your OpenXR runtime, connect the headset and run `run-pcvr.cmd`.

Supply your owned `vcop2.zip` and `segabill.zip` in the headset Download folder or PC package `roms/` folder. A complete `vcop2.zip` containing all 31 required payloads also works. No CHD is needed. Setup checks actual sizes, CRC32 and SHA1; no game data is included in the software packages.

## Controls and options

- A starts or continues P1; X joins P2 after P1 is active. Both native players must acknowledge the join. In dual play the default is P1 left / P2 right; the menu can swap assignments. Solo supports either-hand gun transfer on a fresh trigger pull.
- Triggers fire. B/Y reloads. Button, shoot outside and point outside reload modes are available.
- Menu opens the purple settings panel. It includes Resume, Recenter, gun choice/angle/size, player layout, laser, haptics, world size, panel distance, wider view, view width, Game zoom, How to play, native Service and Quit.
- **Game zoom: Off / On** follows the original camera's automatic zoom-in and zoom-out animations when enabled. Tap and release either grip to toggle it, or use the menu. **Zoom enabled / Zoom disabled** appears in the lower HUD for one second; a later toggle replaces the message and restarts that second. Every new VR launch starts Off, including when an older build saved On. Other preferences remain saved. Hold grip and move the stick to adjust world/gun size; this gesture does not toggle zoom. Head tracking, the original HUD, held guns, controllers and menus retain their own projection. The native flat stage card and Service screens remain unzoomed.
- The three-second focused startup splash clears for the native boot/credits picture, even before the game accepts Start. Real controller models have anchored button labels, including **Grip - Toggle zoom**; How to play shows them again. Stick clicks recenter or toggle the laser; grip plus stick adjusts world/gun size.
- Service uses the original cabinet switches: stick down or B/Y advances; trigger or A/X sends Test to select or exit. Gun calibration changes to actual gun aiming. Right stick click switches back to menu controls when Test is needed. A/X in calibration sends the assigned player's native Start function, which can commit, cancel or adjust values according to the original prompt.
- Gun offers only **In-game gun**, **Arcade**, and **Zapper**. Arcade is the default. In-game gun follows each player's equipped native weapon independently: Rifle, Automatic, Machine Gun, Shot Gun and Magnum pickups appear only while equipped. The standard weapon uses a pistol and hand mesh from the game's character models as a cosmetic; the original standard player's held model has not been established. Models use authored palm/handle pivots at the controller's physical grip position, with the existing aim direction and adjustable gun angle. The physical left-hand model is mirrored, including its embedded hand, regardless of P1/P2 ownership. Older custom/manual pickup settings migrate to In-game gun.
- The original ammo graphics and foreground HUD use their native overlay projection. They remain visible above the hearts when world scenery is nearby. The HUD is excluded from gun aim intersections.

Settings, calibration and NVRAM persist independently from other ports. Native accepted shots drive recoil, muzzle effects and haptics. Aim uses the same surfaces and view lens as stereo drawing. Wider view reveals available original geometry beyond the native crop; it cannot supply scenery absent from the game.

See [validation](docs/validation.md) for exact evidence and remaining physical headset checks.

## Build and verification

Pinned MAME 0.289: `f34f02505e32c1993c6a782b6814232cbfc74e36`. Use a dedicated compiler directory without spaces; the source project itself may contain spaces.

```powershell
python -B tools/game_files.py PATH/TO/vcop2.zip PATH/TO/segabill.zip
python -B -m unittest discover -s tests -v
./tools/build-engine.ps1 -MsysRoot PATH/TO/msys64 -PythonPath PATH/TO/python.exe -WorkEngine D:/Build/vcop2-engine
cmake -S . -B D:/Build/vcop2-host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build D:/Build/vcop2-host
ctest --test-dir D:/Build/vcop2-host --output-on-failure
./tools/build-quest.ps1 -WorkRoot D:/Build/vcop2-quest
```

The Quest build script validates SDK/NDK/JDK/SDL/OpenXR dependencies, compiles ARM64, signs locally and checks APK contents/signature. `-CheckOnly` prints resolved paths. `tools/package.py` creates software-only deliveries and checksums. Engine patches are reconstructed from the pinned source; unknown compiler edits are preserved and rejected.

Diagnostic capture and state tools under `tools/` write only to separate private output directories. Heavy compiler output and private research use this workspace's dedicated D: cache. Existing ports and original game archives are unchanged. See CREDITS.md and licenses/ for component attribution.


Reticle controls (current unpublished candidate): **Custom / None** only, with **Reticle min size25%** and **Reticle shrink distance6.0m** as defaults. Missing, malformed, nonfinite or out-of-range saved shrink distances use6.0m independently of panel distance. Explicit valid saved distances (including1.8m) and other settings remain unchanged. The user can tune each game separately. The mathematical size policy, actual panel reference, real aim target, logical-player colors, independent Laser and menu/service pointers are preserved.
