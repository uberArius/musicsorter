#pragma once
#include "Core.h"
#include <QAbstractScrollArea>
class TrackGridView : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit TrackGridView(TrackModel *model, QWidget *parent=nullptr);
    int columns=4, hovered=-1, active=-1, locked=-1;
    double playbackProgress=0;
    QRect card(int index) const;
    int at(QPoint point) const;
    void setColumns(int count);
    void updateCard(int index);
    QVector<int> visibleIndices() const;
    void refresh();
signals:
    void hoverTrack(int index,double fraction);
    void leaveTrack();
    void clickTrack(int index,double fraction);
    void sortTrack(int index,int category);
    void visibleChanged(const QVector<int> &indices);
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
    void scrollContentsBy(int,int) override;
private:
    TrackModel *model;
    void updateHover(QPoint point);
};
