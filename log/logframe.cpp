#include <QAbstractProxyModel>
#include <QButtonGroup>
#include <QCalendarWidget>
#include <QDebug>
#include <QLineEdit>
#include <QMenu>
#include <QMultiMap>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QVBoxLayout>

#include <binder/ModelBinder.h>
#include <binder/ModelBinderListener.h>
#include <binder/ModelBinderListenerHolder.h>
#include <command/Command.h>
#include <db/DatabaseIteratorReader.h>
#include <db/DatabaseReader.h>
#include <report/ReportPreset.h>
#include <signal/Signal.h>
#include <signal/SignalInfo.h>
#include <signal/SignalType.h>
#include <state/State.h>
#include <state/StateInfo.h>
#include <station/RemoteStation.h>
#include <unit/Unit.h>
#include <user/User.h>

#include "action/addpresetitemaction.h"
#include "action/applypresetitemaction.h"
#include "action/deletepresetitemaction.h"
#include "databasereaderarm.h"
#include "logframe.h"
#include "logpresenter.h"
#include "modelarm.h"
#include "notifications/dimmingmessage.h"
#include "notifications/visualnotifier.h"
#include "print/printerdialog.h"
#include "print/printingtask.h"
#include "quicksearch/quicksearch.h"
#include "utils/types.h"

#include "ui_logframe.h"

class ShowEventsItemAction final : public IItemsAction
{
public:
    ShowEventsItemAction(const QDateTime& from, const QDateTime& to, const std::shared_ptr<LogPresenter>& presenter);
protected:
    virtual void processAcknowledgement(QStandardItem *item) override;
    virtual void processRegulations(QStandardItem *item) override;
    virtual void processSignal(QStandardItem *item) override;
    virtual void processState(QStandardItem *item) override;
    virtual void processCommand(QStandardItem *item) override;

private:
    void emitAll(const model::DatabaseReader::EventType& eventType, const model::UnitItemId& id);
    QDateTime fromTime;
    QDateTime toTime;
    std::shared_ptr<LogPresenter> logPresenter;
};

ShowEventsItemAction::ShowEventsItemAction(const QDateTime &from, const QDateTime &to, const std::shared_ptr<LogPresenter>& presenter)
    : fromTime(from)
    , toTime(to)
    , logPresenter(presenter)
{
}

void ShowEventsItemAction::processAcknowledgement(QStandardItem *item)
{
    model::DatabaseIteratorReader reader(DatabaseReaderArm::reader()->model());
    model::StateConfirmationEventIterator sit = reader.stateConfirmationEventIterator(
                fromTime.toMSecsSinceEpoch()
                , toTime.toMSecsSinceEpoch()
                , item->parent()->data(UserRoles::RemoteStationPointerRole).value<model::RemoteStationPointer>()->id());
    for (sit->first(); !sit->done(); sit->next())
    {
        emit logPresenter->ackReceived(sit->current());
    }
}

void ShowEventsItemAction::processRegulations(QStandardItem *item)
{
    model::DatabaseIteratorReader reader(DatabaseReaderArm::reader()->model());
    model::StateLockEventIterator sit = reader.stateLockEventIterator(
                fromTime.toMSecsSinceEpoch()
                , toTime.toMSecsSinceEpoch()
                , item->parent()->data(UserRoles::RemoteStationPointerRole).value<model::RemoteStationPointer>()->id());

    for (sit->first(); !sit->done(); sit->next())
    {
        emit logPresenter->regulationsReceived(sit->current());
    }
}

void ShowEventsItemAction::processSignal(QStandardItem *item)
{
    model::UnitItemId senderId = item->data(UserRoles::ControlObjectIdRole).value<model::UnitItemId>();
    model::UnitItemId signalId = item->data(UserRoles::SignalIdRole).value<model::UnitItemId>();
    DatabaseReaderArm::binder()->registerSignalChangeListener(logPresenter->holder()->listener()
        , senderId
        , view::ModelBinder::ChangeType::Change | view::ModelBinder::ChangeType::AllSenders
        , signalId);
    emitAll(model::DatabaseReader::EventType::Signal, signalId);
}

