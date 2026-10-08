# First person arms and weapon presentation

## Current implementation

The local player sees their own full Manny body (`SKM_Manny_Simple`) on `FirstPersonArms`, rendered as a first person primitive with a 55 degree viewmodel lens. `UArenaDuelVisualAnimInstance` pins the head joint to the component origin every frame, collapses the neck and thighs, and solves a two bone IK for the support hand. The component offset places that joint relative to the eye so the rifle sight sits on the screen centre while aiming. Hip fire adds the per weapon `HipViewmodelLocation` on `FirstPersonViewmodelRoot`. Both the first and third person guns use the same grip fit on `HandGrip_R`. Gameplay aim and the authoritative trace stay separate from these cosmetic offsets.

`ArenaDuel.Debug.ForceAimPresentation 1` shows the aimed viewmodel without holding the aim input, for captures and tooling.

## Known limits

The four generated gun meshes are blocky placeholders and hide most of both hands in hip fire and while aiming. The rifle clips are full body third person clips, so there is no authored first person fire, sprint or equip motion. `SKM_ArenaDuelFPSArms` is no longer used.
## Required artist and animator deliverables

* One sealed first person arm skeletal mesh compatible with the Manny skeleton used by the project, with clean skin weights at shoulder, elbow, wrist and fingers. The camera facing side must have no open cut surface or missing polygons.
* A non additive two hand weapon ready pose for `upperarm_l`, `lowerarm_l`, `hand_l`, `upperarm_r`, `lowerarm_r`, `hand_r`, finger chains, and optional clavicles. The right hand must hold the weapon grip and the left elbow must have a stable bend direction.
* First person idle, walk, run, sprint, jump, fall, landing, fire, reload and equip clips or layers on that skeleton. Recoil and breathing should be additive. Reload and equip should be non additive where the hand must leave its grip.
* A reload curve named `LeftHandIK` in the interval 0 to 1, or equivalent notifies, to release the support hand during magazine manipulation. Fire and reload event notifies must match actual visual contact frames.
* For each of Arc Rifle, Shade SMG, Rune DMR and Hex Shotgun, a verified `LeftHandGrip` socket on the static mesh and a sight reference relative to the muzzle. The mesh convention is local positive X toward the muzzle. The socket must be placed where the support palm actually contacts the foregrip.
* Weapon specific finger grip poses and calibrated first person hip and ADS transforms for each weapon and target aspect ratio and FOV. The current support grip coordinates are provisional.

## Acceptance still required

Capture both player views at hip and ADS, every weapon, sprint and reload, and inspect the support hand at normal and extreme camera pitch. Check low and high frame rates, multiple world FOV settings, near walls, and client versus host. Automated transform tests cannot prove that the art looks correct.
