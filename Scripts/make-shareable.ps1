# Key Compass - make "Key Compass Shareable.zip" to give to a friend
# Run it through "Make Shareable Package.bat" (double-click). It packs the plug-in and app you have
# already built (the Output folder) with a one-click installer, so your friend needs no build tools.

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $Root 'Output'
$templates = Join-Path $PSScriptRoot 'share'
$zipPath = Join-Path $Root 'Key Compass Shareable.zip'
$dllPart = 'Contents\x86_64-win\Key Compass.vst3'

function Say([string]$text, [string]$colour = 'Gray') { Write-Host $text -ForegroundColor $colour }
function Stop-WithError([string]$text) {
    Say ''
    Say '------------------------------------------------------------' 'Red'
    foreach ($line in ($text -split "`n")) { Say $line 'Red' }
    Say '------------------------------------------------------------' 'Red'
    exit 1
}

Write-Host ''
Write-Host '  ==================================================' -ForegroundColor Yellow
Write-Host '     KEY COMPASS  -  make a shareable package' -ForegroundColor Yellow
Write-Host '  ==================================================' -ForegroundColor Yellow
Write-Host ''

# the version, from CMakeLists.txt
$version = 'unknown'
$cmakeText = Get-Content -LiteralPath (Join-Path $Root 'CMakeLists.txt') -Raw
if ($cmakeText -match 'project\s*\(\s*KeyCompass\s+VERSION\s+([0-9.]+)') { $version = $Matches[1] }

# is there a build, and is it up to date with the source?
$builtVst3 = Join-Path $outDir 'Key Compass.vst3'
$builtDll = Join-Path $builtVst3 $dllPart
$builtExe = Join-Path $outDir 'Key Compass.exe'
$needBuild = $false
$reason = ''
if (-not (Test-Path -LiteralPath $builtDll) -or -not (Test-Path -LiteralPath $builtExe)) {
    $needBuild = $true
    $reason = 'There is no finished build in the Output folder yet.'
} else {
    $builtTime = (Get-Item -LiteralPath $builtDll).LastWriteTime
    $newest = Get-ChildItem -LiteralPath (Join-Path $Root 'Source'), (Join-Path $Root 'Resources') -Recurse -File |
              Sort-Object LastWriteTime -Descending | Select-Object -First 1
    $cmakeTime = (Get-Item -LiteralPath (Join-Path $Root 'CMakeLists.txt')).LastWriteTime
    if (($newest -and $newest.LastWriteTime -gt $builtTime) -or $cmakeTime -gt $builtTime) {
        $needBuild = $true
        $reason = 'The source code is newer than the build in the Output folder.'
    }
}

if ($needBuild) {
    Say $reason 'Yellow'
    $answer = Read-Host '  Build it now so your friend gets the latest version? (Y/n)'
    if ($answer -notmatch '^[Nn]') {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'build.ps1') -NoInstall
        if ($LASTEXITCODE -ne 0) { Stop-WithError 'The build failed - see the messages above (and build-log.txt).' }
    }
    if (-not (Test-Path -LiteralPath $builtDll) -or -not (Test-Path -LiteralPath $builtExe)) {
        Stop-WithError "There is still no finished build. Run 'Build and Install.bat' first, then this again."
    }
}

Say ('Packing Key Compass ' + $version + '...') 'Cyan'

# put the package together in a temporary folder
$stage = Join-Path $env:TEMP ('KeyCompassShareable-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
$pkg = Join-Path $stage 'Key Compass Shareable'
$files = Join-Path $pkg 'Files'
try {
    New-Item -ItemType Directory -Force -Path $files | Out-Null
    Copy-Item -LiteralPath $builtVst3 -Destination $files -Recurse -Force
    Copy-Item -LiteralPath $builtExe -Destination $files -Force
    Copy-Item -LiteralPath (Join-Path $templates 'install.ps1') -Destination $files -Force
    Copy-Item -LiteralPath (Join-Path $templates 'Install Key Compass.bat') -Destination $pkg -Force
    Copy-Item -LiteralPath (Join-Path $templates 'Uninstall Key Compass.bat') -Destination $pkg -Force
    $manual = Join-Path $Root 'Resources\manual\KeyCompassManual.pdf'
    if (Test-Path -LiteralPath $manual) { Copy-Item -LiteralPath $manual -Destination (Join-Path $pkg 'Key Compass Manual.pdf') -Force }
    $readme = (Get-Content -LiteralPath (Join-Path $templates 'READ ME FIRST.txt') -Raw).Replace('@VERSION@', $version)
    [IO.File]::WriteAllText((Join-Path $pkg 'READ ME FIRST.txt'), $readme, (New-Object System.Text.ASCIIEncoding))

    if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::CreateFromDirectory($pkg, $zipPath, [IO.Compression.CompressionLevel]::Optimal, $true)
} catch {
    Stop-WithError ('Could not make the package: ' + $_.Exception.Message)
} finally {
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue }
}

$sizeMb = [math]::Round((Get-Item -LiteralPath $zipPath).Length / 1MB, 1)
Say ''
Say ('Made: ' + $zipPath + '  (' + $sizeMb + ' MB, Key Compass ' + $version + ')') 'Green'
Say ''
Say 'Send it with Google Drive, Dropbox, OneDrive or WeTransfer - email often blocks zips with an app inside.' 'Green'
Say 'Your friend extracts it and double-clicks "Install Key Compass.bat" (READ ME FIRST.txt explains).' 'Green'
Say 'It is for 64-bit Windows 10 / 11.' 'Green'

# show the zip in Explorer
try { Start-Process -FilePath 'explorer.exe' -ArgumentList ('/select,"' + $zipPath + '"') } catch { }
exit 0
