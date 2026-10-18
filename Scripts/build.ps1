# Key Compass - build and install script for Windows
# Run it through "Build and Install.bat" (double-click). It will:
#   1. find Visual Studio's C++ compiler
#   2. download the JUCE framework (first time only)
#   3. build the VST3 plug-in and the standalone app
#   4. copy "Key Compass.vst3" to C:\Program Files\Common Files\VST3 (asks for permission)

param(
    [switch]$NoInstall,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$JuceVersion = '8.0.15'
$Root = Split-Path -Parent $PSScriptRoot
$LogFile = Join-Path $Root 'build-log.txt'
Set-Content -Path $LogFile -Value ("Key Compass build log - " + (Get-Date)) -Encoding UTF8

function Write-Log([string]$text, [string]$colour = 'Gray') {
    Write-Host $text -ForegroundColor $colour
    Add-Content -Path $LogFile -Value $text
}

function Write-Step([string]$text) {
    Write-Log ''
    Write-Log ('==> ' + $text) 'Cyan'
}

function Stop-WithError([string]$text) {
    Write-Log ''
    Write-Log '------------------------------------------------------------' 'Red'
    foreach ($line in ($text -split "`n")) { Write-Log $line 'Red' }
    Write-Log '------------------------------------------------------------' 'Red'
    exit 1
}

function Quote-ForPS([string]$text) { return "'" + $text.Replace("'", "''") + "'" }

function Invoke-Logged([string]$exe, [string[]]$arguments) {
    Write-Log ('> ' + $exe + ' ' + ($arguments -join ' ')) 'DarkGray'
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $exe @arguments 2>&1 | ForEach-Object {
        $line = "$_"
        Write-Host $line
        Add-Content -Path $LogFile -Value $line
    }
    $code = $LASTEXITCODE
    $ErrorActionPreference = $previous
    return $code
}

Write-Host ''
Write-Host '  ==================================================' -ForegroundColor Yellow
Write-Host '     KEY COMPASS  -  build and install' -ForegroundColor Yellow
Write-Host '  ==================================================' -ForegroundColor Yellow
Write-Host ''
Write-Log ('Project folder: ' + $Root)

if ($Root.Length -gt 110) {
    Write-Log 'Warning: this folder path is very long. If the build fails, move the KeyCompass folder somewhere short, e.g. C:\KeyCompass' 'Yellow'
}

# ---------------------------------------------------------------------------------------------
Write-Step 'Looking for Visual Studio (C++ tools)'

$programFilesX86 = ${env:ProgramFiles(x86)}
if (-not $programFilesX86) { $programFilesX86 = $env:ProgramFiles }
if (-not $programFilesX86) { $programFilesX86 = 'C:\Program Files (x86)' }
$vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    Stop-WithError ("Visual Studio was not found.`n" +
        "Install 'Visual Studio Community' (free) from https://visualstudio.microsoft.com/`n" +
        "and tick the workload 'Desktop development with C++'. Then run this again.")
}

$vsPath = & $vswhere -latest -products * -prerelease -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    Stop-WithError ("Visual Studio is installed, but without the C++ tools.`n" +
        "1. Open 'Visual Studio Installer' from the Start menu`n" +
        "2. Click 'Modify' next to your Visual Studio`n" +
        "3. Tick 'Desktop development with C++' and click 'Modify'`n" +
        "4. When it finishes, run 'Build and Install.bat' again.")
}
if ($vsPath -is [array]) { $vsPath = $vsPath[0] }
Write-Log ('Visual Studio: ' + $vsPath)

$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { Stop-WithError ('Could not find ' + $vcvars) }

# Import the Visual Studio developer environment (compiler, cmake, ninja) into this session
$envLines = cmd /c "`"$vcvars`" >nul 2>&1 && set"
foreach ($line in $envLines) {
    if ($line -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
    }
}

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    Stop-WithError ("The Visual Studio C++ compiler could not be started.`n" +
        "Open 'Visual Studio Installer' > Modify > 'Desktop development with C++' and make sure a`n" +
        "'Windows 11 SDK' (or Windows 10 SDK) is ticked on the right-hand side. Then run this again.")
}

