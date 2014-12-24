#include <iostream>
#include <QDateTime>
#include <QStandardItemModel>
#include <QDebug>

#include <binder/CommandWrapper.h>
#include <binder/ModelBinderListener.h>
#include <binder/ModelBinderListenerHolder.h>
#include <binder/SignalWrapper.h>
#include <binder/StateWrapper.h>
#include <command/Command.h>
#include <command/CommandState.h>
#include <control/ControlObject.h>
#include <event/StateConfirmationEvent.h>
#include <event/StateLockEvent.h>
#include <signal/Signal.h>
#include <state/State.h>
#include <state/StateInfo.h>
#include <station/RemoteStation.h>
#include <unit/Unit.h>
#include <user/User.h>

#include "utils/types.h"
#include "modelarm.h"
#include "logpresenter.h"

struct LogPresenter::Impl
{
    Impl(LogPresenter *newParent, QStandardItemModel *newModel)
        : model(newModel)
        , parent(newParent)
        , holder(view::ModelBinderListenerHolder::create())
        , showControlObjectName()
        , minSeverity(0)
    {
        QObject::connect(holder->sender(), &view::ModelBinderListener::signalChanged, [this] (const view::SignalWrapper& sw)
        {
            QDateTime time = QDateTime::fromMSecsSinceEpoch(sw.event().timestamp());
            QString tp = QString::fromStdString(sw.event().remoteStationName());
            QString controlObjectName = QString::fromStdString(
                showControlObjectName ? sw.controlObject()->name() + ": " : "");
            QString text = QString::fromStdString(controlObjectName.toStdString() + sw.event().fullName());

            QString icon = sw.event().boolValue() ? ":/images/icons/signal.png" : ":/images/icons/signal_inactive.png";
            appendRow(icon, tr("Сигнал"), time, tp, QString(), text, sw.event().id(), sw.event().controlObjectId());
        });

        QObject::connect(holder->sender(), &view::ModelBinderListener::stateChanged, [this] (const view::StateWrapper& sw)
        {
            QString iconPath;
            const int severity(sw.event().info()->showEvent());
            if (severity < minSeverity &&
                    !sw.event().active()) {
                return;
            }

            if (severity >= Severity::Fail) {
                iconPath = ":/images/icons/fail.png";
            } else if (severity >= Severity::Warn) {
                iconPath = ":/images/icons/warn.png";
            } else if (severity >= Severity::Info) {
                iconPath = ":/images/icons/info.png";
            }

            if(!sw.event().active())
                iconPath.replace(QRegExp("\\.png$"), "_bw.png");

            QDateTime time = QDateTime::fromMSecsSinceEpoch(sw.event().timestamp());
            QString tp = QString::fromStdString(sw.event().remoteStationName());
            QString controlObjectName = QString::fromStdString(
                showControlObjectName ? sw.controlObject()->unit()->innerControlObjectByState(sw.event().id())->name() + ": " : "");
            QString text = QString::fromStdString(controlObjectName.toStdString() + sw.event().text());

            appendRow(iconPath, tr("Состояние"), time, tp, QString(), text, sw.event().id(), sw.event().controlObjectId());
        });

        QObject::connect(holder->sender(), &view::ModelBinderListener::commandChanged, [this] (const view::CommandWrapper& cw)
        {
            QDateTime time = QDateTime::fromMSecsSinceEpoch(cw.event().timestamp());
            QString tp = QString::fromStdString(cw.event().remoteStationName());
            QString controlObjectName = QString::fromStdString(
                showControlObjectName ? cw.controlObject()->name() + ": " : "");
            QString text = QString::fromStdString(controlObjectName.toStdString() + cw.event().name());

            model::UserIterator uit = model::User::load(ModelArm::instance());
            QString user;
            for (uit->first(); !uit->done(); uit->next())
            {
                if (uit->current()->id() == cw.event().userId())
                {
                    user = QString::fromStdString(uit->current()->name());
                    break;
                }
            }

            QString icon;
            if (cw.event().state().sended())
            {
                icon = ":/images/icons/command_sent.png";
                text += tr(" [Отправлена]");
            }
            else if (cw.event().state().received())
            {
                if (cw.event().state().correct())
                {
                    icon = ":/images/icons/command_received.png";
                    text += tr(" [Принята]");
                }
                if (cw.event().state().incorrect())
                {
                    icon = ":/images/icons/command_received.png";
                    text += tr(" [Некорректна]");
                }
                if (cw.event().state().fault())
                {
                    icon = ":/images/icons/fail.png";
                    text += tr(" [Ошибка блока]");
                }
            }
            else
            {
                icon = ":/images/icons/cog.png";
            }

            appendRow(icon, tr("Команда"), time, tp, user, text, cw.event().id(), cw.event().controlObjectId());
        });

        QObject::connect(parent, &LogPresenter::ackReceived, [this] (const model::StateConfirmationEvent& ack)
        {
            QDateTime time = QDateTime::fromMSecsSinceEpoch(ack.timestamp());
            QString tp = QString::fromStdString(ack.remoteStationName());
            QString controlObjectName = QString::fromStdString(
                showControlObjectName ? ack.controlObjectName() + ": " : "");

            QString text = QString::fromStdString(controlObjectName.toStdString() + ack.name());
            appendRow(":/images/icons/cog.png", tr("Квитирование"), time, tp, QString::fromStdString(ack.userName()), text, ack.id(), ack.controlObjectId());
        });

        QObject::connect(parent, &LogPresenter::regulationsReceived, [this] (const model::StateLockEvent& lock)
        {
            QDateTime time = QDateTime::fromMSecsSinceEpoch(lock.timestamp());
            QString tp = QString::fromStdString(lock.remoteStationName());
            QString controlObjectName = QString::fromStdString(
                showControlObjectName ? lock.controlObjectName() + ": " : "");

            QString text = QString::fromStdString(controlObjectName.toStdString() + lock.name());

            QString icon = lock.locked() ? ":/images/icons/lock.png" : ":/images/icons/unlock.png";
            appendRow(icon, tr("Регламентные работы"), time, tp, QString::fromStdString(lock.userName()), text, lock.id(), lock.controlObjectId());
        });
    }

