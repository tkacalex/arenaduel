# Match upgrades (1v1)

Phase 8B. Builds on the round flow of `docs/RoundFlow.md`. Server-authoritative; the code is in `AArenaDuelGameMode` (selection), `AArenaDuelPlayerState` (levels and effects), `UArenaDuelWeaponComponent` (ammunition and reload), `UArenaDuelUpgradeWidget` and `AArenaDuelPlayerController` (screen and request).

## When

After every **decided** round both players are offered the same three upgrades and take one level of one of them, whoever won. There is no selection after the round that decides the match and none after a round without a winner (that one keeps the plain three second break).

The selection replaces the three second break and lasts at most ten seconds (`UpgradeSelectSeconds`). During it nobody moves or fights, the round winner included. As soon as both have chosen, the next round starts with the full 90 second clock. Score and characters are kept.

A player who has not chosen when the time is up gets an upgrade that is still available with the **lowest own level**; among equals the fixed order Supply, Reload, Abilities decides.

## The three upgrades

| Upgrade | Per level | At level 3 |
| --- | --- | --- |
| Supply | +10 % reserve ammunition; the magazine is unchanged | +30 % |
| Reload | 5 % shorter reload | -15 % |
| Abilities | 5 % shorter regular ability cooldowns | -15 % |

Three levels each. The effect is additive against the unchanged base value, never compounding. A fully upgraded kind stays on screen but cannot be chosen. The numbers are starting values for play tests and are properties of the player state (`MaxUpgradeLevel`, `ReserveAmmoPerLevel`, `ReloadTimePerLevel`, `AbilityCooldownPerLevel`). Damage, maximum health, recoil, spread, movement speed and loadouts are untouched.

## How it is built

- **Server decides.** `RequestUpgradeChoice` accepts a choice only from a duel player, for themselves, while the selection is open, for a known kind below its maximum, and once per selection. A second, a late or an invalid request changes nothing. A refused request for a maxed kind does not use up the choice.
- **Levels live on the player state** (replicated), so they survive rounds and respawns. They are cleared on a match reset, on the return to character selection and when a match is abandoned because a player left.
- **Per player, not global.** The weapon definitions and the cooldown effects are never changed.
  - Supply and Reload: each freshly spawned pawn takes its player's levels once (`ApplyMatchUpgrades`, called from `RestartPlayer`). The weapon component carries two replicated factors; `GetReserveCapacity` and `GetReloadDuration` apply them.
  - Reload on screen: the reload timer on the server, the reload clip on the first person arms and on the world body, the hand on the magazine and the moving parts all read `GetReloadDuration`, so they finish together at the shorter time.
  - Abilities: through the Gameplay Ability System. When a regular cooldown effect (a `Cooldown.*` tag) is applied to a player's ability system component, the server moves its start back by the upgraded share, which shortens what is left; the change replicates with the effect, so the owner's HUD shows the shorter time. New rounds clear cooldowns as before.
- **Upgrades take effect with the next round**, because that is when the new pawn spawns.
- **Screen.** Three cards with name, what a level does, the player's level with its bonus, and what the next level brings. Mouse or keys 1 to 3. After the choice the cards lock and the screen waits for the other player. The controller opens the screen with the cursor when the phase begins and returns to game input when it ends, also if character selection closes in the same refresh.
- **Survival** overrides `UsesMatchUpgrades()` to false; nothing of this runs there.
- `ArenaDuel.Upgrades.Enabled 0` (server console) falls back to the plain break after every round.

## Tests

- `ArenaDuel.Phase8.UpgradeRules`: three kinds, three levels, the additive values, the automatic pick, the texts, ten seconds, none in survival.
- `ArenaDuel.Phase8.Upgrades` (listen server and one client, shortened times):
  - different choices by host and client (the client through the real request), requests outside the selection, invalid ids, double and late requests, everyone locked during the selection, the next round with full clock and kept score, +10 % reserve with unchanged magazine for one player and 5 % shorter reload for the other, the client seeing its shorter reload and getting its game input back, and base values after a match reset;
  - the level cap, a refused maxed kind, the cooldown effect (5 s becomes 4.5 s at Abilities 2, the other player's stays 5 s), the timeout with the lowest available level and the fixed order, +30 % reserve and -10 % reload on the new pawns;
  - no selection after a round without a winner, a player leaving during the selection, nothing granted in character selection;
  - no selection after the match deciding round, upgrades gone after the return to selection.
- The older tests of the plain round break (`Phase6.Network`, `CharacterSelect.Network`, `Admin.Network`, `Rift.Network`, `Visuals.Network`) switch the selection off for their run and on again afterwards (`Tests/ArenaDuelTestUpgrades.h`).

Checked with host and client in the editor (2026-10-11): the selection on both screens with the three cards; the client choosing Reload by key and seeing `UPGRADE TAKEN` and `Waiting for the other player...` while the host still chose; the host choosing Supply and round two starting at once with `31 / 141` reserve on the host against `/ 128` on the client; a second selection left untouched, ending on its own after ten seconds with round three; the third selection showing the automatically raised levels on both. Rounds were ended with `ArenaDuel.Round.Kill`.

Not checked in a session: both players choosing the same upgrade (the rule does not distinguish it; the automated tests cover different choices and the automatic pick), a maxed card on screen, the ability cooldown with a real ability press, a real disconnect (closing the client window ends the editor session; covered by the automated test that logs the player out), and how the shorter reload looks next to the base one.

## Selection serial (2026-10-11)

Every selection has a serial (`AArenaDuelGameState::GetUpgradeSelectSerial`) that grows by one each time a selection opens and is never reset. A choice names the serial of the selection it was made in; the server refuses a choice that names any other. A request that arrives late, in a later selection, therefore cannot use up that selection's choice. See `docs/Phase9.md`, E07.