void ShowEventsItemAction::processState(QStandardItem *item)
{
    model::UnitItemId senderId = item->data(UserRoles::ControlObjectIdRole).value<model::UnitItemId>();
    model::UnitItemId stateId = item->data(UserRoles::StateIdRole).value<model::UnitItemId>();
    DatabaseReaderArm::binder()->registerStateChangeListener(logPresenter->holder()->listener()
        , senderId
        , view::ModelBinder::ChangeType::Change | view::ModelBinder::ChangeType::AllSenders
        , stateId
        , 0
        , model::StateInfo::noShow());
    emitAll(model::DatabaseReader::EventType::State, stateId);
}

void ShowEventsItemAction::processCommand(QStandardItem *item)
{
    model::UnitItemId senderId = item->data(UserRoles::ControlObjectIdRole).value<model::UnitItemId>();
    model::SimpleCommandPointer command = item->data(UserRoles::CommandPointerRole).value<model::SimpleCommandPointer>();
    DatabaseReaderArm::binder()->registerCommandChangeListener(logPresenter->holder()->listener()
        , senderId
        , view::ModelBinder::ChangeType::Change | view::ModelBinder::ChangeType::AllSenders
        , command->id());
    emitAll(model::DatabaseReader::EventType::Command, command->id());
}

void ShowEventsItemAction::emitAll(const model::DatabaseReader::EventType &eventType, const model::UnitItemId &id)
{
    auto load([=]()
    {
        DatabaseReaderArm::reader()->loadById(fromTime.toMSecsSinceEpoch()
                        , toTime.toMSecsSinceEpoch()
                        , eventType
                        , id
                        , false);
    });
    auto decoratedLoad(
        DimmingMessage::decorateWithSplashScreen(load));

    decoratedLoad();
    DatabaseReaderArm::reader()->emitAll();

    DatabaseReaderArm::binder()->unregisterAll();
}

struct LogFrame::Impl
{
    Impl(Ui::LogFrame *newUi, LogFrame *newParent)
        : ui(newUi)
        , parent(newParent)
        , logPresenter(std::make_shared<LogPresenter>(&eventLogModel))
    {
        logPresenter->setShowControlObjectName(true);
        setupModels();
        setupWidgets();
        loadRemoteStations();
    }

    void setupModels()
    {
        controlObjectsModel.setColumnCount(1);
        controlObjectsModel.setHorizontalHeaderItem(
                    0, new QStandardItem(tr("Выберите элементы для отображения")));

        connect(&controlObjectsModel, &QStandardItemModel::itemChanged, [this] (QStandardItem *item)
        {
            if (item->checkState() == Qt::Checked)
            {
                setChecked(item);
                if (reportPresetsApplied)
                {
                    (AddPresetItemAction(ui->presetWidget))({ item });
                }
            }
            else if (item->checkState() == Qt::Unchecked)
            {
                setUnchecked(item);
                if (reportPresetsApplied)
                {
                    (DeletePresetItemAction(ui->presetWidget))({ item });
                }
            }

            updatePartiallyChecked(item->parent());
        });

        eventLogModel.setColumnCount(static_cast<int>(LogPresenter::Columns::_ColumnCount));
        eventLogModel.setHorizontalHeaderLabels({"*", "Тип", "Время события", "ТП", "ФИО","Сообщение"});
    }

    void setupWidgets()
    {
        setupButtons();
        setupEdits();
        setupViews();
        setupReportPresetWidget();

        ui->searchWidget->setModel(&controlObjectsModel);
        ui->searchWidget->setView(ui->controlObjectsTreeView);

        ui->mainSplitter->setStretchFactor(0, 1);
        ui->mainSplitter->setStretchFactor(1, 2);

        ui->splitter->setStretchFactor(0, 1);
        ui->splitter->setStretchFactor(1, 2);
    }

