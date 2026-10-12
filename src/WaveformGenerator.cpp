#include "WaveformGenerator.h"
#include "AudioRepository.h"
WaveData WaveformGenerator::analyze(const QString &path,const CancelFlag &cancel){
    if(cancel->load())return {};
    return CompressedAudio::prepare(path,cancel)->waveform;
}
WaveformGenerator::WaveformGenerator(TrackModel *model):QObject(model),repository(AudioRepository::forModel(model)){
    connect(repository,&AudioRepository::progress,this,&WaveformGenerator::progress);
    QTimer::singleShot(0,this,[this]{repository->prioritize({});});
}
void WaveformGenerator::prioritize(const QVector<int> &indices){repository->prioritize(indices);}
