# Set the game up from a player's own ISO and decrypted executable, and build it. See INSTALL.md.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Windows.Forms

$Root = Split-Path -Parent $PSScriptRoot
$GameDir = Join-Path $Root "Game"
$Sysdir = Join-Path $GameDir "PSP_GAME\SYSDIR"
$Decrypted = Join-Path $Sysdir "ULJM05472_EBOOT.BIN"
$ExpectedElf = "A6334018380F2ECD5E47D8A3F81198015FA6819C"

function Pick-File($title, $filter) {
    $dialog = New-Object System.Windows.Forms.OpenFileDialog
    $dialog.Title = $title
    $dialog.Filter = $filter
    if ($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
        Write-Host "Cancelled."
        exit 1
    }
    return $dialog.FileName
}

function Stop-With($message) {
    Write-Host ""
    Write-Host $message
    exit 1
}

Write-Host "== Project DIVA Recomp: installation"

# ---- Windows can refuse to run anything unsigned, which is what the build makes
$SmartAppControl = $null
try { $SmartAppControl = (Get-MpComputerStatus).SmartAppControlState } catch {}
if ($SmartAppControl -eq "On") {
    Stop-With "Windows Smart App Control is on and will block the game. See 'If something goes wrong' in INSTALL.md."
}

# ---- Python
try {
    $version = (& python --version) 2>&1
    if ("$version" -notmatch "Python 3\.") { throw "wrong" }
} catch {
    Stop-With "Python not found. Install it from https://www.python.org/downloads/, ticking 'Add python.exe to PATH', and open this installer again."
}
Write-Host "$version found."

# ---- the disc
if (Test-Path (Join-Path $Sysdir "EBOOT.BIN")) {
    Write-Host "The game's folder already exists, skipping the ISO."
} else {
    Write-Host ""
    Write-Host "Choose the game's ISO in the window that opened."
    $Iso = Pick-File "Choose the game's ISO (ULJM05472 v1.01)" "Disc image (*.iso)|*.iso"
    New-Item -ItemType Directory -Force $GameDir | Out-Null
    Write-Host "Copying the files from the ISO..."
    & python (Join-Path $PSScriptRoot "extract_iso.py") $Iso $GameDir
    if ($LASTEXITCODE -ne 0) {
        Remove-Item -Recurse -Force (Join-Path $GameDir "PSP_GAME") -ErrorAction SilentlyContinue
        exit 1
    }
}

# ---- the executable
if (-not (Test-Path $Decrypted)) {
    Write-Host ""
    Write-Host "Choose the executable PPSSPP saved (step 2 of INSTALL.md)."
    $Dump = Pick-File "Choose the decrypted EBOOT saved by PPSSPP" "All files (*.*)|*.*"
    if ((Get-FileHash $Dump -Algorithm SHA1).Hash -ne $ExpectedElf) {
        Stop-With "This executable is not the decrypted one of ULJM05472 v1.01. See step 2 of INSTALL.md."
    }
    Copy-Item $Dump $Decrypted
    Write-Host "Executable checked."
}

# ---- the build
Write-Host ""
Write-Host "Compiling the game. This is slow and only happens once, so leave it running."
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "build.ps1") -BuildOnly -SkipTests
if ($LASTEXITCODE -ne 0) {
    Stop-With "Compiling failed. See the file $(Join-Path $Root 'build\logs\build.txt')."
}

Write-Host ""
Write-Host "Done! Now double-click 'Play.bat' to play."
