#pragma once
#include "Core.h"
#include <functional>
#include <vector>

// Immutable, complete compressed audio payload plus packet/time index.
// No decoder or disk access is shared with the audio render callback.
class CompressedAudio {
public:
    struct Impl;
    std::unique_ptr<Impl> impl;
    WaveData waveform;
    qint64 memoryBytes = 0;
    double durationSeconds = 0;
    explicit CompressedAudio();
    ~CompressedAudio();
    static std::shared_ptr<CompressedAudio> prepare(const QString &path, const CancelFlag &cancel,
                                                   qint64 maxBytes = 1024LL*1024*1024*8);
};
using AudioAsset = std::shared_ptr<const CompressedAudio>;

class PacketDecoder {
public:
    struct Impl;
    std::unique_ptr<Impl> impl;
    explicit PacketDecoder(AudioAsset asset, int sampleRate = 48000, int channels = 2);
    ~PacketDecoder();
    void seek(double seconds);
    // Decodes only the next audio frame. false means end of stream.
    bool next(std::vector<float> &samples, double &startSeconds);
};
