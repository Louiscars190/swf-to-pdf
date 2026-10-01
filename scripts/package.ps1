$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$RepoRoot = Split-Path $PSScriptRoot -Parent
$BuildDir = Join-Path $RepoRoot 'build'
$AppDir = Join-Path $RepoRoot 'dist\SWF-to-PDF'
if (-not (Test-Path -LiteralPath (Join-Path $AppDir 'SWF-to-PDF.exe'))) { throw 'Run scripts\build.ps1 first.' }
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$Archive = Join-Path $BuildDir 'ffdec-26.3.0.zip'
$ExpectedHash = '35f4930eb7c380afe66f2117f90b006deac0631473ad7500bb39c78f68645ecd'
if (-not (Test-Path -LiteralPath $Archive) -or (Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $ExpectedHash) {
    Invoke-WebRequest -UseBasicParsing -Uri 'https://github.com/jindrapetrik/jpexs-decompiler/releases/download/version26.3.0/ffdec_26.3.0.zip' -OutFile $Archive
}
if ((Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $ExpectedHash) { throw 'JPEXS archive verification failed.' }
$Extract = Join-Path $BuildDir 'ffdec-extracted'
if (Test-Path -LiteralPath $Extract) { Remove-Item -LiteralPath $Extract -Recurse -Force }
Expand-Archive -LiteralPath $Archive -DestinationPath $Extract
$EngineDir = Join-Path $AppDir 'engine'
New-Item -ItemType Directory -Force -Path $EngineDir | Out-Null
Copy-Item -LiteralPath (Join-Path $Extract 'ffdec.jar') -Destination $EngineDir -Force
Copy-Item -LiteralPath (Join-Path $Extract 'lib') -Destination $EngineDir -Recurse -Force
Copy-Item -LiteralPath (Join-Path $Extract 'license.txt') -Destination (Join-Path $EngineDir 'LICENSE.txt') -Force
$Zip = Join-Path $RepoRoot 'dist\SWF-to-PDF-Native-Windows.zip'
Compress-Archive -LiteralPath $AppDir -DestinationPath $Zip -Force
Write-Host "Packaged: $Zip"
