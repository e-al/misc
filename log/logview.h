#ifndef LOGVIEW_H
#define LOGVIEW_H

#include "tableviewclonable.h"

class FrameParameters;

class LogView : public TableViewClonable
{
    Q_OBJECT
public:
    explicit LogView(QWidget *parent = 0);

signals:
    void showRequest(const FrameParameters &params);

private slots:
    void showIndicationDetails(const QModelIndex &index);
    void showOnScheme(QPoint pos);
};

#endif // LOGVIEW_H
