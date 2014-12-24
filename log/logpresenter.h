#ifndef LOGPRESENTER_H
#define LOGPRESENTER_H

#include <QObject>
#include <binder/Types.hpp>
#include <utils/Types.hpp>

class QStandardItemModel;

namespace model {
    class StateConfirmationEvent;
    class StateLockEvent;
}

class LogPresenter : public QObject
{
    Q_OBJECT
public:
    enum class Columns : int { Icon, Type, DateTime, RemoteStation, User, Text, _ColumnCount };
    enum Severity { Info = 0, Warn = 5, Fail = 10 };

    explicit LogPresenter(QStandardItemModel *model, const int minSeverity = Severity::Info);
    ~LogPresenter();
    void setShowControlObjectName(bool show);
    const view::ModelBinderListenerHolderPointer& holder() const;

signals:
    void ackReceived(const model::StateConfirmationEvent&);
    void regulationsReceived(const model::StateLockEvent&);

private:
    struct Impl;
    model::UniquePointer<Impl> _pimpl;
};

#endif // LOGPRESENTER_H