    void setupReportPresetWidget()
    {
        ui->presetWidget->hide();
        connect(ui->presetWidget, &ReportPresetWidget::presetsLoaded, [this]
        {
            ui->presetWidget->setPresetType(ReportPresetType::Log);
            reportPresetsApplied = false;
        });

        connect(ui->presetWidget, &ReportPresetWidget::presetAdded, [this]
        {
            QList<QStandardItem *> list;
            selectedIds(controlObjectsModel.item(0, 0), list);
            (AddPresetItemAction(ui->presetWidget))(list);
            ui->presetWidget->setCurrentPresetPeriod(ui->fromDateEdit->dateTime()
                                                     , ui->toDateEdit->dateTime());
        });

        connect(ui->presetWidget, &ReportPresetWidget::presetSelectionChanged, [this]
        {
            // Флаг нужен для того, чтобы при uncheckAll() не выполнялся код по изменению элементов пресета в setupModels()
            reportPresetsApplied = false;
            QList<QStandardItem *> list;
            allIds(controlObjectsModel.item(0, 0), list);
            uncheckAll();
            ApplyPresetItemAction action(ui->presetWidget);
            action(list);
            ui->fromDateEdit->blockSignals(true);
            ui->toDateEdit->blockSignals(true);
            ui->fromDateEdit->setDateTime(ui->presetWidget->currentPresetStartDate());
            ui->toDateEdit->setDateTime(ui->presetWidget->currentPresetFinishDate());
            ui->fromDateEdit->blockSignals(false);
            ui->toDateEdit->blockSignals(false);
            reportPresetsApplied = true;
        });

        ui->presetWidget->loadPresets();
    }

    void setupButtons()
    {
        dateButtonGroup.addButton(ui->buttonLastMonth);
        dateButtonGroup.addButton(ui->buttonLastWeek);
        dateButtonGroup.addButton(ui->buttonToday);

        ui->buttonToday->setChecked(Qt::Checked);


        connect(ui->buttonLastMonth, &QPushButton::clicked, [this] (bool)
        {
            ui->fromDateEdit->setDate(QDate::currentDate().addMonths(-1));
            ui->toDateEdit->setDate(QDate::currentDate());
        });

        connect(ui->buttonLastWeek, &QPushButton::clicked, [this] (bool)
        {
            ui->fromDateEdit->setDate(QDate::currentDate().addDays(-7));
            ui->toDateEdit->setDate(QDate::currentDate());
        });

        connect(ui->buttonToday, &QPushButton::clicked, [this] (bool)
        {
            ui->fromDateEdit->setDate(QDate::currentDate());
            ui->toDateEdit->setDate(QDate::currentDate());
        });

        connect(ui->showButton, &QPushButton::clicked, [this] (bool)
        {
            showEventsForPeriod(ui->fromDateEdit->dateTime(), ui->toDateEdit->dateTime());
        });

        connect(ui->uncheckButton, &QPushButton::clicked, [this] (bool)
        {
            uncheckAll();
        });

        connect(ui->expandButton, &QPushButton::clicked, [this] ()
        {
            static bool clicked = true;
            if (clicked)
            {
                ui->controlObjectsTreeView->expandToDepth(0);
                ui->expandButton->setText(tr("Свернуть"));
            }
            else
            {
                ui->controlObjectsTreeView->collapseAll();
                ui->expandButton->setText(tr("Развернуть"));
            }
            ui->expandButton->setDown(clicked);
            clicked = !clicked;
        });

        auto showPrintDialog([this]()
            {
                QString title("Журнал событий для ");
                QList<QStandardItem *> list;
                selectedControlObjects(controlObjectsModel.item(0, 0), list);
                for(int i = 0; i < list.size(); i++)
                {
                    title.append(list.at(i)->text());
                    title.append((i != list.size() - 1) ? ", " : "");
                }
                title.append(QString("<br>за период времени %1 - %2")
                    .arg(ui->fromDateEdit->text())
                    .arg(ui->toDateEdit->text()));
                QString userName(QString::fromStdString(
                    ModelArm::instance()->user()->name()));

                PrintingTask task(title, userName);
                QTableView *clone(ui->eventLogTableView->clone());
                clone->horizontalHeader()->hideSection(
                    static_cast<int>(LogPresenter::Columns::Icon));
                task.addItem(clone, "");

                PrinterDialog::instance()->suggest(task);
                PrinterDialog::instance()->show();
            });

        connect(ui->printButton
            , &QPushButton::clicked
            , DimmingMessage::decorateWithSplashScreen(showPrintDialog));

        ui->showPresetsButton->setCheckable(true);
        connect(ui->showPresetsButton, &QPushButton::toggled, [this] (bool checked)
        {
           ui->presetWidget->setVisible(checked);
        });
    }

