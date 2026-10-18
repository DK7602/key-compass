# Key Compass - installer for the shareable package
# Run it through "Install Key Compass.bat" (or "Uninstall Key Compass.bat", which passes -Uninstall).
#   - the VST3 plug-in goes to  C:\Program Files\Common Files\VST3  (Windows asks for permission)
#   - the standalone app goes to  %LOCALAPPDATA%\Programs\Key Compass, with Desktop and Start menu shortcuts

param([switch]$Uninstall)

$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot

function Say([string]$text, [string]$colour = 'Gray') { Write-Host $text -ForegroundColor $colour }

function Stop-WithError([string]$text) {
    Say ''
    Say '------------------------------------------------------------' 'Red'
    foreach ($line in ($text -split "`n")) { Say $line 'Red' }
    Say '------------------------------------------------------------' 'Red'
    exit 1
}

function Quote-ForPS([string]$text) { return "'" + $text.Replace("'", "''") + "'" }

function Test-InUse([string]$path) {
    # a plug-in loaded by a DAW (or a running app) cannot be opened exclusively
    if (-not (Test-Path -LiteralPath $path)) { return $false }
    try { $fs = [IO.File]::Open($path, 'Open', 'Read', 'None'); $fs.Close(); return $false } catch { return $true }
}

function Get-OpenDaws {
    return Get-Process -ErrorAction SilentlyContinue |
        Where-Object { $_.ProcessName -match '^(Cubase|Nuendo|Dorico|VST ?Live|Reaper|Ableton|FL64|Studio One|Bitwig|Cakewalk)' } |
        Select-Object -ExpandProperty ProcessName -Unique
}

function Invoke-Elevated([string]$command) {
    Say 'Windows needs your permission to change C:\Program Files - click "Yes" on the prompt.' 'Yellow'
    try {
        Start-Process -FilePath 'powershell.exe' -Verb RunAs -Wait -WindowStyle Hidden `
            -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', $command)
    } catch {
        Say 'Permission was not given.' 'Yellow'
    }
}

Write-Host ''
Write-Host '  ==================================================' -ForegroundColor Yellow
if ($Uninstall) {
    Write-Host '     KEY COMPASS  -  uninstall' -ForegroundColor Yellow
} else {
    Write-Host '     KEY COMPASS  -  install' -ForegroundColor Yellow
}
Write-Host '  ==================================================' -ForegroundColor Yellow
Write-Host ''

if (-not [Environment]::Is64BitOperatingSystem) {
    Stop-WithError 'Key Compass needs 64-bit Windows.'
}

# the real (64-bit) Common Files folder, even if this happens to run as a 32-bit process
$common = $env:CommonProgramW6432
if (-not $common) { $common = $env:CommonProgramFiles }
$vst3Folder = Join-Path $common 'VST3'
$target = Join-Path $vst3Folder 'Key Compass.vst3'
$dllPart = 'Contents\x86_64-win\Key Compass.vst3'
$targetDll = Join-Path $target $dllPart
$appDir = Join-Path $env:LOCALAPPDATA 'Programs\Key Compass'
$appExe = Join-Path $appDir 'Key Compass.exe'
$desktopDir = [Environment]::GetFolderPath('Desktop')
if (-not $desktopDir) { $desktopDir = Join-Path $HOME 'Desktop' }
$startDir = [Environment]::GetFolderPath('Programs')
if (-not $startDir) { $startDir = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs' }
$desktopLink = Join-Path $desktopDir 'Key Compass.lnk'
$startLink = Join-Path $startDir 'Key Compass.lnk'

# a DAW with Key Compass loaded keeps the plug-in file locked
for ($attempt = 0; $attempt -lt 5 -and (Test-InUse $targetDll); $attempt++) {
    $daws = Get-OpenDaws
    $who = 'your DAW (Cubase or similar)'
    if ($daws) { $who = ($daws -join ', ') }
    Say ''
    Say ('Key Compass is open in ' + $who + ', so Windows will not let it be changed.') 'Yellow'
    Read-Host ('  Save your work, close ' + $who + ' completely, then press Enter here') | Out-Null
}
for ($attempt = 0; $attempt -lt 5 -and (Test-InUse $appExe); $attempt++) {
    Say ''
    Say 'The Key Compass app is open.' 'Yellow'
    Read-Host '  Close the Key Compass window, then press Enter here' | Out-Null
}

# =============================================================================================
if ($Uninstall) {
    if (Test-Path -LiteralPath $target) {
        try { Remove-Item -LiteralPath $target -Recurse -Force } catch { }
        if (Test-Path -LiteralPath $target) {
            Invoke-Elevated ("Remove-Item -LiteralPath " + (Quote-ForPS $target) + " -Recurse -Force")
        }
        if (Test-Path -LiteralPath $target) {
            Say ('Could not remove the plug-in. Close your DAW and try again, or delete this folder yourself: ' + $target) 'Red'
        } else {
            Say 'Removed the plug-in.' 'Green'
        }
    }
    foreach ($item in @($desktopLink, $startLink, $appDir)) {
        if (Test-Path -LiteralPath $item) { Remove-Item -LiteralPath $item -Recurse -Force -ErrorAction SilentlyContinue }
    }
    Say 'Removed the Key Compass app and its shortcuts.' 'Green'
    Say ''
    Say 'Key Compass is uninstalled. (Songs you made with it stay in your DAW projects.)' 'Green'
    exit 0
}

# =============================================================================================
$srcVst3 = Join-Path $here 'Key Compass.vst3'
$srcExe = Join-Path $here 'Key Compass.exe'
$srcManual = Join-Path (Split-Path -Parent $here) 'Key Compass Manual.pdf'
$srcDll = Join-Path $srcVst3 $dllPart

if (-not (Test-Path -LiteralPath $srcDll) -or -not (Test-Path -LiteralPath $srcExe)) {
    Stop-WithError ("The Key Compass files were not found next to this installer.`n" +
        "Right-click the zip > Extract All..., open the extracted folder,`n" +
        "then double-click 'Install Key Compass.bat' in there.")
}