$cmake = (Get-Command cmake.exe -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $candidate = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (Test-Path $candidate) { $cmake = $candidate }
}
if (-not $cmake) {
    Stop-WithError ("CMake was not found.`n" +
        "Open 'Visual Studio Installer' > Modify > 'Desktop development with C++' and make sure`n" +
        "'C++ CMake tools for Windows' is ticked on the right-hand side. Then run this again.")
}
Write-Log ('CMake: ' + $cmake)

$ninja = (Get-Command ninja.exe -ErrorAction SilentlyContinue).Source
if (-not $ninja) {
    $candidate = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
    if (Test-Path $candidate) { $ninja = $candidate }
}

# ---------------------------------------------------------------------------------------------
Write-Step ('Getting the JUCE framework ' + $JuceVersion)

$juceDir = Join-Path $Root 'JUCE'
if (Test-Path (Join-Path $juceDir 'CMakeLists.txt')) {
    Write-Log 'JUCE is already downloaded.'
} else {
    if (Test-Path $juceDir) { Remove-Item -Recurse -Force $juceDir }

    $git = (Get-Command git.exe -ErrorAction SilentlyContinue).Source
    if (-not $git -and (Test-Path 'C:\Program Files\Git\cmd\git.exe')) { $git = 'C:\Program Files\Git\cmd\git.exe' }

    $ok = $false
    if ($git) {
        Write-Log 'Downloading JUCE with Git (about 110 MB, one time only)...'
        $code = Invoke-Logged $git @('-c', 'advice.detachedHead=false', 'clone', '--depth', '1', '--branch', $JuceVersion, 'https://github.com/juce-framework/JUCE.git', $juceDir)
        $ok = ($code -eq 0) -and (Test-Path (Join-Path $juceDir 'CMakeLists.txt'))
    }

    if (-not $ok) {
        Write-Log 'Downloading JUCE as a zip file...'
        $zip = Join-Path $env:TEMP ('JUCE-' + $JuceVersion + '.zip')
        $tmp = Join-Path $env:TEMP ('JUCE-extract-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
        try {
            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
            $ProgressPreference = 'SilentlyContinue'
            Invoke-WebRequest -Uri ('https://github.com/juce-framework/JUCE/archive/refs/tags/' + $JuceVersion + '.zip') -OutFile $zip -UseBasicParsing
            Expand-Archive -Path $zip -DestinationPath $tmp -Force
            Move-Item -Path (Join-Path $tmp ('JUCE-' + $JuceVersion)) -Destination $juceDir
        } catch {
            Stop-WithError ("Could not download JUCE: " + $_.Exception.Message + "`nCheck your internet connection and run this again.")
        } finally {
            Remove-Item -Force -ErrorAction SilentlyContinue $zip
            Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $tmp
        }
    }

    if (-not (Test-Path (Join-Path $juceDir 'CMakeLists.txt'))) { Stop-WithError 'JUCE could not be downloaded.' }
    Write-Log 'JUCE downloaded.'
}

# ---------------------------------------------------------------------------------------------
Write-Step 'Building Key Compass (this takes about 5-10 minutes the first time)'

$buildDir = Join-Path $Root 'build'
if ($Clean -and (Test-Path $buildDir)) { Remove-Item -Recurse -Force $buildDir }

$configureArgs = @('-S', $Root, '-B', $buildDir, '-DCMAKE_BUILD_TYPE=Release')
if ($ninja) {
    $configureArgs += @('-G', 'Ninja', ('-DCMAKE_MAKE_PROGRAM=' + $ninja))
} else {
    $configureArgs += @('-A', 'x64')
}

$code = Invoke-Logged $cmake $configureArgs
if ($code -ne 0) {
    Write-Log 'Configuration failed - trying once more with a clean build folder...' 'Yellow'
    if (Test-Path $buildDir) { Remove-Item -Recurse -Force $buildDir }
    $code = Invoke-Logged $cmake $configureArgs
    if ($code -ne 0) { Stop-WithError "CMake configuration failed. The details are in build-log.txt." }
}

$code = Invoke-Logged $cmake @('--build', $buildDir, '--config', 'Release', '--parallel')
if ($code -ne 0) { Stop-WithError "The build failed. The details are in build-log.txt (send that file if you need help)." }

$artefacts = Join-Path $buildDir 'KeyCompass_artefacts'
$vst3 = Get-ChildItem -Path $artefacts -Recurse -Directory -Filter 'Key Compass.vst3' -ErrorAction SilentlyContinue | Select-Object -First 1
$exe  = Get-ChildItem -Path $artefacts -Recurse -File -Filter 'Key Compass.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $vst3) { Stop-WithError 'The build finished but Key Compass.vst3 was not found.' }

Write-Log ('Built: ' + $vst3.FullName) 'Green'

# Keep handy copies next to the project
$outDir = Join-Path $Root 'Output'
$outVst3 = Join-Path $outDir 'Key Compass.vst3'
try {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    if (Test-Path -LiteralPath $outVst3) { Remove-Item -LiteralPath $outVst3 -Recurse -Force }
    Copy-Item -LiteralPath $vst3.FullName -Destination $outDir -Recurse -Force
    if ($exe) {
        Copy-Item -LiteralPath $exe.FullName -Destination (Join-Path $outDir 'Key Compass.exe') -Force
        Write-Log ('Standalone app: ' + (Join-Path $outDir 'Key Compass.exe')) 'Green'
    }
} catch {
    Write-Log ('Note: could not update the Output folder (is Key Compass.exe still open?): ' + $_.Exception.Message) 'Yellow'
}

# ---------------------------------------------------------------------------------------------
if ($NoInstall) {
    Write-Log ''
    Write-Log ('Done. The plug-in is in: ' + $outDir) 'Green'
    exit 0
}

Write-Step 'Installing the VST3 plug-in'

$vst3Folder = Join-Path $env:CommonProgramFiles 'VST3'
$target = Join-Path $vst3Folder 'Key Compass.vst3'
$dllPart = 'Contents\x86_64-win\Key Compass.vst3'
$builtDll = Join-Path $vst3.FullName $dllPart
$targetDll = Join-Path $target $dllPart

function Test-InUse([string]$path) {
    # a plug-in loaded by a DAW cannot be opened exclusively
    if (-not (Test-Path -LiteralPath $path)) { return $false }
    try { $fs = [IO.File]::Open($path, 'Open', 'Read', 'None'); $fs.Close(); return $false } catch { return $true }
}

function Test-Installed {
    if (-not (Test-Path -LiteralPath $targetDll) -or -not (Test-Path -LiteralPath $builtDll)) { return $false }
    return (Get-FileHash -LiteralPath $builtDll).Hash -eq (Get-FileHash -LiteralPath $targetDll).Hash
}

# A DAW that has Key Compass loaded keeps the old plug-in file locked - it must be closed first
function Get-OpenDaws {
    return Get-Process -ErrorAction SilentlyContinue |
        Where-Object { $_.ProcessName -match '^(Cubase|Nuendo|Dorico|VST ?Live|Reaper|Ableton|FL64|Studio One|Bitwig)' } |
        Select-Object -ExpandProperty ProcessName -Unique
}
for ($attempt = 0; $attempt -lt 5 -and (Test-InUse $targetDll); $attempt++) {
    $daws = Get-OpenDaws
    $who = if ($daws) { ($daws -join ', ') } else { 'your DAW' }
    Write-Log ''
    Write-Log ('The old Key Compass is still loaded in ' + $who + ', so Windows will not let it be replaced.') 'Yellow'
    Read-Host ('  Save your project, close ' + $who + ' completely, then press Enter here') | Out-Null
}
if (Get-OpenDaws) {
    Write-Log ('Note: ' + ((Get-OpenDaws) -join ', ') + ' is open. Restart it after this finishes so it loads the new version.') 'Yellow'
}

$installed = $false
try {
    New-Item -ItemType Directory -Force -Path $vst3Folder | Out-Null
    if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }
    Copy-Item -LiteralPath $vst3.FullName -Destination $vst3Folder -Recurse -Force
} catch { }
$installed = Test-Installed

