#include "MainWindow.h"
#include "SingleInstance.h"
#include "VisualizerController.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QIcon>
#include "DirectoryTree.h"
int main(int argc, char **argv) {
    qputenv("QT_MEDIA_BACKEND", "ffmpeg");
    QApplication app(argc, argv);
    app.setApplicationName("MusicSorter");
    app.setApplicationVersion("1.4.5");
    app.setStyle("Fusion");
    app.setWindowIcon(QIcon(":/resources/musicsorter.ico"));
    QCommandLineParser parser;parser.addHelpOption();parser.addVersionOption();
    parser.addOption({"directory","Open a music folder.","path"});
    parser.addOption({"smoke-test","Run a hidden startup test and exit after 3 seconds."});
    parser.addOption({"screenshot","Save a window screenshot during the startup test.","path"});
    parser.addOption({"visualizer","Enable the visualizer on startup."});
    parser.addPositionalArgument("file","Play a track and load the other tracks in its folder.","[file]");
    parser.process(app);
    if(parser.positionalArguments().size()>1)parser.showHelp(1);
    const QString file=parser.positionalArguments().isEmpty()?QString():QFileInfo(parser.positionalArguments().first()).absoluteFilePath();
    const QString directory=parser.isSet("directory")?QDir(parser.value("directory")).absolutePath():QString();
    const QString identity=QString::fromLatin1(QCryptographicHash::hash(QDir::homePath().toUtf8(),QCryptographicHash::Sha256).toHex().left(24));
    SingleInstance instance("MusicSorter-"+identity);
    if(!parser.isSet("smoke-test")){
        QString error;const auto result=instance.start(file,directory,&error);
        if(result==SingleInstance::Forwarded)return 0;
        if(result==SingleInstance::Failed){QMessageBox::warning(nullptr,"MusicSorter",error);return 1;}
    }
    MainWindow window;
    if(parser.isSet("smoke-test"))window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    auto open=[&window](const QString &file,const QString &directory){
        if(!file.isEmpty())window.openFile(file);else if(!directory.isEmpty())window.openDirectory(directory);
        if(window.isMinimized())window.showNormal();window.raise();window.activateWindow();
    };
    QObject::connect(&instance,&SingleInstance::requested,&window,open);
    QTimer::singleShot(0,&window,[&]{open(file,directory);if(parser.isSet("visualizer")){auto visualizer=window.findChild<VisualizerController*>();visualizer->setTestMode(parser.isSet("smoke-test"));visualizer->setEnabled(true);}});
    if(parser.isSet("smoke-test"))QTimer::singleShot(3000,&window,[&]{
        if(parser.isSet("visualizer")&&!window.findChild<VisualizerController*>()->isReady()){app.exit(3);return;}
        if(parser.isSet("screenshot")&&!window.grab().save(parser.value("screenshot"))){app.exit(2);return;}
        app.quit();
    });
    return app.exec();
}
