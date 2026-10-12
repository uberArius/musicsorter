#pragma once
#include <QString>
#include <QObject>
#include <QRect>
#include <QVector>
#include <array>
#include <atomic>
#include <memory>

namespace Logic {
constexpr int CardHeight = 40, Gap = 4, WaveBins = 300;
double fraction(double x, double left, double width);
qint64 position(double fraction, qint64 duration);
QRect cardRect(int index, int columns, int width, int scroll);
int hitTest(QPoint point, int columns, int width, int scroll, int count);
struct PlaybackState {
    int active = -1, locked = -1;
    bool hover(int index);
    bool click(int index); // true: start another track
    void leave();
    void stop();
};
class WaveAccumulator {
public:
    explicit WaveAccumulator(qint64 frames);
    void add(double amplitude);
    QVector<float> normalized() const;
private:
    qint64 total, cursor = 0;
    std::array<double, WaveBins> sums{};
    std::array<qint64, WaveBins> counts{};
};
}
struct Category { QString label, directory; };
class ConfigManager {
public:
    std::array<Category, 3> categories{{{"FOLDER1", {}}, {"FOLDER2", {}}, {"FOLDER3", {}}}};
    explicit ConfigManager(QString file = {});
    bool load(QString *error = nullptr);
    bool save(QString *error = nullptr) const;
    QString path;
};
struct Track {
    QString path, name, category, error;
    qint64 duration = 0;
    qint64 fileBytes = 0;
    QVector<float> wave;
    bool copying = false, sorted = false, analyzed = false, audioReady = false;
    bool beginCopy();
    void finishCopy(bool success, const QString &label);
};
struct ScanResult { QVector<Track> tracks; QString error; };
using CancelFlag = std::shared_ptr<std::atomic_bool>;
ScanResult scanDirectory(const QString &path, const CancelFlag &cancel);
class TrackModel : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~TrackModel() override;
    QVector<Track> tracks;
    quint64 generation = 0;
    void load(const QString &path);
signals:
    void reset();
    void loaded();
    void changed(int index);
    void error(const QString &message);
private:
    CancelFlag scanCancel;
};
struct CopyResult { bool success = false; QString destination, error; };
class FileSorter {
public:
    static CopyResult copy(const QString &source, const QString &directory, const CancelFlag &cancel);
};
struct WaveData { QVector<float> values; qint64 duration = 0; QString error; };
class WaveformCache {
public:
    static QString key(const QString &source);
    static WaveData read(const QString &key);
    static void write(const QString &key, const WaveData &data);
};