if (-not $installed) {
    Write-Log 'Windows needs your permission to copy into Program Files - click "Yes" on the prompt.' 'Yellow'
    $qFolder = Quote-ForPS $vst3Folder
    $qTarget = Quote-ForPS $target
    $qSource = Quote-ForPS $vst3.FullName
    $cmd = "New-Item -ItemType Directory -Force -Path $qFolder | Out-Null; " +
           "if (Test-Path -LiteralPath $qTarget) { Remove-Item -LiteralPath $qTarget -Recurse -Force }; " +
           "Copy-Item -LiteralPath $qSource -Destination $qFolder -Recurse -Force"
    try {
        Start-Process -FilePath 'powershell.exe' -Verb RunAs -Wait -WindowStyle Hidden `
            -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', $cmd)
    } catch {
        Write-Log 'Permission was not given.' 'Yellow'
    }
    $installed = Test-Installed
}

if ($installed) {
    Write-Log ('Installed the new plug-in to: ' + $target) 'Green'
} else {
    Stop-WithError ("The NEW plug-in could not be copied, so Cubase would still load the old one.`n" +
        "Most likely Cubase was still open, or the Windows permission prompt was not accepted.`n" +
        "Close Cubase completely and run 'Build and Install.bat' again (click Yes when Windows asks),`n" +
        "or copy it yourself (replace the old one):`n" +
        "   from: " + $outVst3 + "`n" +
        "   to:   " + $vst3Folder)
}

