# Performance

## How it was measured

Two player PIE session (listen server plus one client) on `L_ArenaCore`, server window 3840 wide, standing at the spawn. `ProfileGPU` in the console writes the GPU pass breakdown to `Saved/Logs/ArenaDuel.log`; `r.ProfileGPU.ShowUI 0` keeps the visualizer window closed. Each figure is the middle of three captures. The numbers are GPU time on the development machine, in the editor, not from a packaged build.

## Where the frame went

Server window, internal resolution 1956x1001 upscaled to 3840x1965 by TSR.

| Pass | Epic preset | Project values | 
|---|---|---|
| Whole view | 14.3 ms | 8.6 ms |
| Temporal Super Resolution | 5.8 ms | 2.7 ms |
| Lumen diffuse indirect and AO | 4.5 ms | 2.4 ms |
| Lumen reflections | 0.3 ms | 0.3 ms |

The complete editor frame (both PIE windows plus the editor UI) went from 27.9 ms to 18.1 ms. A side by side screenshot of the arena showed no visible difference.

## What changed

`Config/DefaultScalability.ini` redefines two Epic presets for this project:

- `AntiAliasingQuality@3`: `r.TSR.History.ScreenPercentage` 200 to 100. The history buffer was kept at 7680x3930.
- `GlobalIlluminationQuality@3`: uses the engine's High Lumen values (final gather at one probe per 32 pixels instead of 16, half resolution short range AO, no mesh distance field tracing, smaller radiance cache).

`Config/DefaultEngine.ini` turns motion blur off by default (`r.DefaultFeature.MotionBlur=False`). This is a look change: fast turns stay sharp.

Lumen, virtual shadow maps and Substrate are still enabled.

## Earlier pass on gameplay effects

- Tracers come from a pool of 16 meshes and the muzzle flash is one reused light, so firing allocates nothing.
- The flashbang burst and the enemy glow lights cast no shadows.
- Per-bone hit zone collision and the always-ticking pose exist on the server only.

## Not measured

- CPU frame time and hitches during combat (firing, flashbang, ragdoll).
- A packaged build and a remote client.
- Lower scalability levels (High and below are the engine defaults).
