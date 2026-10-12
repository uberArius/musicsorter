#include "VisualizerProtocol.h"
#include <array>
#include <iostream>
#include <thread>
#include <stdexcept>
#define REQUIRE(x) do{if(!(x))throw std::runtime_error(#x);}while(0)
int main(){try{
    const auto name=L"Local\\SpectralGrid.ProtocolTest."+std::to_wstring(GetCurrentProcessId());
    viz::Mapping writer,reader;REQUIRE(writer.create(name.c_str()));REQUIRE(reader.open(name.c_str()));
    std::array<float,viz::Samples> samples{};
    for(unsigned i=0;i<viz::Slots-1;++i){samples.fill(float(i));REQUIRE(viz::push(writer.data,samples.data(),viz::Samples,48000,double(i),7));}
    REQUIRE(!viz::push(writer.data,samples.data(),viz::Samples,48000,0,7));REQUIRE(viz::load(writer.data->dropped)==1);
    viz::Block block{};
    for(unsigned i=0;i<viz::Slots-1;++i){REQUIRE(viz::pop(reader.data,block));REQUIRE(block.samples[0]==float(i)&&block.samples[511]==float(i));REQUIRE(block.epoch==7&&block.count==512);}
    REQUIRE(!viz::pop(reader.data,block));
    // Producer never waits in push; a full queue leaves unread payload intact.
    std::thread producer([&]{for(int i=0;i<10000;++i){samples.fill(float(i));while(!viz::push(writer.data,samples.data(),512,48000,double(i),9))std::this_thread::yield();}});
    bool valid=true;for(int i=0;i<10000;++i){while(!viz::pop(reader.data,block))std::this_thread::yield();valid=valid&&block.samples[0]==float(i)&&block.samples[511]==float(i)&&block.epoch==9;}
    producer.join();REQUIRE(valid);std::cout<<"Protocol roundtrip, overflow, ordering and concurrent publication passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
