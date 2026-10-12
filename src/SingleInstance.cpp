#include "SingleInstance.h"
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <memory>
SingleInstance::SingleInstance(const QString &n,QObject *parent):QObject(parent),name(n),lock(QDir::tempPath()+"/"+n+".lock"){
    lock.setStaleLockTime(0);server.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&server,&QLocalServer::newConnection,this,[this]{
        while(auto socket=server.nextPendingConnection()){
            socket->setReadBufferSize(65537);
            auto data=std::make_shared<QByteArray>();
            connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
            QTimer::singleShot(5000,socket,[socket]{socket->abort();socket->deleteLater();});
            auto receive=[this,socket,data]{
                *data+=socket->readAll();if(data->size()>65536){socket->abort();return;}
                if(!data->contains('\n'))return;
                const auto doc=QJsonDocument::fromJson(data->left(data->indexOf('\n')));
                if(!doc.isObject()||doc.object().value("version").toInt()!=1){socket->abort();return;}
                const auto obj=doc.object();emit requested(obj.value("file").toString(),obj.value("directory").toString());
                socket->write("OK\n");socket->flush();socket->disconnectFromServer();
            };
            connect(socket,&QLocalSocket::readyRead,this,receive);
            if(socket->bytesAvailable())receive();
        }
    });
}
SingleInstance::Result SingleInstance::start(const QString &file,const QString &directory,QString *error){
    if(lock.tryLock()){
        if(server.listen(name))return Primary;
        *error=server.errorString();lock.unlock();return Failed;
    }
    QElapsedTimer timer;timer.start();
    while(timer.elapsed()<5000){
        QLocalSocket socket;socket.connectToServer(name);
        if(socket.waitForConnected(200)){
            QJsonObject obj{{"version",1},{"file",file},{"directory",directory}};
            socket.write(QJsonDocument(obj).toJson(QJsonDocument::Compact)+"\n");
            socket.waitForBytesWritten(1000);
            QByteArray reply;while(!reply.contains('\n')&&socket.waitForReadyRead(3000))reply+=socket.readAll();
            if(reply=="OK\n")return Forwarded;
            break;
        }
        QThread::msleep(25);
    }
    *error="Could not send the track to the running MusicSorter instance. Please try again.";return Failed;
}
