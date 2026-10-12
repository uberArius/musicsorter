#include "CompressedAudio.h"
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <algorithm>
#include <cmath>
#include <stdexcept>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}
namespace {
void check(int code) {
    if (code >= 0) return;
    char message[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(code, message, sizeof(message));
    throw std::runtime_error(message);
}
struct PacketDelete { void operator()(AVPacket *p) const { av_packet_free(&p); } };
using Packet = std::unique_ptr<AVPacket, PacketDelete>;
int readFile(void *opaque, uint8_t *buffer, int size) {
    auto &file = *static_cast<QFile *>(opaque);
    const auto count = file.read(reinterpret_cast<char *>(buffer), size);
    return count > 0 ? int(count) : (count == 0 ? AVERROR_EOF : AVERROR(EIO));
}
int64_t seekFile(void *opaque, int64_t offset, int whence) {
    auto &file = *static_cast<QFile *>(opaque);
    if (whence == AVSEEK_SIZE) return file.size();
    whence &= ~AVSEEK_FORCE;
    qint64 position = offset;
    if (whence == SEEK_CUR) position += file.pos();
    if (whence == SEEK_END) position += file.size();
    return file.seek(position) ? file.pos() : AVERROR(EIO);
}
}
struct CompressedAudio::Impl {
    AVCodecParameters *parameters = avcodec_parameters_alloc();
    AVRational timeBase{1,48000};
    double origin = 0;
    std::vector<Packet> packets;
    std::vector<double> times;
    ~Impl() { avcodec_parameters_free(&parameters); }
};
CompressedAudio::CompressedAudio() : impl(std::make_unique<Impl>()) {}
CompressedAudio::~CompressedAudio() = default;

struct PacketDecoder::Impl {
    AudioAsset asset;
    AVCodecContext *codec = nullptr;
    AVFrame *frame = av_frame_alloc();
    SwrContext *resampler = nullptr;
    size_t packet = 0;
    int rate, channels;
    bool flushed = false, drained = false;
    double target = 0, fallbackTime = 0;
    Impl(AudioAsset a, int r, int c) : asset(std::move(a)), rate(r), channels(c) {}
    ~Impl() { swr_free(&resampler); av_frame_free(&frame); avcodec_free_context(&codec); }
    void initResampler() {
        if (resampler) return;
        AVChannelLayout out{};
        av_channel_layout_default(&out, channels);
        int result = swr_alloc_set_opts2(&resampler, &out, AV_SAMPLE_FMT_FLT, rate,
                                        &codec->ch_layout, codec->sample_fmt, codec->sample_rate, 0, nullptr);
        av_channel_layout_uninit(&out);
        check(result); check(swr_init(resampler));
    }
};
PacketDecoder::PacketDecoder(AudioAsset asset, int rate, int channels)
    : impl(std::make_unique<Impl>(std::move(asset),rate,channels)) {
    auto &d = *impl;
    auto decoder = avcodec_find_decoder(d.asset->impl->parameters->codec_id);
    if (!decoder) throw std::runtime_error("No audio decoder is available.");
    d.codec = avcodec_alloc_context3(decoder);
    if (!d.codec || !d.frame) throw std::bad_alloc();
    check(avcodec_parameters_to_context(d.codec,d.asset->impl->parameters));
    d.codec->pkt_timebase = d.asset->impl->timeBase;
    d.codec->thread_count = 1;
    check(avcodec_open2(d.codec,decoder,nullptr));
}
PacketDecoder::~PacketDecoder() = default;
void PacketDecoder::seek(double seconds) {
    auto &d = *impl;
    d.target = std::max(0.0,seconds);
    const auto &times = d.asset->impl->times;
    // MP3 bit reservoir and transform overlap need preroll, not a full-song decode.
    auto found = std::lower_bound(times.begin(),times.end(),std::max(0.0,d.target-0.5));
    d.packet = found == times.begin() ? 0 : size_t(found-times.begin()-1);
    d.fallbackTime = times.empty() ? 0 : times[d.packet];
    d.flushed = d.drained = false;
    avcodec_flush_buffers(d.codec);
    swr_free(&d.resampler);
}
bool PacketDecoder::next(std::vector<float> &samples, double &startSeconds) {
    auto &d = *impl;
    for (;;) {
        const int result = avcodec_receive_frame(d.codec,d.frame);
        if (result == AVERROR_EOF) {
            if (d.drained || !d.resampler) return false;
            d.drained = true;
            samples.resize(size_t(std::max(1,swr_get_out_samples(d.resampler,0)))*d.channels);
            auto *out = reinterpret_cast<uint8_t *>(samples.data());
            const int frames = swr_convert(d.resampler,&out,int(samples.size()/d.channels),nullptr,0);
            check(frames); samples.resize(size_t(frames)*d.channels);
            startSeconds = d.fallbackTime;
            return frames > 0;
        }
        if (result == AVERROR(EAGAIN)) {
            if (d.packet < d.asset->impl->packets.size()) {
                check(avcodec_send_packet(d.codec,d.asset->impl->packets[d.packet++].get()));
            } else if (!d.flushed) {
                check(avcodec_send_packet(d.codec,nullptr));d.flushed=true;
            } else return false;
            continue;
        }
        check(result);
        d.initResampler();
        double time = d.frame->best_effort_timestamp == AV_NOPTS_VALUE ? d.fallbackTime
            : double(d.frame->best_effort_timestamp)*av_q2d(d.asset->impl->timeBase)-d.asset->impl->origin;
        if(d.frame->best_effort_timestamp!=AV_NOPTS_VALUE)
            time-=double(swr_get_delay(d.resampler,d.codec->sample_rate))/d.codec->sample_rate;
        const int capacity = swr_get_out_samples(d.resampler,d.frame->nb_samples);
        samples.resize(size_t(std::max(1,capacity))*d.channels);
        auto *out = reinterpret_cast<uint8_t *>(samples.data());
        const int frames = swr_convert(d.resampler,&out,capacity,
                                       const_cast<const uint8_t **>(d.frame->extended_data),d.frame->nb_samples);
        check(frames);av_frame_unref(d.frame);
        samples.resize(size_t(frames)*d.channels);
        d.fallbackTime = time+double(frames)/d.rate;
        if (d.fallbackTime <= d.target || frames == 0) continue;
        const int skip = std::clamp(int(std::ceil((d.target-time)*d.rate)),0,frames);
        if (skip) samples.erase(samples.begin(),samples.begin()+size_t(skip)*d.channels);
        startSeconds = time+double(skip)/d.rate;
        return !samples.empty();
    }
}
std::shared_ptr<CompressedAudio> CompressedAudio::prepare(const QString &path,const CancelFlag &cancel,qint64 maxBytes) {
    auto asset = std::make_shared<CompressedAudio>();
    try {
        if (cancel->load()) return asset;
        const auto key = WaveformCache::key(path);
        asset->waveform = WaveformCache::read(key);
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error(file.errorString().toStdString());
        if (file.size() > maxBytes) throw std::runtime_error("Not enough available RAM for the entire file.");
        AVFormatContext *format = avformat_alloc_context();
        auto *buffer = static_cast<unsigned char *>(av_malloc(64*1024));
        AVIOContext *io = avio_alloc_context(buffer,64*1024,0,&file,readFile,nullptr,seekFile);
        if (!format || !io) { av_free(buffer); if(format)avformat_close_input(&format);throw std::bad_alloc(); }
        auto cleanup = qScopeGuard([&]{avformat_close_input(&format);av_free(io->buffer);avio_context_free(&io);});
        format->pb = io;
        format->flags |= AVFMT_FLAG_CUSTOM_IO;
        format->interrupt_callback = {[](void *opaque)->int{return static_cast<std::atomic_bool *>(opaque)->load()?1:0;},cancel.get()};
        check(avformat_open_input(&format,nullptr,nullptr,nullptr));
        check(avformat_find_stream_info(format,nullptr));
        int stream = av_find_best_stream(format,AVMEDIA_TYPE_AUDIO,-1,-1,nullptr,0);check(stream);
        auto *st = format->streams[stream];
        check(avcodec_parameters_copy(asset->impl->parameters,st->codecpar));
        asset->impl->timeBase=st->time_base;
        asset->impl->origin=st->start_time==AV_NOPTS_VALUE?0:st->start_time*av_q2d(st->time_base);
        Packet packet(av_packet_alloc());
        double nextTime=0;
        for (;;) {
            if(cancel->load())return std::make_shared<CompressedAudio>();
            const int result=av_read_frame(format,packet.get());
            if(result==AVERROR_EOF)break;check(result);
            if(packet->stream_index==stream){
                double time=packet->pts==AV_NOPTS_VALUE?nextTime:packet->pts*av_q2d(st->time_base)-asset->impl->origin;
                asset->impl->times.push_back(time);
                nextTime=time+packet->duration*av_q2d(st->time_base);
                asset->memoryBytes+=packet->size+sizeof(AVPacket)+sizeof(double)+64;
                for(int n=0;n<packet->side_data_elems;++n)asset->memoryBytes+=packet->side_data[n].size;
                if(asset->memoryBytes>maxBytes)throw std::runtime_error("Not enough available RAM for the entire track.");
                Packet copy(av_packet_clone(packet.get()));if(!copy)throw std::bad_alloc();
                asset->impl->packets.push_back(std::move(copy));
            }
            av_packet_unref(packet.get());
        }
        if(asset->impl->packets.empty())throw std::runtime_error("The file contains no audio samples.");
        // Vorbis and some other codecs have leading overlap without emitted PCM.
        // Normalize the audible timeline so sample zero means the first output sample.
        {
            PacketDecoder probe(asset);probe.seek(0);
            std::vector<float> first;double firstTime=0;
            if(probe.next(first,firstTime)&&firstTime>0){
                asset->impl->origin+=firstTime;
                for(auto &time:asset->impl->times)time-=firstTime;
                nextTime-=firstTime;
            }
        }
        asset->durationSeconds=asset->waveform.duration>0?asset->waveform.duration/1000.0:std::max(0.0,nextTime);
        if(!asset->waveform.values.isEmpty())return asset;
        // One decode pass. 64-frame amplitude summaries keep analysis memory small.
        struct Block { double sum=0; int count=0; };
        std::vector<Block> blocks;Block block; qint64 total=0;
        PacketDecoder decoder(asset);decoder.seek(0);
        std::vector<float> pcm;double time=0;
        while(!cancel->load()&&decoder.next(pcm,time)){
            for(size_t i=0;i+1<pcm.size();i+=2){
                double value=(std::abs(pcm[i])+std::abs(pcm[i+1]))*0.5;
                block.sum+=std::isfinite(value)?value:0; ++block.count;++total;
                if(block.count==64){blocks.push_back(block);block={};}
            }
        }
        if(cancel->load())return std::make_shared<CompressedAudio>();
        if(block.count)blocks.push_back(block);
        if(!total)throw std::runtime_error("The file contains no audio samples.");
        QVector<double> sums(300,0),counts(300,0);qint64 start=0;
        for(const auto &b:blocks){
            double left=double(start),right=double(start+b.count);
            int first=int(left*300/total),last=std::min(299,int((right-1)*300/total));
            for(int bin=first;bin<=last;++bin){
                double overlap=std::max(0.0,std::min(right,double(bin+1)*total/300)-std::max(left,double(bin)*total/300));
                sums[bin]+=b.sum/b.count*overlap;counts[bin]+=overlap;
            }
            start+=b.count;
        }
        asset->waveform.values.resize(300);float peak=0;
        for(int i=0;i<300;++i){asset->waveform.values[i]=counts[i]>0?float(sums[i]/counts[i]):0;peak=std::max(peak,asset->waveform.values[i]);}
        if(peak>0)for(auto &v:asset->waveform.values)v/=peak;
        asset->durationSeconds=double(total)/48000;
        asset->waveform.duration=std::max<qint64>(1,qRound64(asset->durationSeconds*1000));
        if(key==WaveformCache::key(path))WaveformCache::write(key,asset->waveform);
    } catch(const std::exception &e) {
        asset->waveform.error=QString::fromUtf8(e.what());
        asset->impl->packets.clear();asset->memoryBytes=0;
    }
    return asset;
}
