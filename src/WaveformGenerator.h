#pragma once
#include "Core.h"
class AudioRepository;
class WaveformGenerator : public QObject {
    Q_OBJECT
public:
    explicit WaveformGenerator(TrackModel *model);
    void prioritize(const QVector<int> &indices);
    static WaveData analyze(const QString &path,const CancelFlag &cancel);
signals:
    void progress(int done,int total);
private:
    AudioRepository *repository;
};
