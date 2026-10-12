#include "Core.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QCollator>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QScopeGuard>
#include <QLockFile>
#include <algorithm>
#include <cmath>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

namespace {
bool atomicWrite(const QString &path,const QByteArray &bytes,QString *error=nullptr) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if(file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size() && file.commit())return true;
    if(error)*error=file.errorString();return false;
}
}

double Logic::fraction(double x, double left, double width) { return width > 0 ? std::clamp((x-left)/width, 0.0, 1.0) : 0; }
qint64 Logic::position(double f, qint64 duration) { return duration > 0 ? qRound64(std::clamp(f,0.0,1.0)*double(duration)) : 0; }
QRect Logic::cardRect(int i, int c, int w, int s) {
    const int usable = std::max(c, w-Gap*(c+1));
    const int col = i%c, left = Gap + col*usable/c + col*Gap;
    return {left, Gap+(i/c)*(CardHeight+Gap)-s, (col+1)*usable/c-col*usable/c, CardHeight};
}
int Logic::hitTest(QPoint p, int c, int w, int s, int count) {
    if (p.x()<0 || p.x()>=w || p.y()+s<Gap) return -1;
    const int row=(p.y()+s-Gap)/(CardHeight+Gap);
    for (int col=0;col<c;++col) { int i=row*c+col; if(i<count && cardRect(i,c,w,s).contains(p)) return i; }
    return -1;
}
bool Logic::PlaybackState::hover(int i) { if(locked>=0) return false; bool changed=active!=i; active=i; return changed; }
bool Logic::PlaybackState::click(int i) {
    if(locked==i) { locked=-1; return false; }
    if(locked>=0) { locked=active=i; return true; }
    if(active==i) { locked=i; return false; }
    active=i; return true;
}
void Logic::PlaybackState::leave() { if(locked<0) active=-1; }
void Logic::PlaybackState::stop() { active=locked=-1; }
Logic::WaveAccumulator::WaveAccumulator(qint64 frames) : total(std::max<qint64>(1,frames)) {}
void Logic::WaveAccumulator::add(double a) {
    const auto bin=std::min<qint64>(WaveBins-1, cursor*WaveBins/total);
    sums[size_t(bin)]+=std::isfinite(a)?std::abs(a):0; ++counts[size_t(bin)]; ++cursor;
}
QVector<float> Logic::WaveAccumulator::normalized() const {
    QVector<float> out(WaveBins,0); float peak=0;
    for(int i=0;i<WaveBins;++i) { if(counts[i]) out[i]=float(sums[i]/double(counts[i])); peak=std::max(peak,out[i]); }
    if(peak>0) for(auto &v:out) v/=peak;
    return out;
}
ConfigManager::ConfigManager(QString file) : path(std::move(file)) {
    if(path.isEmpty()) {
#ifdef Q_OS_WIN
        path=qEnvironmentVariable("APPDATA")+"/MusicSorter/config.json";
#else
        path=QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)+"/config.json";
#endif
    }
}
bool ConfigManager::load(QString *error) {
    QFile f(path); if(!f.exists()) return true;
    if(!f.open(QIODevice::ReadOnly)) { if(error)*error=f.errorString(); return false; }
    QJsonParseError e; auto doc=QJsonDocument::fromJson(f.readAll(),&e);
    if(e.error!=QJsonParseError::NoError || !doc.isObject()) { if(error)*error="The configuration file is corrupted."; return false; }
    auto next=categories; const auto obj=doc.object();
    for(int i=0;i<3;++i) {
        const auto label=obj.value(QString("label%1").arg(i+1)); const auto dir=obj.value(QString("dir%1").arg(i+1));
        if(!label.isString() || label.toString().trimmed().isEmpty() || !dir.isString()) { if(error)*error="Invalid categories in the configuration."; return false; }
        next[i]={label.toString(),dir.toString()};
    }
    categories=next; return true;
}
bool ConfigManager::save(QString *error) const {
    QJsonObject obj;
    for(int i=0;i<3;++i) { obj[QString("label%1").arg(i+1)]=categories[i].label; obj[QString("dir%1").arg(i+1)]=categories[i].directory; }
    auto bytes=QJsonDocument(obj).toJson();
    return atomicWrite(path,bytes,error);
}
bool Track::beginCopy() { if(sorted || copying) return false; copying=true; return true; }
void Track::finishCopy(bool ok,const QString &label) { copying=false; if(ok) { sorted=true; category=label; } }
ScanResult scanDirectory(const QString &path,const CancelFlag &cancel) {
    ScanResult out; QFileInfo info(path);
    if(!info.isDir() || !info.isReadable()) { out.error="Cannot read folder: "+path; return out; }
    const QStringList extensions={"mp3","wav","flac","ogg","m4a","aac","opus"};
    QDirIterator it(path,QDir::Files|QDir::NoDotAndDotDot);
    while(it.hasNext() && !cancel->load()) { it.next(); auto f=it.fileInfo(); if(extensions.contains(f.suffix().toLower())) { Track t; t.path=f.absoluteFilePath(); t.name=f.completeBaseName(); t.fileBytes=f.size(); out.tracks.push_back(t); } }
    QCollator collator; collator.setCaseSensitivity(Qt::CaseInsensitive); collator.setNumericMode(true);
    std::sort(out.tracks.begin(),out.tracks.end(),[&](const Track&a,const Track&b){return collator.compare(QFileInfo(a.path).fileName(),QFileInfo(b.path).fileName())<0;});
    return out;
}
TrackModel::~TrackModel() { if(scanCancel) scanCancel->store(true); }
void TrackModel::load(const QString &path) {
    if(scanCancel) scanCancel->store(true); scanCancel=std::make_shared<std::atomic_bool>(false);
    const auto ticket=++generation; tracks.clear(); emit reset();
    auto watcher=new QFutureWatcher<ScanResult>(this);
    connect(watcher,&QFutureWatcher<ScanResult>::finished,this,[this,watcher,ticket]{
        auto result=watcher->result(); watcher->deleteLater(); if(ticket!=generation)return;
        tracks=std::move(result.tracks); emit reset(); emit loaded(); if(!result.error.isEmpty())emit error(result.error);
    });
    watcher->setFuture(QtConcurrent::run(scanDirectory,path,scanCancel));
}
CopyResult FileSorter::copy(const QString &source,const QString &directory,const CancelFlag &cancel) {
    CopyResult result;
    if(directory.isEmpty()) { result.error="Choose a destination folder in Settings."; return result; }
    if(!QDir().mkpath(directory)) { result.error="Cannot create the destination folder."; return result; }
    QFile input(source); if(!input.open(QIODevice::ReadOnly)) { result.error=input.errorString(); return result; }
    auto temp=std::make_unique<QTemporaryFile>(QDir(directory).filePath(".musicsorter-XXXXXX.part"));
    if(!temp->open()) { result.error=temp->errorString(); return result; }
    while(!input.atEnd()) {
        if(cancel->load()) { result.error="Copy cancelled."; return result; }
        auto block=input.read(1024*1024);
        if(block.isEmpty() && input.error()!=QFile::NoError) {result.error=input.errorString();return result;}
        if(temp->write(block)!=block.size()) {result.error=temp->errorString();return result;}
    }
    if(!temp->flush()) {result.error=temp->errorString();return result;}
    const auto temporaryPath=temp->fileName();
    auto cleanup=qScopeGuard([&]{QFile::remove(temporaryPath);});
    temp->setAutoRemove(false);temp.reset();input.close();
    QFileInfo original(source);
    for(int n=0;n<100000;++n) {
        if(cancel->load()) {result.error="Copy cancelled.";return result;}
        const QString name=n==0?original.fileName():original.completeBaseName()+QString(" (%1)").arg(n)+(original.suffix().isEmpty()?QString():"."+original.suffix());
        auto destination=QDir(directory).filePath(name);
        if(QFileInfo::exists(destination)) continue;
        // QTemporaryFile::rename can replace a racing destination on Windows.
        // MoveFileEx without REPLACE_EXISTING provides atomic no-clobber publication.
#ifdef Q_OS_WIN
        const bool renamed=MoveFileExW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(temporaryPath).utf16()),reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(destination).utf16()),MOVEFILE_WRITE_THROUGH)!=0;
        const auto renameError=GetLastError();
