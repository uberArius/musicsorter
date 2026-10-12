#pragma once
#include "CompressedAudio.h"
#include <QIODevice>
#include <QAudioFormat>
#include <condition_variable>
#include <mutex>
#include <thread>

// The device always supplies audio (silence while stopped), so QAudioSink stays open.
namespace viz {struct Shared;}
class AudioEngine : public QIODevice {
public:
    explicit AudioEngine(const QAudioFormat &format,QObject *parent=nullptr);
    ~AudioEngine() override;
    void request(AudioAsset asset,double seconds,bool fade);
    void stopPlayback();
    void setVisualizer(viz::Shared *shared);
    void setLoop(bool enabled){loop.store(enabled);}
    bool hasStarted()const{return started.load();}
    bool hasEnded()const{return ended.load();}
    double position()const{return playPosition.load();}
    QString takeError();
    quint64 requestNumber()const{return serial.load();}
    qint64 bytesAvailable()const override;
    bool isSequential()const override{return true;}
protected:
    qint64 readData(char *data,qint64 maxLength) override;
    qint64 writeData(const char *,qint64) override{return -1;}
private:
    QAudioFormat format;
    viz::Shared *visualizer=nullptr; // protected by ringMutex
    std::vector<float> ring;
    size_t head=0,tail=0,count=0;
    std::mutex ringMutex,commandMutex;
    std::condition_variable wake;
    std::thread worker;
    AudioAsset requested;
    double requestedPosition=0;
    bool requestedFade=true;
    QString failure;
    std::atomic_bool quitting=false,loop=false,started=false,ended=false;
    std::atomic<quint64> serial=0;
    std::atomic<double> playPosition=0;
    void run();
};
