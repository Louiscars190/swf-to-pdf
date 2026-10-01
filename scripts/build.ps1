param([string]$Compiler = 'gcc', [string]$ResourceCompiler = 'windres')
$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path $PSScriptRoot -Parent
$SourceDir = Join-Path $RepoRoot 'src'
$BuildDir = Join-Path $RepoRoot 'build'
$AppDir = Join-Path $RepoRoot 'dist\SWF-to-PDF'
New-Item -ItemType Directory -Force -Path $BuildDir,$AppDir | Out-Null
$Resource = Join-Path $BuildDir 'app-resource.o'
& $ResourceCompiler '-I' $SourceDir (Join-Path $SourceDir 'app.rc') $Resource
if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed.' }
& $Compiler '-municode' '-mwindows' '-O2' '-s' '-static' '-Wall' '-Werror' '-Wl,--stack,8388608' (Join-Path $SourceDir 'app.c') $Resource '-o' (Join-Path $AppDir 'SWF-to-PDF.exe') '-lcomctl32' '-lgdi32' '-luser32' '-lshell32' '-lshlwapi' '-lole32' '-loleaut32' '-luuid' '-lwindowscodecs' '-ladvapi32'
if ($LASTEXITCODE -ne 0) { throw 'Application compilation failed.' }
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'setup-runtime.ps1') -Destination $AppDir -Force
Copy-Item -LiteralPath (Join-Path $RepoRoot 'LICENSE') -Destination $AppDir -Force
Copy-Item -LiteralPath (Join-Path $RepoRoot 'THIRD_PARTY.md') -Destination $AppDir -Force
Copy-Item -LiteralPath (Join-Path $RepoRoot 'README.md') -Destination $AppDir -Force
Write-Host "Built: $AppDir"
