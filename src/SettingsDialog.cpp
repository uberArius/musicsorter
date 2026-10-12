#include "SettingsDialog.h"
#include "WindowsIntegration.h"
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
SettingsDialog::SettingsDialog(ConfigManager *config,QWidget *parent):QDialog(parent){
    setWindowTitle("Settings");resize(760,240);auto layout=new QGridLayout(this);
    layout->addWidget(new QLabel("Files are always copied. Originals remain unchanged."),0,0,1,4);
    std::array<QLineEdit*,3> names,dirs;
    for(int i=0;i<3;++i){names[i]=new QLineEdit(config->categories[i].label);dirs[i]=new QLineEdit(config->categories[i].directory);dirs[i]->setReadOnly(true);auto browse=new QPushButton("Browse…");
        layout->addWidget(new QLabel(QString::number(i+1)),i+1,0);layout->addWidget(names[i],i+1,1);layout->addWidget(dirs[i],i+1,2);layout->addWidget(browse,i+1,3);
        connect(browse,&QPushButton::clicked,this,[this,edit=dirs[i]]{auto path=QFileDialog::getExistingDirectory(this,"Destination folder",edit->text(),QFileDialog::ShowDirsOnly|QFileDialog::DontUseNativeDialog);if(!path.isEmpty())edit->setText(path);});
    }
    auto buttons=new QDialogButtonBox;buttons->addButton("Save",QDialogButtonBox::AcceptRole);buttons->addButton("Cancel",QDialogButtonBox::RejectRole);layout->addWidget(buttons,6,0,1,4);
    auto defaults=new QPushButton("Set as default player…");defaults->setObjectName("configureDefaults");layout->addWidget(defaults,4,0,1,4);
    auto hint=new QLabel("Confirm file associations in Windows. If you move the application, use this button again.");hint->setWordWrap(true);layout->addWidget(hint,5,0,1,4);
    connect(defaults,&QPushButton::clicked,this,[this]{QString error;if(!WindowsIntegration::configureDefaults(&error))QMessageBox::warning(this,"Default apps",error);});
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,this,[=]{auto copy=*config;for(int i=0;i<3;++i){if(names[i]->text().trimmed().isEmpty()){QMessageBox::warning(this,"Settings","The category name cannot be empty.");return;}copy.categories[i]={names[i]->text().trimmed(),dirs[i]->text()};}QString error;if(!copy.save(&error)){QMessageBox::warning(this,"Save error",error);return;}*config=copy;accept();});
}
