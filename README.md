# Project Diva Recomp

Static recompilation of *Hatsune Miku: Project DIVA* (PSP, ULJM05472 v1.01) to run natively on PC, built so that mods are easy to create.

**Status:** playable but incomplete. Songs run from start to their results screen at 60 fps with sound, drawn by the GPU at up to 8 times the PSP's resolution, and progress is saved to `%LOCALAPPDATA%\ProjectDivaRecomp\data`. The menus can be played in English, German or Japanese. System dialogs and the save slot list are not shown, the opening movie is skipped because there is no video decoder yet, and ad hoc play does not work.

What has changed is in the [changelog](CHANGELOG.md). To install and play, see [INSTALL.md](INSTALL.md).

## Before you play: set Note timing

**This is strongly recommended.** Sound and picture take a moment to reach you on a PC, and that moment differs from one computer, monitor and pair of headphones to another. Until it is set, your presses can count as late even when they feel right, which makes Cools and long combos harder than they should be.

1. Play one song all the way through, following the sound.
2. In the main menu, open **Options** and go to **Note timing**. Its description says how late or early your presses were in the last song.
3. Press left or right until it says **on time**, then play again.

Check it again if you change your headphones, speakers or monitor. Bluetooth headphones in particular can add a lot. This setting is in the recommended edition, which is the default.

## Suggestions welcome

