#pragma once
#include "CompressedAudio.h"
#include <QThreadPool>
#include <QHash>
#include <QSet>
#include <QTimer>

class AudioRepository : public QObject {
    Q_OBJECT
public:
    static AudioRepository *forModel(TrackModel *model);
    explicit AudioRepository(TrackModel *model);
    ~AudioRepository() override;
    AudioAsset get(int index);
    void request(int index);
    void prioritize(const QVector<int> &indices);
    void pin(int index);
    qint64 usedBytes() const;
    qint64 budgetBytes() const { return budget; }
    void setBudgetForTesting(qint64 bytes);
    void trim();
signals:
    void prepared(int index);
    void progress(int done,int total);
    void memoryChanged(qint64 bytes,int ready);
private:
    struct Entry { AudioAsset asset; quint64 touched=0; };
    TrackModel *model;
    QThreadPool pool;
    QHash<int,Entry> entries;
    QSet<int> running, attempted;
    QVector<int> visible;
    CancelFlag cancel;
    QTimer pressureTimer,retryTimer;
    int pinned=-1, urgent=-1, concurrency=4;
    quint64 clock=0;
    qint64 budget=0, reserved=0;
    bool testBudget=false;
    void reset();
    void pump();
    bool launch(int index);
};
