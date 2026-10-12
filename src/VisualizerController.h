#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <memory>
class QWidget;class QLabel;class AudioController;
namespace viz {class Mapping;struct Shared;}
class VisualizerController : public QObject {
    Q_OBJECT
public:
    VisualizerController(AudioController *audio,QWidget *panel,QObject *parent=nullptr);
    ~VisualizerController() override;
    void setEnabled(bool enabled);
    void setAttached(bool attached);
    bool isRunning()const{return process.state()!=QProcess::NotRunning;}
    bool isAttached()const;
    bool isReady()const;
    qint64 processId()const{return process.processId();}
    quint64 windowId()const;
    qint64 consumedBlocks()const;
    viz::Shared *sharedForTesting()const;
    void setTestMode(bool value){testMode=value;}
    QWidget *surfaceWidget()const{return surface;}
    void setExecutableForTesting(const QString &path){executable=path;}
signals:
    void enabledChanged(bool enabled);
    void error(const QString &message);
private:
    AudioController *audio;
    QWidget *panel,*surface;
    QLabel *status;
    QProcess process;QTimer monitor,startTimeout,killTimeout;
    std::unique_ptr<viz::Mapping> mapping;
    void *job=nullptr;
    QString executable;
    bool stopping=false,connected=false,testMode=false;
    void cleanup();
    void poll();
};
