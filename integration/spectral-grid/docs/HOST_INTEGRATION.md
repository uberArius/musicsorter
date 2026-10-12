# Host integration v1 (Windows x64)

Launch with `--host-session <Local mapping name> --host-pid <decimal process id>`.
The host creates `viz::Shared` from `src/VisualizerProtocol.h`, initializes magic,
version, byte count, PID, native parent HWND and attachRequested before launch.
Only the hosting session's child is controlled. Normal launch needs no host.

Hosted settings are seeded from standalone settings on first use, then saved separately.

The child opens the mapping, validates its header and HWND owner, and holds a
SYNCHRONIZE handle to the host process. Window creation and SetParent happen in
the child process. After D3D11/DirectComposition initialization, the child publishes
its HWND and ready flag. Host and child must use compatible per-monitor DPI modes.

The host writes attachRequested, suspended and closeRequested. The child publishes
attached, ready, HWND, frame/size/PCM diagnostics. A frame-paced poll applies window
changes. F10 and the connection panel change the attachment request. While docked,
the host owns geometry; while detached the visualizer owns it. Closing the host
ends the session even when detached. The host should use KILL_ON_JOB_CLOSE as a
fallback for a hung renderer; an independent standalone process is never assigned
to this job. Suspended pauses rendering, not analysis. Off must end the process.

Audio uses a 64-slot SPSC ring of up to 512 mono float samples per block, with
sample rate, QPC seconds and seek epoch. The producer is the host audio callback,
which copies consumed PCM after fades, before host volume, so monitoring levels remain unchanged even when playback is muted.
It performs no IPC waits, allocation, decoding or FFT. Windows Interlocked release /
acquire barriers publish payload ownership. Full queue: drop incoming block, never
overwrite an unread slot or delay playback. Consumer discards blocks older than
150 ms, resets analysis across epochs, and generates silence on input starvation.
The timeline retains the visualizer's existing 80 ms presentation offset.
The protocol is Windows-specific, not a portable C++ shared-atomic ABI.

The host must disconnect the callback under its ring lock before unmapping memory.
This keeps callback pointer lifetime safe during stop/crash/restart. A version or
layout mismatch is rejected. Shared names should be unpredictable and session-local;
use default per-user object security and never expose this channel over a network.

Tests: `spectral_protocol` checks overflow, ordering and concurrent publication.
MusicSorter's `VisualizerTests` launches real child processes with D3D11 and checks
PCM input, native parenting, resize, detach/reattach, stop, close, host exit, renderer
failure and playback latency. Test-only mapping fields suppress visible windows and
request a GPU screenshot. `SPECTRAL_GRID_CONFIG` redirects settings for isolation.
`--benchmark 3 --headless-test` exercises standalone D3D11/WASAPI without showing UI.
Mixed-monitor DPI and desktop compositor behavior still need manual validation.

While attached, the renderer forces an opaque background and disables the transparency slider. The saved detached transparency preference is preserved.
