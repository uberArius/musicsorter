param([string]$QtRoot = "$PSScriptRoot\.tools\Qt\6.10.2\msvc2022_64", [switch]$SkipTests, [switch]$SkipVisualizer)
$ErrorActionPreference = 'Stop'
if (!(Test-Path "$QtRoot\lib\cmake\Qt6\Qt6Config.cmake")) { throw "Brak Qt: $QtRoot. Podaj -QtRoot do zestawu Qt >= 6.8 MSVC x64 z Multimedia." }
$localCmake = "$PSScriptRoot\.tools\python\cmake\data\bin"
$localNinja = "$PSScriptRoot\.tools\python\bin"
$env:PATH = "$localCmake;$localNinja;$QtRoot\bin;$env:PATH"
foreach ($tool in @('cmake','ninja')) { if (!(Get-Command $tool -ErrorAction SilentlyContinue)) { throw "Brak narzędzia $tool w PATH." } }
if (!(Get-Command cl -ErrorAction SilentlyContinue)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (!(Test-Path $vswhere)) { throw 'Zainstaluj Visual Studio 2022 Build Tools: Desktop development with C++.' }
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (!$vs) { throw 'Nie znaleziono kompilatora MSVC x64.' }
    $devcmd = Join-Path $vs 'Common7\Tools\VsDevCmd.bat'
    cmd /c "`"$devcmd`" -arch=x64 -host_arch=x64 >nul && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process') }
    }
}
cmake -S $PSScriptRoot -B "$PSScriptRoot\build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release "-DCMAKE_PREFIX_PATH=$QtRoot"
if ($LASTEXITCODE) { throw 'Konfiguracja CMake nie powiodła się.' }
cmake --build "$PSScriptRoot\build" --parallel 4
if ($LASTEXITCODE) { throw 'Kompilacja nie powiodła się.' }
if (!$SkipVisualizer) {
    $visualizerSource = Join-Path $PSScriptRoot 'integration/spectral-grid'
    $visualizerBuild = Join-Path $visualizerSource 'build'
    $extraArgs = @()
    $siblingDeps = Join-Path (Split-Path $PSScriptRoot) 'wizualizator/build/_deps'
    foreach ($dependency in @('json','imgui')) {
        $dependencySource = Join-Path $siblingDeps "$dependency-src"
        if (Test-Path $dependencySource) { $extraArgs += "-DFETCHCONTENT_SOURCE_DIR_$($dependency.ToUpper())=$dependencySource" }
    }
    if ((Get-FileHash "$PSScriptRoot/src/VisualizerProtocol.h").Hash -ne (Get-FileHash "$visualizerSource/src/VisualizerProtocol.h").Hash) { throw 'Visualizer protocol headers differ.' }
    cmake -S $visualizerSource -B $visualizerBuild -G 'Visual Studio 17 2022' -A x64 @extraArgs
    if ($LASTEXITCODE) { throw 'Visualizer configuration failed.' }
    cmake --build $visualizerBuild --config Release --parallel 4 -- /nodeReuse:false
    if ($LASTEXITCODE) { throw 'Visualizer build failed.' }
}
if (!$SkipTests) {
    ctest --test-dir "$PSScriptRoot\build" --output-on-failure
    if ($LASTEXITCODE) { throw 'Testy nie powiodły się.' }
}
Write-Host "Gotowe: $PSScriptRoot\build\MusicSorter.exe"
