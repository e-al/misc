#ifndef APPLYPRESETITEMACTION_H
#define APPLYPRESETITEMACTION_H

#include <action/iitemsaction.h>

class ReportPresetWidget;

class ApplyPresetItemAction final : public IItemsAction
{
public:
    ApplyPresetItemAction(ReportPresetWidget *preset);
protected:
    virtual void processAcknowledgement(QStandardItem *item) override;
    virtual void processRegulations(QStandardItem *item) override;
    virtual void processSignal(QStandardItem *item) override;
    virtual void processState(QStandardItem *item) override;
    virtual void processCommand(QStandardItem *item) override;

private:
    template<typename T>
    int toInt(const T& type) { return static_cast<int>(type); }

    ReportPresetWidget *reportPreset;
};

#endif // APPLYPRESETITEMACTION_H
