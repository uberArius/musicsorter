#include "MainWindow.h"
#include "TrackGridView.h"
#include "DirectoryTree.h"
#include "AudioController.h"
#include "AudioRepository.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QScrollBar>
#include <QDataStream>
#include <QFile>
#include <QStandardPaths>
#include <QFontDatabase>
#include <QSlider>
#include "WindowsIntegration.h"
#include "SingleInstance.h"
#include <QSettings>
#include <QUuid>
#include <QtConcurrent>
#include <QProcess>

static void silentWave(const QString &path){QFile f(path);if(!f.open(QIODevice::WriteOnly))return;QDataStream s(&f);s.setByteOrder(QDataStream::LittleEndian);s.writeRawData("RIFF",4);s<<quint32(36+48000*4*5);s.writeRawData("WAVEfmt ",8);s<<quint32(16)<<quint16(1)<<quint16(2)<<quint32(48000)<<quint32(192000)<<quint16(4)<<quint16(16);s.writeRawData("data",4);s<<quint32(48000*4*5);f.write(QByteArray(48000*4*5,0));}
class UiTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){QStandardPaths::setTestModeEnabled(true);qputenv("QT_MEDIA_BACKEND","ffmpeg");QDir().mkpath("test-output");QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");QFontDatabase::addApplicationFont("C:/Windows/Fonts/seguisym.ttf");}
    void virtualizedGrid(){TrackModel model;for(int i=0;i<5000;++i){Track t;t.name=QString("Żółć — utwór %1 z bardzo długą nazwą").arg(i+1);t.duration=185000;t.wave=QVector<float>(300,0.6f);model.tracks<<t;}TrackGridView grid(&model);grid.resize(1100,600);grid.show();QTest::qWait(50);for(int columns:{1,4,8}){grid.setColumns(columns);QVERIFY(grid.visibleIndices().size()<150);QCOMPARE(grid.findChildren<QWidget*>().size()<10,true);for(int index:{0,3,10}){auto r=grid.card(index);QCOMPARE(grid.at(r.center()),index);}grid.grab().save(QString("test-output/grid-x%1.png").arg(columns));grid.verticalScrollBar()->setValue(grid.verticalScrollBar()->maximum());QVERIFY(grid.visibleIndices().contains(4999));grid.verticalScrollBar()->setValue(0);}QElapsedTimer timer;timer.start();for(int i=0;i<100;++i){grid.verticalScrollBar()->setValue(i*44);grid.viewport()->repaint();}QVERIFY2(timer.elapsed()<5000,qPrintable(QString("100 scroll repaints: %1 ms").arg(timer.elapsed())));}
    void audioSeekingAndLock(){
        QTemporaryDir dir;for(const auto name:{"a.wav","b.wav","c.wav"})silentWave(dir.filePath(name));
        TrackModel model;AudioController audio(&model);model.load(dir.path());
        QTRY_COMPARE(model.tracks.size(),3);QTRY_VERIFY(model.tracks[0].audioReady&&model.tracks[1].audioReady&&model.tracks[2].audioReady);
        QSignalSpy error(&audio,&AudioController::error);QSignalSpy progress(&audio,&AudioController::progress);
        audio.setVolume(37);QCOMPARE(audio.volume(),37);audio.setVolume(0);QCOMPARE(audio.volume(),0);audio.setVolume(100);QCOMPARE(audio.volume(),100);
        audio.hover(0,0.8);QTRY_VERIFY(!progress.isEmpty()&&progress.last()[0].toDouble()>0.75);
        audio.click(0,0.1);QCOMPARE(audio.state.locked,0);audio.hover(1,0.9);audio.leave();QCOMPARE(audio.state.active,0);
        QTRY_COMPARE_WITH_TIMEOUT(audio.state.active,1,3000);QCOMPARE(audio.state.locked,1);
        audio.previous();QCOMPARE(audio.state.active,0);QCOMPARE(audio.state.locked,0);
        model.tracks[1].sorted=true;audio.next();QCOMPARE(audio.state.active,2);QCOMPARE(audio.state.locked,2);
        audio.togglePlayback();QCOMPARE(audio.state.active,-1);audio.togglePlayback();QCOMPARE(audio.state.active,2);QCOMPARE(audio.state.locked,2);
        audio.click(0,0.5);QCOMPARE(audio.state.active,0);QCOMPARE(audio.state.locked,0);
        audio.click(2,0.85);QCOMPARE(audio.state.locked,2);
        QTRY_COMPARE_WITH_TIMEOUT(audio.state.active,-1,3000);QCOMPARE(audio.state.locked,-1);
        audio.togglePlayback();audio.click(2,0);QCOMPARE(audio.state.locked,-1);audio.leave();QCOMPARE(audio.state.active,-1);
        QVERIFY2(error.isEmpty(),error.isEmpty()?"":qPrintable(error.first()[0].toString()));
    }
    void mainWindowSmoke(){QTemporaryDir dir;silentWave(dir.filePath("Muzyka Łódź.wav"));MainWindow window;window.show();auto tree=window.findChild<DirectoryTree*>();QVERIFY(tree);emit tree->directorySelected(dir.path());auto model=window.findChild<TrackModel*>();QTRY_COMPARE_WITH_TIMEOUT(model->tracks.size(),1,5000);QTRY_VERIFY_WITH_TIMEOUT(model->tracks[0].analyzed,10000);QCOMPARE(model->tracks[0].wave.size(),300);window.grab().save("test-output/main-window.png");
        auto top=window.findChild<QFrame*>("topbar");auto volume=window.findChild<QSlider*>("volume");auto play=window.findChild<QPushButton*>("playStop");
        QVERIFY(volume&&play&&top);QCOMPARE(volume->parentWidget(),top);QCOMPARE(play->parentWidget(),top);
        volume->setValue(42);QCOMPARE(window.findChild<AudioController*>()->volume(),42);
        QTest::mouseClick(play,Qt::LeftButton);QCOMPARE(model->tracks.size(),1);QCOMPARE(window.findChild<AudioController*>()->state.locked,0);
        QTest::mouseClick(play,Qt::LeftButton);QCOMPARE(window.findChild<AudioController*>()->state.active,-1);
        window.resize(860,450);QTest::qWait(30);window.grab().save("test-output/main-window-small.png");
        QVERIFY(top->width()<=window.width());QVERIFY(volume->geometry().right()<top->width());auto grid=window.findChild<TrackGridView*>();QVERIFY(grid);QSignalSpy sort(grid,&TrackGridView::sortTrack);QTest::mouseClick(grid->viewport(),Qt::RightButton,Qt::NoModifier,grid->card(0).center());QCOMPARE(sort.size(),1);window.close();}
    void explorerOpen(){
        QTemporaryDir dir,other;silentWave(dir.filePath("a.wav"));silentWave(dir.filePath("Żółć z odstępami.wav"));silentWave(other.filePath("inny.wav"));
        MainWindow window;window.show();auto model=window.findChild<TrackModel*>();auto audio=window.findChild<AudioController*>();
        window.openFile(dir.filePath("Żółć z odstępami.wav"));
        QTRY_COMPARE(model->tracks.size(),2);QTRY_COMPARE(audio->state.locked,1);QTRY_VERIFY(model->tracks[1].audioReady);
        auto tree=window.findChild<DirectoryTree*>();auto fs=static_cast<QFileSystemModel*>(tree->model());
        QTRY_COMPARE(QDir::cleanPath(fs->filePath(tree->currentIndex())),QDir::cleanPath(dir.path()));
        const auto generation=model->generation;window.openFile(dir.filePath("a.wav"));QCOMPARE(audio->state.locked,0);QCOMPARE(model->generation,generation);
        window.openFile(other.filePath("inny.wav"));window.openFile(dir.filePath("Żółć z odstępami.wav"));
        QTRY_COMPARE(model->tracks.size(),2);QTRY_COMPARE(audio->state.locked,1);
        QCOMPARE(model->tracks[1].path,dir.filePath("Żółć z odstępami.wav"));
        window.openDirectory(other.path());QTRY_COMPARE(model->tracks.size(),1);QCOMPARE(audio->state.active,-1);
    }
    void registration(){
        QTemporaryDir dir;QSettings settings(dir.filePath("registry.ini"),QSettings::IniFormat);
        const QString exe="C:/Aplikacje Łódź/MusicSorter.exe";
        settings.setValue("Software/Classes/.mp3/.","ExistingPlayer");
        QVERIFY(WindowsIntegration::writeRegistration(settings,exe));
        QCOMPARE(settings.value("Software/Classes/.mp3/.").toString(),"ExistingPlayer");
        QCOMPARE(settings.value("Software/RegisteredApplications/MusicSorter").toString(),QString("Software\\MusicSorter\\Capabilities"));
        for(const auto ext:{"mp3","wav","flac","ogg","m4a","aac","opus"}){
            const auto id=settings.value(QString("Software/MusicSorter/Capabilities/FileAssociations/.")+ext).toString();
            QCOMPARE(settings.value("Software/Classes/"+id+"/shell/open/command/.").toString(),"\""+QDir::toNativeSeparators(exe)+"\" -- \"%1\"");
        }
        QVERIFY(WindowsIntegration::writeRegistration(settings,"D:/New/MusicSorter.exe"));
        QVERIFY(settings.value("Software/Classes/MusicSorter.Audio.mp3/shell/open/command/.").toString().contains("D:\\New"));
        for(const auto &key:settings.allKeys())QVERIFY(!key.contains("UserChoice"));
    }
    void singleInstance(){
        const auto name="MusicSorter-test-"+QUuid::createUuid().toString(QUuid::Id128);
        {
            SingleInstance first(name);QString error;QCOMPARE(first.start({}, {}, &error),SingleInstance::Primary);
            QSignalSpy requests(&first,&SingleInstance::requested);
            QProcess client;client.start(QCoreApplication::applicationDirPath()+"/MusicSorterInstanceProbe.exe",{name,"C:/Muzyka/Żółć z odstępami.mp3"});
            QTRY_COMPARE_WITH_TIMEOUT(client.state(),QProcess::NotRunning,10000);QCOMPARE(client.exitCode(),0);QCOMPARE(client.exitStatus(),QProcess::NormalExit);
            QCOMPARE(requests.size(),1);QCOMPARE(requests[0][0].toString(),"C:/Muzyka/Żółć z odstępami.mp3");
        }
        SingleInstance restarted(name);QString error;QCOMPARE(restarted.start({}, {}, &error),SingleInstance::Primary);
    }
    void preparedOutputLatency(){
        QTemporaryDir dir;silentWave(dir.filePath("a.wav"));silentWave(dir.filePath("b.wav"));
        TrackModel model;AudioController audio(&model);model.load(dir.path());
        QTRY_COMPARE(model.tracks.size(),2);QTRY_VERIFY(model.tracks[0].audioReady&&model.tracks[1].audioReady);
        QSignalSpy playing(&audio,&AudioController::playing);QVector<qint64> times;
        for(int i=0;i<10;++i){
            playing.clear();QElapsedTimer elapsed;elapsed.start();audio.hover(i%2,0.2+0.04*i);
            bool started=false;
            while(!started&&elapsed.elapsed()<500){QCoreApplication::processEvents();for(const auto &event:playing)if(event[0].toBool())started=true;if(!started)QTest::qWait(1);}
            QVERIFY(started);times<<elapsed.elapsed();
        }
        const auto maximum=*std::max_element(times.begin(),times.end());
        qInfo()<<"Prepared preview to audio callback + GUI notification, max ms:"<<maximum<<"actual output buffer ms:"<<audio.outputBufferMilliseconds();
        QVERIFY2(maximum<200,"Prepared preview exceeded 200ms before GUI playback notification.");audio.stop();
    }
};
QTEST_MAIN(UiTests)
#include "UiTests.moc"
