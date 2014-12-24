#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include <memory>

#include <QDateTime>
#include <QPushButton>
#include <QWidget>

#include <main/Model.h>

#include "frame/iframe.h"

namespace Ui {
class LogFrame;
}

class IItemsIterator;

class LogFrame : public IFrame
{
    Q_OBJECT
friend class IItemsIterator;
public:
    explicit LogFrame(QWidget *parent = 0);
    ~LogFrame();

    void showEventLogForControlObject(model::SimpleControlObjectPointer controlObject);


    // IFrame interface
public:
    virtual FrameId id() const;
    virtual bool activate(const FrameParameters &params);
    virtual void init(model::SimpleModelPointer model, const view::ModelBinderPointer& binder, const FrameParameters& params);

signals:
    void showRequest(const FrameParameters &params);
    void eventsPresented();

private:
    Ui::LogFrame *ui;
    QList<QPushButton*> filterButtons;
    QList<QWidget*> dateWidgets;

    struct Impl;
    std::shared_ptr<Impl> _pimpl;
};

#endif // LOGWIDGET_H
