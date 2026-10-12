#include "TrackGridView.h"
#include <QPainter>
#include <QMouseEvent>
#include <QScrollBar>
#include <QToolTip>

TrackGridView::TrackGridView(TrackModel *m,QWidget *parent):QAbstractScrollArea(parent),model(m){
    setMouseTracking(true);viewport()->setMouseTracking(true);setFrameShape(QFrame::NoFrame);setMinimumWidth(160);
    connect(m,&TrackModel::reset,this,[this]{hovered=-1;verticalScrollBar()->setValue(0);refresh();});
    connect(m,&TrackModel::changed,this,&TrackGridView::updateCard);
}
QRect TrackGridView::card(int i)const{return Logic::cardRect(i,columns,viewport()->width(),verticalScrollBar()->value());}
int TrackGridView::at(QPoint p)const{return Logic::hitTest(p,columns,viewport()->width(),verticalScrollBar()->value(),int(model->tracks.size()));}
QVector<int> TrackGridView::visibleIndices()const{
    QVector<int> out;int first=std::max(0,verticalScrollBar()->value()/44-1)*columns;
    int last=std::min(int(model->tracks.size()),((verticalScrollBar()->value()+viewport()->height())/44+2)*columns);
    for(int i=first;i<last;++i)out<<i;return out;
}
void TrackGridView::refresh(){verticalScrollBar()->setPageStep(viewport()->height());verticalScrollBar()->setSingleStep(44);verticalScrollBar()->setRange(0,std::max(0,((int(model->tracks.size())+columns-1)/columns)*44+4-viewport()->height()));viewport()->update();emit visibleChanged(visibleIndices());}
void TrackGridView::setColumns(int count){columns=count;refresh();updateHover(viewport()->mapFromGlobal(QCursor::pos()));}
void TrackGridView::updateCard(int i){if(i>=0)viewport()->update(card(i));}
void TrackGridView::resizeEvent(QResizeEvent *e){QAbstractScrollArea::resizeEvent(e);refresh();}
void TrackGridView::scrollContentsBy(int,int){viewport()->update();emit visibleChanged(visibleIndices());updateHover(viewport()->mapFromGlobal(QCursor::pos()));}
void TrackGridView::updateHover(QPoint p){
    int previous=hovered;hovered=viewport()->rect().contains(p)?at(p):-1;
    if(hovered>=0 && (model->tracks[hovered].sorted||model->tracks[hovered].copying))hovered=-1;
    if(previous!=hovered){updateCard(previous);updateCard(hovered);}
    if(hovered<0){emit leaveTrack();return;}
    const auto r=card(hovered);emit hoverTrack(hovered,Logic::fraction(p.x(),r.left(),r.width()));
    setToolTip(model->tracks[hovered].path+"\nClick to lock playback."+(model->tracks[hovered].error.isEmpty()?QString():"\n"+model->tracks[hovered].error));
}
void TrackGridView::mouseMoveEvent(QMouseEvent *e){updateHover(e->position().toPoint());}
void TrackGridView::leaveEvent(QEvent *e){int old=hovered;hovered=-1;updateCard(old);emit leaveTrack();QAbstractScrollArea::leaveEvent(e);}
void TrackGridView::mousePressEvent(QMouseEvent *e){
    const int i=at(e->position().toPoint());if(i<0 || model->tracks[i].sorted || model->tracks[i].copying)return;
    hovered=i;const auto r=card(i);
    if(e->button()==Qt::RightButton)emit sortTrack(i,2);
    if(e->button()==Qt::LeftButton)emit clickTrack(i,Logic::fraction(e->position().x(),r.left(),r.width()));
}
void TrackGridView::paintEvent(QPaintEvent *event){
    QPainter p(viewport());p.fillRect(event->rect(),QColor("#0f0f1a"));p.setRenderHint(QPainter::Antialiasing);
    static const char *colors[][2]={{"#2d1b4e","#4a2d7a"},{"#1b3a4e","#2d5f7a"},{"#1b4e2d","#2d7a4a"},{"#4e3a1b","#7a5e2d"},{"#4e1b2d","#7a2d4a"},{"#1b2d4e","#2d4a7a"},{"#3a1b4e","#5e2d7a"},{"#4e4e1b","#7a7a2d"}};
    if(model->tracks.isEmpty()){p.setPen(QColor("#94a3b8"));p.drawText(viewport()->rect(),Qt::AlignCenter,"Choose a music folder\nMP3 · WAV · FLAC · OGG · M4A · AAC · OPUS");return;}
    for(int i:visibleIndices()){
        auto r=card(i);if(!event->region().intersects(r))continue;const auto &t=model->tracks[i];
        p.save();p.setClipRect(r);QLinearGradient gradient(r.topLeft(),r.topRight());gradient.setColorAt(0,QColor(colors[i%8][0]));gradient.setColorAt(1,QColor(colors[i%8][1]));p.setBrush(gradient);p.setPen(Qt::NoPen);p.drawRoundedRect(r,5,5);
        const double progress=i==active?playbackProgress:0;
        if(i==active)p.fillRect(QRectF(r.x(),r.y(),r.width()*progress,r.height()),QColor(6,182,212,38));
        const int bars=std::max(1,std::min(300,r.width()/3));
        for(int b=0;b<bars && !t.wave.isEmpty();++b){
            int start=b*300/bars,end=std::max(start+1,(b+1)*300/bars);float amplitude=0;
            for(int k=start;k<end;++k)amplitude+=t.wave[k];amplitude/=float(end-start);
            qreal x=r.left()+(b+0.5)*r.width()/bars,h=std::max(1.0,double(amplitude)*38);
            p.setPen(QPen(double(b)/bars<progress?QColor(6,182,212,230):QColor(255,255,255,77),2));p.drawLine(QPointF(x,r.center().y()-h/2),QPointF(x,r.center().y()+h/2));
        }
        p.fillRect(r,QColor(0,0,0,65));
        auto font=p.font();font.setBold(true);p.setFont(font);p.setPen(Qt::white);
        const int right=(columns==1?64:8)+(locked==i?20:0);QString text=QString::number(i+1)+"  "+t.name;
        if(t.sorted)text+=" → "+t.category;else if(t.copying)text+=" · copying…";else if(!t.error.isEmpty())text+=" · !";
        p.drawText(r.adjusted(8,0,-right,0),Qt::AlignVCenter,p.fontMetrics().elidedText(text,Qt::ElideRight,std::max(0,r.width()-right-8)));
        if(columns==1){QString duration=t.duration>0?QString("%1:%2").arg(t.duration/60000).arg((t.duration/1000)%60,2,10,QChar('0')):"—";p.drawText(r.adjusted(0,0,-8,0),Qt::AlignRight|Qt::AlignVCenter,duration);}
        if(locked==i){int x=r.right()-(columns==1?76:18);p.setBrush(Qt::NoBrush);p.setPen(QPen(Qt::white,1.5));p.drawRoundedRect(QRectF(x+3,r.y()+10,8,10),4,4);p.setBrush(Qt::white);p.drawRoundedRect(QRectF(x,r.y()+17,14,12),2,2);}
        if(t.sorted)p.fillRect(r,QColor(0,0,0,178));
        if(i==hovered || i==locked){p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor(i==locked?"#06b6d4":"#a78bfa"),1));p.drawRoundedRect(r.adjusted(0,0,-1,-1),5,5);}
        if(i==active){p.setPen(QPen(QColor("#06b6d4"),2));const int x=r.x()+int(r.width()*progress);p.drawLine(x,r.top(),x,r.bottom());}
        p.restore();
    }
}