I am accepting suggestions for things to add to the game. If there is something you would like to see, [open an issue](https://github.com/Rhalekey/ProjectDivaRecomp/issues).

## Found a bug?

Please report it. [Open an issue](https://github.com/Rhalekey/ProjectDivaRecomp/issues) saying what you were doing and what happened, with a screenshot if it helps. If the game failed or crashed, attach `build\logs\build.txt` and `build\logs\run.txt` from the project's folder. Your home folder is written as `%USERPROFILE%` in them, so your user name does not show in those paths, but give them a look first if you prefer. Never attach the game's files.

## Made with AI, and a note to rights holders

I made all of this project with AI (Claude, by Anthropic): the code, the tools, the translations and this documentation. I directed it and tested it, but I did not write it by hand, so expect mistakes.

I have tried to keep everything from the game out of this repository. If I have accidentally violated any right or broken any law, any company connected with the game can contact me by [opening an issue](https://github.com/Rhalekey/ProjectDivaRecomp/issues), and I will cooperate.

## You need your own copy of the game

This repository contains no game files, no assets and no code generated from the game, and it will not accept any. You must supply a legally obtained copy and dump it yourself.

The expected input is the decrypted executable, which can be dumped with PPSSPP.

## Building

Windows only for now. You need Python 3 and your dump, with the decrypted executable saved as `PSP_GAME\SYSDIR\ULJM05472_EBOOT.BIN`.

```
git clone --recurse-submodules https://github.com/Rhalekey/ProjectDivaRecomp.git
powershell -ExecutionPolicy Bypass -File Tools\build.ps1 -Game <folder holding PSP_GAME>
```

The script installs a compiler once into `%LOCALAPPDATA%\ProjectDivaRecomp`, generates C from your executable, builds `build\game\diva.exe` and runs it for 60 seconds. Logs and the last frame go to `build\`. The generated code is derived from your copy of the game, so it stays in `build\` and must not be shared.

To play, double-click `Play.bat` once it is built. For development, a `dev.ini` file in `%LOCALAPPDATA%\ProjectDivaRecomp` turns on test switches, such as a life gauge that stays full, as `src\host\settings.c` explains. Without that file they are all off, and `Play.bat` ignores it.

## Graphics

The recommended edition adds a **Graphics** row to the main menu: display mode (window, borderless or full screen), resolution (1x to 8x), smoothing, image shape (bars, crop or extend) and the frame rate counter. F11 or Alt+Enter switches between the window and full screen.

The frame rate cannot be chosen yet: the game's speed follows its refresh rate, so it stays at 60.

## Editions

The same program plays two ways, set by a file in `profiles\`:

| | `raw` | `recommended` (default) |
|---|---|---|
| Resolution | The PSP's | Three times the PSP's |
| Picture | Hard pixels | Smoothed |
| Songs | About 30 fps | 60 fps |
| Messages about saves | None | Shown |
| Mods | Off | On |
| Language question and English | None | Asked on every start |
| Letters and redrawn pieces | The game's own | Drawn at 4 times the size |

Build and run one with `Tools\build.ps1 -Edition raw`. To change a setting for yourself, put the line in `%LOCALAPPDATA%\ProjectDivaRecomp\settings.ini`, which is read after the edition's file.

The raw edition is as close to the original as this project can make it, not a proven match. It still has no system dialogs, and its frame rate has not been compared with a real PSP.

## Languages

The game can be played in Japanese, English and German. It asks which language to play in each time it starts, between its intros and its title. Put `language_prompt = 0` in `settings.ini` to be asked only when no choice has been saved. Choices are kept beside the saves, in `ms\PSP\SAVEDATA\ULJM05472RECOMP\OPTIONS.INI`, not inside the game's save, whose format stays as a PSP would read it.

The German translation was written without a native speaker and has not been reviewed by one yet, so expect some awkward or wrong wording. Corrections are welcome in `lang/de.txt` and `lang/de_images.txt`.

The recommended edition adds rows to the game's own Options window: language, song names and lyrics in romaji (each on its own), whether to ask the language at each start, the confirm button, and the keyboard and controller buttons, with a second key if wanted. It also adds a background dim for songs, from off to 100%, and [Note timing](#before-you-play-set-note-timing), from -100 to +300 ms. The song names come from `lang\romaji_titles.txt`. The lyrics are never in this repository: `Tools\romaji.py` makes them during the build from your copy of the game, using a dictionary, so a line can come out misread. The language changes at once. These are kept in the same file, and the row that asks wins over `language_prompt`. Put `extras = 0` in `settings.ini` to leave the window as the game has it.

The Japanese is never replaced. `lang\en.txt` lists the address of each original text and its English, and the game's words are swapped as they reach the screen. Song names, lyrics and illustrator credits have no lines in it, so they stay as they are. `lang\en_images.txt` does the same for words drawn into pictures: `Tools\lang_images.py` makes those pictures from your copy of the game each time it builds, in a folder outside this repository.

`Tools\lang.py` shows both sides of a translation, and lists what the game has displayed with no translation yet.

## Fonts

`fonts\` holds [M PLUS Rounded 1c](https://fonts.google.com/specimen/M+PLUS+Rounded+1c) under the SIL Open Font License (`fonts\OFL.txt`). `Tools\hd_font.py` redraws the game's letters from it at four times their size. To use another font, put its file in `%LOCALAPPDATA%\ProjectDivaRecomp\fonts`, where it takes priority. Fonts placed there stay on your computer.

## Larger textures

`Tools\hud_vector.py` draws the song screen's HUD and note symbols from vectors, and `Tools\hd_font.py` and `Tools\lang_images.py` redraw letters and translated words from a font, all at four times the size. Every other texture is the game's own, unchanged. Set `hd_textures = 0` to play with the originals everywhere.

To list what is still at the game's own resolution, build with `Tools\build.ps1 -SaveTextures`. Those textures are saved to `%LOCALAPPDATA%\ProjectDivaRecomp\data\texture_cache`, beside the saves. They are the game's pictures, so never share them.

## Mods

A mod is a folder in `%LOCALAPPDATA%\ProjectDivaRecomp\mods`, and every file in it replaces the game's file of the same name. Your game files are never changed, and removing the folder undoes the mod. The `raw` edition ignores them.

```
mods\my mod\title.png                     an entry in an AFS archive
mods\my mod\pv_01_easy.dsc                folders inside a mod are only for tidiness
mods\my mod\disc\PSP_GAME\USRDIR\x.ini    any other file on the disc
```

If two mods replace one file, the later folder in alphabetical order wins. Use `Tools\afs.py` to find and extract files to start from. A replacement must be in the format the game expects, and very large ones may be more than the game has memory for. Never share files extracted from the game.

## Tools

| Script | Purpose |
|---|---|
| `Tools/check_file.py` | Prints a file's size, SHA-256 and first bytes |
| `Tools/elf_info.py` | Prints the ELF header, segments and PSP module info |
| `Tools/afs.py` | Lists and extracts the files inside the game's AFS archives |
| `Tools/lang.py` | Shows a translation beside the original, and lists what is still untranslated |
| `Tools/lang_images.py` | Makes translated copies of the game's pictures of text |
| `Tools/hd_font.py` | Redraws the game's bitmap fonts from a font file |
| `Tools/psp_analyze.py` | Lists sections, imports and exports, counts instructions and finds relevant strings |

```
python Tools/psp_analyze.py <path to decrypted ELF> [--json output.json]
```

## Fonts and licenses

The MIT license below covers this project's code. `fonts\` is under its own license, as `fonts\OFL.txt` says.

The build downloads [SDL3](https://www.libsdl.org/), under the zlib license, from its official release and checks its hash. It reads controllers other than Xbox ones.

## License

The code in this repository is under the [MIT License](LICENSE). It does not cover the game or its assets, which belong to their respective owners. This project is not affiliated with SEGA or Crypton Future Media. The `lang\` tables are unofficial fan translations.
