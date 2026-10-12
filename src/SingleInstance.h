#pragma once
#include <QObject>
#include <QLocalServer>
#include <QLockFile>
class SingleInstance : public QObject {
    Q_OBJECT
public:
    explicit SingleInstance(const QString &name,QObject *parent=nullptr);
    ~SingleInstance() override {server.close();lock.unlock();}
    enum Result { Primary, Forwarded, Failed };
    Result start(const QString &file,const QString &directory,QString *error);
signals:
    void requested(const QString &file,const QString &directory);
private:
    QString name;
    QLocalServer server;
    QLockFile lock;
};
