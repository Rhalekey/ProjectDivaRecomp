# Changelog

Everything this project has added to or changed about *Hatsune Miku: Project DIVA* (PSP, ULJM05472 v1.01) on PC, newest first. All of it was made with AI; see the [README](README.md#made-with-ai-and-a-note-to-rights-holders).

The recompiler's own changes are in the [psprecomp fork](https://github.com/Rhalekey/psprecomp/tree/diva-recomp), branch `diva-recomp`. Those this game depends on are listed here too.

## 2026-11-11

### Installing
- **Faster pictures**: each language's pictures take about 3.5 minutes where they took 5.4, with the same bytes out. The pixels are written whole rather than a byte at a time, PNG rows with no filter are left as they are, a shape is filled a run at a time, and the menu sheets drawn once for each of the six sets of button signs are drawn once, only their signs for each.
- FFmpeg, SDL and the romaji's dictionary are downloaded while the game compiles, each waited for only where it is used.

### Fixes
- Files from outside read safely: a zip in `Import`, a PMX, FBX or IGB model, a TGA picture and the game's archives, cut short or made up, are refused or read within their bounds, where some were read past their end or asked for any memory a count said. A zip's names can no longer reach out of their folder through `\` or `:`.
- Keys pressed in another program no longer reach the project's own screens: an Enter typed elsewhere could answer "Delete this data?", and a key rebound or a letter typed into an Edit's name.
- `OPTIONS.INI`, `PLAYED.INI` and `SONGTIMING.INI` are written beside themselves and then put in place, so a game closed halfway leaves the old ones whole.
- Discord's status shows the translations' accented letters, where it showed half-width kana in their places.
- The Options rows' lines are cut short at their room if a translation is longer, never past it.

### Languages
- **Japanese menu pieces from vectors**: the footer's hints, the windows' buttons, the editor's buttons, the results' header and the titles are drawn as vectors in Japanese too, with the game's own words (`lang\ja_images.txt`), as they already were in the other languages. The cards before the songs keep the game's own Japanese. The build draws only these for Japanese, as its pictures with words are the game's.
- **French, Spanish and Italian**: the game's text, the words in its pictures and this project's own, each language with its flag on the language screen and in the Options row's turn. Written without native speakers, as German was; corrections are welcome.
- The Options row for the language goes through every language in `lang\languages.txt`, where it went through English, Japanese and German only. Right goes to the next and Left to the one before, there and in the rows for icons, notes and what hits notes, which all went forward whichever was pressed.
- A language's menu pieces are drawn again only when the words in its pictures change, not when another language's name is added to its words.
- **This project's own words in files**: the words of its menus, screens and messages, about 170 in each language, were in ten C files in four encodings. They are now in `lang\<code>_ui.txt`, `key = text` in UTF-8, read by `src/host/ui_text.c`, which gives each word as the game's font, the screens or the messages need it. The words drawn into the title screen's and the song list's pictures are there too, out of `Tools/ui_vector.py`. A word missing from a language shows in English.
- **One list of languages**: `lang\languages.txt`, read by the language screen, the translation tables and the build. A language is added with its files and a flag.
- **Accented letters**: `lang\letters.txt` lists the letters the game's font lacks and the characters it never shows that they are drawn over, read by the game and by `Tools/hd_font.py`. It has German's and those French, Spanish and Italian need, 53 in all. Before, German's seven were written out in four places.
- The game's memory keeps the translation and the Options rows' words of the language being played only, where it kept those of every language, so more languages take nothing from the game.
- The build draws the pictures of several languages at once, as many as the computer has processors and free memory for (about 400 MB each), in `Tools/lang_pictures.ps1`; on one processor, one at a time as before. English and German took 5 minutes in place of 11 on four.

### System screens
- **Ad Hoc over the local network**: Send Data and Receive Data reach other computers on the same network, or another copy of the game on the same computer, instead of the PSP's own wireless. Each copy says on the network who it is and which room it is in, a few times a second; rooms are found by those messages, and Edits go straight to the other computer, sent again until it confirms them. Others see the computer's name. `Tools\build.ps1 -Second` starts a second copy, with its own Memory Stick holding a copy of the main save and the options only, to try it on one computer.
- For it, in the recompiler: `sceNet`, `sceNetAdhoc`, `sceNetAdhocctl` and `sceNetAdhocMatching` over UDP, and Netconf's ad hoc dialog joining its room. The game's handlers are called at a frame's wait of the thread that uses the network: called at any wait, one ran inside the game's own allocator and the game crashed once the last data arrived.

## 2026-11-10

### System screens
- Music on the Memory Stick need not be MP3: an M4A or AAC (as a phone records), Ogg, Opus, FLAC, WAV or WMA file in `MUSIC` shows in the list as an MP3 of its name and plays, as does one of those kinds that is named .mp3. FFmpeg's reader reads it whole, as an M4A keeps its index at its end, and it is played at 44100 Hz in stereo; the build keeps `avformat` with FFmpeg's other DLLs for it. Export song takes such a file as it is. In the recompiler, bytes that only look like an MP3 frame no longer pass for one: another frame must begin where it ends.
- **Export song**: in Rhythm Game Edit's lists to load or delete from, Triangle (E on the keyboard) writes a zip of the chosen Edit's song to `Documents\Project DIVA Recomp\Export`: every Edit made with that song, and its MP3 if it is your own. A zip put in `Documents\Project DIVA Recomp\Import` is brought in when the list to load from opens: its Edits go into free slots, its MP3 into the Memory Stick's MUSIC, and the zip is moved to `Import\Imported`.
- For it, in the recompiler: the host's list of saves can ask for the slots to be read again and the player asked again.
- The system's message, which the game asks for only to report an error by its number (the Memory Stick full, save data damaged or missing, the stick not read or written), is told over the game as "Game saved" is, in the game's language with the error's number, and stays until a button is pressed. Before, it was answered unseen. In the recompiler, the message's kind, number, text and buttons are passed to the host.

### Interface
- **Module skins, head and body apart**: what the model's head alone moves goes into the head file, with pictures of its own, so the face has room for detail and each file stays under the game's room (about 1 MB each, about 1.5 MB together, past which its memory was wrecked); what the neck moves too stays in the body, so nothing parts where they meet. Making a model simpler costs more where the pictures would shift and around the head, so faces keep their shape. Pictures made smaller keep thin dark lines (eyelashes, eyebrows) by keeping the strongest alpha of each block. Feet and toes follow the leg bones MMD weights them to and are not stretched past twice. Models are made simpler to 10000 triangles by default.
- **Module skins from MMD and with many materials**: PMX models (2.0 and 2.1) are read too, their Japanese bone names and the D bones MMD legs are weighted to matched to Miku's, materials hidden until an expression shows them left out. Each material shows its own picture (a copy of the body's attribute set for each, materials showing one picture made one, pictures sized by their own size), TGA pictures are read. Models are made simpler by quadric edge collapse, keeping open edges and the pictures' seams, to 8000 triangles or `skin.ini`'s `triangles`, and further until the body file fits the game's room of about 1 MB, which files past it overran. Miku's own parts kept in each file are made as small as they can be (one triangle each, her hands' poses too), from about 680 KB to a few. Pieces hold at most 600 triangles, as the PSP's block for a piece's vertices does not draw past 64 KB. Module Convert's list is made with room for 128 rows when there are more than 11 looks.
- **Models converted as the game starts**: a skinned character as a binary FBX in a `mods\modules` folder becomes a look, made from module 0's files read from the disc: its bones matched to Miku's by name, scaled to her height, each part moved, turned and stretched onto her matching bone, cut into pieces of at most eight bones, its picture found in the folder and made 8 bit. Kept in a cache until the model, picture or `skin.ini` change. New in C: an IGB reader and writer (`igb.c`, as `Tools/igb.py`), an FBX reader with its own inflate, the conversion, and picture reading shared with note skins. All the looks go through two entries past the end of `DivaDataList.afs`, whose files change as each is loaded: the game keeps something by entry number with room for only two past its 1757, and entries further on crashed it or wrecked its memory.
- **Module skins**: each folder in `mods\modules` with a `body.igb` (and if it likes a `head.igb` and a `skin.ini` naming its base module) is one more row at the end of Module Convert's list. It is worn over a module of the game's, whose motions it keeps: picking it changes Miku into it, as picking the base again changes her back, and songs, menus and Miku's Room load its models in the base's place. Its files are added as entries past the end of `DivaDataList.afs`, nothing on disk changed, and it is kept in `OPTIONS.INI` by name. For it, in the recompiler: up to 512 hooks (past 32 they were left out without a word, and now a warning says so), and a hook inside a function no longer needs one at its entry. `Tools/igb.py` can now add objects and memory, give a changed block its own directory entry (alike entries are shared, and changing a shared one changed every block using it), and leave out what nothing refers to.
- The default options, for a player with none saved, had shifted by one place since note skins: restart key, Rin's and Len's modules and the d-pad for notes started wrong.
- **Note skins**: each folder of PNGs in `mods\notes` is one more choice in Options > Notes, after the game's own styles, under its name: the four notes and, if it has them, the targets. It is kept in `OPTIONS.INI` by name. The game writes a template to start from in `mods\notes\_template`, drawn from code by `Tools/hud_vector.py`. File replacement mods no longer read `mods\notes` or `mods\modules`.
- The hit effects of the song screen, the rings of light, the gold flare, the bubbles with a note and a ring, the stars, sparks, crescent and lightning, are drawn from shapes and gradients at four times their size (`Tools/fx_vector.py`), in place of the game's small pictures.
- In every style of notes, X is PlayStation's cross, as in Mega Mix, in the style's colour.
- The results screen's words have their letters at one height: a round letter of the picture's, as the O of NORMAL, no longer comes out larger.
- The description frame's shadow no longer has gaps: taking the old shadow out of the picture took the panel's own dark with it.
- Graphics is in the main menu just after Options, and within it the rows go Display, Image shape, Resolution, Smoothing, Blur, Show FPS.
- Options' Buttons row is a button: the confirm button opens it.
- Triangle on a song whose PV is still locked says so, as "Game saved" is told, besides the game's sound.
- The song card's three small icons, the star, the clapperboard and the crown, are drawn as shapes in the game's blue with a white outline, no longer traced from their pixels.

### Language
- German is shown by Austria's flag on the language screen.

### Controls and notes
- **Mute on miss** in Options: set to No, a missed note no longer silences the singing.
- **Note timing by song**: each song learns its own correction, added to Note timing as it starts. After a song with at least 20 presses, how late they were against the setting moves that song's correction towards it, all the way the first time and a third of the way after, at most 80 ms either way. Kept in `SONGTIMING.INI` beside the port's options. The measure in Options leaves the song's correction out.

## 2026-11-09

### System screens
- Rhythm Game Edit can make Edit data with your own MP3s, as on the PSP: put them in `%LOCALAPPDATA%\ProjectDivaRecomp\data\ms\MUSIC` and they show under "Memory Stick", to preview, play and save with. The PSP's MP3 player is written for it, over FFmpeg's decoder.
- Rhythm Game Edit's data can be saved, loaded and deleted by choosing a slot, as the PSP's own list of saves does: new data or which to save over, which to load, which to delete, with a question before saving over or deleting. Its data is named with an on-screen keyboard, with a controller or by typing. The list and the keyboard no longer hide behind the game's picture.

### Movies
- The opening movie plays, with its sound: the PSP's movie player (sceMpeg) is written for it, over FFmpeg's H.264 and ATRAC3plus decoders, which the build downloads and the game loads while it runs. Without them, the movie is skipped as before.
- For it, in the recompiler: a thread is given a copy of what it is started with, on its own stack, as the console does (the movie's thread read its player from a stack that had been used again); the status of an event flag and of a thread can be asked for; and sceDmacMemcpy copies. A screen the game fills with a copy, as the movie's pictures, is shown from memory even if it was drawn on before, so every other picture is no longer the white of what was last drawn there, and it is smoothed as the rest is.

### Controls and notes
- **Practice**: Start in the Free Play song list switches it on or off. In practice a song cannot be failed and no note scores, so nothing is kept from it. A message says which it is, and the footer has **START: Practice** among its hints.
- A press counts when it was made, not when the game next looked at the buttons: the game sees them once a frame, so a press could count up to 16 ms late. The buttons are read about once a millisecond, and the note is judged as it stood then. With this the measure for **Note timing** comes out a few milliseconds nearer zero, so it is worth measuring again.
- After a hold-up in a song, as a restart or a controller plugged in, the sound no longer stays late to the notes for many seconds: what is left waiting is cut back at once. The measure at the end of a song also says how long the sound waited in each half.

### Redrawn
- The Project DIVA badge in the demo's corner and the lists: Miku's head and twin tails in clean lines, with Project DIVA on her front. The game's is too small to trace, so it is drawn anew in its spirit.
- The title screen's logo: its edges take the colour from inside each part, so they no longer step in a light fringe, and specks are left out.
- The makers' logos at the start are shown as the game has them again: traced, they came out with too many flaws.
- The menus' titles: the dark band under the plate with the menu's name runs on to its cut edge in one diagonal, and where lines run under it, the white diagonal that takes them up and its shadow are straight too. They were the last steps left in the titles.
- The lists' rows and the glow of the one chosen: each row of their streaks is one even blend along its length, with no dithering and no blotches where the glow fades out.
- The shapes on the rhythm screen's beat lines, Triangle, Circle, Cross and Square: clean, with their white rim and a soft glow in their colour.
- Rhythm Game Edit's icons in clean lines, in their two blues: on the grid's buttons, lit or not (the arrows, the house, the note and the ©), and the camera's move, turn, preview, time and camera.
- The notes and targets as Xbox or Nintendo letters, or arrows, fill the same square as the PlayStation shapes, B a little narrower as letters are: the letters were narrower, so they looked smaller. Their targets are all of one size too: the game draws each of its shapes at a scale of its own, and that scale had been kept, so the one in place of the square was about a third larger than the one in place of the circle. The letters in the hints' buttons keep their shape.

### Menus
- In Rhythm Game Edit, the footer's hints, which run along, no longer overlap where their words are wider than the game's: each moves right as far as needed, into the gaps the footer already has.
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

- Ad hoc play does not work, and system dialogs and the save slot list are not shown.
- The frame rate cannot be chosen: the game's speed follows its refresh rate, so it stays at 60. Other rates would need interpolation or changes to the game.
- The original art outside the redrawn pieces is still at the game's own resolution.
- Moving between screens can drop a few frames, and now and then cuts the sound for a moment. Songs themselves hold about 60 fps.
- Japanese versions of the vector pieces do not exist yet, so Japanese uses the game's own.
