#include "SingleInstance.h"
#include <QCoreApplication>
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    if(app.arguments().size()!=3)return 3;
    SingleInstance instance(app.arguments()[1]);QString error;
    return instance.start(app.arguments()[2],{},&error)==SingleInstance::Forwarded?0:2;
}
