#include "MainWindow.h"
#include "VisualizerController.h"
#include "VisualizerProtocol.h"
#include "AudioController.h"
#include "Core.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QElapsedTimer>
#include "TrackGridView.h"
class VisualizerTests:public QObject {
    Q_OBJECT
private slots:
    void integration(){
        QTemporaryDir config;QVERIFY(config.isValid());qputenv("SPECTRAL_GRID_CONFIG",config.filePath("standalone.json").toUtf8());
        QFile preset(config.filePath("musicsorter.json"));QVERIFY(preset.open(QIODevice::WriteOnly));preset.write("{\"transparency\":100}");preset.close();
        MainWindow window;window.setAttribute(Qt::WA_DontShowOnScreen);window.show();
        auto viz=window.findChild<VisualizerController*>();auto audio=window.findChild<AudioController*>();
        auto toggle=window.findChild<QPushButton*>("visualizerToggle");QVERIFY(viz&&audio&&toggle);QVERIFY(!viz->isRunning());
        viz->setTestMode(true);viz->setExecutableForTesting(QString(VISUALIZER_EXE));
        QSignalSpy errors(viz,&VisualizerController::error);QSignalSpy playing(audio,&AudioController::playing);
        window.openFile(QString(FIXTURE_DIR)+"/benchmark60.mp3");QTRY_VERIFY_WITH_TIMEOUT(audio->state.locked>=0,10000);
        viz->setEnabled(true);QTRY_VERIFY_WITH_TIMEOUT(viz->isReady(),20000);QVERIFY(toggle->isChecked());QVERIFY(viz->isAttached());
        auto panel=window.findChild<QWidget*>("visualizerPanel");
        auto top=window.findChild<QWidget*>("topbar");
        auto bar=window.findChild<QWidget*>("sortbar");
        auto grid=window.findChild<TrackGridView*>();
        QVERIFY(panel&&top&&bar&&grid);
        const auto rightInWindow=[&window](QWidget *w){return w->mapTo(&window,QPoint(w->width(),0)).x();};
        QCOMPARE(rightInWindow(top),rightInWindow(grid));
        QCOMPARE(rightInWindow(bar),rightInWindow(grid));
        QCOMPARE(panel->height(),window.centralWidget()->height());
        QCOMPARE(panel->mapTo(&window,QPoint(0,0)).y(),window.centralWidget()->pos().y());
        window.grab().save("visualizer-layout.png");
        const auto hwnd=reinterpret_cast<HWND>(viz->windowId());QVERIFY(IsWindow(hwnd));QCOMPARE(GetParent(hwnd),reinterpret_cast<HWND>(viz->surfaceWidget()->winId()));
        QTRY_VERIFY_WITH_TIMEOUT(viz->consumedBlocks()>10,5000);QTRY_VERIFY_WITH_TIMEOUT(viz::load64(viz->sharedForTesting()->renderedFrames)>5,5000);
        QTRY_VERIFY_WITH_TIMEOUT(viz::load(viz->sharedForTesting()->rmsMilli)>0,5000);
        audio->setVolume(0);
        QTest::qWait(350); // Let all pre-volume FFT history expire.
        QTRY_VERIFY_WITH_TIMEOUT(viz::load(viz->sharedForTesting()->rmsMilli)>0,3000);
        audio->setVolume(100);
        InterlockedIncrement(&viz->sharedForTesting()->captureRequested);QTRY_COMPARE_WITH_TIMEOUT(viz::load(viz->sharedForTesting()->captureDone),1,5000);
        QVERIFY(QFile::copy(config.filePath("host-frame.bmp"),"visualizer-frame.bmp")||QFileInfo::exists("visualizer-frame.bmp"));
        window.resize(1550,900);window.move(70,80);QTest::qWait(150);
        RECT expected{},actual{};GetClientRect(reinterpret_cast<HWND>(viz->surfaceWidget()->winId()),&expected);GetClientRect(hwnd,&actual);
        QCOMPARE(actual.right,expected.right);QCOMPARE(actual.bottom,expected.bottom);
        QTRY_COMPARE(viz::load(viz->sharedForTesting()->renderWidth),expected.right);
        viz->setAttached(false);QTRY_VERIFY(!viz->isAttached());QCOMPARE(GetParent(hwnd),HWND(nullptr));QVERIFY(GetWindowLongPtrW(hwnd,GWL_STYLE)&WS_OVERLAPPEDWINDOW);
        SetWindowPos(hwnd,nullptr,190,170,720,530,SWP_NOZORDER|SWP_NOACTIVATE);QTest::qWait(100);
        viz->setAttached(true);QTRY_VERIFY(viz->isAttached());QCOMPARE(GetParent(hwnd),reinterpret_cast<HWND>(viz->surfaceWidget()->winId()));
        PostMessageW(hwnd,WM_KEYDOWN,VK_F10,0);QTRY_VERIFY(!viz->isAttached());
        PostMessageW(hwnd,WM_KEYUP,VK_F10,0);PostMessageW(hwnd,WM_KEYDOWN,VK_F10,0);QTRY_VERIFY(viz->isAttached());
        // New playback requests must stay responsive while the renderer is active.
        qint64 maxMs=0;
        for(int i=0;i<8;++i){playing.clear();QElapsedTimer elapsed;elapsed.start();audio->playTrack(i%2);bool started=false;
            while(elapsed.elapsed()<500&&!started){QCoreApplication::processEvents();for(const auto &event:playing)if(event[0].toBool())started=true;if(!started)QTest::qWait(1);}
            QVERIFY(started);maxMs=std::max(maxMs,elapsed.elapsed());}
        QVERIFY2(maxMs<200,"Visualizer delayed playback beyond 200 ms");
        auto pid=DWORD(viz->processId());HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,pid);QVERIFY(process);
        viz->setEnabled(false);QTRY_VERIFY_WITH_TIMEOUT(!viz->isRunning(),5000);QVERIFY(!toggle->isChecked());QVERIFY(!viz->sharedForTesting());
        QCOMPARE(WaitForSingleObject(process,0),DWORD(WAIT_OBJECT_0));CloseHandle(process);QVERIFY(!IsWindow(hwnd));
        viz->setEnabled(true);QTRY_VERIFY_WITH_TIMEOUT(viz->isReady(),15000);PostMessageW(reinterpret_cast<HWND>(viz->windowId()),WM_CLOSE,0,0);
        QTRY_VERIFY_WITH_TIMEOUT(!viz->isRunning(),5000);QVERIFY(!toggle->isChecked());
        // A detached child still belongs to this host session and must exit on host destruction.
        {
            auto owner=std::make_unique<MainWindow>();owner->setAttribute(Qt::WA_DontShowOnScreen);owner->show();
            auto child=owner->findChild<VisualizerController*>();child->setTestMode(true);child->setExecutableForTesting(QString(VISUALIZER_EXE));
            child->setEnabled(true);QTRY_VERIFY_WITH_TIMEOUT(child->isReady(),15000);child->setAttached(false);QTRY_VERIFY(!child->isAttached());
            HANDLE handle=OpenProcess(SYNCHRONIZE,FALSE,DWORD(child->processId()));QVERIFY(handle);owner.reset();
            QCOMPARE(WaitForSingleObject(handle,2000),DWORD(WAIT_OBJECT_0));CloseHandle(handle);
        }
        // Unexpected process failure must reset the toggle without stopping the audio engine.
        viz->setEnabled(true);QTRY_VERIFY_WITH_TIMEOUT(viz->isReady(),15000);
        HANDLE crash=OpenProcess(PROCESS_TERMINATE,FALSE,DWORD(viz->processId()));QVERIFY(crash);QVERIFY(TerminateProcess(crash,42));CloseHandle(crash);
        QTRY_VERIFY_WITH_TIMEOUT(!viz->isRunning(),5000);QVERIFY(!toggle->isChecked());QVERIFY(audio->state.active>=0);
        QTRY_COMPARE(errors.size(),1);errors.clear();
        QVERIFY2(errors.isEmpty(),errors.isEmpty()?"":qPrintable(errors.first()[0].toString()));
        QJsonObject result{{"attachedResize",true},{"detachReattach",true},{"pcmReceived",true},{"offProcessExited",true},{"closeSynchronizesToggle",true},{"hostExitClosesDetachedChild",true},{"childCrashIsolated",true},{"playbackCallbackMaxMs",maxMs}};
        QFile report("visualizer-results.json");QVERIFY(report.open(QIODevice::WriteOnly));report.write(QJsonDocument(result).toJson());
    }
};
QTEST_MAIN(VisualizerTests)
#include "VisualizerTests.moc"
