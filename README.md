# Sacred Gold Vanilla Language Loader

Universal language loader mod for **Sacred Gold** (Steam, GOG, and CD/DVD retail releases).
Allows the game to load external translated `global.res` files without modifying `Sacred.exe` and without touching original game files.

---

## The Problem in Vanilla Sacred

In the official releases of Sacred Gold, `Sacred.exe` does not load string tables from external files on disk.

The original English string table is embedded directly inside the `Sacred.exe` executable as an internal Windows resource (Resource BINARY #107), encrypted with a rolling XOR cipher. On startup, the engine's internal language function directly invokes `FindResourceA` to load this embedded resource, ignoring any external files placed in language folders. Furthermore, the internal language table inside the executable rejects any language code that is not part of the default 11 built-in languages.

---

## How This Mod Solves It

Loaded as a plugin via **Sacred Gold Mod Loader**:
1. **Dynamic Language Detection:** On startup, reads the `LANGUAGE` key configured in `Settings.cfg`.
2. **In-Memory Encryption:** Locates `scripts\<LANGUAGE>\global.res` on disk, reads the translation, and applies Ascaron's native rolling XOR cipher directly in RAM.
3. **In-Memory Table Patching:** Dynamically updates Sacred.exe's internal language tables in RAM to accept the chosen language code, without altering the executable on disk.
4. **Resource Hooking:** Win32 resource APIs are hooked so that when the game requests embedded resource 107, it receives the translated data loaded from disk.

The engine then proceeds through its normal initialization routine, decrypting the string table natively.

---

## Prerequisites

This mod requires [sacred-gold-mod-loader](https://github.com/leonardokr/sacred-gold-mod-loader) installed in your Sacred Gold directory (`version.dll`).

---

## Installation

1. Make sure **Sacred Gold Mod Loader** is installed in your game folder.
2. Download `sacred-language-loader.dll` from the latest [Releases](../../releases) tab.
3. Place `sacred-language-loader.dll` into the `mods/` directory in your game folder:
   ```
   Sacred Gold/
   ├── Sacred.exe
   ├── version.dll                 <- sacred-gold-mod-loader
   └── mods/
       └── sacred-language-loader.dll
   ```
4. Create a folder matching your language code inside `scripts/` (e.g. `scripts\pt`).
5. Place your translated `global.res` file into that folder (e.g. `scripts\pt\global.res`).
6. Open `Settings.cfg` in a text editor and add or edit the `LANGUAGE` line:
   ```
   LANGUAGE : pt
   ```
7. Launch the game normally via Steam, GOG, or `Sacred.exe`.

---

## Building from Source

Requirements:
- Visual Studio 2022 (MSVC x86 / 32-bit compiler tools)

Run:
```cmd
src\build.bat
```
The compiled DLL will be placed in `bin\sacred-language-loader.dll` and root `sacred-language-loader.dll`.
