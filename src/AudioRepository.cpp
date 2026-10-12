#include "AudioRepository.h"
#include <QFileInfo>
#include <QFutureWatcher>
#include <QThread>
#include <QtConcurrent>
#include <algorithm>
#include <limits>
#define NOMINMAX
#include <windows.h>
namespace {
constexpr qint64 MiB=1024*1024,GiB=1024*MiB;
struct Memory { qint64 available=4*GiB,total=8*GiB; };
Memory memory(){MEMORYSTATUSEX info{};info.dwLength=sizeof(info);if(GlobalMemoryStatusEx(&info))return {qint64(info.ullAvailPhys),qint64(info.ullTotalPhys)};return {};}
}
AudioRepository *AudioRepository::forModel(TrackModel *model){
    auto found=model->findChild<AudioRepository*>(QString(),Qt::FindDirectChildrenOnly);
    return found?found:new AudioRepository(model);
}
AudioRepository::AudioRepository(TrackModel *m):QObject(m),model(m){
    concurrency=std::clamp(QThread::idealThreadCount()/2,2,8);
    pool.setMaxThreadCount(concurrency+1);pool.setThreadPriority(QThread::LowPriority);
    auto mem=memory();budget=std::max<qint64>(64*MiB,std::min(mem.total*65/100,mem.available-std::max(2*GiB,mem.total*15/100)));
    cancel=std::make_shared<std::atomic_bool>(false);
    connect(model,&TrackModel::reset,this,&AudioRepository::reset);
    connect(&pressureTimer,&QTimer::timeout,this,[this]{trim();pump();});pressureTimer.start(1000);
    retryTimer.setSingleShot(true);retryTimer.setInterval(20);
    connect(&retryTimer,&QTimer::timeout,this,&AudioRepository::pump);
}
AudioRepository::~AudioRepository(){cancel->store(true);pool.waitForDone();}
void AudioRepository::reset(){
    cancel->store(true);cancel=std::make_shared<std::atomic_bool>(false);
    entries.clear();running.clear();attempted.clear();visible.clear();reserved=0;pinned=urgent=-1;
    emit memoryChanged(0,0);QTimer::singleShot(0,this,&AudioRepository::pump);
}
qint64 AudioRepository::usedBytes()const{qint64 bytes=0;for(const auto &e:entries)bytes+=e.asset->memoryBytes;return bytes;}
AudioAsset AudioRepository::get(int i){auto it=entries.find(i);if(it==entries.end())return {};it->touched=++clock;return it->asset;}
void AudioRepository::pin(int i){pinned=i;}
void AudioRepository::request(int i){if(i<0||i>=model->tracks.size())return;if(entries.contains(i)){urgent=-1;emit prepared(i);return;}urgent=i;pump();}
void AudioRepository::prioritize(const QVector<int>&indices){visible=indices;pump();}
void AudioRepository::setBudgetForTesting(qint64 bytes){testBudget=true;budget=bytes;trim();}
void AudioRepository::trim(){
    const auto mem=memory();const qint64 reserve=std::max(2*GiB,mem.total*15/100);
    if(!testBudget)budget=std::max<qint64>(16*MiB,std::min(mem.total*65/100,mem.available+usedBytes()-reserve));
    qint64 target=budget;
    if(!testBudget&&mem.available<reserve)target=std::max<qint64>(0,usedBytes()-(reserve-mem.available));
    while(usedBytes()+reserved>target&&!entries.isEmpty()){
        int victim=-1;quint64 oldest=std::numeric_limits<quint64>::max();
        for(auto it=entries.begin();it!=entries.end();++it){
            if(it.key()==pinned||it->asset.use_count()>1)continue;
            const quint64 score=model->tracks[it.key()].sorted?0:it->touched+(visible.contains(it.key())?1000000000ULL:0);
            if(score<oldest){oldest=score;victim=it.key();}
        }
        if(victim<0)break;entries.remove(victim);
        if(victim<model->tracks.size()){model->tracks[victim].audioReady=false;emit model->changed(victim);}
    }
    emit memoryChanged(usedBytes(),int(entries.size()));
}
void AudioRepository::pump(){
    if(urgent>=0&&!entries.contains(urgent)&&!running.contains(urgent)&&running.size()<concurrency+1){int i=urgent;if(launch(i))urgent=-1;else if(!retryTimer.isActive())retryTimer.start();}
    while(running.size()<concurrency){
        int next=-1;
        for(int i:visible)if(i>=0&&i<model->tracks.size()&&!attempted.contains(i)&&!running.contains(i)){next=i;break;}
        if(next<0)for(int i=0;i<model->tracks.size();++i)if(!attempted.contains(i)&&!running.contains(i)){next=i;break;}
        if(next<0)break;
        if(!launch(next))break;
    }
    int done=0;for(const auto &t:model->tracks)done+=t.analyzed;
    emit progress(done,int(model->tracks.size()));
}
bool AudioRepository::launch(int i){
    const qint64 allocation=std::min(budget,std::max<qint64>(4*MiB,model->tracks[i].fileBytes*3/2+2*MiB));
    if(reserved+allocation>budget)return false;
    reserved+=allocation;trim();
    if(usedBytes()+reserved>budget){reserved-=allocation;return false;}
    running.insert(i);attempted.insert(i);
    const auto ticket=model->generation;
    auto watcher=new QFutureWatcher<std::shared_ptr<CompressedAudio>>(this);
    connect(watcher,&QFutureWatcher<std::shared_ptr<CompressedAudio>>::finished,this,[this,watcher,i,ticket,allocation]{
        auto asset=watcher->result();watcher->deleteLater();
        if(ticket!=model->generation)return;
        reserved-=allocation;running.remove(i);if(urgent==i)urgent=-1;
        auto &track=model->tracks[i];
        track.wave=asset->waveform.values;track.duration=asset->waveform.duration;
        track.error=asset->waveform.error;track.analyzed=true;
        if(track.error.isEmpty()&&asset->memoryBytes>0){entries.insert(i,{asset,++clock});track.audioReady=true;}
        emit model->changed(i);emit prepared(i);asset.reset();trim();pump();
    });
    watcher->setFuture(QtConcurrent::run(&pool,[path=model->tracks[i].path,token=cancel,allocation]{return CompressedAudio::prepare(path,token,allocation);}));
    return true;
}