# warn about other copies that Cubase might load instead
$otherPlaces = @((Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3'), (Join-Path ${env:ProgramFiles(x86)} 'Common Files\VST3'))
foreach ($place in $otherPlaces) {
    if ($place -and (Test-Path -LiteralPath $place)) {
        Get-ChildItem -LiteralPath $place -Recurse -Directory -Filter 'Key Compass*.vst3' -ErrorAction SilentlyContinue | ForEach-Object {
            Write-Log ('Note: there is another, older copy here - delete it so Cubase uses the new one: ' + $_.FullName) 'Yellow'
        }
    }
}

# Desktop shortcut for the standalone app
if ($exe) {
    try {
        $desktop = [Environment]::GetFolderPath('Desktop')
        $shell = New-Object -ComObject WScript.Shell
        $link = $shell.CreateShortcut((Join-Path $desktop 'Key Compass.lnk'))
        $link.TargetPath = (Join-Path $outDir 'Key Compass.exe')
        $link.WorkingDirectory = $outDir
        $link.IconLocation = (Join-Path $outDir 'Key Compass.exe') + ',0'
        $link.Save()
        # ask Windows to refresh its icon cache so the new compass icon shows straight away
        $refresher = Join-Path $env:SystemRoot 'System32\ie4uinit.exe'
        if (Test-Path $refresher) { Start-Process -FilePath $refresher -ArgumentList '-show' -WindowStyle Hidden -ErrorAction SilentlyContinue }
        Write-Log 'Added a "Key Compass" shortcut (standalone app) to your desktop.' 'Green'
    } catch { }
}

Write-Log ''
Write-Log 'All done! Open Cubase, then add Key Compass as an insert effect (look under Analyzer).' 'Green'
Write-Log 'Right-click the background of Key Compass: the menu shows the version you are running.' 'Green'
Write-Log 'If it does not show up: Studio > VST Plug-in Manager > Rescan.' 'Green'
exit 0
