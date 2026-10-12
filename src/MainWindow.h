#pragma once
#include <QMainWindow>
#include "Core.h"
#include <QLabel>
#include <QPushButton>
#include <QSet>
class TrackGridView;
class AudioController;
class WaveformGenerator;
class VisualizerController;
class QStatusBar;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void openFile(const QString &path);
    void openDirectory(const QString &path);
protected:
    bool eventFilter(QObject *object,QEvent *event) override;
private:
    ConfigManager config;
    TrackModel *model;
    TrackGridView *grid;
    AudioController *audio;
    WaveformGenerator *waveforms;
    VisualizerController *visualizer;
    QStatusBar *feedback;
    QLabel *counter,*analysis;
    std::array<QPushButton*,3> sortButtons;
    QVector<CancelFlag> copies;
    int lastHovered=-1;
    QSet<QString> copyingPaths;
    QString currentDirectory,pendingFile;
    void playPendingFile();
    void sort(int index,int category);
    void updateLabels();
    void updateCount();
};
