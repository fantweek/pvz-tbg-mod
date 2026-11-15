<p align="center">
  <img src="assets/banner.png" alt="PvZ TBG Mod" width="720">
</p>

<p align="center">
  <a href="https://github.com/fantweek/pvz-tbg-mod/actions/workflows/ci.yml"><img src="https://github.com/fantweek/pvz-tbg-mod/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
</p>

<p align="center"><a href="README.md">Русский</a> · <b>English</b></p>

# PvZ TBG Mod

An in-game mod menu for **Plants vs. Zombies GOTY Edition (Steam, version 1.2.0.1096, English)**. Press **Tab** in game to open it.
Single-player only. The interface is in Russian.

> Unofficial fan project, not affiliated with PopCap / Electronic Arts. The repository contains no game files; you need your own copy of the game.

## Features

- **Plants:** free plants, sun giveaway, sun value multiplier, sky-sun and sun-plant speed (faster or slower), planting bonus, auto-collect, attack speed, seed recharge, premium plants without the base plant, plant anywhere, health / invincibility.
- **Zombies:** wave size and composition, wave and walking speed, body/armour health, **sun drops from zombies**, zombie cost in "I, Zombie".
- **Level:** random events, random plants / zombies / seed set, **conveyor belt** (feed speed, custom plant pool).
- **Game:** unlocks, level launcher, money and Zen Garden items, plant and achievement grants, coin value, game speed.
- **Builds:** 11 ready-made builds with adventure progress tracking and a **freeze** of all other mod functions while the build is active; saved configurations.

Hotkeys: **Tab** menu, **F6** give sun, **F7** kill all zombies, **F8** speed ×2.

## Screenshots

<p align="center">
  <img src="docs/screenshots/plants.png" width="49%" alt="Plants tab">
  <img src="docs/screenshots/zombies.png" width="49%" alt="Zombies tab">
  <img src="docs/screenshots/level-conveyor.png" width="49%" alt="Level tab: conveyor">
  <img src="docs/screenshots/builds.png" width="49%" alt="Builds tab">
</p>

<sub>Rendered by the repository's own menu code on Linux (`python tools/screenshots/make_screenshots.py`) with DejaVu Sans; in game the menu uses Segoe UI. Values are sample data.</sub>

## Install

1. Download the zip from [Releases](https://github.com/fantweek/pvz-tbg-mod/releases) and put `pvzmod.dll` and `PvZModLauncher.exe` next to `PlantsVsZombies.exe`.
2. In Steam → game → Properties → Launch options: `"C:\path\to\PvZModLauncher.exe" %command%`.
3. Start the game and press **Tab**.

More: [docs/INSTALL.en.md](docs/INSTALL.en.md).

## Status

Beta. The mod only enables itself on Steam build 1.2.0.1096 (it checks the exe timestamp) and verifies code bytes before patching.
Logic is unit-tested against simulated game memory (`python scripts/build.py test`), but not everything has been verified on a real game.
Not implemented: 16:10 widescreen (needs layout addresses for this build).

## Build

```bash
pip install ziglang pillow
python scripts/build.py all
```

Cross-compiles 32-bit Windows binaries with zig; no MSVC needed. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## License

[MIT](LICENSE); third-party: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
