#include "CompressedAudio.h"
#include "AudioRepository.h"
#include "AudioEngine.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <cmath>

static CancelFlag token(){return std::make_shared<std::atomic_bool>(false);}
class AudioTests:public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){QStandardPaths::setTestModeEnabled(true);}
    void seekInMemory_data(){QTest::addColumn<QString>("ext");for(const char *e:{"mp3","wav","flac","ogg","m4a","aac","opus","vbr"})QTest::newRow(e)<<QString(e);}
    void seekInMemory(){
        QFETCH(QString,ext);QTemporaryDir dir;const auto path=dir.filePath("audio."+ext);
        QVERIFY(QFile::copy(QString(FIXTURE_DIR)+(ext=="vbr"?"/seek-vbr.mp3":"/tone."+ext),path));
        auto asset=CompressedAudio::prepare(path,token());QVERIFY2(asset->waveform.error.isEmpty(),qPrintable(asset->waveform.error));
        QVERIFY(QFile::remove(path)); // Every later operation MUST work without the source file.
        PacketDecoder decoder(asset);decoder.seek(0);std::vector<float> reference,block;double start=0;
        while(decoder.next(block,start))reference.insert(reference.end(),block.begin(),block.end());
        QVERIFY(reference.size()>80000);
        for(double fraction:{0.1,0.73,0.25,0.9,0.0}){
            const double requested=fraction*asset->durationSeconds;decoder.seek(requested);
            QVERIFY(decoder.next(block,start));QVERIFY(std::abs(start-requested)<0.003);
            const size_t offset=size_t(std::llround(start*48000))*2;
            const size_t count=std::min({block.size(),reference.size()-std::min(offset,reference.size()),size_t(256)});
            QVERIFY(count>0);double error=0;
            for(size_t i=0;i<count;++i)error+=std::abs(block[i]-reference[offset+i]);
            QVERIFY2(error/count<0.003,qPrintable(QString("%1 seek %2 error %3").arg(ext).arg(requested).arg(error/count)));
        }
    }
    void cancellationAndBudget(){QTemporaryDir dir;auto file=dir.filePath("tone.mp3");QVERIFY(QFile::copy(QString(FIXTURE_DIR)+"/tone.mp3",file));auto cancelled=token();cancelled->store(true);QCOMPARE(CompressedAudio::prepare(file,cancelled)->memoryBytes,0);QVERIFY(!CompressedAudio::prepare(file,token(),4)->waveform.error.isEmpty());}
    void wholeTrackEviction(){
        QTemporaryDir dir;for(int i=0;i<3;++i)QVERIFY(QFile::copy(QString(FIXTURE_DIR)+"/tone.mp3",dir.filePath(QString("%1.mp3").arg(i))));
        TrackModel model;auto repo=AudioRepository::forModel(&model);model.load(dir.path());
        QTRY_COMPARE_WITH_TIMEOUT(model.tracks.size(),3,5000);QTRY_VERIFY_WITH_TIMEOUT(model.tracks[0].audioReady&&model.tracks[1].audioReady&&model.tracks[2].audioReady,10000);
        auto first=repo->get(0);const auto size=first->memoryBytes;repo->pin(0);first.reset();repo->setBudgetForTesting(size+64);
        QVERIFY(repo->get(0));QVERIFY(!repo->get(1));QVERIFY(!repo->get(2));QVERIFY(repo->usedBytes()<=size+64);
        QVERIFY(!model.tracks[1].wave.isEmpty());repo->pin(-1);repo->setBudgetForTesting(0);QCOMPARE(repo->usedBytes(),0);
    }
    void latestRequestWins(){
        auto asset=CompressedAudio::prepare(QString(FIXTURE_DIR)+"/tone.mp3",token());QVERIFY(asset->waveform.error.isEmpty());
        QAudioFormat format;format.setSampleRate(48000);format.setChannelCount(2);format.setSampleFormat(QAudioFormat::Float);AudioEngine engine(format);
        for(int i=0;i<500;++i)engine.request(asset,(i%90)/100.0,false);
        engine.request(asset,0.3,false);QByteArray buffer(3840,0);QElapsedTimer wait;wait.start();
        while(!engine.hasStarted()&&wait.elapsed()<200){engine.read(buffer.data(),buffer.size());QTest::qSleep(1);}
        QVERIFY(engine.hasStarted());QVERIFY(engine.position()>=0.3&&engine.position()<0.34);
        engine.stopPlayback();engine.read(buffer.data(),buffer.size());QVERIFY(!engine.hasStarted());QCOMPARE(buffer,QByteArray(buffer.size(),0));
    }
    void hundredTracksLatency(){
        QTemporaryDir dir;const auto original=QString(FIXTURE_DIR)+"/benchmark60.mp3";
        QVERIFY2(QFileInfo::exists(original),"Generate benchmark60.mp3 using scripts/generate-fixtures.ps1");
        for(int i=0;i<100;++i)QVERIFY(QFile::copy(original,dir.filePath(QString("track%1.mp3").arg(i))));
        TrackModel model;auto repo=AudioRepository::forModel(&model);QElapsedTimer preparation;preparation.start();model.load(dir.path());
        QTRY_COMPARE_WITH_TIMEOUT(model.tracks.size(),100,5000);
        QTRY_VERIFY_WITH_TIMEOUT(std::all_of(model.tracks.begin(),model.tracks.end(),[](const Track&t){return t.analyzed;}),120000);
        const auto loadMs=preparation.elapsed();QVector<AudioAsset> assets;
        for(int i=0;i<100;++i){auto asset=repo->get(i);QVERIFY2(bool(asset),qPrintable(model.tracks[i].error));assets<<asset;}
        QAudioFormat format;format.setSampleRate(48000);format.setChannelCount(2);format.setSampleFormat(QAudioFormat::Float);AudioEngine engine(format);
        QByteArray buffer(3840,0);QVector<double> measurements;
        for(int i=0;i<200;++i){
            const auto &asset=assets[i%100];const double fraction=((i*37)%97)/100.0;
            QElapsedTimer timer;timer.start();engine.request(asset,fraction*asset->durationSeconds,i<100);
            while(!engine.hasStarted()&&timer.elapsed()<500){engine.read(buffer.data(),buffer.size());if(!engine.hasStarted())QTest::qSleep(1);}
            QVERIFY2(engine.hasStarted(),qPrintable(engine.takeError()));measurements<<timer.nsecsElapsed()/1e6;
        }
        std::sort(measurements.begin(),measurements.end());
        QJsonObject report{{"tracks",100},{"trackSeconds",60},{"preparationMs",double(loadMs)},{"compressedRamMB",repo->usedBytes()/1e6},{"softwareFirstPcmMedianMs",measurements[100]},{"softwareFirstPcmP95Ms",measurements[190]},{"softwareFirstPcmMaxMs",measurements.last()}};
        QFile output("audio-benchmark.json");QVERIFY(output.open(QIODevice::WriteOnly));output.write(QJsonDocument(report).toJson());
        qInfo().noquote()<<QJsonDocument(report).toJson(QJsonDocument::Compact);
        QVERIFY2(measurements.last()<200,"Prepared-track decode-to-PCM exceeded 200ms; not a hardware latency measurement.");
    }
};
QTEST_GUILESS_MAIN(AudioTests)
#include "AudioTests.moc"
