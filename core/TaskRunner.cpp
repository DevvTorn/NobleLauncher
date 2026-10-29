#include "TaskRunner.h"

#include "Logger.h"

#include <QMetaObject>
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>

namespace Noble {

TaskRunner::TaskRunner(QObject *parent)
    : QObject(parent)
{
}

void TaskRunner::start(const QString &title, const std::function<void(TaskRunner *)> &fn, bool cancellable)
{
    if (m_running)
        return;
    m_title = title;
    m_stage.clear();
    m_message.clear();
    m_progress = 0;
    m_cancellable = cancellable;
    m_cancelled = false;
    m_running = true;
    emit titleChanged();
    emit progressChanged();
    emit runningChanged();

    QPointer<TaskRunner> guard(this);
    QtConcurrent::run([guard, fn, this] {
        QString error;
        bool ok = true;
        try {
            fn(this);
        } catch (const std::exception &e) {
            ok = false;
            error = QString::fromUtf8(e.what());
        } catch (...) {
            ok = false;
            error = QStringLiteral("Неизвестная ошибка");
        }
        if (m_cancelled.load())
            ok = false;
        QMetaObject::invokeMethod(this, [this, ok, error] { setFinished(ok, error); }, Qt::QueuedConnection);
    });
}

void TaskRunner::cancel()
{
    if (!m_running)
        return;
    m_cancelled = true;
    emit cancelled();
}

void TaskRunner::report(const QString &stage, const QString &message, double progress)
{
    QMetaObject::invokeMethod(this, [this, stage, message, progress] {
        if (!m_running)
            return;
        m_stage = stage;
        m_message = message;
        m_progress = progress;
        emit progressChanged();
        emit stageChanged(stage, message, progress);
    }, Qt::QueuedConnection);
}

void TaskRunner::logLine(const QString &type, const QString &text)
{
    Logger::debug(QStringLiteral("[task] %1: %2").arg(type, text));
    QMetaObject::invokeMethod(this, [this, type, text] { emit logLineEmitted(type, text); }, Qt::QueuedConnection);
}

void TaskRunner::setFinished(bool ok, const QString &error)
{
    if (!m_running)
        return;
    m_running = false;
    emit runningChanged();
    emit finished(ok, error);
}

} // namespace Noble
