# Main menu and 1v1 lobby

## Main menu

The game starts on `L_MainMenu` (`GameDefaultMap`), a map with nothing but `AArenaDuelMenuGameMode`. Its player controller is the game's own controller, so saved settings apply here, and shows `UArenaDuelMainMenuWidget`.

| Page | Entries |
|---|---|
| Main | Play, Settings, Quit game |
| Play | 1 vs 1 - create lobby, 1 vs 1 - join a friend, Zombie Survival, Back |
| Join | Address field, Connect, Back |

Every button works with the mouse and has a hover and a pressed colour. With the keyboard, arrow keys or Tab move, Enter or Space activates, Escape goes back a page. A new page fades and slides in. Settings opens the existing settings menu (sensitivity, zoom sensitivity, field of view, volume, window mode, resolution, VSync, frame limit). Quit closes the game. The pause menu in a game has a new "Main menu" entry.

## 1v1 with a friend

Unreal's own listen server is used; the project has no online service such as Steam or EOS configured, so there are no invite codes, friend lists or matchmaking.

- **Create lobby** opens `L_ArenaDistrict` as a listen server. The host lands in the lobby, the character select screen that was already there: both players with their names, connection state, a ready button each, and a countdown into the match once both are ready.
- The host's lobby shows `HOST | JOIN: <address>:<port>`, this machine's address and the game port (7777).
- **Join a friend** takes that address. A bare address gets the default port. Text that is not a host name or address is refused before any connection is tried, including anything with a path or options in it.
- If the connection fails or the host goes away, the game returns to the main menu and shows why: no lobby at that address, connection lost, or different game versions.

What that means in practice: on the same network the shown address works as it is. Over the internet the host has to forward UDP port 7777 to their machine and give the friend their public address; the lobby shows the local one.

The duel itself is unchanged: two duel slots, a third connection gets no slot and cannot ready up or play, a player leaving returns the other to the lobby, and after a match the result is shown and both are back in the lobby on the same connection, where readying up again starts the next match.

## Tests

`ArenaDuel.Menu.JoinAddress` checks the address handling. In a play session the menu was clicked through main, play and join, an empty address produced the error line, and Zombie Survival was started from the menu.

## Not verified

- Creating a lobby from the menu and joining it from a second machine or a second game instance. Two players in one lobby were only run as an editor play session, where the editor makes the connection.
- The lobby's address line, the failure messages after a real failed join, keyboard navigation with real keys, the Settings and Quit buttons, and the pause menu's way back to the main menu.

## Not built

- Invite codes and internet play without port forwarding; both need an online service.
- A separate start button for the host: the match starts on its own countdown when both are ready, as before.
- Kills and deaths on the result screen, and a rematch button; the existing result shows the winner and the round score and returns to the lobby.
- Menu sounds. There are no UI sound assets in the project.
