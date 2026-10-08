<#
.SYNOPSIS
    Refreshes the ashfall game repo from this engine's Release build.

.DESCRIPTION
    Copies, and prints, every file:
      build\PhoenixEngine\Release\x64\PhoenixEngine.exe, .pdb, *.cso  ->  <ashfall>\engine\
      build\Player\Release\x64\Player.exe, UIFont.spritefont,
        WinPixEventRuntime.dll, *.cso                                  ->  <ashfall>\engine\Player\  (File > Build uses it)
      build\PhoenixCore\{Release,Debug}\x64\PhoenixCore.lib, .pdb     ->  <ashfall>\engine\GameScript\Lib\{Release,Debug}\x64\
      Source\API\Phoenix_{Render,Scene,UI,VFX}.h,
        Source\{GameObject,Component,ComponentTransform,ComponentScript}.h
                                                                     ->  <ashfall>\engine\GameScript\Character\EngineDecls\
      SCRIPTING_GUIDE.md                                             ->  <ashfall>\engine\docs\
    The PDBs sit next to the libs so linking GameScript.dll finds them (no LNK4099).
    The vendored headers keep their own "Vendored, exact copy ..." comment block (everything before the first
    #include / namespace / class / struct line); the rest is replaced by the engine's file (minus its #pragma once
    and any engine-only #include the GameScript project can't resolve). A header whose content is already
    identical (ignoring line endings) is left alone. EngineDecls\ComponentAnimation.h is not synced: it is a
    deliberately minimal declaration (see the comment at its top), not a layout copy.

    Builds nothing: build Engine.vcxproj, Player.vcxproj and PhoenixCore.vcxproj (Release, and Debug for the lib) first. Close the
    editor first, or the exe copy fails. Stops at the first missing source or failed copy.

.PARAMETER AshfallRoot
    The ashfall repo root. Default: the "ashfall" folder next to this engine repo.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\release\Export-Release.ps1
#>
param(
    [string]$AshfallRoot = ""
)

$ErrorActionPreference = 'Stop'

$engineRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $AshfallRoot) { $AshfallRoot = Join-Path (Split-Path $engineRoot -Parent) 'ashfall' }
$gameEngine = Join-Path $AshfallRoot 'engine'
if (-not (Test-Path $gameEngine)) { throw "ashfall engine folder not found: $gameEngine" }

$script:copied = 0
$script:unchanged = 0

function Require([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { throw "MISSING SOURCE: $path" }
}

function Copy-One([string]$src, [string]$dstDir) {
    Require $src
    if (-not (Test-Path $dstDir)) { New-Item -ItemType Directory -Force -Path $dstDir | Out-Null }
    $dst = Join-Path $dstDir (Split-Path $src -Leaf)
    Copy-Item -LiteralPath $src -Destination $dst -Force
    $script:copied++
    Write-Host ("  copied     {0}  ->  {1}" -f $src, $dst)
}

function Sync-VendoredHeader([string]$src, [string]$dst, [string[]]$dropIncludes = @()) {
    Require $src
    Require $dst
    $codeStart = '^\s*(#include|namespace|class|struct)\b'
    $vendored = [IO.File]::ReadAllText($dst) -split "`r?`n"
    $engine = [IO.File]::ReadAllText($src) -split "`r?`n"
    foreach ($inc in $dropIncludes) {
        $engine = @($engine | Where-Object { $_ -notmatch ('^\s*#include\s+"' + [regex]::Escape($inc) + '"') })
    }

    $headEnd = 0
    while ($headEnd -lt $vendored.Count -and $vendored[$headEnd] -notmatch $codeStart) { $headEnd++ }
    if ($headEnd -ge $vendored.Count) { throw "No code found in vendored header (can't find its comment block): $dst" }
    $bodyStart = 0
    while ($bodyStart -lt $engine.Count -and $engine[$bodyStart] -notmatch $codeStart) { $bodyStart++ }
    if ($bodyStart -ge $engine.Count) { throw "No code found in engine header: $src" }

    $new = @($vendored[0..($headEnd - 1)]) + @($engine[$bodyStart..($engine.Count - 1)])
    $newText = ($new -join "`r`n")
    $oldText = ($vendored -join "`r`n")
    if ($newText -eq $oldText) {
        $script:unchanged++
        Write-Host ("  unchanged  {0}" -f $dst)
        return
    }
    [IO.File]::WriteAllText($dst, $newText, (New-Object Text.UTF8Encoding($false)))
    $script:copied++
    Write-Host ("  copied     {0}  ->  {1}  (vendored header kept)" -f $src, $dst)
}

$exeDir = Join-Path $engineRoot 'build\PhoenixEngine\Release\x64'

Write-Host "Engine binaries and shaders -> $gameEngine"
Copy-One (Join-Path $exeDir 'PhoenixEngine.exe') $gameEngine
Copy-One (Join-Path $exeDir 'PhoenixEngine.pdb') $gameEngine
$shaders = @(Get-ChildItem -Path $exeDir -Filter '*.cso' -File)
if ($shaders.Count -eq 0) { throw "MISSING SOURCE: no *.cso in $exeDir" }
foreach ($s in $shaders) { Copy-One $s.FullName $gameEngine }

Write-Host "Prebuilt Player -> Player (the editor's Build copies it; no engine source needed)"
$playerDir = Join-Path $engineRoot 'build\Player\Release\x64'
$playerDst = Join-Path $gameEngine 'Player'
foreach ($f in 'Player.exe', 'UIFont.spritefont', 'WinPixEventRuntime.dll') { Copy-One (Join-Path $playerDir $f) $playerDst }
$playerShaders = @(Get-ChildItem -Path $playerDir -Filter '*.cso' -File)
if ($playerShaders.Count -eq 0) { throw "MISSING SOURCE: no *.cso in $playerDir" }
foreach ($s in $playerShaders) { Copy-One $s.FullName $playerDst }

Write-Host "PhoenixCore libs -> GameScript\Lib"
foreach ($cfg in 'Release', 'Debug') {
    foreach ($f in 'PhoenixCore.lib', 'PhoenixCore.pdb') {
        Copy-One (Join-Path $engineRoot "build\PhoenixCore\$cfg\x64\$f") (Join-Path $gameEngine "GameScript\Lib\$cfg\x64")
    }
}

Write-Host "Engine headers -> GameScript\Character\EngineDecls"
$decls = Join-Path $gameEngine 'GameScript\Character\EngineDecls'
foreach ($h in 'Phoenix_Render.h', 'Phoenix_Scene.h', 'Phoenix_UI.h', 'Phoenix_VFX.h') {
    Sync-VendoredHeader (Join-Path $engineRoot "Source\API\$h") (Join-Path $decls $h)
}
foreach ($h in 'GameObject.h', 'Component.h', 'ComponentScript.h') {
    Sync-VendoredHeader (Join-Path $engineRoot "Source\$h") (Join-Path $decls $h)
}
# ModuleD3D12.h only supplies the math types, which GameScript gets from its own Globals.h.
Sync-VendoredHeader (Join-Path $engineRoot 'Source\ComponentTransform.h') (Join-Path $decls 'ComponentTransform.h') @('ModuleD3D12.h')

Write-Host "Docs -> docs"
Copy-One (Join-Path $engineRoot 'SCRIPTING_GUIDE.md') (Join-Path $gameEngine 'docs')

Write-Host ("Done: {0} file(s) copied, {1} header(s) already up to date." -f $script:copied, $script:unchanged)
