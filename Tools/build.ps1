# Builds the recompilation from your own game dump, then runs it for a while.
#
#   powershell -ExecutionPolicy Bypass -File Tools\build.ps1 [-Game <dir>] [-Edition raw|recommended] [-Seconds <n>] [-SkipTests] [-SaveTextures] [-Player] [-Second]
#
# -Second also starts a second copy of the game, to try ad hoc play on one computer: its saves are a
# copy of the main save and options only (no Edits, no music), in %LOCALAPPDATA%\ProjectDivaRecomp\data2,
# and its log is build\logs\run2.txt.
#
# -Player ignores the test switches in dev.ini, as Play.bat does. They are in src\host\settings.c.
#
# -Game is the folder holding PSP_GAME; its SYSDIR needs ULJM05472_EBOOT.BIN, the
# decrypted executable dumped with PPSSPP. The compiler is installed once into
# %LOCALAPPDATA%\ProjectDivaRecomp, not system-wide. Output and logs go to build\.

param(
    [string]$Game = "",
    [string]$Edition = "recommended",
    [switch]$SaveTextures,
    [switch]$BuildOnly,
    [switch]$Player,
    [int]$Seconds = 60,
    [switch]$SkipTests,
    [switch]$Second
)

$Root = Split-Path -Parent $PSScriptRoot
if (-not $Game) {
    $Game = (Get-ChildItem $Root -Directory | Where-Object { Test-Path (Join-Path $_.FullName "PSP_GAME") } | Select-Object -First 1).FullName
}
$Elf = Join-Path $Game "PSP_GAME\SYSDIR\ULJM05472_EBOOT.BIN"
if (-not (Test-Path $Elf)) { Write-Host "Could not find $Elf. See INSTALL.md."; exit 1 }

# Everything this project knows about the game's code and files is for this one version.
$ExpectedElf = "A6334018380F2ECD5E47D8A3F81198015FA6819C"
if ((Get-FileHash $Elf -Algorithm SHA1).Hash -ne $ExpectedElf) {
    Write-Host "This executable is not ULJM05472 v1.01, or it was not extracted decrypted."
    Write-Host "Expected SHA-1 $ExpectedElf. See INSTALL.md."
    exit 1
}

$EditionFile = Join-Path $Root "profiles\$Edition.ini"
if (-not (Test-Path $EditionFile)) { Write-Host "Unknown edition: $Edition"; exit 1 }

$Build = Join-Path $Root "build"
$Logs = Join-Path $Build "logs"
New-Item -ItemType Directory -Force $Logs | Out-Null
$Log = Join-Path $Logs "build.txt"
$RunLog = Join-Path $Logs "run.txt"
Remove-Item $Log, $RunLog -ErrorAction SilentlyContinue

