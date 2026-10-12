#include "VisualizerController.h"
#include "VisualizerProtocol.h"
#include "AudioController.h"
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QBoxLayout>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QUuid>
VisualizerController::VisualizerController(AudioController *a,QWidget *p,QObject *parent):QObject(parent),audio(a),panel(p){
    panel->setObjectName("visualizerPanel");panel->setMinimumWidth(300);
    auto layout=new QVBoxLayout(panel);layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);
    auto header=new QWidget;auto row=new QHBoxLayout(header);row->setContentsMargins(8,4,8,4);
    status=new QLabel("Visualizer");row->addWidget(status,1);
    auto detach=new QPushButton("Detach");detach->setToolTip("Detach the window; press F10 in the visualizer to attach it again");row->addWidget(detach);
    auto close=new QPushButton("×");close->setAccessibleName("Turn off the visualizer");row->addWidget(close);layout->addWidget(header);
    surface=new QWidget;surface->setObjectName("visualizerSurface");surface->setAttribute(Qt::WA_NativeWindow);surface->setMinimumSize(280,200);layout->addWidget(surface,1);panel->hide();
    connect(detach,&QPushButton::clicked,this,[this]{setAttached(false);});connect(close,&QPushButton::clicked,this,[this]{setEnabled(false);});
    monitor.setInterval(33);connect(&monitor,&QTimer::timeout,this,&VisualizerController::poll);
    startTimeout.setSingleShot(true);startTimeout.setInterval(15000);
    connect(&startTimeout,&QTimer::timeout,this,[this]{emit error("The visualizer did not become ready.");setEnabled(false);});
    killTimeout.setSingleShot(true);killTimeout.setInterval(2000);connect(&killTimeout,&QTimer::timeout,&process,&QProcess::kill);
    connect(&process,&QProcess::started,this,[this]{
        if(job){HANDLE child=OpenProcess(PROCESS_SET_QUOTA|PROCESS_TERMINATE,FALSE,DWORD(process.processId()));if(child){AssignProcessToJobObject(static_cast<HANDLE>(job),child);CloseHandle(child);}}
    });
    connect(&process,&QProcess::finished,this,[this](int code,QProcess::ExitStatus state){const bool unexpected=!stopping&&(code!=0||state==QProcess::CrashExit);cleanup();if(unexpected)emit error("The visualizer exited with an error. Audio playback continues.");});
    connect(&process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){if(e==QProcess::FailedToStart){const auto message=process.errorString();cleanup();emit error("Cannot start the visualizer: "+message);}});
}
VisualizerController::~VisualizerController(){
    disconnect(&process,nullptr,this,nullptr);audio->setVisualizer(nullptr);
    if(mapping)InterlockedExchange(&mapping->data->closeRequested,1);
    if(process.state()!=QProcess::NotRunning&&!process.waitForFinished(1200)){process.kill();process.waitForFinished(500);}
    if(job)CloseHandle(static_cast<HANDLE>(job));
}
viz::Shared *VisualizerController::sharedForTesting()const{return mapping?mapping->data:nullptr;}
bool VisualizerController::isReady()const{return mapping&&viz::load(mapping->data->ready)!=0;}
bool VisualizerController::isAttached()const{return mapping&&viz::load(mapping->data->attached)!=0;}
quint64 VisualizerController::windowId()const{return mapping?quint64(viz::load64(mapping->data->childWindow)):0;}
qint64 VisualizerController::consumedBlocks()const{return mapping?viz::load64(mapping->data->consumedBlocks):0;}
void VisualizerController::setAttached(bool attached){if(mapping&&!stopping){if(attached)panel->show();InterlockedExchange(&mapping->data->attachRequested,attached?1:0);}}
void VisualizerController::setEnabled(bool enabled){
    if(!enabled){
        if(!isRunning()){cleanup();return;}
        stopping=true;audio->setVisualizer(nullptr);startTimeout.stop();
        if(mapping)InterlockedExchange(&mapping->data->closeRequested,1);
        panel->hide();killTimeout.start();return;
    }
    if(isRunning()||stopping)return;
    QString program=executable;
    if(program.isEmpty()){
        program=QCoreApplication::applicationDirPath()+"/visualizer/SpectralGrid.exe";
        if(!QFileInfo::exists(program))program=QCoreApplication::applicationDirPath()+"/../integration/spectral-grid/build/Release/SpectralGrid.exe";
    }
    if(!QFileInfo::exists(program)){emit enabledChanged(false);emit error("SpectralGrid.exe is missing. Extract the complete application package, including the visualizer folder.");return;}
    const QString session="Local\\MusicSorter.Visualizer."+QUuid::createUuid().toString(QUuid::Id128);
    mapping=std::make_unique<viz::Mapping>();
    if(!mapping->create(session.toStdWString().c_str())){cleanup();emit error("Cannot create the visualizer buffer.");return;}
    mapping->data->testMode=testMode?1:0;mapping->data->hostPid=GetCurrentProcessId();mapping->data->parentWindow=quint64(surface->winId());InterlockedExchange(&mapping->data->attachRequested,1);
    job=CreateJobObjectW(nullptr,nullptr);if(job){JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};info.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;SetInformationJobObject(static_cast<HANDLE>(job),JobObjectExtendedLimitInformation,&info,sizeof(info));}
    stopping=false;connected=false;status->setText("Starting…");panel->show();
    process.setWorkingDirectory(QFileInfo(program).absolutePath());process.start(program,{"--host-session",session,"--host-pid",QString::number(GetCurrentProcessId())});
    emit enabledChanged(true);monitor.start();startTimeout.start();
}
void VisualizerController::poll(){
    if(!mapping||stopping)return;
    if(isReady()&&!connected){connected=true;audio->setVisualizer(mapping->data);startTimeout.stop();status->setText("Visualizer · MusicSorter");}
    const bool wantsAttached=viz::load(mapping->data->attachRequested)!=0;
    panel->setVisible(wantsAttached||isAttached());
    InterlockedExchange(&mapping->data->suspended,isAttached()&&panel->window()->isMinimized()?1:0);
}
void VisualizerController::cleanup(){
    audio->setVisualizer(nullptr);monitor.stop();startTimeout.stop();killTimeout.stop();panel->hide();mapping.reset();
    if(job){CloseHandle(static_cast<HANDLE>(job));job=nullptr;}stopping=false;connected=false;emit enabledChanged(false);
}
