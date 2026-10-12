// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "VisualizerProtocol.h"
#include "WinUtil.h"
#include <string>
namespace sg {
class HostSession {
public:
    void open(const std::wstring &name,DWORD pid){
        if(name.empty())return;
        if(!mapping_.open(name.c_str())||mapping_.data->hostPid!=pid)throw std::runtime_error("Invalid host session");
        parentProcess_.value=OpenProcess(SYNCHRONIZE,FALSE,pid);
        if(!parentProcess_.value)throw std::runtime_error("Host process unavailable");
        parent_=reinterpret_cast<HWND>(mapping_.data->parentWindow);DWORD owner=0;GetWindowThreadProcessId(parent_,&owner);
        if(!IsWindow(parent_)||owner!=pid)throw std::runtime_error("Invalid host window");
    }
    bool enabled()const{return mapping_.data!=nullptr;}
    viz::Shared *audio()const{return mapping_.data;}
    bool attached()const{return attached_;}
    bool suspended()const{return enabled()&&viz::load(mapping_.data->suspended)!=0;}
    void toggle(){if(enabled())InterlockedExchange(&mapping_.data->attachRequested,attached_?0:1);}
    void ready(HWND window){if(enabled()){InterlockedExchange64(&mapping_.data->childWindow,LONG64(window));InterlockedExchange(&mapping_.data->ready,1);}}
    bool poll(HWND window){
        if(!enabled())return true;
        if(WaitForSingleObject(parentProcess_.value,0)!=WAIT_TIMEOUT||viz::load(mapping_.data->closeRequested)||!IsWindow(parent_))return false;
        const bool want=viz::load(mapping_.data->attachRequested)!=0;
        if(!initialized_||want!=attached_){
            ShowWindow(window,SW_HIDE);SetLastError(0);
            if(want){
                if(initialized_)GetWindowRect(window,&floating_);
                SetWindowLongPtrW(window,GWL_STYLE,WS_CHILD|WS_CLIPSIBLINGS|WS_CLIPCHILDREN);
                SetWindowLongPtrW(window,GWL_EXSTYLE,WS_EX_NOREDIRECTIONBITMAP);
                if(!SetParent(window,parent_)&&GetLastError())throw std::runtime_error("Cannot attach visualizer window (DPI/parent mismatch)");
            }else{
                if(!SetParent(window,nullptr)&&GetLastError())throw std::runtime_error("Cannot detach visualizer window");
                SetWindowLongPtrW(window,GWL_STYLE,WS_OVERLAPPEDWINDOW);
                SetWindowLongPtrW(window,GWL_EXSTYLE,WS_EX_NOREDIRECTIONBITMAP|WS_EX_APPWINDOW);
                SetWindowPos(window,HWND_NOTOPMOST,floating_.left,floating_.top,floating_.right-floating_.left,floating_.bottom-floating_.top,SWP_FRAMECHANGED|SWP_NOACTIVATE);
            }
            initialized_=true;attached_=want;lastWidth_=lastHeight_=0;
            InterlockedExchange(&mapping_.data->attached,want?1:0);
            if(!viz::load(mapping_.data->testMode))ShowWindow(window,SW_SHOWNOACTIVATE);
        }
        if(attached_){RECT r{};GetClientRect(parent_,&r);const int w=std::max(1L,r.right),h=std::max(1L,r.bottom);
            if(w!=lastWidth_||h!=lastHeight_){SetWindowPos(window,nullptr,0,0,w,h,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);lastWidth_=w;lastHeight_=h;}}
        return true;
    }
private:
    viz::Mapping mapping_;Handle parentProcess_;HWND parent_=nullptr;
    bool initialized_=false,attached_=false;int lastWidth_=0,lastHeight_=0;
    RECT floating_{160,100,1060,760};
};
}
