#ifndef ADDPRESETITEMACTION_H
#define ADDPRESETITEMACTION_H

#include <action/iitemsaction.h>
#include <QDateTime>

class ReportPresetWidget;

class AddPresetItemAction final : public IItemsAction
{
public:
    AddPresetItemAction(ReportPresetWidget *preset);
protected:
    virtual void processAcknowledgement(QStandardItem *item) override;
    virtual void processRegulations(QStandardItem *item) override;
    virtual void processSignal(QStandardItem *item) override;
    virtual void processState(QStandardItem *item) override;
    virtual void processCommand(QStandardItem *item) override;
    virtual void finalize();

private:
    template<typename T>
    int toInt(const T& type) { return static_cast<int>(type); }

    QDateTime fromTime;
    QDateTime toTime;
    ReportPresetWidget *reportPreset;
};
#endif // ADDPRESETITEMACTION_H
