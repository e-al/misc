#ifndef DELETEPRESETITEMACTION_H
#define DELETEPRESETITEMACTION_H

#include <action/iitemsaction.h>

class ReportPresetWidget;

class DeletePresetItemAction final : public IItemsAction
{
public:
    DeletePresetItemAction(ReportPresetWidget *preset);
protected:
    virtual void processAcknowledgement(QStandardItem *item) override;
    virtual void processRegulations(QStandardItem *item) override;
    virtual void processSignal(QStandardItem *item) override;
    virtual void processState(QStandardItem *item) override;
    virtual void processCommand(QStandardItem *item) override;
    virtual void finalize();

private:
    template<typename T>
    int toInt(T type) { return static_cast<int>(type); }

    ReportPresetWidget *reportPreset;
};

#endif // DELETEPRESETITEMACTION_H
