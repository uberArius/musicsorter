# FFmpeg headers

Public header dependency closure from FFmpeg **7.1.2** (tag n7.1.2).
Source: https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n7.1.2.zip
Licenses: `../../licenses/ffmpeg/` and copyright notices in the headers.

`libavutil/avconfig.h` describes the Windows x64 target (little endian,
unaligned memory access). No private implementation is compiled here.
`cmake/FFmpeg.cmake` generates MSVC import libraries for the public functions
exported by the FFmpeg DLLs in the tested Qt 6.10.2 kit:
avcodec-61, avformat-61, avutil-59, swresample-5.

Do not substitute headers or DLLs with another major ABI without updating
this integration and exercising all audio/seek tests.
