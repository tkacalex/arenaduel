# Round flow (1v1)

Phase 8A. Server-authoritative; the relevant code is `AArenaDuelGameMode`, `AArenaDuelGameState` and `UArenaDuelHUDWidget`.

## Phases

Character select, ready (or auto ready after 12 s), a 3.35 s countdown, then rounds until one player has five round wins. After a decided round both players take a match upgrade (at most 10 s, nobody moves; see `docs/MatchUpgrades.md`). A round without a winner is followed by a 3 s break in which nobody has won and nobody moves; after the match the result stays for 5 s and both players return to character selection with everything reset.

## The round clock

- `RoundDurationSeconds` (90) on the game mode. `BeginRound` gives every started round a serial, sets the round state, publishes `RoundEndServerTime` on the game state and arms one server timer for that serial.
- Clients never count down themselves. `GetRoundSecondsRemaining()` is `RoundEndServerTime` minus the synchronised server time, so a local menu, a hitch or a late join cannot stop or shift it. The local pause/settings menu does not pause the game.
- The match header shows `R<round>   M:SS`, rounded up, red in the last ten seconds.
- Survival overrides `UsesRoundTimer()` to false and never calls `BeginRound`; its game state never carries a clock and its HUD keeps the plain label.

## What happens at the limit (interim rule)

If both players are alive when the 90 seconds are over, **overtime** begins: the header reads `OVERTIME`, a banner says `OVERTIME - NEXT KILL WINS` for four seconds, and the round simply goes on. Health, ammunition, positions, abilities and cooldowns are untouched. The next regular kill ends the round exactly as a kill inside the 90 seconds would. There is no extra damage, no shrinking zone, no upgrade. Deaths are handled one after another on the server, also inside one frame: the first death ends the round and gives the other player the win, and damage that arrives after that, a lethal hit on the winner included, no longer counts. There is no simultaneous double death and no drawn round from two kills.

This is deliberately a placeholder for the overtime objective named in `docs/GAME_DESIGN.md`. An overtime has no time limit of its own.

## One evaluation per round

`EndRoundForDevelopment` is the only place a round is decided. It refuses a second call for the same round serial, counts the evaluation, clears the time limit and awards at most one round win. Deaths, the time limit and the admin menu all go through it.

- A kill and the limit in the same frame: whichever is processed first wins. Kill first: the round is over and the limit does nothing. Limit first: overtime begins and the kill decides it. One point either way.
- A time limit carries the serial of its round; one that fires for an older serial is ignored.
- `ClearPendingRoundAndMatchTimers` clears the time limit together with the other round and match timers. It runs on a restart, a match reset, the return to selection and when a player leaves. A player leaving returns the duel to selection without a point.
- Every change of round state clears the clock on the game state; only `BeginRound` sets a new one.

## Development commands (not in shipping builds, server only)

- `ArenaDuel.Round.TimeLeft <seconds>`: set the regular time left in the running round.
- `ArenaDuel.Round.Kill <0|1>`: kill the player in that duel slot through the normal damage path.

## Tests

- `ArenaDuel.Phase8.RoundRules`: 90 seconds, five wins, the clock format, no clock in survival.
- `ArenaDuel.Phase8.RoundFlow` (listen server and one client, shortened times):
  - the clock on server and client, the limit with both alive, overtime keeping the same pawn, health, ammunition and position, the client seeing overtime, the next kill deciding once even when a death, an admin award and the limit all ask again, and a clean next round;
  - kill then limit and limit then kill in one frame, and a stale limit;
  - the fifth win, the return to selection, a new match with a full clock, and a player leaving in overtime.

Checked with host and client in the editor (2026-10-11): both headers counting down the same time, red last seconds, the overtime banner, overtime lasting until a kill, the kill ending the round with one point, round two with a fresh clock, a kill within the last three seconds giving one point, the fifth win and `FINAL SCORE 1 - 5`, selection, and a new match at 0 : 0 with a clock. The first of these runs used the real 90 seconds; the rest were driven with the two commands above. A real disconnect was not checked in a session: closing the client's window ends the whole editor play session. That case is covered by the automated test, which logs the player out on the server.
