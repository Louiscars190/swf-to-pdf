$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$AppDir = $PSScriptRoot
$StageDir = Join-Path $AppDir 'runtime-setup'
$RuntimeDir = Join-Path $AppDir 'runtime'
try {
    Write-Host 'SWF to PDF - first-time setup' -ForegroundColor Cyan
    Write-Host 'Downloading the Java runtime. This only happens once.'
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    New-Item -ItemType Directory -Force -Path $StageDir | Out-Null
    $Archive = Join-Path $StageDir 'java.zip'
    Invoke-WebRequest -UseBasicParsing -Uri 'https://github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.20.1%2B1/OpenJDK17U-jre_x64_windows_hotspot_17.0.20.1_1.zip' -OutFile $Archive -TimeoutSec 600
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $Archive).Hash.ToLowerInvariant() -ne 'bc21a93923103cdaac93ee337b0ae4365e739fde36df823dd456bc67c8a9d352') {
        throw 'The download could not be verified. Please run the app again to retry.'
    }
    Write-Host 'Preparing the app...'
    $ExtractDir = Join-Path $StageDir 'extracted'
    if (Test-Path -LiteralPath $ExtractDir) { Remove-Item -LiteralPath $ExtractDir -Recurse -Force }
    Expand-Archive -LiteralPath $Archive -DestinationPath $ExtractDir
    $RuntimeFolder = Get-ChildItem -LiteralPath $ExtractDir -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'bin\javaw.exe') } | Select-Object -First 1
    if ($null -eq $RuntimeFolder) { throw 'The runtime download did not contain Java.' }
    if (Test-Path -LiteralPath $RuntimeDir) { throw 'A runtime folder already exists. Rename it and run the app again.' }
    Move-Item -LiteralPath $RuntimeFolder.FullName -Destination $RuntimeDir
    Remove-Item -LiteralPath $StageDir -Recurse -Force
    Start-Process -FilePath (Join-Path $AppDir 'SWF-to-PDF.exe') -WorkingDirectory $AppDir
} catch {
    Write-Host ('Setup failed: ' + $_.Exception.Message) -ForegroundColor Red
    Write-Host 'Check your internet connection and run SWF-to-PDF.exe again.'
    Read-Host 'Press Enter to close'
    exit 1
}
