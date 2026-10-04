# Architecture

## Established principles

1. ArenaDuel is multiplayer first.
2. Gameplay authority lives on the server.
3. Client prediction may improve responsiveness but must not replace server validation.
4. Foundational systems belong in C++.
5. Designer facing configuration and presentation may use Blueprint and Data Assets where appropriate.
6. Future abilities, attributes, costs, cooldowns, buffs, and debuffs should use the Gameplay Ability System where appropriate.
7. Future player input should use Enhanced Input.
8. Systems should avoid unnecessary Tick, RPCs, hard references, and broad ownership.
9. Gameplay code must not depend on cosmetic VFX execution.

## Planned source organization

Create directories only when real code needs them. The likely long term module areas are `AbilitySystem`, `Characters`, `Combat`, `Components`, `Core`, `Game`, `Input`, `Player`, `UI`, and `Data` under `Source/ArenaDuel/`.

## Planned content organization

Create content folders in Unreal Editor only when real assets need them. The likely root is `Content/ArenaDuel/`, with focused areas for characters, abilities, animation, audio, data, effects, input, maps, materials, UI, and VFX.

## Dependency plan

The initial project should use only engine supplied dependencies. Future milestones are expected to evaluate `GameplayAbilities`, `GameplayTags`, `GameplayTasks`, and `EnhancedInput`. Online services, dedicated server hosting, and third party plugins require separate evaluation before adoption.