    void setupEdits()
    {
        ui->fromDateEdit->setCalendarPopup(true);
        ui->fromDateEdit->setDate(QDate::currentDate());
        ui->toDateEdit->setCalendarPopup(true);
        ui->toDateEdit->setDate(QDate::currentDate());

        connect(ui->toDateEdit, &DateTimeEdit::dateTimeChanged, [this]
        {
            ui->presetWidget->setCurrentPresetPeriod(ui->fromDateEdit->dateTime()
                                                     , ui->toDateEdit->dateTime());
        });

        connect(ui->fromDateEdit, &DateTimeEdit::dateTimeChanged, [this]
        {
            ui->presetWidget->setCurrentPresetPeriod(ui->fromDateEdit->dateTime()
                                                     , ui->toDateEdit->dateTime());
        });
    }

    void setupViews()
    {
        ui->splitter->setStretchFactor(0, 1);
        ui->splitter->setStretchFactor(1, 2);
        ui->controlObjectsTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->controlObjectsTreeView->setSortingEnabled(true);
        ui->controlObjectsTreeView->sortByColumn(0, Qt::AscendingOrder);

        ui->eventLogTableView->setModel(&eventLogModel);
        ui->eventLogTableView->setIconSize(QSize(24, 24));
        ui->eventLogTableView->setColumnWidth(static_cast<int>(LogPresenter::Columns::Icon), 32);
        ui->eventLogTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->eventLogTableView->setSortingEnabled(true);
        ui->eventLogTableView->setContextMenuPolicy(Qt::CustomContextMenu);
        ui->eventLogTableView->setSelectionMode(QAbstractItemView::SingleSelection);
        QHeaderView *header = ui->eventLogTableView->horizontalHeader();
        header->setStretchLastSection(true);
        header->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
        header->setSectionResizeMode(static_cast<int>(LogPresenter::Columns::Icon), QHeaderView::Fixed);

        connect(parent, &LogFrame::eventsPresented, [this]()
        {
            ui->printButton->setEnabled(eventLogModel.rowCount());
        });
    }


    void loadRemoteStations()
    {
        model::RemoteStationIterator rsit = ModelArm::instance()->remoteStationIterator();
        if (rsit)
        {
            for (rsit->first(); !rsit->done(); rsit->next())
            {
                QStandardItem *item = new QStandardItem(rsit->current()->name().c_str());
                item->setData(QVariant::fromValue(ObjectTypes::RemoteStation), UserRoles::ObjectTypeRole);
                item->setData(QVariant::fromValue(rsit->current()), UserRoles::RemoteStationPointerRole);

                item->setCheckable(true);

                loadControlObjectsForRemoteStation(rsit->current(), item);

                QStandardItem *ackItem = new QStandardItem(tr("Квитирование"));
                ackItem->setData(QVariant::fromValue(ObjectTypes::Acknowledgement), UserRoles::ObjectTypeRole);
                ackItem->setCheckable(true);
                item->appendRow(ackItem);

                QStandardItem *regItem = new QStandardItem(tr("Регламентные работы"));
                regItem->setData(QVariant::fromValue(ObjectTypes::Regulations), UserRoles::ObjectTypeRole);
                regItem->setCheckable(true);
                item->appendRow(regItem);

                controlObjectsModel.appendRow(item);
            }
        }
    }

