#include "MainWindow.h"
#include "VisualizerController.h"
#include <QSignalBlocker>
#include "DirectoryTree.h"
#include "TrackGridView.h"
#include "AudioController.h"
#include "WaveformGenerator.h"
#include "AudioRepository.h"
#include "SettingsDialog.h"
#include <QApplication>
#include <QSplitter>
#include <QBoxLayout>
#include <QStatusBar>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QAbstractSpinBox>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QButtonGroup>
#include <QPainter>
#include <QTimer>
#include <QSlider>
#include <QFileInfo>
#include <QScrollBar>
#include <QPainterPath>

class CopyButton final : public QPushButton {
public:
    CopyButton(int key,QColor color):key(key),accent(color){setCursor(Qt::PointingHandCursor);setMinimumSize(140,52);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);setAttribute(Qt::WA_Hover);}
    QSize sizeHint()const override{return {230,52};}
protected:
    void paintEvent(QPaintEvent *)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const QRectF box=QRectF(rect()).adjusted(1,1,-1,-1);
        QLinearGradient bg(box.topLeft(),box.bottomRight());
        bg.setColorAt(0,accent.darker(isDown()?360:underMouse()?280:440));bg.setColorAt(1,QColor("#202033"));
        p.setBrush(bg);p.setPen(QPen(hasFocus()?accent.lighter(135):accent.darker(underMouse()?100:180),hasFocus()?2:1));p.drawRoundedRect(box,9,9);
        p.setPen(Qt::NoPen);p.setBrush(accent);p.drawRoundedRect(QRectF(11,11,30,30),6,6);
        QFont f=font();f.setPixelSize(16);f.setBold(true);p.setFont(f);p.setPen(QColor("#111323"));p.drawText(QRect(11,11,30,30),Qt::AlignCenter,QString::number(key));
        f.setPixelSize(12);p.setFont(f);p.setPen(QColor("#f1f5f9"));p.drawText(QRect(53,0,width()-83,height()),Qt::AlignVCenter,fontMetrics().elidedText(text(),Qt::ElideRight,width()-83));
        p.setPen(QPen(accent,1.6));p.drawLine(width()-24,26,width()-14,26);p.drawLine(width()-18,22,width()-14,26);p.drawLine(width()-18,30,width()-14,26);
    }
