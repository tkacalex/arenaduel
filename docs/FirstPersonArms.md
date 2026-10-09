# First person arms and weapon presentation

## Current implementation

The local player sees a second copy of the full Manny body (`FirstPersonArms`, attached to the world mesh), rendered as a first person primitive with Epic's template lens (70 degree viewmodel field of view, first person scale 0.6). It runs Epic's `ABP_FP_Copy`, which copies the world body's pose and applies `CtrlRig_FPWarp` to bend arms and weapon toward the camera. Both assets come from the engine's First Person template and live in `/Game/FirstPerson/Anims`.

The gameplay camera stays on the capsule. `AArenaDuelCharacter::UpdateLocalMovementCamera` slides the local body so its head socket sits under the camera at the template's eye offset, and adds the `FirstPersonViewmodelRoot` offset on top. That offset is zero while aiming and the per weapon `HipViewmodelLocation` for hip fire, plus recoil, bob and landing motion.

Weapons use Epic's template firearms (`/Game/Weapons`), snapped to `HandGrip_R` with no offset. `UArenaDuelVisualAnimInstance` solves a two bone IK for the support hand on the world body, so the first person copy and opponents see the same grip. The grip point is tunable live with `ArenaDuel.Debug.LeftGripX/Y/Z`. `ArenaDuel.Debug.ForceAimPresentation 1` shows the aimed viewmodel without holding the aim input.

## Known limits

Arc Rifle, Shade SMG and Rune DMR share the rifle model and the Hex Shotgun uses the grenade launcher model. The support hand grip point was fitted for the rifle only. The clips are third person rifle clips, so there is no authored first person fire, reload, sprint or equip motion. The generated `SM_*` gun meshes and `SKM_ArenaDuelFPSArms` are no longer used.
## Required artist and animator deliverables

* One sealed first person arm skeletal mesh compatible with the Manny skeleton used by the project, with clean skin weights at shoulder, elbow, wrist and fingers. The camera facing side must have no open cut surface or missing polygons.
* A non additive two hand weapon ready pose for `upperarm_l`, `lowerarm_l`, `hand_l`, `upperarm_r`, `lowerarm_r`, `hand_r`, finger chains, and optional clavicles. The right hand must hold the weapon grip and the left elbow must have a stable bend direction.
* First person idle, walk, run, sprint, jump, fall, landing, fire, reload and equip clips or layers on that skeleton. Recoil and breathing should be additive. Reload and equip should be non additive where the hand must leave its grip.
* A reload curve named `LeftHandIK` in the interval 0 to 1, or equivalent notifies, to release the support hand during magazine manipulation. Fire and reload event notifies must match actual visual contact frames.
* For each of Arc Rifle, Shade SMG, Rune DMR and Hex Shotgun, a verified `LeftHandGrip` socket on the static mesh and a sight reference relative to the muzzle. The mesh convention is local positive X toward the muzzle. The socket must be placed where the support palm actually contacts the foregrip.
* Weapon specific finger grip poses and calibrated first person hip and ADS transforms for each weapon and target aspect ratio and FOV. The current support grip coordinates are provisional.

## Acceptance still required

Capture both player views at hip and ADS, every weapon, sprint and reload, and inspect the support hand at normal and extreme camera pitch. Check low and high frame rates, multiple world FOV settings, near walls, and client versus host. Automated transform tests cannot prove that the art looks correct.
