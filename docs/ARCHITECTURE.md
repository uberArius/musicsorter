# Architecture

MusicSorter is native C++20 with Qt Widgets, Multimedia and Concurrent.

- Core: geometry, playback state, configuration, scanning, atomic copy without overwrite, and disposable waveform cache.
- AudioRepository: immutable compressed packets and seek indexes for whole tracks in RAM; bounded parallel preparation and whole-track eviction.
- CompressedAudio / PacketDecoder: FFmpeg 7.1.2 demuxing, waveform analysis and indexed in-memory seeking.
- AudioEngine: persistent output with a short PCM ring buffer. A separate worker decodes; the output callback performs no file I/O or decoding.
- AudioController: hover, seek, lock, transport and generation checks. Audio output runs independently of the GUI thread.
- UI: one virtualized QAbstractScrollArea draws track cards; there is no widget per track.
- Optional visualization: a separate Spectral Grid process, documented in its own project.

Original music files are read-only inputs. Sorting copies to a temporary file and commits atomically without replacing an existing destination. Directory changes cancel stale background work.
