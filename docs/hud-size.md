# HUD size — local candidate

`HUD size` follows `Panel distance` in the VR menu. The default is 100%,
with 50–200% in 5% steps. It changes the physical width and height of the
known native gameplay UI about its existing center. Panel distance, world
geometry, aiming, native textures and relocated reticles retain their behavior.

Supported layers: native HUD meshes and the separate foreground layer (including native text). This is a general native 2D gameplay UI control;
it does not claim individual classification of every game message or embedded
3D HUD object. Unidentified primitives remain unchanged.

The saved value is effective only during active Wider / large / Stereo 3D
gameplay. Original view, boot/title, native Service and complete raster
fallbacks remain at 100%. Switching views preserves the saved choice. Invalid,
nonfinite or out-of-range saved values load as 100%. Explicit Reset to defaults
saves 100% with the rest of the VR menu defaults; native calibration/game data
are untouched.

Software and build receipts are recorded by the overseer. No physical Quest or
PCVR play, headset installation or GitHub publication is implied by this candidate.