    void loadControlObjectsForRemoteStation(model::RemoteStationPointer remoteStation, QStandardItem *parent)
    {   
        if (remoteStation && parent)
        {
            model::SimpleUnitIterator uit = remoteStation->unitIterator();
            if (uit)
            {
                QStandardItem *unitsGroupItem = new QStandardItem(tr("Блоки"));
                unitsGroupItem->setCheckable(true);

                QMultiMap<model::SimpleControlObjectPointer, model::SimpleSignalPointer> sigs;
                QMultiMap<model::SimpleControlObjectPointer, model::SimpleStatePointer> states;
                QMultiMap<model::SimpleControlObjectPointer, model::SimpleCommandPointer> commands;

                for (uit->first(); !uit->done(); uit->next())
                {
                    model::SimpleControlObjectIterator coit = uit->current()->controlObjectIterator();
                    for (coit->first(); !coit->done(); coit->next())
                    {
                        fetchSignalsMapForControlObject(coit->current(), sigs);
                        fetchStatesMapForControlObject(coit->current(), states);
                        fetchCommandsMapForControlObject(coit->current(), commands);
                    }

                    fetchSignalsMapForControlObject(uit->current(), sigs);
                    fetchStatesMapForControlObject(uit->current(), states);
                    fetchCommandsMapForControlObject(uit->current(), commands);
                }

                model::ControlObjectIterator coit = ModelArm::instance()->controlObjectIterator();
                for (coit->first(); !coit->done(); coit->next())
                {
                    populateObjectsTree(parent, coit->current().get(), sigs, states, commands);
                }

                for (uit->first(); !uit->done(); uit->next())
                {
                    populateObjectsTree(unitsGroupItem, uit->current(), sigs, states, commands);
                }

                parent->appendRow(unitsGroupItem);
            }
        }
    }

    void populateObjectsTree(QStandardItem* parent
                        , model::SimpleControlObjectPointer controlObject
                        , QMultiMap<model::SimpleControlObjectPointer, model::SimpleSignalPointer>& sigs
                        , QMultiMap<model::SimpleControlObjectPointer, model::SimpleStatePointer>& states
                        , QMultiMap<model::SimpleControlObjectPointer, model::SimpleCommandPointer>& commands)
    {
        QStandardItem *controlObjectitem = new QStandardItem(QString::fromStdString(controlObject->name()));
        controlObjectitem->setData(QVariant::fromValue(ObjectTypes::ControlObject), UserRoles::ObjectTypeRole);
        controlObjectitem->setData(QVariant::fromValue(controlObject->id()), UserRoles::ControlObjectIdRole);
        controlObjectitem->setCheckable(true);

        QStandardItem *signalGroupItem = new QStandardItem(tr("Сигналы"));
        signalGroupItem->setCheckable(true);
        for (const auto& sig : sigs.values(controlObject))
        {
            QString signalName = QString::fromStdString(controlObject->decorateName(sig));
            QStandardItem *item = new QStandardItem(signalName);
            item->setData(QVariant::fromValue(ObjectTypes::Signal), UserRoles::ObjectTypeRole);
            item->setData(QVariant::fromValue(sig->id()), UserRoles::SignalIdRole);
            item->setData(QVariant::fromValue(controlObject->id()), UserRoles::ControlObjectIdRole);

            item->setCheckable(true);

            signalGroupItem->appendRow(item);
        }

        QStandardItem *stateGroupItem = new QStandardItem(tr("Состояния"));
        stateGroupItem->setCheckable(true);
        for (const auto& state : states.values(controlObject))
        {
            QStandardItem *item = new QStandardItem(state->name().c_str());
            item->setData(QVariant::fromValue(ObjectTypes::State), UserRoles::ObjectTypeRole);
            item->setData(QVariant::fromValue(state->id()), UserRoles::StateIdRole);
            item->setData(QVariant::fromValue(controlObject->id()), UserRoles::ControlObjectIdRole);

            item->setCheckable(true);

            stateGroupItem->appendRow(item);
        }

        QStandardItem *commandGroupItem = new QStandardItem(tr("Команды"));
        commandGroupItem->setCheckable(true);
        for (const auto& command : commands.values(controlObject))
        {
            QStandardItem *item = new QStandardItem(command->name().c_str());
            item->setData(QVariant::fromValue(ObjectTypes::Command), UserRoles::ObjectTypeRole);
            item->setData(QVariant::fromValue(command), UserRoles::CommandPointerRole);
            item->setData(QVariant::fromValue(controlObject->id()), UserRoles::ControlObjectIdRole);

            item->setCheckable(true);

            commandGroupItem->appendRow(item);
        }

        if (signalGroupItem->hasChildren())
        {
            controlObjectitem->appendRow(signalGroupItem);
        }
        if (stateGroupItem->hasChildren())
        {
            controlObjectitem->appendRow(stateGroupItem);
        }
        if (commandGroupItem->hasChildren())
        {
            controlObjectitem->appendRow(commandGroupItem);
        }
        if (controlObjectitem->hasChildren())
        {
            parent->appendRow(controlObjectitem);
        }
    }

