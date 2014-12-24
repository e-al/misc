#include <QStandardItem>

#include "utils/types.h"
#include "iitemsaction.h"

void IItemsAction::operator()(const QList<QStandardItem *> &list)
{
    for (QStandardItem *item : list)
    {
        ObjectTypes objectEventType = item->data(UserRoles::ObjectTypeRole).value<ObjectTypes>();
        switch(objectEventType)
        {
            case ObjectTypes::Acknowledgement:
            {
                processAcknowledgement(item);
                break;
            }
            case ObjectTypes::Regulations:
            {
                processRegulations(item);
                break;
            }
            case ObjectTypes::Signal:
            {
                processSignal(item);
                break;
            }
            case ObjectTypes::State:
            {
                processState(item);
                break;
            }
            case ObjectTypes::Command:
            {
                processCommand(item);
                break;
            }
            default:
                break;
        }
    }

    finalize();
}