function Step($name) {
    Write-Host "== $name"
    "== $name  $(Get-Date -Format HH:mm:ss)" *>> $Log
}
# What a step made is good while what it was made from is the same, as its stamp says.
function Is-Stale($stamp, $wanted) {
    return -not (Test-Path $stamp) -or (Get-Content $stamp) -ne $wanted
}
# Someone may attach the logs to a public issue, so their home folder, which has their user name
# in it, is not spelled out. A failure here only leaves the logs as they were.
function Hide-UserFolder {
    if (-not $env:USERPROFILE -or $env:USERPROFILE.Length -lt 4) { return }
    $Back = [regex]::Escape($env:USERPROFILE)
    $Forward = [regex]::Escape($env:USERPROFILE.Replace('\', '/'))
    foreach ($File in @($Log, $RunLog)) {
        if (-not (Test-Path $File)) { continue }
        try {
            $Text = Get-Content -Raw $File -ErrorAction Stop
            if ($Text) { ($Text -replace $Back, '%USERPROFILE%' -replace $Forward, '%USERPROFILE%') | Set-Content -NoNewline -Encoding Unicode $File -ErrorAction Stop }
        } catch {}
    }
}
function Fail($name) {
    "FAILED: $name" *>> $Log
    Write-Host "FAILED: $name (see $Log)"
    "FIM" *>> $Log
    Hide-UserFolder
    exit 1
}

# ---- toolchain: a path without spaces, because cmd has to run these wrappers
$Tc = Join-Path $env:LOCALAPPDATA "ProjectDivaRecomp\toolchain"
$Py = Join-Path $Tc "Scripts\python.exe"
if (-not (Test-Path (Join-Path $Tc "Scripts\zcc.cmd"))) {
    Step "installing the compiler"
    python -m venv $Tc *>> $Log
    & $Py -m pip install -q cmake ninja ziglang *>> $Log
    if ($LASTEXITCODE -ne 0) { Fail "install the compiler" }
    foreach ($pair in @(@("cc", "zcc"), @("ar", "zar"), @("ranlib", "zranlib"))) {
        "@`"$Py`" -m ziglang $($pair[0]) %*" | Set-Content -Encoding ascii (Join-Path $Tc "Scripts\$($pair[1]).cmd")
    }
}
$env:PATH = "$(Join-Path $Tc 'Scripts');$env:PATH"
$Tools = @(
    "-DCMAKE_C_COMPILER=$(Join-Path $Tc 'Scripts\zcc.cmd')",
    "-DCMAKE_AR=$(Join-Path $Tc 'Scripts\zar.cmd')",
    "-DCMAKE_RANLIB=$(Join-Path $Tc 'Scripts\zranlib.cmd')"
)

# ---- the toolkit's own tests
if (-not $SkipTests) {
    Step "the recompiler's tests"
    $Tests = Join-Path $Build "tests"
    cmake -S (Join-Path $Root "third_party\psprecomp") -B $Tests -G Ninja -DCMAKE_BUILD_TYPE=Release @Tools -DPSPRECOMP_REAL_THREADS=ON *>> $Log
    cmake --build $Tests *>> $Log
    if ($LASTEXITCODE -ne 0) { Fail "compile the tests" }
    # Tests that passed pass again until one of them is built anew.
    $Passed = Join-Path $Tests "passed.txt"
    $Newest = (Get-ChildItem $Tests -Recurse -Filter *.exe | Measure-Object LastWriteTime -Maximum).Maximum
    if (-not (Test-Path $Passed) -or (Get-Item $Passed).LastWriteTime -lt $Newest) {
        Remove-Item $Passed -ErrorAction SilentlyContinue
        ctest --test-dir $Tests --output-on-failure *>> $Log
        if ($LASTEXITCODE -ne 0) { Fail "tests" }
        Get-Date | Set-Content $Passed
    }
}

# ---- recompile, then build the game against what came out
$Out = Join-Path $Build "game"
$Gen = Join-Path $Out "generated"
Step "compiling the recompiler"
cmake -S $Root -B $Out -G Ninja -DCMAKE_BUILD_TYPE=Release @Tools *>> $Log
cmake --build $Out --target allegrexrecomp *>> $Log
if ($LASTEXITCODE -ne 0) { Fail "compile the recompiler" }

Step "generating C from the game"
New-Item -ItemType Directory -Force $Gen | Out-Null
$Recomp = Join-Path $Out "third_party\psprecomp\tools\allegrexrecomp\allegrexrecomp.exe"
# translation.c, letters.c, controls.c, hooks.c, extras.c, timing.c, song_sort.c, module_sort.c and practice.c say what these are.
$Hooks = @(
    "0883D598", "0883EC34", "0883F104", "08851CD8", "08AF1C54", "08815FF4", "08ABF1B8", "0886B304", "08864C90", "08AD49B4",
    "08AB5908", "08AB5A4C", "08AB5448", "08AB5528", "08AB59A0", "08AB41C4", "08AB4C58", "08AB4D18", "08AB5EF0", "08AB5F38", "08AB4384",
    "08857818", "08857E70", "08858200", "08858314", "08AC00B0", "08AC0514", "08862198", "0886B08C", "08AB36B0", "08AB179C",
    "088628A8", "08A8C1A8", "08A94C70", "0885BBD0", "08AC1F0C", "08AF2344"
)
$GenStamp = Join-Path $Out "generated_from.txt"
$GenWanted = "$ExpectedElf $((Get-FileHash $Recomp).Hash) $($Hooks -join ' ')"
if ((Is-Stale $GenStamp $GenWanted) -or -not (Test-Path (Join-Path $Gen "diva_register.c"))) {
    Remove-Item $GenStamp -ErrorAction SilentlyContinue
    & $Recomp funcs $Elf *>> $Log
    & $Recomp emit $Elf $Gen diva ($Hooks | ForEach-Object { "--hook"; $_ }) *>> $Log
    if ($LASTEXITCODE -ne 0) { Fail "generate C" }
    & $Recomp funcs $Elf --list *> (Join-Path $Logs "funcs.txt")
    $GenWanted | Set-Content $GenStamp
}

# ---- FFmpeg, for the movies and the Memory Stick's music: its LGPL decoders and reader, loaded by
# the game at run time. Only the headers it is compiled with, the DLLs it loads and the licence are
# kept. The game runs without it, so a failure here only costs the movies and the music. The hash is the one the build's release publishes.
$FfVersion = "n8.1.3-9-g29e619e767"
$FfName = "ffmpeg-$FfVersion-win64-lgpl-shared-8.1"
$FfHash = "3E47BDA1607740550141E37C0E49D1E5182B34699F15ADFD137EE266D346811A"
$FfDir = Join-Path $env:LOCALAPPDATA "ProjectDivaRecomp\$FfName"
$FfDlls = @("avcodec-62.dll", "avutil-60.dll", "swresample-6.dll", "avformat-62.dll")
if (-not (Test-Path (Join-Path $FfDir "include\libavcodec\avcodec.h")) -or -not (Test-Path (Join-Path $FfDir "bin\avformat-62.dll"))) {
    Step "downloading FFmpeg $FfVersion (for the movies)"
    $FfZip = Join-Path $Build "$FfName.zip"
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $ProgressPreference = "SilentlyContinue"
        Invoke-WebRequest -UseBasicParsing -OutFile $FfZip "https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2026-09-30-13-08/$FfName.zip" -ErrorAction Stop
        if ((Get-FileHash $FfZip -Algorithm SHA256).Hash -ne $FfHash) { throw "the downloaded file does not match its hash" }
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $zip = [IO.Compression.ZipFile]::OpenRead($FfZip)
        try {
            foreach ($entry in $zip.Entries) {
                $inner = $entry.FullName.Substring($entry.FullName.IndexOf("/") + 1)
                $keep = $inner -like "include/*" -or $inner -eq "LICENSE.txt" -or ($FfDlls | Where-Object { $inner -eq "bin/$_" })
                if (-not $keep -or $entry.Name -eq "") { continue }
                $target = Join-Path $FfDir ($inner -replace "/", "\")
                New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target, $true)
            }
        } finally { $zip.Dispose() }
    } catch {
        "FFmpeg not installed: $_" *>> $Log
        Write-Host "Warning: could not download FFmpeg ($_). The opening movie will be skipped."
    }
    Remove-Item $FfZip -ErrorAction SilentlyContinue
}
$FfInclude = Join-Path $FfDir "include"
$FfArgs = @()
if (Test-Path (Join-Path $FfInclude "libavcodec\avcodec.h")) {
    $FfArgs = @("-DPSPRECOMP_FFMPEG_INCLUDE=$($FfInclude -replace '\\', '/')")
    foreach ($dll in $FfDlls) {
        $from = Join-Path $FfDir "bin\$dll"
        if (Test-Path $from) { Copy-Item -Force $from $Out }
    }
} else {
    $FfArgs = @("-DPSPRECOMP_FFMPEG_INCLUDE=")
}

Step "compiling the game (this can take a long time)"
$sw = [Diagnostics.Stopwatch]::StartNew()
cmake -S $Root -B $Out @FfArgs *>> $Log
cmake --build $Out --target diva -- -k 0 *>> $Log
$built = $LASTEXITCODE
"compile time: $([math]::Round($sw.Elapsed.TotalMinutes,1)) min" *>> $Log
if ($built -ne 0) { Fail "compile the game" }

# ---- SDL3, for controllers other than Xbox ones. The game runs without it, so a failure here
# only costs those controllers. The hash is the one the official release publishes.
$SdlVersion = "3.4.18"
$SdlHash = "75C2C0FC74E7D1206AAEDC22893A35887332F4F85F5C8B74FDDC10D177985F2C"
$SdlDir = Join-Path $env:LOCALAPPDATA "ProjectDivaRecomp\SDL3-$SdlVersion"
$SdlDll = Join-Path $SdlDir "SDL3.dll"
if (-not (Test-Path $SdlDll)) {
    Step "downloading SDL $SdlVersion"
    $SdlZip = Join-Path $Build "SDL3-$SdlVersion.zip"
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $ProgressPreference = "SilentlyContinue"
        Invoke-WebRequest -UseBasicParsing -OutFile $SdlZip "https://github.com/libsdl-org/SDL/releases/download/release-$SdlVersion/SDL3-$SdlVersion-win32-x64.zip" -ErrorAction Stop
        if ((Get-FileHash $SdlZip -Algorithm SHA256).Hash -ne $SdlHash) { throw "the downloaded file does not match its hash" }
        Expand-Archive -Force $SdlZip $SdlDir -ErrorAction Stop
    } catch {
        "SDL not installed: $_" *>> $Log
        Write-Host "Warning: could not download SDL ($_). Only Xbox controllers will work."
    }
    Remove-Item $SdlZip -ErrorAction SilentlyContinue
}
if (Test-Path $SdlDll) { Copy-Item -Force $SdlDll $Out }

$HdDir = Join-Path $env:LOCALAPPDATA "ProjectDivaRecomp\textures_hd"
New-Item -ItemType Directory -Force $HdDir | Out-Null

# ---- the song screen's HUD, drawn from vectors. Made again when the drawing changes.
$HudTool = Join-Path $Root "Tools\hud_vector.py"
$HudStamp = Join-Path $HdDir "hud_made_from.txt"
$HudWanted = (Get-FileHash $HudTool).Hash
if (Is-Stale $HudStamp $HudWanted) {
    Step "drawing the HUD"
    python $HudTool $Game $HdDir *>> $Log
    if ($LASTEXITCODE -ne 0) { Fail "draw the HUD" }
    $HudWanted | Set-Content $HudStamp
}

# ---- the hit effects, drawn from code. Made again when the drawing changes.
$FxTool = Join-Path $Root "Tools\fx_vector.py"
$FxStamp = Join-Path $HdDir "fx_made_from.txt"
$FxWanted = (Get-FileHash $FxTool).Hash
if (Is-Stale $FxStamp $FxWanted) {
    Step "drawing the hit effects"
    python $FxTool $HdDir *>> $Log
    if ($LASTEXITCODE -ne 0) { Fail "draw the hit effects" }
    $FxWanted | Set-Content $FxStamp
}

# ---- text from a font of the player's own if one is in the folder, or else from the free
# one this project carries. Made once for each font.
$FontDir = Join-Path $env:LOCALAPPDATA "ProjectDivaRecomp\fonts"
New-Item -ItemType Directory -Force $FontDir | Out-Null
$FontFile = Get-ChildItem $FontDir -File | Where-Object { $_.Extension -match '^\.(ttf|ttc|otf)$' } | Select-Object -First 1
if (-not $FontFile) { $FontFile = Get-Item (Join-Path $Root "fonts\MPLUSRounded1c-Medium.ttf") }
if ($FontFile) {
    $Stamp = Join-Path $HdDir "font_made_from.txt"
    # The number changes when what the tool writes does.
    $Made = "$($FontFile.Name) 5"
    if (Is-Stale $Stamp $Made) {
        Step "drawing the font $($FontFile.Name) (takes a few minutes, only the first time)"
        python (Join-Path $Root "Tools\hd_font.py") $Game $FontFile.FullName $HdDir *>> $Log
        if ($LASTEXITCODE -ne 0) { Fail "draw the font" }
        $Made | Set-Content $Stamp
    }
}

# ---- pictures with words in them, made from the game's own, for each language. These use
# the font as well, so they are made again when it or the words change. Their large versions go in
# the language's own folder, which the game looks in first: the shared one would hold whichever
# language was made last, and show it in the others, Japanese included.
$LangTool = Join-Path $Root "Tools\lang_images.py"
foreach ($Language in @("en", "de")) {
    $LangArt = Join-Path $env:LOCALAPPDATA "ProjectDivaRecomp\lang\$Language"
    $LangTable = Join-Path $Root "lang\$($Language)_images.txt"
    $LangStamp = Join-Path $HdDir "pictures_$($Language)_made_from.txt"
    $LangWanted = "$($FontFile.Name) $((Get-FileHash $LangTable).Hash) $((Get-FileHash $LangTool).Hash) large in lang"
    if (Is-Stale $LangStamp $LangWanted) {
        Step "translating the pictures ($Language)"
        python $LangTool $Game $LangTable $LangArt $FontFile.FullName $LangArt *>> $Log
        if ($LASTEXITCODE -ne 0) { Fail "translate the pictures" }
        $LangWanted | Set-Content $LangStamp
    }
}

# ---- the menus' common pieces, from vectors, for each language with words for them.
$UiTool = Join-Path $Root "Tools\ui_vector.py"
foreach ($Language in @("en", "de")) {
    $UiDir = Join-Path $HdDir "vector\$Language"
    $UiTable = Join-Path $Root "lang\$($Language)_images.txt"
    $UiStamp = Join-Path $HdDir "vector_$($Language)_made_from.txt"
    $UiWanted = "$($FontFile.Name) $((Get-FileHash $UiTable).Hash) $((Get-FileHash $UiTool).Hash) $((Get-FileHash $HudTool).Hash) $((Get-FileHash $LangTool).Hash)"
    if (Is-Stale $UiStamp $UiWanted) {
        Step "drawing the menus from vectors ($Language)"
        $env:DIVA_UI_SMOOTH = "1"
        python $LangTool $Game $UiTable (Join-Path $env:TEMP "diva_ui_$Language") $FontFile.FullName $UiDir *>> $Log
        Remove-Item Env:DIVA_UI_SMOOTH
        if ($LASTEXITCODE -ne 0) { Fail "clean the pictures" }
        python $UiTool $Game $FontFile.FullName $UiTable $UiDir *>> $Log
        if ($LASTEXITCODE -ne 0) { Fail "draw the menus from vectors" }
        $UiWanted | Set-Content $UiStamp
    }
}

# ---- song names and lyrics in romaji, made here from this copy of the game. The game runs
# without them, so a failure only costs the romaji, and the next build tries again.
$RomajiTool = Join-Path $Root "Tools\romaji.py"
$RomajiTitles = Join-Path $Root "lang\romaji_titles.txt"
$RomajiStamp = Join-Path $HdDir "romaji_made_from.txt"
$RomajiWanted = "$((Get-FileHash $RomajiTool).Hash) $((Get-FileHash $RomajiTitles).Hash)"
if (Is-Stale $RomajiStamp $RomajiWanted) {
    Step "generating romaji for song names and lyrics"
    & $Py -m pip install -q fugashi==1.5.2 unidic-lite==1.0.8 *>> $Log
    & $Py $RomajiTool $Elf $RomajiTitles (Join-Path $env:LOCALAPPDATA "ProjectDivaRecomp\lang\romaji") *>> $Log
    if ($LASTEXITCODE -eq 0) { $RomajiWanted | Set-Content $RomajiStamp }
    else { Write-Host "Warning: the lyrics could not be converted to romaji. Song names may still be available. See $Log." }
}

if ($BuildOnly) { "FIM" *>> $Log; Hide-UserFolder; exit 0 }

# ---- run
Step "running the game for $Seconds seconds"
$env:DIVA_PROFILE = $EditionFile
$env:DIVA_LANG_DIR = Join-Path $Root "lang"
if ($Player) { $env:DIVA_NO_DEV = "1" }
if ($SaveTextures) { $env:DIVA_SAVE_TEXTURES = "1" } else { Remove-Item Env:DIVA_SAVE_TEXTURES -ErrorAction SilentlyContinue }
# Saves live outside build\, so that cleaning a build never loses them.
$Data = Join-Path $env:LOCALAPPDATA "ProjectDivaRecomp\data"
$Exe = Join-Path $Out "diva.exe"
if ($Second) {
    # The second copy's own Memory Stick: the main save and the options, nothing else.
    $Data2 = Join-Path $env:LOCALAPPDATA "ProjectDivaRecomp\data2"
    $Saves2 = Join-Path $Data2 "ms\PSP\SAVEDATA"
    New-Item -ItemType Directory -Force $Saves2 | Out-Null
    foreach ($Folder in "ULJM05472MAIN00", "ULJM05472RECOMP") {
        $From = Join-Path $Data "ms\PSP\SAVEDATA\$Folder"
        if ((Test-Path $From) -and -not (Test-Path (Join-Path $Saves2 $Folder))) { Copy-Item -Recurse $From $Saves2 }
    }
    $env:DIVA_NICKNAME = "Second copy"
    Start-Process -FilePath $Exe -ArgumentList "`"$Elf`"", "`"$Game`"", "`"$Data2`"", $Seconds -RedirectStandardError (Join-Path $Logs "run2.txt") -RedirectStandardOutput (Join-Path $Logs "run2_out.txt")
    Remove-Item Env:DIVA_NICKNAME
}
& $Exe $Elf $Game $Data $Seconds *> $RunLog
"game exit code: $LASTEXITCODE" *>> $Log
"FIM" *>> $Log
Hide-UserFolder
Write-Host "Done. Logs are in $Logs"
