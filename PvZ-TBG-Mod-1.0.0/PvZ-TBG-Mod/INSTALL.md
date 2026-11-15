# Installing PvZ TBG Mod

Requires **Plants vs. Zombies GOTY Edition from Steam, version 1.2.0.1096 (English)**. On any other build the mod detects a foreign exe and stays disabled.

## Files

The archive contains `pvzmod.dll` and `PvZModLauncher.exe` (copy these into the game folder) and `source/` (the project source code, not needed to play).

Close the game. Steam → Plants vs. Zombies GOTY → Manage → Browse local files. Copy `pvzmod.dll` and `PvZModLauncher.exe` next to `PlantsVsZombies.exe`.

## Launching (pick one)

- **Steam launch option (recommended):** Properties → General → Launch Options:
  `"C:\full\path\to\PvZModLauncher.exe" %command%`. The normal *Play* button then starts the game with the mod.
- **Watcher:** run `PvZModLauncher.exe` once and answer *Yes* to the autostart question. It stays in the tray and injects the mod whenever the game starts
  (`--watch`, `--autostart on|off`).
- **Manual:** start the game, wait for the main menu, run `PvZModLauncher.exe`.

Press **Tab** in game to open the menu.

## Data

`PvZMod\` next to `pvzmod.dll`: `last.ini` (current settings), `profiles\*.ini` (saved configurations), `runs.ini` (build progress),
`backup\userdata_first_run` (copy of your game save taken on first launch).

## Troubleshooting

- *"Unable to load Steam.dll"*: the game itself shows it when a second instance starts or when it is launched outside Steam. Close every `PlantsVsZombies.exe`
  and `PvZModLauncher.exe`, launch via *Play*, and don't run Steam and the launcher with different privileges.
- *Menu not visible over the game*: use windowed mode.
- *"Part of the patches disabled"*: code bytes didn't match — a different build or another mod loader.
- Antivirus warnings: the launcher injects a DLL into the game process; the source is open and the build is reproducible.

## Uninstall

Delete `pvzmod.dll`, `PvZModLauncher.exe` and the `PvZMod` folder, clear Steam launch options, and turn off autostart (tray menu or `--autostart off`).
