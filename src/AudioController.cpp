#include "AudioController.h"
#include "AudioRepository.h"
#include "AudioEngine.h"
#include <QMediaDevices>
#include <QAudioDevice>
#include <cmath>
namespace {constexpr int SeekIntervalMs=20;constexpr double MinimumFractionChange=0.001;}
AudioController::AudioController(TrackModel *m):QObject(m),model(m),repository(AudioRepository::forModel(m)){
    const auto device=QMediaDevices::defaultAudioOutput();
    QAudioFormat format;format.setSampleRate(48000);format.setChannelCount(2);format.setSampleFormat(QAudioFormat::Float);
    if(!device.isFormatSupported(format))format=device.preferredFormat();
    if(format.isValid()){
        outputContext=new QObject;
        outputContext->moveToThread(&outputThread);
        connect(&outputThread,&QThread::finished,outputContext,&QObject::deleteLater);
        outputThread.setObjectName("MusicSorter audio output");
        outputThread.start(QThread::HighPriority);
        // QAudioSink pulls its QIODevice from the owning event loop. Both must
        // live here: a busy GUI must never prevent delivery of prepared PCM.
        QMetaObject::invokeMethod(outputContext,[this,device,format]{
            engine=std::make_unique<AudioEngine>(format);
            sink=std::make_unique<QAudioSink>(device,format);
            sink->setBufferSize(format.bytesForDuration(20000));
            connect(sink.get(),&QAudioSink::stateChanged,outputContext,[this](QtAudio::State outputState){
                const auto failure=sink->error();outputError.store(failure);
                if(outputState==QtAudio::StoppedState&&failure!=QtAudio::NoError)
                    QMetaObject::invokeMethod(this,[this]{stop();emit error("The audio device is unavailable. Reconnect it and restart the application.");},Qt::QueuedConnection);
            });
            sink->start(engine.get());outputError.store(sink->error());
            outputBufferMs=format.durationForBytes(sink->bufferSize())/1000.0;
        },Qt::BlockingQueuedConnection);
    }
    seekTimer.setSingleShot(true);seekTimer.setInterval(SeekIntervalMs);
    connect(&seekTimer,&QTimer::timeout,this,[this]{seek();});
    connect(repository,&AudioRepository::prepared,this,[this](int i){
        if(i!=state.active||!waiting)return;
        if(!model->tracks[i].error.isEmpty()){const auto message=model->tracks[i].name+": "+model->tracks[i].error;stop();emit error(message);return;}
        seek(true);
    });
    connect(model,&TrackModel::reset,this,[this]{stop();selected=-1;});
    connect(&progressTimer,&QTimer::timeout,this,[this]{
        if(!engine||state.active<0)return;
        auto message=engine->takeError();if(!message.isEmpty()){stop();emit error(message);return;}
        if(engine->hasStarted()&&!audible){audible=true;emit playing(true);}
        const auto duration=model->tracks[state.active].duration;
        if(duration>0)emit progress(std::clamp(engine->position()*1000/duration,0.0,1.0));
        if(engine->hasEnded()){if(state.locked>=0)next();else stop();}
    });progressTimer.start(16);
}
AudioController::~AudioController(){
    progressTimer.stop();seekTimer.stop();
    if(outputContext){
        QMetaObject::invokeMethod(outputContext,[this]{sink->reset();sink.reset();engine.reset();},Qt::BlockingQueuedConnection);
        outputThread.quit();outputThread.wait();
    }
}
double AudioController::outputBufferMilliseconds()const{return outputBufferMs;}
void AudioController::start(int i,double f){
    selected=i;seekTimer.stop();desired=f;waiting=true;audible=false;repository->pin(i);
    if(engine)engine->stopPlayback();emit changed();
    if(!engine||outputError.load()!=QtAudio::NoError){stop();emit error("No audio device is available.");return;}
    if(repository->get(i))seek(true);else repository->request(i);
}
void AudioController::hover(int i,double f){
    if(state.locked>=0||!playable(i))return;
    if(state.hover(i)){start(i,f);return;}
    if(std::abs(f-desired)<MinimumFractionChange)return;
    desired=f;if(!seekTimer.isActive())seekTimer.start();
}
void AudioController::seek(bool fade){
    if(state.active<0||!engine)return;
    auto asset=repository->get(state.active);if(!asset){waiting=true;repository->request(state.active);return;}
    waiting=false;
    const double seconds=std::clamp(desired*asset->durationSeconds,0.0,std::max(0.0,asset->durationSeconds-0.001));
    engine->request(asset,seconds,fade);
}
void AudioController::click(int i,double f){
    if(!playable(i))return;
    const bool newTrack=state.click(i);if(newTrack)start(i,f);else seekTimer.stop();
    emit changed();
}
bool AudioController::playable(int i)const{return i>=0&&i<model->tracks.size()&&!model->tracks[i].sorted&&!model->tracks[i].copying&&model->tracks[i].error.isEmpty();}
void AudioController::setVolume(int percent){
    requestedVolume=std::clamp(percent,0,100);const float gain=requestedVolume/100.0f;
    if(outputContext)QMetaObject::invokeMethod(outputContext,[this,gain]{sink->setVolume(gain);},Qt::QueuedConnection);
}
void AudioController::setVisualizer(viz::Shared *shared){if(engine)engine->setVisualizer(shared);}
int AudioController::volume()const{return requestedVolume;}
void AudioController::togglePlayback(){
    if(state.active>=0){stop();return;}
    int i=selected;if(!playable(i)){i=0;while(i<model->tracks.size()&&!playable(i))++i;}
    if(playable(i)){state.active=state.locked=i;start(i,0);}
}
void AudioController::move(int direction){
    const int from=state.active>=0?state.active:selected;
    int i=from<0?(direction>0?0:int(model->tracks.size())-1):from+direction;
    while(i>=0&&i<model->tracks.size()&&!playable(i))i+=direction;
    if(playable(i)){state.active=state.locked=i;start(i,0);}
    else if(direction>0)stop();
}
void AudioController::previous(){move(-1);}
void AudioController::playTrack(int i){if(i>=0&&i<model->tracks.size()&&!model->tracks[i].copying){state.active=state.locked=i;start(i,0);}}
void AudioController::next(){move(1);}
void AudioController::leave(){if(state.locked<0)stop();}
void AudioController::stop(){seekTimer.stop();waiting=false;audible=false;state.stop();repository->pin(-1);if(engine)engine->stopPlayback();emit changed();emit playing(false);}
