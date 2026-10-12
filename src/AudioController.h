#pragma once
#include "Core.h"
#include <QAudioSink>
#include <QTimer>
#include <QThread>
namespace viz {struct Shared;}
class AudioRepository;
class AudioEngine;
class AudioController : public QObject {
    Q_OBJECT
public:
    explicit AudioController(TrackModel *model);
    ~AudioController() override;
    Logic::PlaybackState state;
    void hover(int index,double fraction);
    void click(int index,double fraction);
    void leave();
    void stop();
    void togglePlayback();
    void playTrack(int index);
    void previous();
    void next();
    void setVolume(int percent);
    void setVisualizer(viz::Shared *shared);
    int volume() const;
    double outputBufferMilliseconds()const;
signals:
    void changed();
    void progress(double fraction);
    void playing(bool playing);
    void error(const QString &message);
private:
    friend class PlaybackDiagnostics;
    TrackModel *model;
    AudioRepository *repository;
    std::unique_ptr<AudioEngine> engine;
    std::unique_ptr<QAudioSink> sink;
    QTimer seekTimer,progressTimer;
    QThread outputThread;
    QObject *outputContext=nullptr;
    std::atomic<QtAudio::Error> outputError=QtAudio::NoError;
    double outputBufferMs=0;
    int requestedVolume=100;
    double desired=0;
    bool waiting=false,audible=false;
    int selected=-1;
    bool playable(int index) const;
    void move(int direction);
    void start(int index,double fraction);
    void seek(bool fade=false);
};
