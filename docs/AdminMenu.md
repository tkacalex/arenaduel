# Admin menu

A development tool, separate from everything a player sees. `F1` opens it on the host in the editor and in a game started with `-ArenaDuelAdmin`. In any other game the key does nothing, no screen mentions it, and the server refuses its commands (`AArenaDuelPlayerController::CanUseDevelopmentAdmin`). It is compiled out of shipping builds.

It is one opaque window in the middle of the screen over the dimmed game (`UArenaDuelAdminWidget`):

- **Left**: the target player (1 or 2) and their state as rows: health, alive or dead, round wins or, in survival, points, weapon, and the three overrides.
- **Tabs**: Player, Weapons, Round (duel only), Movement, Debug, Survival (survival only). The header shows which mode is running.
- **Footer**: the last command and who it went to, for example `FULL HEAL  >  PLAYER 1`.

| Tab | What it does |
|---|---|
| Player | Set health, full heal, god mode, reset, kill, archetype |
| Weapons | Equip one of the four firearms, refill ammunition, infinite ammunition |
| Round | Restart or advance the round, award it, set scores, reset the match (asks twice) |
| Movement | Live state, speed, stamina, mode and position; refill or infinite stamina |
| Debug | Debug overlay and hit zone drawing on this screen; network and round state |
| Survival | Wave, zombies left, phase, zombie damage; Kill All, Next Wave Now, Restart Run, +5000 points, zombie damage on or off |

Kill All removes the whole wave, including what has not spawned yet; killing only the living would leave the rest of the queue to follow.

Checked in a play session: opened with a real `F1` in Zombie Survival, the Survival tab shown and Round hidden, Kill All ended the wave (zombies left 0, phase break), +5000 points arrived. `ArenaDuel.Admin.*` tests pass. Not checked: the window in a duel session, a game started outside the editor with and without the switch, and other screen sizes.