# files that came in a downloaded zip are marked "from the internet"; clear that so Windows runs them
Get-ChildItem -LiteralPath $here -Recurse -File -ErrorAction SilentlyContinue | Unblock-File -ErrorAction SilentlyContinue

function Test-Installed {
    if (-not (Test-Path -LiteralPath $targetDll)) { return $false }
    return (Get-FileHash -LiteralPath $srcDll).Hash -eq (Get-FileHash -LiteralPath $targetDll).Hash
}

Say '==> Installing the VST3 plug-in' 'Cyan'
try {
    New-Item -ItemType Directory -Force -Path $vst3Folder | Out-Null
    if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }
    Copy-Item -LiteralPath $srcVst3 -Destination $vst3Folder -Recurse -Force
} catch { }
if (-not (Test-Installed)) {
    $cmd = "New-Item -ItemType Directory -Force -Path " + (Quote-ForPS $vst3Folder) + " | Out-Null; " +
           "if (Test-Path -LiteralPath " + (Quote-ForPS $target) + ") { Remove-Item -LiteralPath " + (Quote-ForPS $target) + " -Recurse -Force }; " +
           "Copy-Item -LiteralPath " + (Quote-ForPS $srcVst3) + " -Destination " + (Quote-ForPS $vst3Folder) + " -Recurse -Force"
    Invoke-Elevated $cmd
}
if (Test-Installed) {
    Say ('Installed the plug-in to: ' + $target) 'Green'
} else {
    Stop-WithError ("The plug-in could not be copied into Program Files.`n" +
        "Close your DAW, run 'Install Key Compass.bat' again and click Yes when Windows asks.`n" +
        "Or copy it yourself:`n" +
        "   from: " + $srcVst3 + "`n" +
        "   to:   " + $vst3Folder)
}

Say ''
Say '==> Installing the Key Compass app' 'Cyan'
try {
    New-Item -ItemType Directory -Force -Path $appDir | Out-Null
    Copy-Item -LiteralPath $srcExe -Destination $appExe -Force
    if (Test-Path -LiteralPath $srcManual) {
        Copy-Item -LiteralPath $srcManual -Destination (Join-Path $appDir 'Key Compass Manual.pdf') -Force
    }
    Say ('Installed the app to: ' + $appDir) 'Green'
} catch {
    Say ('Could not install the app: ' + $_.Exception.Message) 'Yellow'
    Say ('You can still run it straight from this folder: ' + $srcExe) 'Yellow'
}

if (Test-Path -LiteralPath $appExe) {
    try {
        $shell = New-Object -ComObject WScript.Shell
        foreach ($linkPath in @($desktopLink, $startLink)) {
            $link = $shell.CreateShortcut($linkPath)
            $link.TargetPath = $appExe
            $link.WorkingDirectory = $appDir
            $link.IconLocation = $appExe + ',0'
            $link.Description = 'Key Compass - key, chord and note finder + Song Mapper'
            $link.Save()
        }
        Say 'Added "Key Compass" to your Desktop and Start menu.' 'Green'
    } catch { }
}

# other copies of the plug-in that a DAW might load instead
$x86 = ${env:ProgramFiles(x86)}
$otherPlaces = @((Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3'))
if ($x86) { $otherPlaces += (Join-Path $x86 'Common Files\VST3') }
foreach ($place in $otherPlaces) {
    if (Test-Path -LiteralPath $place) {
        Get-ChildItem -LiteralPath $place -Recurse -Directory -Filter 'Key Compass*.vst3' -ErrorAction SilentlyContinue | ForEach-Object {
            Say ('Note: there is another copy here - delete it so your DAW uses the new one: ' + $_.FullName) 'Yellow'
        }
    }
}

Say ''
Say 'All done!' 'Green'
Say '  - In Cubase (or any DAW that uses VST3), add Key Compass as an insert effect - it is under "Analyzer".' 'Green'
Say '    If it is not listed: Studio > VST Plug-in Manager > Rescan (or your DAW''s plug-in rescan).' 'Green'
Say '  - Or open the "Key Compass" app from your Desktop.' 'Green'
Say '  - The manual is built in: click MANUAL at the top-right of Key Compass.' 'Green'
exit 0
