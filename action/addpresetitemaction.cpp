#include <QStandardItem>

#include <command/Command.h>
#include <signal/Signal.h>
#include <state/State.h>
#include <station/RemoteStation.h>
#include <utils/Id.hpp>
#include <utils/Types.hpp>

#include <report/reportpresetwidget.h>

#include "utils/types.h"
#include "addpresetitemaction.h"

AddPresetItemAction::AddPresetItemAction(ReportPresetWidget *preset)
    : reportPreset(preset)
{

}

void AddPresetItemAction::processAcknowledgement(QStandardItem *item)
{
    int remoteStationId = item->parent()->data(UserRoles::RemoteStationIdRole).toInt();
    reportPreset->addCurrentPresetContent(toInt(ObjectTypes::Acknowledgement), remoteStationId);
}

void AddPresetItemAction::processRegulations(QStandardItem *item)
{
    int remoteStationId = item->parent()->data(UserRoles::RemoteStationIdRole).toInt();
    reportPreset->addCurrentPresetContent(toInt(ObjectTypes::Regulations), remoteStationId);
}

void AddPresetItemAction::processSignal(QStandardItem *item)
{
    model::UnitItemId signalId = item->data(UserRoles::SignalIdRole).value<model::UnitItemId>();
    model::UnitItemId senderId = item->data(UserRoles::ControlObjectIdRole).value<model::UnitItemId>();
    reportPreset->addCurrentPresetContent(toInt(ObjectTypes::Signal), senderId, signalId);
}

void AddPresetItemAction::processState(QStandardItem *item)
{
    model::UnitItemId stateId = item->data(UserRoles::StateIdRole).value<model::UnitItemId>();
    model::UnitItemId senderId = item->data(UserRoles::ControlObjectIdRole).value<model::UnitItemId>();
    reportPreset->addCurrentPresetContent(toInt(ObjectTypes::State), senderId, stateId);
}

void AddPresetItemAction::processCommand(QStandardItem *item)
{
    model::UnitItemId commandId = item->data(UserRoles::CommandIdRole).value<model::UnitItemId>();
    model::UnitItemId senderId = item->data(UserRoles::ControlObjectIdRole).value<model::UnitItemId>();
    reportPreset->addCurrentPresetContent(toInt(ObjectTypes::Command), senderId, commandId);
}

void AddPresetItemAction::finalize()
{
//    reportPreset->save();
}