    void appendRow(const QString& iconPath
        , const QString& type
        , const QDateTime& time
        , const QString& tp
        , const QString& user
        , const QString& text
        , const model::UnitItemId& id
        , const model::UnitItemId& controlObjectId)
    {
        QStandardItem *iconItem = new QStandardItem(QIcon(iconPath), "");

        QStandardItem *typeItem = new QStandardItem(type);
        QStandardItem *timeItem = new QStandardItem(time.toString("dd.MM.yy hh:mm:ss.zzz"));
        timeItem->setData(QVariant::fromValue(time), UserRoles::EventLogViewRoles::DateTimeRole);

        QStandardItem *tpItem = new QStandardItem(tp);
        tpItem->setData(QVariant::fromValue(id.unitId.remoteStationId), UserRoles::EventLogViewRoles::TpIndexRole);

        QStandardItem *userItem = new QStandardItem(user);

        QStandardItem *msgItem = new QStandardItem(text);
        msgItem->setData(QVariant::fromValue(controlObjectId), UserRoles::ControlObjectIdRole);
        msgItem->setData(QVariant::fromValue(id), UserRoles::StateIdRole);

        model->appendRow({ iconItem, typeItem, timeItem, tpItem, userItem, msgItem });
    }

    QStandardItemModel *model;
    LogPresenter *parent;
    view::ModelBinderListenerHolderPointer holder;
    bool showControlObjectName;
    int minSeverity;
};

LogPresenter::LogPresenter(QStandardItemModel *model, const int minSeverity)
{
    try
    {
        _pimpl = ::model::UniquePointer<Impl>(new Impl(this, model));
        _pimpl->minSeverity = minSeverity;
    }
    catch(const std::exception &e)
    {
        qWarning() << e.what();
    }
}

LogPresenter::~LogPresenter() = default;

void LogPresenter::setShowControlObjectName(bool show)
{
    _pimpl->showControlObjectName = show;
}

const view::ModelBinderListenerHolderPointer& LogPresenter::holder() const
{
    return _pimpl->holder;
}
