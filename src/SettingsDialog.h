#pragma once
#include "Core.h"
#include <QDialog>
#include <QLineEdit>
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(ConfigManager *config,QWidget *parent=nullptr);
};
