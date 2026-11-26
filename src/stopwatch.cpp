#include "stopwatch.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <time.h>

namespace {

qint64 bootClock() {
    // CLOCK_BOOTTIME is monotonic and, unlike CLOCK_MONOTONIC, includes time
    // spent suspended.
    timespec ts{};
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return qint64(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

qint64 wallClock() { return QDateTime::currentMSecsSinceEpoch(); }

QString defaultStateFile() {
    const QString state = qEnvironmentVariable("XDG_STATE_HOME", QDir::homePath() + "/.local/state");
    return state + "/stopwatch/state.json";
}

} // namespace

Stopwatch::Stopwatch(const QString &stateFile, Clock monotonic, Clock wall, QObject *parent)
    : QObject(parent),
      m_stateFile(stateFile.isEmpty() ? defaultStateFile() : stateFile),
      m_monotonic(monotonic ? monotonic : Clock(bootClock)),
      m_wall(wall ? wall : Clock(wallClock)) {
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &Stopwatch::tick);
    load();
    m_shown = seconds();
    schedule();
}

Stopwatch::~Stopwatch() { save(); }

qint64 Stopwatch::elapsedMs() const {
    return m_baseMs + (m_running ? m_monotonic() - m_anchor : 0);
}

void Stopwatch::toggle() { m_running ? pause() : start(); }

void Stopwatch::start() {
    if (m_running)
        return;
    setRunning(true);
    changed();
}

void Stopwatch::pause() {
    if (!m_running)
        return;
    setRunning(false);
    changed();
}

void Stopwatch::reset() {
    if (elapsedMs() == 0 && !m_running)
        return;
    Edit edit{Edit::Reset, 0};
    execute(edit);
    record(edit);
}

bool Stopwatch::setTime(const QString &text) {
    const qint64 value = parse(text);
    if (value < 0) {
        setError(tr("Type a time like 1:30:00, 45:00, or 1h 30m."));
        return false;
    }
    setError({});
    Edit edit{Edit::Set, value * 1000};
    execute(edit);
    record(edit);
    return true;
}

void Stopwatch::adjust(int seconds) {
    Edit edit{Edit::Adjust, seconds};
    execute(edit);
    if (edit.delta != 0)
        record(edit);
}

void Stopwatch::undo() {
    if (m_undo.isEmpty())
        return;
    Edit edit = m_undo.takeLast();
    // A reset that stopped a running watch by mistake comes back as if it
    // never happened: running again, with the paused stretch counted.
    if (edit.pausedAt >= 0 && !m_running) {
        m_baseMs += m_monotonic() - edit.pausedAt;
        setRunning(true);
    }
    shift(-edit.delta);
    m_redo.append(edit);
    emit historyChanged();
    changed();
}

void Stopwatch::redo() {
    if (m_redo.isEmpty())
        return;
    Edit edit = m_redo.takeLast();
    edit.delta = 0;
    edit.pausedAt = -1;
    execute(edit);
    m_undo.append(edit);
    emit historyChanged();
}

void Stopwatch::execute(Edit &edit) {
    const qint64 before = elapsedMs();
    switch (edit.kind) {
    case Edit::Set:
        edit.delta = edit.value - before;
        break;
    case Edit::Reset:
        edit.delta = -before;
        if (m_running) {
            edit.pausedAt = m_monotonic();
            setRunning(false);
        }
        break;
    case Edit::Adjust:
        // Never below zero: taking a minute off 0:00:20 lands on 0:00:00.
        edit.delta = qMax(edit.value * 1000, -before);
        break;
    }
    shift(edit.delta);
    changed();
}

void Stopwatch::shift(qint64 deltaMs) {
    m_baseMs += deltaMs;
    const qint64 now = elapsedMs();
    if (now < 0)
        m_baseMs -= now;
}

void Stopwatch::setRunning(bool running) {
    if (running == m_running)
        return;
    if (running) {
        m_anchor = m_monotonic();
    } else {
        m_baseMs = elapsedMs();
    }
    m_running = running;
    emit runningChanged();
}

void Stopwatch::record(const Edit &edit) {
    m_undo.append(edit);
    m_redo.clear();
    emit historyChanged();
}

void Stopwatch::changed() {
    save();
    m_shown = -1;
    tick();
}

void Stopwatch::tick() {
    const qint64 now = seconds();
    if (now != m_shown) {
        m_shown = now;
        emit secondsChanged();
    }
    schedule();
}

void Stopwatch::schedule() {
    if (!m_running) {
        m_timer.stop();
        return;
    }
    // Wake just after the next whole second rather than on a fixed interval,
    // so the shown time never drifts and nothing redraws in between.
    const qint64 untilNext = 1000 - elapsedMs() % 1000;
    m_timer.start(int(untilNext) + 2);
}

// The state is saved on every change rather than every second, so a crash or
// a closed window loses nothing and the disk sees a write only when you act.
void Stopwatch::save() {
    QDir().mkpath(QFileInfo(m_stateFile).absolutePath());
    QSaveFile file(m_stateFile);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(tr("Can't save the time to %1.").arg(m_stateFile));
        return;
    }
    const QJsonObject state{{"elapsed_ms", elapsedMs()}, {"running", m_running}, {"saved_at", m_wall()}};
    file.write(QJsonDocument(state).toJson(QJsonDocument::Compact));
    if (!file.commit())
        setError(tr("Can't save the time to %1.").arg(m_stateFile));
}

// A watch that was running when the app closed keeps counting the time it
// was closed, the way a phone's stopwatch does.
void Stopwatch::load() {
    QFile file(m_stateFile);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject state = QJsonDocument::fromJson(file.readAll()).object();
    qint64 elapsed = qMax<qint64>(0, state.value("elapsed_ms").toInteger());
    const bool running = state.value("running").toBool();
    if (running)
        elapsed += qMax<qint64>(0, m_wall() - state.value("saved_at").toInteger());
    m_baseMs = elapsed;
    if (running) {
        m_anchor = m_monotonic();
        m_running = true;
    }
}

void Stopwatch::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}

qint64 Stopwatch::parse(const QString &input) {
    const QString text = input.trimmed().toLower();
    if (text.isEmpty())
        return -1;

    // 1:02:03, 02:03, or 45. The first part may be any size (90:00 is an
    // hour and a half); the parts after it must be under 60.
    static const QRegularExpression colon(R"(^(\d{1,6})(?:[:.](\d{1,2}))?(?:[:.](\d{1,2}))?$)");
    if (const auto m = colon.match(text); m.hasMatch()) {
        QList<qint64> parts;
        for (int i = 1; i <= 3; ++i)
            if (!m.captured(i).isEmpty())
                parts.append(m.captured(i).toLongLong());
        for (int i = 1; i < parts.size(); ++i)
            if (parts[i] >= 60)
                return -1;
        qint64 total = 0;
        for (qint64 part : parts)
            total = total * 60 + part;
        return total;
    }

    // 1h 30m, 90m, 1h5s.
    static const QRegularExpression units(R"(^(?:(\d{1,6})\s*h)?\s*(?:(\d{1,6})\s*m)?\s*(?:(\d{1,6})\s*s)?$)");
    if (const auto m = units.match(text); m.hasMatch() && m.capturedLength(0) > 0)
        return m.captured(1).toLongLong() * 3600 + m.captured(2).toLongLong() * 60 + m.captured(3).toLongLong();

    return -1;
}

QString Stopwatch::format(qint64 seconds) {
    return QStringLiteral("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QChar('0'))
        .arg(seconds / 60 % 60, 2, 10, QChar('0'))
        .arg(seconds % 60, 2, 10, QChar('0'));
}
