#ifndef IITEMSACTION_H
#define IITEMSACTION_H

#include <QList>

class QStandardItem;

class IItemsAction
{
public:
    IItemsAction() = default;
    void operator()(const QList<QStandardItem*>& list);
protected:
    virtual void processAcknowledgement(QStandardItem *item) = 0;
    virtual void processRegulations(QStandardItem *item) = 0;
    virtual void processSignal(QStandardItem *item) = 0;
    virtual void processState(QStandardItem *item) = 0;
    virtual void processCommand(QStandardItem *item) = 0;
    virtual void finalize() {}
};


#endif // IITEMSACTION_H
