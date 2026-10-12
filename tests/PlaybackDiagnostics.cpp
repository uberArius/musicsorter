#include "MainWindow.h"
#include "AudioController.h"
#include "AudioEngine.h"
#include "AudioRepository.h"
#include "TrackGridView.h"
#include "VisualizerController.h"
#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QMouseEvent>
#include <QThread>
#include <QElapsedTimer>

// Diagnostic-only decorator: preserves the unbuffered pull interface and records
// which thread Qt uses to refill its output ring. No changes to production audio.
class PullMeter : public QIODevice {
public:
    explicit PullMeter(AudioEngine *e):engine(e){open(ReadOnly|Unbuffered);clock.start();}
    bool isSequential()const override{return true;}
    qint64 bytesAvailable()const override{return engine->bytesAvailable();}
    void resetMeter(){calls=guiCalls=0;maximumGap=0;last=clock.nsecsElapsed();}
    qint64 calls=0,guiCalls=0,maximumGap=0;
protected:
    qint64 readData(char *data,qint64 length)override{
        const auto now=clock.nsecsElapsed();maximumGap=std::max(maximumGap,now-last);last=now;++calls;
        if(QThread::currentThread()==qApp->thread())++guiCalls;
        return engine->read(data,length);
    }
    qint64 writeData(const char*,qint64)override{return -1;}
private:
    AudioEngine *engine;QElapsedTimer clock;qint64 last=0;
};

class PlaybackDiagnostics : public QObject {
    Q_OBJECT
private slots:
    void lockedHover(){
        MainWindow window;window.setAttribute(Qt::WA_DontShowOnScreen);window.show();
        auto audio=window.findChild<AudioController*>();auto model=window.findChild<TrackModel*>();
        auto grid=window.findChild<TrackGridView*>();auto visualizer=window.findChild<VisualizerController*>();
        QVERIFY(!visualizer->isRunning());
        window.openFile(QString(FIXTURE_DIR)+"/benchmark60.mp3");
        QTRY_VERIFY_WITH_TIMEOUT(audio->state.locked>=0&&audio->engine->hasStarted(),10000);
        QTRY_VERIFY_WITH_TIMEOUT(model->tracks[0].audioReady,10000);
        const auto request=audio->engine->requestNumber();const int locked=audio->state.locked;
        PullMeter *meter=nullptr;
        QMetaObject::invokeMethod(audio->outputContext,[&]{audio->sink->reset();meter=new PullMeter(audio->engine.get());audio->sink->start(meter);},Qt::BlockingQueuedConnection);
        auto cleanup=qScopeGuard([&]{QMetaObject::invokeMethod(audio->outputContext,[&]{audio->sink->reset();audio->sink->start(audio->engine.get());delete meter;},Qt::BlockingQueuedConnection);});
        QTest::qWait(300);
        QMetaObject::invokeMethod(audio->outputContext,[&]{meter->resetMeter();},Qt::BlockingQueuedConnection);
        int idle=0;auto connection=connect(audio->sink.get(),&QAudioSink::stateChanged,this,[&](QtAudio::State s){if(s==QtAudio::IdleState)++idle;});
        auto disconnectOnExit=qScopeGuard([&]{disconnect(connection);});
        auto button=window.findChild<QFrame*>("sortbar")->findChildren<QPushButton*>().first();
        for(int i=0;i<500;++i){
            QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(grid->viewport(),&leave);
            QEnterEvent enterButton(QPointF(10,10),QPointF(10,10),QPointF(10,10));QCoreApplication::sendEvent(button,&enterButton);button->repaint();
            QEvent leaveButton(QEvent::Leave);QCoreApplication::sendEvent(button,&leaveButton);
            const QPointF p=grid->card(i%int(model->tracks.size())).center();
            QEnterEvent enterGrid(p,p,p);QCoreApplication::sendEvent(grid->viewport(),&enterGrid);
            QMouseEvent move(QEvent::MouseMove,p,p,Qt::NoButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(grid->viewport(),&move);
            grid->viewport()->repaint();QTest::qWait(2);
        }
        QCOMPARE(audio->state.locked,locked);QCOMPARE(audio->engine->requestNumber(),request);
        qint64 maximumGap=0,calls=0,guiCalls=0;
        QMetaObject::invokeMethod(audio->outputContext,[&]{maximumGap=meter->maximumGap;calls=meter->calls;guiCalls=meter->guiCalls;meter->resetMeter();},Qt::BlockingQueuedConnection);
        QJsonObject report{{"visualizerRunning",visualizer->isRunning()},{"lockedTransitions",500},
            {"playbackRequestsDuringTransitions",qint64(audio->engine->requestNumber()-request)},
            {"sinkBufferMs",audio->outputBufferMilliseconds()},{"transitionRefillMaxGapMs",maximumGap/1e6},
            {"transitionIdleEvents",idle},{"pullCalls",calls},{"pullCallsOnGuiThread",guiCalls}};
        idle=0;
        const double before=audio->engine->position();
        // Controlled event-loop stall: decoded PCM is already available in RAM.
        QThread::msleep(150);
        const double after=audio->engine->position();
        QTest::qWait(100);
        QMetaObject::invokeMethod(audio->outputContext,[&]{maximumGap=meter->maximumGap;},Qt::BlockingQueuedConnection);
        report["controlledGuiStallMs"]=150;
        report["pcmPositionAdvanceDuringGuiStallMs"]=(after-before)*1000;
        report["stallRefillMaxGapMs"]=maximumGap/1e6;report["stallIdleEvents"]=idle;
        QFile file("playback-diagnostics.json");QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(report).toJson());
        qInfo().noquote()<<QJsonDocument(report).toJson();
        QCOMPARE(guiCalls,0);QVERIFY(calls>0);
        QVERIFY2(after-before>0.10,"Prepared audio stopped advancing while the GUI was blocked");
        QVERIFY2(maximumGap<75000000,"Output refill stalled despite its dedicated event loop");
    }
};
QTEST_MAIN(PlaybackDiagnostics)
#include "PlaybackDiagnostics.moc"
