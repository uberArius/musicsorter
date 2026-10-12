#include "AudioEngine.h"
#include "VisualizerProtocol.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#define NOMINMAX
#include <windows.h>

AudioEngine::AudioEngine(const QAudioFormat &f,QObject *parent):QIODevice(parent),format(f),ring(size_t(f.sampleRate()*f.channelCount()/5)){
    open(QIODevice::ReadOnly|QIODevice::Unbuffered);
    worker=std::thread([this]{run();});
    SetThreadPriority(worker.native_handle(),THREAD_PRIORITY_ABOVE_NORMAL);
}
AudioEngine::~AudioEngine(){quitting.store(true);wake.notify_all();worker.join();}
void AudioEngine::request(AudioAsset asset,double seconds,bool fade){
    std::scoped_lock lock(commandMutex,ringMutex);
    requested=std::move(asset);requestedPosition=seconds;requestedFade=fade;
    ++serial;head=tail=count=0;playPosition.store(seconds);started.store(false);ended.store(false);failure.clear();wake.notify_all();
}
void AudioEngine::setVisualizer(viz::Shared *shared){std::lock_guard lock(ringMutex);visualizer=shared;}
void AudioEngine::stopPlayback(){request({},0,false);loop.store(false);}
QString AudioEngine::takeError(){std::lock_guard lock(commandMutex);auto message=failure;failure.clear();return message;}
qint64 AudioEngine::bytesAvailable()const{return format.bytesForDuration(20000)+QIODevice::bytesAvailable();}
qint64 AudioEngine::readData(char *data,qint64 maxLength){
    const int sampleBytes=format.bytesPerSample();
    if(sampleBytes<=0)return 0;
    const size_t samples=size_t(maxLength/sampleBytes);
    std::memset(data,format.sampleFormat()==QAudioFormat::UInt8?128:0,samples*sampleBytes);
    std::unique_lock lock(ringMutex,std::try_to_lock);
    if(!lock.owns_lock())return qint64(samples*sampleBytes);
    const size_t available=std::min(samples,count);
    if(visualizer){
        float mono[viz::Samples];const size_t channels=size_t(format.channelCount());
        const size_t frames=available/channels;const double now=viz::seconds();
        for(size_t begin=0;begin<frames;begin+=viz::Samples){
            const auto n=std::min(size_t(viz::Samples),frames-begin);
            for(size_t f=0;f<n;++f){float sum=0;for(size_t c=0;c<channels;++c)sum+=std::clamp(ring[(head+(begin+f)*channels+c)%ring.size()],-1.0f,1.0f);mono[f]=sum/float(channels);}
            viz::push(visualizer,mono,uint32_t(n),uint32_t(format.sampleRate()),now+double(begin)/format.sampleRate(),serial.load());
        }
    }
    for(size_t i=0;i<available;++i){
        const float value=std::clamp(ring[head],-1.0f,1.0f);head=(head+1)%ring.size();
        switch(format.sampleFormat()){
        case QAudioFormat::Float:std::memcpy(data+i*4,&value,4);break;
        case QAudioFormat::Int16:{auto v=qint16(value*32767);std::memcpy(data+i*2,&v,2);break;}
        case QAudioFormat::Int32:{auto v=qint32(double(value)*2147483647);std::memcpy(data+i*4,&v,4);break;}
        case QAudioFormat::UInt8:data[i]=char(std::clamp(int(value*127+128),0,255));break;
        default:break;
        }
    }
    count-=available;
    if(available){started.store(true);playPosition.store(playPosition.load()+double(available)/format.channelCount()/format.sampleRate());}
    wake.notify_one();return qint64(samples*sampleBytes);
}
void AudioEngine::run(){
    AudioAsset asset;std::unique_ptr<PacketDecoder> decoder;
    quint64 current=0;std::vector<float> pcm;size_t offset=0;qint64 fadedFrames=0;
    bool fade=true,eof=false;
    while(!quitting.load()){
        if(serial.load()!=current){
            double position=0;
            {std::lock_guard lock(commandMutex);current=serial.load();if(asset!=requested){asset=requested;decoder.reset();}position=requestedPosition;fade=requestedFade;}
            pcm.clear();offset=0;fadedFrames=0;eof=false;
            if(asset)try{if(!decoder)decoder=std::make_unique<PacketDecoder>(asset,format.sampleRate(),format.channelCount());decoder->seek(position);}
            catch(const std::exception &e){std::lock_guard lock(commandMutex);if(current==serial.load())failure=QString::fromUtf8(e.what());asset.reset();decoder.reset();}
        }
        if(!asset){std::unique_lock lock(commandMutex);wake.wait_for(lock,std::chrono::milliseconds(5));continue;}
        if(eof){
            bool restart=false;
            {std::scoped_lock lock(commandMutex,ringMutex);
                if(count==0&&current==serial.load()){
                    if(loop.load()){playPosition.store(0);restart=true;}
                    else ended.store(true);
                }
            }
            if(restart){decoder->seek(0);eof=false;continue;}
            std::unique_lock lock(commandMutex);wake.wait_for(lock,std::chrono::milliseconds(2));continue;
        }
        size_t free=0;{std::lock_guard lock(ringMutex);free=ring.size()-count;}
        if(!free){std::unique_lock lock(commandMutex);wake.wait_for(lock,std::chrono::milliseconds(2));continue;}
        if(offset>=pcm.size()){
            double time=0;
            try{if(!decoder->next(pcm,time)){eof=true;continue;}}
            catch(const std::exception &e){std::lock_guard lock(commandMutex);if(current==serial.load())failure=QString::fromUtf8(e.what());asset.reset();decoder.reset();continue;}
            offset=0;
            // New-source fade: 25% to 100% in 250ms. Seek uses a 5ms de-click ramp.
            for(size_t i=0;i<pcm.size();++i){
                const double elapsed=double(fadedFrames+qint64(i/format.channelCount()))/format.sampleRate();
                const double gain=fade?0.25+0.75*std::min(1.0,elapsed/0.25):std::min(1.0,elapsed/0.005);
                pcm[i]*=float(gain);
            }
            fadedFrames+=qint64(pcm.size()/format.channelCount());
        }
        {std::lock_guard lock(ringMutex);
            if(current!=serial.load())continue;
            const size_t amount=std::min({ring.size()-count,pcm.size()-offset,size_t(format.sampleRate()*format.channelCount()/100)});
            for(size_t i=0;i<amount;++i){ring[tail]=pcm[offset++];tail=(tail+1)%ring.size();}
            count+=amount;
        }
    }
}
