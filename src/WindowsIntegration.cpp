#include "WindowsIntegration.h"
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
bool WindowsIntegration::writeRegistration(QSettings &s,const QString &executable){
    const QString exe=QDir::toNativeSeparators(executable);
    const QString command="\""+exe+"\" -- \"%1\"";
    const QString caps="Software/MusicSorter/Capabilities";
    s.setValue(caps+"/ApplicationName","MusicSorter");
    s.setValue(caps+"/ApplicationDescription","Music playback and fast audio browsing");
    s.setValue(caps+"/ApplicationIcon","\""+exe+"\",0");
    s.setValue("Software/RegisteredApplications/MusicSorter","Software\\MusicSorter\\Capabilities");
    const QString app="Software/Classes/Applications/MusicSorter.exe";
    s.setValue(app+"/FriendlyAppName","MusicSorter");
    s.setValue(app+"/shell/open/command/.",command);
    for(const auto &ext:QStringList{"mp3","wav","flac","ogg","m4a","aac","opus"}){
        const QString id="MusicSorter.Audio."+ext;
        const QString root="Software/Classes/"+id;
        s.setValue(root+"/.","MusicSorter — "+ext.toUpper());
        s.setValue(root+"/DefaultIcon/.","\""+exe+"\",0");
        s.setValue(root+"/shell/open/command/.",command);
        s.setValue(caps+"/FileAssociations/."+ext,id);
        s.setValue("Software/Classes/."+ext+"/OpenWithProgids/"+id,QString());
        s.setValue(app+"/SupportedTypes/."+ext,QString());
    }
    s.sync();return s.status()==QSettings::NoError;
}
bool WindowsIntegration::configureDefaults(QString *error){
    QSettings registry("HKEY_CURRENT_USER",QSettings::NativeFormat);
    if(!writeRegistration(registry,QCoreApplication::applicationFilePath())){*error="Could not register the application in Windows settings.";return false;}
    SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,nullptr,nullptr);
    if(!QDesktopServices::openUrl(QUrl("ms-settings:defaultapps?registeredAppUser=MusicSorter"))){*error="Open Windows Settings → Apps → Default apps and select MusicSorter.";return false;}
    return true;
}