    void fetchSignalsMapForControlObject(model::SimpleControlObjectPointer controlObject
                                         , QMultiMap<model::SimpleControlObjectPointer, model::SimpleSignalPointer>& map)
    {
        model::SimpleSignalIterator sit = controlObject->signalIterator();
        for (sit->first(); !sit->done(); sit->next())
        {
#warning PROVISIONAL
                if (sit->current()->signalInfo()->isBinary()
                        && sit->current()->signalInfo()->signalType()->showEvent() >= 3)
                {
                    if (!map.contains(controlObject, sit->current()))
                    {
                        map.insert(controlObject, sit->current());
                    }
                }
        }
    }

    void fetchStatesMapForControlObject(model::SimpleControlObjectPointer controlObject
                                         , QMultiMap<model::SimpleControlObjectPointer, model::SimpleStatePointer>& map)
    {
        model::SimpleStateIterator sit = controlObject->stateIterator();
        for (sit->first(); !sit->done(); sit->next())
        {
            if (!map.contains(controlObject, sit->current()))
            {
                map.insert(controlObject, sit->current());
            }
        }
    }

    void fetchCommandsMapForControlObject(model::SimpleControlObjectPointer controlObject
                                         , QMultiMap<model::SimpleControlObjectPointer, model::SimpleCommandPointer>& map)
    {
        model::SimpleCommandIterator cit = controlObject->commandIterator();
        for (cit->first(); !cit->done(); cit->next())
        {
            if (!map.contains(controlObject, cit->current()))
            {
                map.insert(controlObject, cit->current());
            }
        }
    }

    void showEventsForPeriod(const QDateTime& from, const QDateTime& to)
    {
        QList<QStandardItem *> list;
        selectedIds(controlObjectsModel.item(0, 0), list);

        ui->eventLogTableView->setUpdatesEnabled(false);
        ui->eventLogTableView->hide();
        eventLogModel.removeRows(0, eventLogModel.rowCount());

        ShowEventsItemAction(from, to, logPresenter)(list);

        DatabaseReaderArm::binder()->unregisterAll();

        ui->eventLogTableView->sortByColumn(static_cast<int>(LogPresenter::Columns::DateTime), Qt::AscendingOrder);
        ui->eventLogTableView->setUpdatesEnabled(true);
        emit parent->eventsPresented();
        ui->eventLogTableView->show();
    }

    void showEventsLogForControlObject(model::SimpleControlObjectPointer controlObject)
    {
        uncheckAll();
        ui->searchWidget->edit()->clear();
        QList<QStandardItem*> items = controlObjectsModel.findItems(QString::fromStdString(controlObject->name()), Qt::MatchRecursive);
        QAbstractItemModel *currentModel = ui->controlObjectsTreeView->model();
        const QModelIndexList& indexes = currentModel->match(currentModel->index(0, 0)
                                                         , Qt::DisplayRole
                                                         , QString::fromStdString(controlObject->name())
                                                         , 1
                                                         , Qt::MatchRecursive);
        for (const auto& item : items)
        {
            if (!(item->data(UserRoles::ControlObjectIdRole).value<model::UnitItemId>() != controlObject->id()))
            {
                item->setCheckState(Qt::Checked);
                ui->controlObjectsTreeView->setCurrentIndex(controlObjectsModel.indexFromItem(item));
            }
        }

        for (const auto& index : indexes)
        {
            if (!(index.data(UserRoles::ControlObjectIdRole).value<model::UnitItemId>() != controlObject->id()))
            {
                ui->controlObjectsTreeView->setCurrentIndex(index);
                ui->controlObjectsTreeView->expand(index);
                break;
            }
        }
    }

