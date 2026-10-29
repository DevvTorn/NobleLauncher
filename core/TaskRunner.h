#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <functional>

namespace Noble {

class TaskRunner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString stage READ stage NOTIFY progressChanged)
    Q_PROPERTY(QString message READ message NOTIFY progressChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(bool cancellable READ cancellable CONSTANT)

public:
    explicit TaskRunner(QObject *parent = nullptr);

    void start(const QString &title, const std::function<void(TaskRunner *)> &fn, bool cancellable = true);
    void cancel();
    bool running() const { return m_running; }
    QString title() const { return m_title; }
    QString stage() const { return m_stage; }
    QString message() const { return m_message; }
    double progress() const { return m_progress; }
    bool cancellable() const { return m_cancellable; }
    std::atomic_bool *cancelFlag() { return &m_cancelled; }

    /** Потокобезопасные репортеры (можно вызывать из рабочего потока). */
    void report(const QString &stage, const QString &message, double progress);
    void logLine(const QString &type, const QString &text);

signals:
    void runningChanged();
    void titleChanged();
    void progressChanged();
    void stageChanged(const QString &stage, const QString &message, double progress);
    void logLineEmitted(const QString &type, const QString &text);
    void finished(bool ok, const QString &error);
    void cancelled();

private:
    void setFinished(bool ok, const QString &error);
    std::atomic_bool m_cancelled{false};
    bool m_running = false;
    bool m_cancellable = true;
    QString m_title;
    QString m_stage;
    QString m_message;
    double m_progress = 0;
};

} // namespace Noble
