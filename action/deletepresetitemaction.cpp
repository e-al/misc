#include <QStandardItem>

#include <command/Command.h>
#include <signal/Signal.h>
#include <state/State.h>
#include <station/RemoteStation.h>
#include <utils/Id.hpp>
#include <utils/Types.hpp>

#include <report/reportpresetwidget.h>

#include "utils/types.h"
#include "deletepresetitemaction.h"

DeletePresetItemAction::DeletePresetItemAction(ReportPresetWidget *preset)
    : reportPreset(preset)
{
}

void DeletePresetItemAction::processAcknowledgement(QStandardItem *item)
{
    int remoteStationId = item->parent()->data(UserRoles::RemoteStationIdRole).toInt();
    reportPreset->removeCurrentPresetContent(toInt(ObjectTypes::Acknowledgement), remoteStationId);
}

void DeletePresetItemAction::processRegulations(QStandardItem *item)
{
    int remoteStationId = item->parent()->data(UserRoles::RemoteStationIdRole).toInt();
    reportPreset->removeCurrentPresetContent(toInt(ObjectTypes::Regulations), remoteStationId);
}

void DeletePresetItemAction::processSignal(QStandardItem *item)
{
    model::UnitItemId signalId = item->data(UserRoles::SignalIdRole).value<model::UnitItemId>();
    model::UnitItemId senderId = item->data(UserRoles::ControlObjectIdRole).value<model::UnitItemId>();
    reportPreset->removeCurrentPresetContent(toInt(ObjectTypes::Signal), senderId, signalId);
}

void DeletePresetItemAction::processState(QStandardItem *item)
{
    model::UnitItemId stateId = item->data(UserRoles::StateIdRole).value<model::UnitItemId>();
    model::UnitItemId senderId = item->data(UserRoles::ControlObjectIdRole).value<model::UnitItemId>();
    reportPreset->removeCurrentPresetContent(toInt(ObjectTypes::State), senderId, stateId);
}

void DeletePresetItemAction::processCommand(QStandardItem *item)
{
    model::UnitItemId commandId = item->data(UserRoles::CommandIdRole).value<model::UnitItemId>();
    model::UnitItemId senderId = item->data(UserRoles::ControlObjectIdRole).value<model::UnitItemId>();
    reportPreset->removeCurrentPresetContent(toInt(ObjectTypes::Command), senderId, commandId);
}

void DeletePresetItemAction::finalize()
{
//    reportPreset->save();
}