private:
    int key;QColor accent;
};
MainWindow::MainWindow(QWidget *parent):QMainWindow(parent){
    setWindowTitle("Music Sorter");resize(1440,850);setMinimumSize(980,450);
    setStyleSheet(R"(
        QWidget { background:#0f0f1a; color:#e2e8f0; font-family:'Segoe UI'; font-size:12px; }
        QFrame#topbar {background:#16213e;} QFrame#sortbar {background:#1a1a2e;}
        QFrame#topbar QLabel,QFrame#sortbar QLabel {background:transparent;}
        QPushButton {background:#24243b;border:1px solid #3d3d5e;border-radius:5px;padding:7px 10px;}
        QPushButton:hover {background:#343451;} QPushButton:checked {background:#7c3aed;border-color:#a78bfa;}
        QTreeView {background:#121220;border:0;outline:0;} QTreeView::item {height:27px;}
        QTreeView::item:hover {background:#1e1e38;} QTreeView::item:selected {background:#2a1f5e;}
        QSplitter::handle {background:#2d2d4e;} QSplitter::handle:hover,QSplitter::handle:pressed {background:#06b6d4;}
        QLineEdit {background:#1a1a2e;border:1px solid #3d3d5e;border-radius:3px;padding:6px;}
        QScrollBar:vertical {background:#121220;width:12px;} QScrollBar::handle:vertical {background:#41415f;min-height:25px;border-radius:4px;}
        QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical {height:0;}
        QToolTip {background:#24243b;color:#e2e8f0;border:1px solid #7c3aed;}
        QSlider {background:transparent;} QSlider::groove:horizontal {height:4px;background:#3d4560;border-radius:2px;}
        QSlider::sub-page:horizontal {background:#a78bfa;border-radius:2px;} QSlider::handle:horizontal {background:#ede9fe;width:12px;margin:-4px 0;border-radius:6px;}
    )");
    model=new TrackModel(this);audio=new AudioController(model);waveforms=new WaveformGenerator(model);
    auto outer=new QSplitter;outer->setObjectName("mainSplit");outer->setHandleWidth(4);outer->setChildrenCollapsible(false);setCentralWidget(outer);
    auto central=new QWidget;central->setObjectName("sorterPanel");outer->addWidget(central);outer->setStretchFactor(0,2);
    auto layout=new QVBoxLayout(central);layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);
    auto top=new QFrame;top->setObjectName("topbar");auto topLayout=new QHBoxLayout(top);topLayout->setContentsMargins(16,12,16,12);
    auto title=new QLabel("♫  Music Sorter");title->setStyleSheet("font-size:20px;font-weight:700;");topLayout->addWidget(title);
    topLayout->addSpacing(12);
    auto previous=new QPushButton("|◀");previous->setObjectName("previousTrack");previous->setToolTip("Previous track");previous->setAccessibleName("Previous track");
    auto play=new QPushButton("▶ PLAY");play->setObjectName("playStop");play->setFixedWidth(86);play->setToolTip("Play with hover preview disabled / stop");
    auto next=new QPushButton("▶|");next->setObjectName("nextTrack");next->setToolTip("Next track");next->setAccessibleName("Next track");
    topLayout->addWidget(previous);topLayout->addWidget(play);topLayout->addWidget(next);
    connect(previous,&QPushButton::clicked,audio,&AudioController::previous);connect(play,&QPushButton::clicked,audio,&AudioController::togglePlayback);connect(next,&QPushButton::clicked,audio,&AudioController::next);
    auto volume=new QSlider(Qt::Horizontal);volume->setObjectName("volume");volume->setRange(0,100);volume->setValue(audio->volume());volume->setFixedWidth(90);volume->setAccessibleName("Volume");
    auto volumeLabel=new QLabel("100%");volumeLabel->setFixedWidth(34);topLayout->addSpacing(8);topLayout->addWidget(new QLabel("Volume"));topLayout->addWidget(volume);topLayout->addWidget(volumeLabel);
    connect(volume,&QSlider::valueChanged,this,[this,volumeLabel,volume](int value){audio->setVolume(value);volumeLabel->setText(QString::number(value)+"%");volume->setToolTip(QString("Volume: %1%").arg(value));});
    topLayout->addStretch();auto modes=new QButtonGroup(this);modes->setExclusive(true);
    for(int c:{1,4,8}){auto button=new QPushButton(QString("x%1").arg(c));button->setCheckable(true);button->setChecked(c==4);button->setToolTip(QString("Columns: %1").arg(c));modes->addButton(button,c);topLayout->addWidget(button);}
    auto visualizerToggle=new QPushButton("Visualizer");visualizerToggle->setObjectName("visualizerToggle");visualizerToggle->setCheckable(true);visualizerToggle->setToolTip("Toggle the visualizer. Press F10 in its window to attach / detach.");topLayout->addWidget(visualizerToggle);
    auto settings=new QPushButton("⚙  Settings");topLayout->addWidget(settings);layout->addWidget(top);
    auto bar=new QFrame;bar->setObjectName("sortbar");auto barLayout=new QHBoxLayout(bar);barLayout->setContentsMargins(12,8,12,8);
    barLayout->addWidget(new QLabel("COPY TO"));const char *colors[]={"#c4b5fd","#67e8f9","#6ee7b7"};
    for(int i=0;i<3;++i){sortButtons[i]=new CopyButton(i+1,QColor(colors[i]));barLayout->addWidget(sortButtons[i],1);connect(sortButtons[i],&QPushButton::clicked,this,[this,i]{sort(audio->state.locked>=0?audio->state.active:lastHovered,i);});}
    counter=new QLabel;barLayout->addWidget(counter);layout->addWidget(bar);
    auto split=new QSplitter;split->setHandleWidth(4);auto tree=new DirectoryTree;grid=new TrackGridView(model);split->addWidget(tree);split->addWidget(grid);split->setStretchFactor(1,1);split->setChildrenCollapsible(false);split->setSizes({240,1200});layout->addWidget(split,1);
    auto visualizerPanel=new QWidget;outer->addWidget(visualizerPanel);outer->setStretchFactor(1,1);
    visualizer=new VisualizerController(audio,visualizerPanel,this);
    connect(visualizerToggle,&QPushButton::clicked,visualizer,&VisualizerController::setEnabled);
    connect(visualizer,&VisualizerController::enabledChanged,this,[visualizerToggle,outer](bool enabled){QSignalBlocker block(visualizerToggle);visualizerToggle->setChecked(enabled);if(enabled){const int width=outer->width();outer->setSizes({width*2/3,width/3});}});
    connect(visualizer,&VisualizerController::error,this,[this](const QString &message){feedback->showMessage(message,15000);});
    feedback=new QStatusBar;layout->addWidget(feedback);
    analysis=new QLabel;feedback->addPermanentWidget(analysis);feedback->setSizeGripEnabled(false);
    auto ramStatus=new QLabel;feedback->addPermanentWidget(ramStatus);
    connect(AudioRepository::forModel(model),&AudioRepository::memoryChanged,this,[ramStatus,this](qint64 bytes,int ready){ramStatus->setText(QString("Ready to play: %1/%2 · RAM: %3 MB").arg(ready).arg(model->tracks.size()).arg(bytes/(1024*1024)));});
    connect(tree,&DirectoryTree::directorySelected,this,&MainWindow::openDirectory);
    connect(modes,&QButtonGroup::idClicked,grid,&TrackGridView::setColumns);
    connect(grid,&TrackGridView::hoverTrack,audio,&AudioController::hover);connect(grid,&TrackGridView::leaveTrack,audio,&AudioController::leave);connect(grid,&TrackGridView::clickTrack,audio,&AudioController::click);connect(grid,&TrackGridView::sortTrack,this,&MainWindow::sort);
    connect(grid,&TrackGridView::visibleChanged,waveforms,&WaveformGenerator::prioritize);
    connect(grid,&TrackGridView::hoverTrack,this,[this](int i,double){lastHovered=i;});
    connect(model,&TrackModel::reset,this,[this]{lastHovered=-1;});
    connect(model,&TrackModel::loaded,this,&MainWindow::playPendingFile);
    connect(audio,&AudioController::changed,this,[this,play]{const int old=grid->active,oldLock=grid->locked;grid->active=audio->state.active;grid->locked=audio->state.locked;if(old!=grid->active)grid->playbackProgress=0;grid->updateCard(old);grid->updateCard(oldLock);grid->updateCard(grid->active);play->setText(grid->active>=0?"■ STOP":"▶ PLAY");});
    connect(audio,&AudioController::progress,this,[this](double f){grid->playbackProgress=f;grid->updateCard(grid->active);});
    connect(audio,&AudioController::error,this,[this](const QString &error){feedback->showMessage("Audio error: "+error,15000);});
    connect(model,&TrackModel::error,this,[this](const QString &error){feedback->showMessage(error,15000);});
    connect(model,&TrackModel::reset,this,&MainWindow::updateCount);
    connect(waveforms,&WaveformGenerator::progress,this,[this](int done,int total){analysis->setText(total>0?QString("Analysis: %1 / %2").arg(done).arg(total):QString());});
    connect(settings,&QPushButton::clicked,this,[this]{audio->stop();SettingsDialog dialog(&config,this);if(dialog.exec()==QDialog::Accepted)updateLabels();});
    QString error;if(!config.load(&error))QTimer::singleShot(0,this,[this,error]{feedback->showMessage(error+" Default settings restored.",15000);});
    updateLabels();updateCount();qApp->installEventFilter(this);
}
MainWindow::~MainWindow(){delete visualizer;visualizer=nullptr;for(const auto &cancel:copies)cancel->store(true);audio->stop();qApp->removeEventFilter(this);}
void MainWindow::openDirectory(const QString &path){
    pendingFile.clear();currentDirectory=QDir(path).absolutePath();audio->stop();
    findChild<DirectoryTree*>()->selectDirectory(currentDirectory);model->load(currentDirectory);
}
void MainWindow::openFile(const QString &path){
    const QFileInfo file(path);
    if(!QStringList{"mp3","wav","flac","ogg","m4a","aac","opus"}.contains(file.suffix().toLower())){feedback->showMessage("Unsupported file format: "+path,15000);return;}
    const QString directory=file.absolutePath();
    if(currentDirectory.compare(directory,Qt::CaseInsensitive)==0){
        for(const auto &track:model->tracks)if(track.path.compare(file.absoluteFilePath(),Qt::CaseInsensitive)==0){pendingFile=file.absoluteFilePath();playPendingFile();return;}
    }
    openDirectory(directory);pendingFile=file.absoluteFilePath();
}
void MainWindow::playPendingFile(){
    if(pendingFile.isEmpty())return;
    for(int i=0;i<model->tracks.size();++i)if(model->tracks[i].path.compare(pendingFile,Qt::CaseInsensitive)==0){
        pendingFile.clear();audio->playTrack(i);lastHovered=i;
        const auto r=grid->card(i);if(!grid->viewport()->rect().contains(r))grid->verticalScrollBar()->setValue(grid->verticalScrollBar()->value()+r.top());
        return;
    }
    feedback->showMessage("Track not found in folder: "+pendingFile,15000);pendingFile.clear();
}
void MainWindow::updateLabels(){for(int i=0;i<3;++i){sortButtons[i]->setText(config.categories[i].label);sortButtons[i]->setAccessibleName(QString("%1 — copy to %2").arg(i+1).arg(config.categories[i].label));sortButtons[i]->setToolTip(QString("Copy to %1 · key %2%3").arg(config.categories[i].label).arg(i+1).arg(i==2?" or right-click":""));}}
void MainWindow::updateCount(){int sorted=0;for(const auto &t:model->tracks)sorted+=t.sorted;counter->setText(QString("Tracks: %1 | Sorted: %2").arg(model->tracks.size()).arg(sorted));}
bool MainWindow::eventFilter(QObject *object,QEvent *event){
    if(event->type()==QEvent::KeyPress && QApplication::activeWindow()==this && !QApplication::activeModalWidget()){
        auto key=static_cast<QKeyEvent*>(event);auto focus=QApplication::focusWidget();
        if(!qobject_cast<QLineEdit*>(focus)&&!qobject_cast<QTextEdit*>(focus)&&!qobject_cast<QPlainTextEdit*>(focus)&&!qobject_cast<QAbstractSpinBox*>(focus)&&key->modifiers()==Qt::NoModifier&&key->key()>=Qt::Key_1&&key->key()<=Qt::Key_3){
            auto p=grid->viewport()->mapFromGlobal(QCursor::pos());int i=grid->viewport()->rect().contains(p)?grid->at(p):-1;
            if(i>=0){if(!key->isAutoRepeat())sort(i,key->key()-Qt::Key_1);return true;}
        }
    }
    return QMainWindow::eventFilter(object,event);
}
void MainWindow::sort(int i,int category){
    if(i<0||i>=model->tracks.size()||category<0||category>2)return;
    const auto target=config.categories[category];if(target.directory.isEmpty()){feedback->showMessage("First choose a category folder in Settings.",10000);return;}
    auto &track=model->tracks[i];if(copyingPaths.contains(track.path)||!track.beginCopy())return;
    const auto source=track.path;copyingPaths.insert(source);
    if(audio->state.active==i)audio->stop();emit model->changed(i);
    auto cancel=std::make_shared<std::atomic_bool>(false);copies<<cancel;auto ticket=model->generation;auto watcher=new QFutureWatcher<CopyResult>(this);
    connect(watcher,&QFutureWatcher<CopyResult>::finished,this,[this,watcher,ticket,i,target,cancel,source]{auto result=watcher->result();watcher->deleteLater();copies.removeOne(cancel);copyingPaths.remove(source);
        if(ticket==model->generation && i<model->tracks.size()){model->tracks[i].finishCopy(result.success,target.label);emit model->changed(i);updateCount();}
        feedback->showMessage(result.success?"Copied: "+QDir::toNativeSeparators(result.destination):"Copy error: "+result.error,12000);
    });
    watcher->setFuture(QtConcurrent::run(&FileSorter::copy,track.path,target.directory,cancel));
}