#else
        const bool renamed=QFile::rename(temporaryPath,destination);
#endif
        if(renamed) {result.success=true;result.destination=destination;return result;}
        if(QFileInfo::exists(destination)) continue;
#ifdef Q_OS_WIN
        result.error=QString("Cannot finalize the copy (Windows error %1).").arg(renameError);
#else
        result.error="Cannot finalize the copy.";
#endif
        return result;
    }
    result.error="Cannot find an available file name.";return result;
}
QString WaveformCache::key(const QString &source) {
    QFileInfo f(source); const auto identity=f.absoluteFilePath().toUtf8()+'\0'+QByteArray::number(f.size())+'\0'+QByteArray::number(f.lastModified().toMSecsSinceEpoch())+"/v3";
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/waveforms/"+QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex()+".wave";
}
WaveData WaveformCache::read(const QString &key) {
    WaveData d; QFile f(key); if(!f.open(QIODevice::ReadOnly) || f.size()!=16+Logic::WaveBins*4+32)return d;
    auto bytes=f.readAll();auto payload=bytes.chopped(32);
    if(QCryptographicHash::hash(payload,QCryptographicHash::Sha256)!=bytes.right(32))return {};
    QDataStream s(payload); s.setFloatingPointPrecision(QDataStream::SinglePrecision); quint32 magic,version; s>>magic>>version>>d.duration;
    if(magic!=0x4d535746 || version!=2 || d.duration<=0)return {};
    d.values.resize(Logic::WaveBins); for(auto &v:d.values) {s>>v;if(!std::isfinite(v)||v<0||v>1)return {};}
    return s.status()==QDataStream::Ok?d:WaveData{};
}
void WaveformCache::write(const QString &key,const WaveData &d) {
    if(d.values.size()!=Logic::WaveBins || d.duration<=0)return;
    QByteArray bytes; QDataStream s(&bytes,QIODevice::WriteOnly);s.setFloatingPointPrecision(QDataStream::SinglePrecision);s<<quint32(0x4d535746)<<quint32(2)<<d.duration;
    for(float v:d.values)s<<v;
    if(s.status()!=QDataStream::Ok)return;
    bytes.append(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256));
    if(atomicWrite(key,bytes))return;
    // Cache is disposable: on filesystems without atomic rename, serialize writers
    // and validate exact length/range on read. Never use this fallback for settings.
    QLockFile lock(key+".lock");if(!lock.tryLock(0))return;
    QFile file(key);if(file.open(QIODevice::WriteOnly|QIODevice::Truncate))file.write(bytes);
}
