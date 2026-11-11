# The pictures of one language, as Tools\build.ps1 asks, which runs several of these at once.
#
#   -Pictures 1   the game's pictures with words in them, in the language's own folder
#   -Vectors 1    the menus' pieces drawn from vectors, from those pictures cleaned and enlarged
#
# Each part writes its stamp only when it has worked. What the tools say goes to -Log; the exit code
# is 1 if anything failed.

param(
    [string]$Root, [string]$Game, [string]$Font, [string]$Language, [string]$Log,
    [string]$Pictures = "0", [string]$PicturesStamp = "", [string]$PicturesWanted = "",
    [string]$Vectors = "0", [string]$VectorsStamp = "", [string]$VectorsWanted = ""
)

$Table = Join-Path $Root "lang\$($Language)_images.txt"
$LangTool = Join-Path $Root "Tools\lang_images.py"
$UiTool = Join-Path $Root "Tools\ui_vector.py"
$HdDir = Join-Path $env:LOCALAPPDATA "ProjectDivaRecomp\textures_hd"

if ($Pictures -eq "1") {
    "== translating the pictures ($Language)  $(Get-Date -Format HH:mm:ss)" *>> $Log
    $LangArt = Join-Path $env:LOCALAPPDATA "ProjectDivaRecomp\lang\$Language"
    python $LangTool $Game $Table $LangArt $Font $LangArt *>> $Log
    if ($LASTEXITCODE -ne 0) { "FAILED: translate the pictures ($Language)" *>> $Log; exit 1 }
    $PicturesWanted | Set-Content $PicturesStamp
}

if ($Vectors -eq "1") {
    "== drawing the menus from vectors ($Language)  $(Get-Date -Format HH:mm:ss)" *>> $Log
    $UiDir = Join-Path $HdDir "vector\$Language"
    $env:DIVA_UI_SMOOTH = "1"
    python $LangTool $Game $Table (Join-Path $env:TEMP "diva_ui_$Language") $Font $UiDir *>> $Log
    $Cleaned = $LASTEXITCODE
    Remove-Item Env:DIVA_UI_SMOOTH
    if ($Cleaned -ne 0) { "FAILED: clean the pictures ($Language)" *>> $Log; exit 1 }
    python $UiTool $Game $Font $Table $UiDir *>> $Log
    if ($LASTEXITCODE -ne 0) { "FAILED: draw the menus from vectors ($Language)" *>> $Log; exit 1 }
    $VectorsWanted | Set-Content $VectorsStamp
}
exit 0
