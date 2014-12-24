#include <QApplication>
#include <QDebug>
#include <QMenu>
#include <QStandardItem>

#include <control/ControlObject.h>
#include <state/State.h>

#include "utils/types.h"
#include "breadcrumbs/breadcrumbs.h"
#include "indicationdetails/indicationdetailsframe.h"
#include "logpresenter.h"
#include "modelarm.h"
#include "mnemoschema/mnemoschemaframe.h"
#include "notifications/dimmingmessage.h"

#include "logview.h"

LogView::LogView(QWidget *parent)
    : TableViewClonable(parent)
{
    connect(this
        , SIGNAL(doubleClicked(QModelIndex))
        , this
        , SLOT(showIndicationDetails(QModelIndex)));

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this
        , SIGNAL(customContextMenuRequested(QPoint))
        , this
        , SLOT(showOnScheme(QPoint)));
}

void LogView::showIndicationDetails(const QModelIndex &index)
{
    Q_ASSERT(model());

    QStandardItemModel *m = dynamic_cast<QStandardItemModel*>(model());
    if(!m)
    {
        qWarning() << "Не задана модель";
        return;
    }

    // Извлечь данные из ячейки "Сообщение"
    QStandardItem *msgItem(m->item(index.row(), static_cast<int>(LogPresenter::Columns::Text)));
    model::UnitItemId controlObjectId = msgItem->data(UserRoles::ControlObjectIdRole).value<model::UnitItemId>();
    model::UnitItemId stateId = msgItem->data(UserRoles::StateIdRole).value<model::UnitItemId>();

    model::SimpleControlObjectPointer controlObject = ModelArm::instance()->findControlObject(controlObjectId);

    // Извлечь данные из ячейки "Дата"
    QStandardItem *dateTimeItem(m->item(index.row(), static_cast<int>(LogPresenter::Columns::DateTime)));
    QVariant dateTimeData(dateTimeItem->data(UserRoles::EventLogViewRoles::DateTimeRole));
    QDateTime dateTime(dateTimeData.value<QDateTime>());

    FrameParameters params;
    params.id = FrameId::IndicationDetails;
    params.crumbText = QString::fromStdString(controlObject->name());
    params.remoteStationId = controlObject->unit()->remoteStation()->id();
    params.controlObjectId = controlObjectId;
    params.dateTime = dateTime;
    params.mode = RemoteStationMode::History;

    IndicationDetailsFrame::HistoryInfo info;
    info.from = dateTime;
    info.to = QDateTime::currentDateTime();
    info.focusEventId = stateId;
    params.aux = QVariant::fromValue(info);

    emit showRequest(params);
}

void LogView::showOnScheme(QPoint pos)
{
    Q_ASSERT(model());

    QStandardItemModel *m = dynamic_cast<QStandardItemModel*>(model());
    if(!m)
    {
        qWarning() << "Не задана модель";
        return;
    }

    const QModelIndex& index = indexAt(pos);
    if (index.isValid())
    {
        QMenu *menu = new QMenu();
        QAction *action = new QAction(tr("Показать на схеме"), menu);

        connect(action
            , &QAction::triggered
            , [this, m, index] (bool)
                {
                    int row = index.row();
                    QStandardItem *item = m->item(row
                        , static_cast<int>(LogPresenter::Columns::DateTime));
                    QDateTime dateTime = item->data(
                        UserRoles::EventLogViewRoles::DateTimeRole).value<QDateTime>();

                    item = m->item(row
                        , static_cast<int>(LogPresenter::Columns::RemoteStation));
                    int stationId = item->data(
                        UserRoles::EventLogViewRoles::TpIndexRole).toInt();

                    MnemoSchemaFrame::HistoryInfo info;
                    info.stationId = stationId;
                    info.date = dateTime;
                    QVariant arg(QVariant::fromValue(info));

                    CrumbModel::ProgramStateId stateId(
                        BreadCrumbs::instance()->getState(
                            BreadCrumbs::State::MnemoSchema));
                    BreadCrumbs::instance()->model()->setCurrentState(stateId, true, arg);
                });

        menu->addAction(action);
        menu->popup(viewport()->mapToGlobal(pos));
    }
}
