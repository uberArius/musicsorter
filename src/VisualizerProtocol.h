// MusicSorter / Spectral Grid local integration protocol v1.
// SPDX-License-Identifier: Apache-2.0
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <algorithm>
namespace viz {
constexpr uint32_t Magic=0x53475631, Version=1, Slots=64, Samples=512;
struct Block { double time; uint32_t count,rate; uint64_t epoch; float samples[Samples]; };
// Only Windows x64, identical layout in both executables. Interlocked operations
// supply publication/acquisition barriers; payload slots have exactly one owner.
struct alignas(64) Shared {
    uint32_t magic,version,bytes,hostPid;
    uint64_t parentWindow;
    alignas(64) volatile LONG writeIndex,readIndex,dropped;
    volatile LONG closeRequested,attachRequested,suspended,ready,attached;
    volatile LONG error,testMode,captureRequested,captureDone,renderWidth,renderHeight,rmsMilli;
    alignas(8) volatile LONG64 renderedFrames;
    alignas(8) volatile LONG64 childWindow;
    alignas(8) volatile LONG64 consumedBlocks;
    Block blocks[Slots];
};
inline LONG load(volatile LONG &value){return InterlockedCompareExchange(&value,0,0);}
inline LONG64 load64(volatile LONG64 &value){return InterlockedCompareExchange64(&value,0,0);}
inline double seconds(){LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return double(t.QuadPart)/double(f.QuadPart);}
inline bool push(Shared *s,const float *mono,uint32_t count,uint32_t rate,double time,uint64_t epoch){
    const auto w=load(s->writeIndex);const auto next=(w+1)%Slots;
    if(next==uint32_t(load(s->readIndex))){InterlockedIncrement(&s->dropped);return false;}
    auto &b=s->blocks[w];b.time=time;b.count=std::min(count,Samples);b.rate=rate;b.epoch=epoch;
    std::memcpy(b.samples,mono,b.count*sizeof(float));InterlockedExchange(&s->writeIndex,LONG(next));return true;
}
inline bool pop(Shared *s,Block &block){
    const auto r=load(s->readIndex);if(r==load(s->writeIndex))return false;
    block=s->blocks[r];InterlockedExchange(&s->readIndex,(r+1)%Slots);InterlockedIncrement64(&s->consumedBlocks);
    return block.count<=Samples&&block.rate>=8000&&block.rate<=384000;
}
class Mapping {
public:
    HANDLE handle=nullptr;Shared *data=nullptr;
    ~Mapping(){reset();}
    Mapping()=default;Mapping(const Mapping&)=delete;Mapping& operator=(const Mapping&)=delete;
    void reset(){if(data)UnmapViewOfFile(data);if(handle)CloseHandle(handle);data=nullptr;handle=nullptr;}
    bool create(const wchar_t *name){reset();handle=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Shared),name);if(!handle)return false;if(GetLastError()==ERROR_ALREADY_EXISTS){reset();return false;}return map(true);}
    bool open(const wchar_t *name){reset();handle=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name);return handle&&map(false);}
private:
    bool map(bool init){data=static_cast<Shared*>(MapViewOfFile(handle,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));if(!data){reset();return false;}
        if(init){std::memset(data,0,sizeof(Shared));data->magic=Magic;data->version=Version;data->bytes=sizeof(Shared);}
        if(data->magic!=Magic||data->version!=Version||data->bytes!=sizeof(Shared)){reset();return false;}return true;}
};
static_assert(sizeof(void*)==8);
}
