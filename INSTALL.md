# Project DIVA Recomp: how to install and play

This project runs *Hatsune Miku: Project DIVA* (PSP) natively on PC. It includes nothing from the game: each person uses their own copy, and the PC version is generated on their own computer. Do not share the game's files or anything the build generates from them.

**Made with AI:** I made this whole project with the help of AI (Claude, by Anthropic). The full notice, including how companies connected with the game can reach me, is in the [README](README.md#made-with-ai-and-a-note-to-rights-holders).

What has changed is in the [changelog](CHANGELOG.md).

## What you need

- Windows 10 or 11.
- The game's ISO: Japanese, **ULJM05472 v1.01**. The installer checks the version.
- The [PPSSPP](https://www.ppsspp.org/) emulator, only for step 2.
- [Git](https://git-scm.com/downloads), only for step 1.
- [Python](https://www.python.org/downloads/). On the first screen of its installer, tick **Add python.exe to PATH**.
- Internet during installation, to download the compiler, controller support and the dictionary for romaji lyrics.
- About 5 GB free.

## 1. Download the project

In a folder with a simple path, such as `C:\ProjectDiva`, run:

```
git clone --recurse-submodules https://github.com/Rhalekey/ProjectDivaRecomp.git
```

GitHub's "Download ZIP" button will not work, because it leaves out the recompiler, which is in a submodule.

## 2. Extract the game's executable

The executable on the ISO is encrypted, and the project needs the open version, which PPSSPP saves for you:

1. In PPSSPP, go to **Settings > Tools > Developer tools** and turn on **Dump Decrypted EBOOT.BIN on game boot**.
2. Open the game, wait for the first screen to appear, and close it.
3. The file is in PPSSPP's memory stick folder, in `memstick\PSP\SYSTEM\DUMP`, with `ULJM05472` in its name.

Menu names can vary with PPSSPP's version and language. If you cannot find the option, look for "Dump" in the developer tools.

## 3. Install

Double-click **`Install.bat`**. It asks for the ISO and the file from step 2, checks both, and compiles the game.

Compiling is slow, and it is normal for it to seem stuck: it can take a long time. It only happens once.

## 4. Play

Double-click **`Play.bat`**. After the intros, the game asks for a language: Japanese, English or German.

| Keyboard | PSP button |
|---|---|
| Arrows | D-pad |
| X | Circle (confirm) |
| Z | Cross (cancel) |
| A | Square |
| S | Triangle |
| Q and W | L and R |
| Enter | Start |
| Backspace | Select |

F11 or Alt+Enter switches between the window and full screen.

Xbox, PS4 and PS5 controllers and Nintendo's Pro Controller should work over USB or Bluetooth, as should many others. If one is on USB and does not respond, try a rear port. With more than one connected, they all work at once, as if they were one.

Buttons go by position, not by what is printed on them. The bottom button is always the PSP's Cross: A on Xbox, ✕ on PlayStation and B on Nintendo.

## 5. Options

- **Options**, in the main menu: volume, language, song names and lyrics in romaji, the confirm button, and changing keys and buttons. Each button can have a second key, which helps with fast repeated notes.
- **Graphics**, in the main menu: display mode, resolution, smoothing, image shape and the frame rate counter.

The opening movie does not play in this version, and ad hoc play does not work. This is expected.

## If something goes wrong

Open an [issue](https://github.com/Rhalekey/ProjectDivaRecomp/issues) saying what you were doing, with the files `build\logs\build.txt` and `build\logs\run.txt`, from the project's folder. Your home folder is written as `%USERPROFILE%` in them, so your user name does not show in those paths, but give them a look first if you prefer. Never attach the game's files.

| What appears | What to do |
|---|---|
| `Python not found` | Install Python, ticking **Add python.exe to PATH**, and open the installer again |
| `This ISO is not the original ULJM05472 v1.01` | Your copy is another version or is patched. The original is needed |
| `This executable is not the decrypted one of ULJM05472 v1.01` | Choose the file from step 2, not the `EBOOT.BIN` from the ISO |
| A controller does not respond | Try another USB port |
| `An Application Control policy has blocked this file`, or the installer warns about Smart App Control | See below |

### Windows blocking the game

The game is compiled on your PC and has no digital signature, and Windows 11's **Smart App Control** blocks programs like that. To check, run this in PowerShell:

```powershell
(Get-MpComputerStatus).SmartAppControlState
```

- **`On`:** it is blocking the game. To play on this PC, it has to be turned off in **Windows Security > App & browser control > Smart App Control settings**. **That decision is yours:** it is a system protection, and in many versions of Windows it can only be turned back on by reinstalling Windows. Read the warning before confirming, or use another PC.
- **`Off`:** the cause is something else, such as a company or school policy. In that case, talk to whoever manages the PC.

Your saves are in `%LOCALAPPDATA%\ProjectDivaRecomp\data\ms\PSP\SAVEDATA`.
