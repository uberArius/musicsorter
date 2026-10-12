# Validation

```powershell
./build.ps1
./test-windows.ps1
# Optional real-time performance run:
./test-windows.ps1 -SoakSeconds 30
```

Core tests cover configuration, audio conversion, FFT and bands, smoothing, timing, scrolling, camera motion, gravity, rebound and fixed lifetime color. The executable reports each assertion; CTest wraps the suite as one test.

The Windows integration harness needs an interactive desktop and hardware Direct3D 11 GPU. It checks CPU/GPU particle agreement, palette weights, full-yellow output, additive overlap, composition transparency, window modes, renderer recreation and relevant settings. Reports and screenshots go to `test-output`, excluded from Git and release archives.

GitHub Actions runs the Release build and core suite. Hosted CI does not substitute for hardware GPU and desktop checks. Performance depends on hardware and settings. Release-specific validation results appear in release notes.

## 1.1 local validation (2026-10-11)

- Release MSVC x64 build and both CTest suites (spectral_core, spectral_protocol): pass.
- MusicSorter native Windows integration harness: shared PCM/RMS, D3D11 frames and
  GPU readback, native parent relationship, resizing, detach/reattach, child close,
  off process exit, host destruction while detached and child crash isolation: pass.
- Maximum host playback callback + GUI notification latency with renderer running:
  17 ms across eight prepared-track requests (not acoustic latency).
- Standalone `--benchmark 3 --headless-test`: WASAPI loopback, RTX 4090, 181 frames,
  60.3 FPS; silent input in this run. Independent standalone mode remains available.
- Tests use hidden windows. No fresh physical mixed-DPI monitor test or desktop
  compositor/transparency validation was performed for the new hosted mode.
- Integration test data are in the MusicSorter build reports, not this public source
  archive. The protocol test is self-contained and runs in this repository.
