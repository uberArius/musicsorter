# Contributing

Report reproducible problems through GitHub Issues, including Windows version, audio device, file format and steps to reproduce. Do not upload private or copyrighted recordings; a synthetic sample is preferable.

Use native C++20 and Qt Widgets. Preserve original audio files: sorting only copies, never moves, deletes or overwrites. Keep I/O and decoding off the GUI and audio callback threads. Cache complete compressed tracks, not selected fragments.

Build with ./build.ps1 and run its tests before submitting changes. Audio/GPU tests require suitable Windows hardware. Keep third-party notices and dependency versions accurate.

Changes to Spectral Grid itself belong in https://github.com/uberArius/spectral-grid.
