#include <QStandardItem>

#include <command/Command.h>
#include <signal/Signal.h>
#include <state/State.h>
#include <station/RemoteStation.h>
#include <utils/Id.hpp>
#include <utils/Types.hpp>

#include <report/reportpresetwidget.h>

#include "utils/types.h"
#include "applypresetitemaction.h"

ApplyPresetItemAction::ApplyPresetItemAction(ReportPresetWidget *preset)
    : reportPreset(preset)
{

}

void ApplyPresetItemAction::processAcknowledgement(QStandardItem *item)
{
    int remoteStationId = item->parent()->data(UserRoles::RemoteStationIdRole).toInt();
    if (reportPreset->containsCurrentPresetContent(toInt(ObjectTypes::Acknowledgement), remoteStationId))
    {
        item->setCheckState(Qt::Checked);
    }
}

void ApplyPresetItemAction::processRegulations(QStandardItem *item)
{
    int remoteStationId = item->parent()->data(UserRoles::RemoteStationIdRole).toInt();
    if (reportPreset->containsCurrentPresetContent(toInt(ObjectTypes::Regulations), remoteStationId))
    {
        item->setCheckState(Qt::Checked);
    }
}

void ApplyPresetItemAction::processSignal(QStandardItem *item)
{
    model::UnitItemId signalId = item->data(UserRoles::SignalIdRole).value<model::UnitItemId>();
    model::UnitItemId senderId = item->data(UserRoles::ControlObjectIdRole).value<model::UnitItemId>();
    if (reportPreset->containsCurrentPresetContent(toInt(ObjectTypes::Signal), senderId, signalId))
    {
        item->setCheckState(Qt::Checked);
    }
}

void ApplyPresetItemAction::processState(QStandardItem *item)
{
    model::UnitItemId stateId = item->data(UserRoles::StateIdRole).value<model::UnitItemId>();
    model::UnitItemId senderId = item->data(UserRoles::ControlObjectIdRole).value<model::UnitItemId>();
    if (reportPreset->containsCurrentPresetContent(toInt(ObjectTypes::State), senderId, stateId))
    {
        item->setCheckState(Qt::Checked);
    }
}

void ApplyPresetItemAction::processCommand(QStandardItem *item)
{
    model::UnitItemId commandId = item->data(UserRoles::CommandIdRole).value<model::UnitItemId>();
    model::UnitItemId senderId = item->data(UserRoles::ControlObjectIdRole).value<model::UnitItemId>();
    if (reportPreset->containsCurrentPresetContent(toInt(ObjectTypes::Command), senderId, commandId))
    {
        item->setCheckState(Qt::Checked);
    }
}
