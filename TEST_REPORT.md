# Validation — MusicSorter 1.4.5

Tested on Windows with Qt 6.10.2, FFmpeg 7.1.2 and MSVC x64.

All five CTest suites passed for the English-interface release (18.49 seconds):
- Core: geometry, configuration, safe copying, scanning and waveform analysis.
- UI: controls, file opening, instance forwarding and simulated association registration.
- Audio: memory preparation, seek PCM comparison, cancellation, eviction and a 100-track benchmark.
- Playback resilience: output continues during a controlled 150 ms GUI stall.
- Visualizer integration: process lifecycle, attachment, resizing, PCM delivery and playback isolation.

Seek tests cover MP3 including VBR, WAV, FLAC, OGG, M4A, AAC and OPUS. They remove their own temporary source file before decoding and compare sought PCM with a sequential reference.

The packaged release passed a hidden startup check with audio and the visualizer. English UI screenshots were reviewed for layout.

These are software checks, not acoustic latency measurements. No claim is made of a clean-system installation test, every DPI configuration, or manual listening verification. Reproduce with ./build.ps1; local detailed reports are written under build/.
