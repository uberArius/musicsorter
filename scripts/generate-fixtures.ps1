$ErrorActionPreference='Stop'
$target=Join-Path $PSScriptRoot '..\tests\fixtures'
New-Item -ItemType Directory -Force -Path $target | Out-Null
foreach ($format in @('wav','mp3','flac','ogg','m4a','aac','opus')) {
    & ffmpeg -hide_banner -loglevel error -y -f lavfi -i 'sine=frequency=440:sample_rate=48000:duration=1' -ac 2 (Join-Path $target "tone.$format")
    if($LASTEXITCODE){throw "Fixture generation failed: $format"}
}
& ffmpeg -hide_banner -loglevel error -y -f lavfi -i 'sine=frequency=440:sample_rate=48000:duration=60' -ac 2 -b:a 192k (Join-Path $target 'benchmark60.mp3')
if($LASTEXITCODE){throw 'Benchmark fixture generation failed'}
& ffmpeg -hide_banner -loglevel error -y -f lavfi -i 'aevalsrc=0.1*sin(2*PI*(220*t+30*t*t)):s=44100:d=8' -ac 2 -q:a 4 (Join-Path $target 'seek-vbr.mp3')
if($LASTEXITCODE){throw 'VBR fixture generation failed'}
