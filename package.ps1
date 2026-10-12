param([string]$QtRoot = "$PSScriptRoot\.tools\Qt\6.10.2\msvc2022_64", [switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
if (!$SkipBuild) { & "$PSScriptRoot\build.ps1" -QtRoot $QtRoot }
$deploy = Join-Path $QtRoot 'bin\windeployqt.exe'
if (!(Test-Path $deploy)) { throw "Missing windeployqt: $deploy" }
if (!(Test-Path "$PSScriptRoot\licenses\SOURCES.txt")) { throw 'Missing licenses. Run python scripts/collect_licenses.py first.' }
$dist = Join-Path $PSScriptRoot 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$stage = Join-Path $dist ('MusicSorter-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item -LiteralPath "$PSScriptRoot\build\MusicSorter.exe" -Destination $stage
& $deploy --release --no-translations --no-system-d3d-compiler --no-opengl-sw --no-compiler-runtime "$stage\MusicSorter.exe"
if ($LASTEXITCODE) { throw 'windeployqt failed.' }
# Use redistributable CRT files from the official MSVC Redist directory, never System32.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$redist = Get-ChildItem "$vs\VC\Redist\MSVC" -Directory | Where-Object Name -Match '^14\.' | Sort-Object Name -Descending | Select-Object -First 1
$crt = Join-Path $redist.FullName 'x64\Microsoft.VC143.CRT'
if (!(Test-Path $crt)) { throw 'Missing official MSVC redistributable CRT directory.' }
Copy-Item "$crt\*.dll" $stage
Copy-Item -LiteralPath "$PSScriptRoot\licenses" -Destination $stage -Recurse
New-Item -ItemType Directory -Path "$stage\licenses\sbom" -Force | Out-Null
foreach ($module in @('qtbase','qtmultimedia','qtsvg')) { Copy-Item "$QtRoot\sbom\$module-*.spdx" "$stage\licenses\sbom" }
Copy-Item -LiteralPath "$PSScriptRoot\README.md","$PSScriptRoot\THIRD_PARTY_NOTICES.md","$PSScriptRoot\TEST_REPORT.md","$PSScriptRoot\LICENSE","$PSScriptRoot\NOTICE" -Destination $stage
@"
[Paths]
Prefix=.
Plugins=.
"@ | Set-Content "$stage\qt.conf" -Encoding ascii
foreach ($required in @('MusicSorter.exe','Qt6Core.dll','Qt6Widgets.dll','Qt6Multimedia.dll','platforms\qwindows.dll','multimedia\ffmpegmediaplugin.dll','avcodec-61.dll','avformat-61.dll','avutil-59.dll','swresample-5.dll','msvcp140.dll','vcruntime140.dll')) {
    if (!(Test-Path (Join-Path $stage $required))) { throw "Incomplete deployment: $required" }
}
Copy-Item -LiteralPath "$PSScriptRoot/docs" -Destination $stage -Recurse
$visualizerBuild = Join-Path $PSScriptRoot 'integration/spectral-grid/build'
if (!(Test-Path "$visualizerBuild/Release/SpectralGrid.exe")) { throw 'Build the visualizer before packaging.' }
$cmakeCommand = "$PSScriptRoot/.tools/python/cmake/data/bin/cmake.exe"
if (!(Test-Path $cmakeCommand)) { $cmakeCommand = (Get-Command cmake -ErrorAction Stop).Source }
& $cmakeCommand --install $visualizerBuild --config Release --prefix "$stage/visualizer"
if ($LASTEXITCODE) { throw 'Visualizer deployment failed.' }
$zip = Join-Path $dist 'MusicSorter-Windows-x64.zip'
Compress-Archive -Path "$stage\*" -DestinationPath $zip -Force
Set-Content -LiteralPath "$dist\latest-package.txt" -Value $stage -Encoding utf8
Write-Host "Portable: $stage\MusicSorter.exe"
Write-Host "ZIP: $zip"
