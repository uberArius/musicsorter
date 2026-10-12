#pragma once
#include <QTreeView>
#include <QFileSystemModel>
class DirectoryTree : public QTreeView {
    Q_OBJECT
public:
    explicit DirectoryTree(QWidget *parent=nullptr):QTreeView(parent){
        auto model=new QFileSystemModel(this);model->setFilter(QDir::AllDirs|QDir::NoDotAndDotDot|QDir::Drives);model->setRootPath({});setModel(model);
        setHeaderHidden(true);for(int c=1;c<4;++c)hideColumn(c);setMinimumWidth(140);setMaximumWidth(500);setAnimated(false);setUniformRowHeights(true);
        setExpandsOnDoubleClick(false);setTextElideMode(Qt::ElideMiddle);
        connect(this,&QTreeView::clicked,this,[this,model](const QModelIndex &index){expand(index);emit directorySelected(model->filePath(index));});
    }
signals:
    void directorySelected(const QString &path);
public:
    void selectDirectory(const QString &path){
        auto fs=static_cast<QFileSystemModel*>(model());const auto index=fs->index(path);
        for(auto p=index.parent();p.isValid();p=p.parent())expand(p);
        setCurrentIndex(index);expand(index);scrollTo(index);
    }
};
