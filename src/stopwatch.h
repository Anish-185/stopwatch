#pragma once

#include <QObject>
#include <QTimer>
#include <QVector>
#include <functional>

// A count-up stopwatch that can start from any value. It keeps time as a base
// plus the time since it was last started, and only wakes once a second, on
// the second boundary, so the window redraws once a second and no more.
class Stopwatch : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(qint64 seconds READ seconds NOTIFY secondsChanged)
    Q_PROPERTY(QString display READ display NOTIFY secondsChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    using Clock = std::function<qint64()>;

    // The state file and both clocks can be swapped out by tests. The
    // monotonic clock keeps counting through suspend, so a laptop lid closed
    // mid-session still counts as time passed, like a real stopwatch.
    explicit Stopwatch(const QString &stateFile = {}, Clock monotonic = {}, Clock wall = {},
                       QObject *parent = nullptr);
    ~Stopwatch() override;

    bool running() const { return m_running; }
    qint64 elapsedMs() const;
    qint64 seconds() const { return elapsedMs() / 1000; }
    QString display() const { return format(seconds()); }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QString error() const { return m_error; }

    Q_INVOKABLE void toggle();
    Q_INVOKABLE void start();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void reset();
    Q_INVOKABLE bool setTime(const QString &text);
    Q_INVOKABLE void adjust(int seconds);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    // Accepts 1:02:03, 02:03, 45, and 1h 2m 3s. Returns -1 when it can't
    // read the text.
    Q_INVOKABLE static qint64 parse(const QString &text);
    static QString format(qint64 seconds);

    // Re-reads the clock and notifies if the shown second changed. The timer
    // calls it; tests call it after moving their fake clock.
    void tick();

signals:
    void runningChanged();
    void secondsChanged();
    void historyChanged();
    void errorChanged();

private:
    // Each edit is kept as the change it made, so undoing it removes just
    // that change and keeps whatever time has passed since.
    struct Edit {
        enum Kind { Set, Reset, Adjust } kind;
        qint64 value;      // target ms for Set, seconds for Adjust
        qint64 delta = 0;  // ms added to the base
        qint64 pausedAt = -1; // monotonic ms when a Reset paused a running watch
    };

    void execute(Edit &edit);
    void shift(qint64 deltaMs);
    void setRunning(bool running);
    void record(const Edit &edit);
    void changed();
    void schedule();
    void load();
    void save();
    void setError(const QString &error);

    QString m_stateFile;
    Clock m_monotonic;
    Clock m_wall;
    qint64 m_baseMs = 0;
    qint64 m_anchor = 0;
    bool m_running = false;
    qint64 m_shown = -1;
    QVector<Edit> m_undo, m_redo;
    QTimer m_timer;
    QString m_error;
};
