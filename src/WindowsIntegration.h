#pragma once
#include <QString>
class QSettings;
namespace WindowsIntegration {
bool writeRegistration(QSettings &settings,const QString &executable);
bool configureDefaults(QString *error);
}
