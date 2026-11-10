# Changelog

Everything this project has added to or changed about *Hatsune Miku: Project DIVA* (PSP, ULJM05472 v1.01) on PC, newest first. All of it was made with AI; see the [README](README.md#made-with-ai-and-a-note-to-rights-holders).

The recompiler's own changes are in the [psprecomp fork](https://github.com/Rhalekey/psprecomp/tree/diva-recomp), branch `diva-recomp`. Those this game depends on are listed here too.

## 2026-11-09

### Controls and notes
- A press counts when it was made, not when the game next looked at the buttons: the game sees them once a frame, so a press could count up to 16 ms late. The buttons are read about once a millisecond, and the note is judged as it stood then. With this the measure for **Note timing** comes out a few milliseconds nearer zero, so it is worth measuring again.
- After a hold-up in a song, as a restart or a controller plugged in, the sound no longer stays late to the notes for many seconds: what is left waiting is cut back at once. The measure at the end of a song also says how long the sound waited in each half.

### Redrawn
- The Project DIVA badge in the demo's corner and the lists: Miku's head and twin tails in clean lines, with Project DIVA on her front. The game's is too small to trace, so it is drawn anew in its spirit.
- The title screen's logo: its edges take the colour from inside each part, so they no longer step in a light fringe, and specks are left out.
- The makers' logos at the start are shown as the game has them again: traced, they came out with too many flaws.
- The menus' titles: the dark band under the plate with the menu's name runs on to its cut edge in one diagonal, and where lines run under it, the white diagonal that takes them up and its shadow are straight too. They were the last steps left in the titles.
- The lists' rows and the glow of the one chosen: each row of their streaks is one even blend along its length, with no dithering and no blotches where the glow fades out.
- Rhythm Game Edit's icons in clean lines, in their two blues: on the grid's buttons, lit or not (the arrows, the house, the note and the ©), and the camera's move, turn, preview, time and camera.
- The notes and targets as Xbox or Nintendo letters, or arrows, fill the same square as the PlayStation shapes, B a little narrower as letters are: the letters were narrower, so they looked smaller. Their targets are all of one size too: the game draws each of its shapes at a scale of its own, and that scale had been kept, so the one in place of the square was about a third larger than the one in place of the circle. The letters in the hints' buttons keep their shape.

### Menus
- In Rhythm Game Edit, the number of bars has a space before its word (2 Bar, 2 Takt), and the rhythm screen's title in English is shorter: Rhythm Settings.
- Hints for what this edition adds, in the footer: **L R: Module** and **SELECT: Sort** in the song list, **SELECT: Sort** on the modules' screen. They come and go with the game's own hints.
- The footer's hints no longer overlap on screens whose hints change with the row chosen, as Options.
- The footer's band slides in to its place instead of jumping there once the game's slide has stopped. Where it goes is kept in `footer_places.txt` beside the save, so only a screen never seen before in that size of window can still jump, once.
- The messages for sorting are in the language played: English, German or Japanese.
- A module's name in the message as it changes keeps its German letters, as ß.
- Thin letters, as l and i, no longer have a sliver of the next letter beside them.

### Graphics
- Pictures with words are kept per language. Those of the last language made were also kept for all, so Japanese showed the German ones, and English only showed its own from copies left by older builds. About 220 MB less on disk.
- A picture drawn at several times the window's size, as at a high render scale in a small window, is shrunk in halves, so its edges stay smooth.

### Discord
- The status now shows Home and Miku's Room. Their words reach the screen through the game's text queue, which the status did not look at.

## 2026-11-08

### Controls and notes
- **Button icons** in Options: PlayStation, or A, B, X and Y as on Xbox or Nintendo controllers, by where each button is. The menus, the screens over the game and the card before a song follow it.
- **Notes** in Options: the notes and their targets as PlayStation shapes, Xbox or Nintendo letters in their colours, or arrow keys. A target takes the shape of its letter or arrow, with the game's own clock hand.
- At first start, after the language: the button icons, then the confirm button drawn in them, then the notes, each with a preview. Back goes a step back.
- The face buttons are named by where they are: South, East, West and North.
- **Hit notes with** in Options: the stick, the d-pad (arrow keys included), both or neither also hit notes in songs, each way the button on that side: up Triangle, right Circle, down Cross, left Square.
- The left stick works as the d-pad, past half way, in menus and songs.
- **Quick restart**: L and R together in a song play it again from the start. Its keyboard key and controller buttons can be changed in Controls. It puts the pause menu's cursor on RETRY itself.
- **Sorting**: Select in the Free Play song list goes through the game's order, by name, by best score and by BPM (slowest first, as the song list shows it) and last played first (kept in `PLAYED.INI` beside `OPTIONS.INI`, as the game keeps no such thing); on the modules' screen, through the game's order, by name, those obtained first and those last played in first. Out of the game's order, songs not unlocked and modules not obtained go last.
- **Quick module change**: L and R in the song list change the module worn, between those obtained, of any singer, as the game allows. Miku behind the menu changes into it as on the modules' screen, with her changing motions. On Kagamine Rin's and Len's songs they change theirs, between their own and their swimwear.
- **Rin & Len songs** in Options: the game puts Kagamine Rin and Len in their own songs whatever is worn. Set to Module, those songs are sung in the module worn, as all the others are.
- The Controls screen no longer waits for ever on a button or key that never reads as let go.

### Graphics
- **Blur** in Graphics turns the blurred copies past a picture's sides off. Strongly recommended off in 16:9.
- In windows wider than 16:9, up to 32:9: the footer, the results and the language screen keep to the corners, the start menu is no longer stretched, the combo stays with its notes, the meter and lyrics no longer stick to the middle at a song's start or after a chance time, and the card before a song shows again.
- Pictures of a song's stage get a blurred copy beside them, and the white light over some stages reaches the window's edges.
- Faster blurs, drawn from a smaller copy of the picture, and nothing drawn past what the window shows.

### Redrawn
- The language and controls screens, in a plain modern look in the game's teal. Their hints name keys or a controller's buttons by what was pressed last.
- The menus' footer is laid out at the widths of its words, and its band follows them.
- The title screen: its logo, traced smooth from the player's copy, and its menu and PRESS START in the game's squared letters, in English and German. The PRESS START of the demo that plays after it too.
- The makers' logos at the start, traced smooth.
- The notes and targets in their other styles, and the card's and results' signs for them.
- The card before a song: difficulty, stars, numbers, ON and OFF, START and RETURN, and its buttons.
- The results: the rank (PERFECT to MISS×TAKE, in both of its flashing colours), the difficulty, and the colons of the list.

### Options
- Options and Graphics are in a new order: what changes a song, the buttons, then the language.

## 2026-11-07

- On the results screen in a taller window, NOT CLEAR goes down with the bottom band.
- The d-pad's circles in the menu hints are slightly smaller.
- OK, Back and Retry are centred on their buttons, with their signs.
- Test switches moved to a developer's own `dev.ini`, and are all off without it.
- The test bot is gone, as there is nothing left for it to collect.
- Each button can have a second key, to alternate on fast repeated notes.
- **Background dim** in Options darkens a song's background from 1% to 100%, leaving the notes and HUD as they are, on 3D stages and 2D backgrounds alike.
- Fixed a white fade when coming back to the main menu from Graphics.
- Fixed the HUD jumping around in songs with a full-screen 2D background, in windows of other shapes.
- **Note timing** in Options moves when a press counts as on time, from -100 to +300 ms, leaving the notes and the song as they are. Each song measures how late or early your presses were and the row shows it, so setting it right reads as "on time".
- Buttons are read about once a millisecond instead of every 16 or more, and a press between two of the game's reads is no longer lost.
- Fixed songs with a full-screen 2D background, such as Levan Polkka, stuttering at high resolutions in windows the background does not fill. The blurred copy behind a picture is now drawn only around it.
- Sound reaches you sooner and stays there: its queue used to grow during a session, past 180 ms in a long one, and now holds at about 25 ms, played off slightly faster when it runs long. This can leave more short gaps when moving between screens. If you set Note timing before this change, check it again.
- The run log says how long sound waits before it reaches the sound card, and what each song measured.

## 2026-11-06: first public version

### Graphics
- New **Graphics** row in the main menu, in the game's own list, with its own descriptions in English, Japanese and German.
- Display mode: window, borderless or full screen. F11 and Alt+Enter switch between the window and the last covering mode.
- Resolution of 1x, 2x, 3x, 4x, 6x or 8x the PSP's, changed while the game runs.
- Picture smoothing on or off.
- Image shape: bars, crop or extend (recommended).
- Frame rate counter, moved here from Options.
- All of these are saved in `OPTIONS.INI`.

### Image shape
- The image follows the window's shape, from 4:3 to wide screens, showing more of the 3D scene with no stretching.
- The HUD and bands go to the window's edges, lists stay on the left and previews on the right.
- Fades and veils cover the whole window.
- Full-screen pictures that have nothing past their edges get a blurred copy behind them.
- The language screen fills the window too.
- On the results screen in a taller window, the bottom band reaches the window's bottom, with the buttons' text.

### Performance
- Fixed a frame lost about every 15 frames at high resolutions, caused by a copy of the screen made only for a debug capture. Songs now hold about 60 fps at 8x.
- Removed a leftover diagnostic that ran on every draw.
- The build skips the tests and code generation when nothing they depend on has changed.

### Text and menus
- Text is spaced by each letter's real width, in place of the fixed cells the game gives Latin letters.
- Menu pieces in English and German are redrawn as vectors, with signs that follow the confirm button.
- The song screen's HUD, notes and targets are redrawn in code on a single grid.

### Controls
- Keyboard keys and controller buttons can be changed in Options.
- The confirm button, Circle or Cross, is asked at first start and can be changed in Options. Notes in a song keep their own buttons.

### Saves
- Messages in the game's language for saving, deleting and failing to save. Loading and having no save stay silent.
- A save that exists but cannot be read shows a warning until a button is pressed.
- The last frame stays on screen while saving, removing a black flash.

### Fixed
- A warning waiting for a button could be dismissed by the same press that brought it on.
- RETRY and RETURN sat low on their plates.
- The combo counter's word overlapped its digits.
- Ad Hoc menu lines did not share a centre.

### Removed
- Texture filters, mipmaps, anisotropic filtering and their shortcuts: they gave no visible gain. Better textures will be redrawn by hand, not generated by AI.
- Everything that asked testers to send zips of the game's pictures.
- Portuguese names and messages in the scripts and guide: everything public is now in English.
- The last of the game's words from the source.

### Logs
- The logs stay on the player's PC, are replaced each session, and write the home folder as `%USERPROFILE%`, so a user name does not show in their paths if someone attaches them to an issue.

### Test tools, not used by players
- Keys to compare drawn pieces with the originals and to trace a frame.

## 2026-11-05

- **German**, with its letters drawn over half-width kana the game's font does not use. It has not been reviewed by a native speaker yet.
- **Romaji** for song names and lyrics, each switched on its own. Lyrics are never in this repository: the build makes them from the player's copy of the game.
- More rows in the game's own **Options** window: language, whether to ask it at each start, and the frame rate counter.
- **Controllers** other than Xbox ones, through SDL3: PlayStation, Nintendo and many more, all usable at once.
- The language screen comes between the intros and the title, with flags drawn in code, and the language changes at once in Options.
- The song's HUD ring, level and icons are redrawn from measurements of the original.
- Translated text and buttons stay centred. Dialog lines are measured from the translation.
- The installer warns about Windows' Smart App Control before it blocks the build.

## 2026-11-04: from nothing to playable

### Running the game
- Recompiled the game's code to C, and added the firmware calls it needs.
- It boots, reaches the menus and plays songs to their results.
- Real threads, real-time waits, and the display interrupt placed mid-frame for a steady 60 fps.
- Sound output, and the game's model skinning.
- OpenGL renderer at three times the PSP's resolution.
- Saves stored for real, outside the build folder.
- The opening movie is skipped until there is a video decoder.

### Making it yours
- **Editions:** `raw`, as close to the original as this project can make it, and `recommended`, the default.
- **Mods:** folders of files that replace the game's by name, with nothing on disk changed.
- **English** menus and dialogs, with the game's Japanese left where it is and swapped as it reaches the screen.
- Pictures with words in them are translated at build time, from the player's own copy.
- Letters are redrawn from a free font, and the HUD from vectors.
- `Install.bat` sets the game up from the player's own ISO and the executable they extract with PPSSPP, checking both versions.

## Known gaps

- No video decoder, so movies do not play.
- Ad hoc play does not work, and system dialogs and the save slot list are not shown.
- The frame rate cannot be chosen: the game's speed follows its refresh rate, so it stays at 60. Other rates would need interpolation or changes to the game.
- The original art outside the redrawn pieces is still at the game's own resolution.
- Moving between screens can drop a few frames, and now and then cuts the sound for a moment. Songs themselves hold about 60 fps.
- Japanese versions of the vector pieces do not exist yet, so Japanese uses the game's own.