    void setChecked(QStandardItem *item)
    {
        QAbstractItemModel *currentModel = ui->controlObjectsTreeView->model();
        if (currentModel == ui->searchWidget->filterModel())
        {
            QAbstractProxyModel *proxyModel = dynamic_cast<QAbstractProxyModel*>(currentModel);
            if (proxyModel->mapFromSource(item->index()) == QModelIndex())
            {
                return;
            }
        }

        item->setCheckState(Qt::Checked);

        for (int row = 0; row < item->rowCount(); ++row)
        {
            for (int col = 0; col < item->columnCount(); ++col)
            {
                setChecked(item->child(row, col));
            }
        }

    }

    void setUnchecked(QStandardItem *item)
    {
        item->setCheckState(Qt::Unchecked);

        for (int row = 0; row < item->rowCount(); ++row)
        {
            for (int col = 0; col < item->columnCount(); ++col)
            {
                setUnchecked(item->child(row, col));
            }
        }
    }

    void updatePartiallyChecked(QStandardItem *item)
    {
        if (!item)
        {
            return;
        }

        if (!item->hasChildren())
        {
            return;
        }

        bool allChecked = true;
        bool allUnchecked = true;
        for (int row = 0; row < item->rowCount(); ++row)
        {
            for (int col = 0; col < item->columnCount(); ++col)
            {
                Qt::CheckState cs = item->child(row, col)->checkState();
                if (cs != Qt::Checked)
                {
                    allChecked = false;
                }

                if (cs != Qt::Unchecked)
                {
                    allUnchecked = false;
                }
            }
        }

        if (allChecked)
        {
            item->setCheckState(Qt::Checked);
        }
        else if (allUnchecked)
        {
            item->setCheckState(Qt::Unchecked);
        }
        else
        {
            item->setCheckState(Qt::PartiallyChecked);
        }
    }

    void uncheckAll()
    {
        for (int i = 0; i < controlObjectsModel.rowCount(); ++i)
        {
            setUnchecked(controlObjectsModel.item(0, i));
        }

        eventLogModel.removeRows(0, eventLogModel.rowCount());
    }

    void selectedIds(QStandardItem *parent, QList<QStandardItem *>& list)
    {
        for (int row = 0; row < parent->rowCount(); ++row)
        {
            for (int col = 0; col < parent->columnCount(); ++col)
            {
                selectedIds(parent->child(row, col), list);
            }
        }

        if (parent->checkState() == Qt::Checked)
        {
            list.append(parent);
        }
    }

    void allIds(QStandardItem *parent, QList<QStandardItem *>& list)
    {
        for (int row = 0; row < parent->rowCount(); ++row)
        {
            for (int col = 0; col < parent->columnCount(); ++col)
            {
                allIds(parent->child(row, col), list);
            }
        }

        list.append(parent);
    }

    void selectedControlObjects(QStandardItem *parent, QList<QStandardItem *>& list)
    {
        for (int row = 0; row < parent->rowCount(); ++row)
        {
            for (int col = 0; col < parent->columnCount(); ++col)
            {
                selectedControlObjects(parent->child(row, col), list);
            }
        }

        ObjectTypes objectType(parent->data(UserRoles::ObjectTypeRole).value<ObjectTypes>());
        bool isControlObject(objectType == ObjectTypes::ControlObject);
        if (parent->checkState() == Qt::Checked && isControlObject)
        {
            list.append(parent);
        }
    }

    Ui::LogFrame *ui;
    LogFrame *parent;

    QStandardItemModel controlObjectsModel, eventLogModel, reportPresetModel;
    std::shared_ptr<LogPresenter> logPresenter;
    QButtonGroup dateButtonGroup;
    bool reportPresetsApplied = false;
};

LogFrame::LogFrame(QWidget *parent) :
    IFrame(parent)
    , ui(new Ui::LogFrame)
{
    ui->setupUi(this);
    _pimpl = std::make_shared<Impl>(ui, this);
}

LogFrame::~LogFrame()
{
    delete ui;
}

void LogFrame::showEventLogForControlObject(model::SimpleControlObjectPointer controlObject)
{
    _pimpl->showEventsLogForControlObject(controlObject);
}


FrameId LogFrame::id() const
{
    return FrameId::Log;
}

bool LogFrame::activate(const FrameParameters &params)
{
    return params.id == id();
}

void LogFrame::init(model::SimpleModelPointer, const view::ModelBinderPointer& , const FrameParameters &)
{
}
