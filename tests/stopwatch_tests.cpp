#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include "stopwatch.h"
#include "theme.h"
#include "translator.h"

// A clock the test moves by hand.
struct FakeClock {
    qint64 now = 1000000;
    Stopwatch::Clock fn() { return [this] { return now; }; }
};

class StopwatchTests : public QObject {
    Q_OBJECT

private slots:
    void countsUpWhileRunning()
    {
        QTemporaryDir dir;
        FakeClock mono, wall;
        Stopwatch sw(dir.filePath("state.json"), mono.fn(), wall.fn());
        QCOMPARE(sw.display(), QString("00:00:00"));
        sw.start();
        mono.now += 61500;
        QCOMPARE(sw.seconds(), 61);
        QCOMPARE(sw.display(), QString("00:01:01"));
        sw.pause();
        mono.now += 10000;
        QCOMPARE(sw.seconds(), 61);
    }

    void notifiesOnlyWhenTheSecondChanges()
    {
        QTemporaryDir dir;
        FakeClock mono, wall;
        Stopwatch sw(dir.filePath("state.json"), mono.fn(), wall.fn());
        sw.start();
        QSignalSpy spy(&sw, &Stopwatch::secondsChanged);
        mono.now += 400;
        sw.tick();
        QCOMPARE(spy.count(), 0);
        mono.now += 700;
        sw.tick();
        QCOMPARE(spy.count(), 1);
    }

    void startsFromATypedTime()
    {
        QTemporaryDir dir;
        FakeClock mono, wall;
        Stopwatch sw(dir.filePath("state.json"), mono.fn(), wall.fn());
        QVERIFY(sw.setTime("1:30:00"));
        QCOMPARE(sw.display(), QString("01:30:00"));
        sw.start();
        mono.now += 5000;
        QCOMPARE(sw.display(), QString("01:30:05"));
        QVERIFY(!sw.setTime("nonsense"));
        QVERIFY(!sw.error().isEmpty());
        QCOMPARE(sw.display(), QString("01:30:05"));
    }

    void settingWhileRunningLandsExactly()
    {
        QTemporaryDir dir;
        FakeClock mono, wall;
        Stopwatch sw(dir.filePath("state.json"), mono.fn(), wall.fn());
        sw.start();
        mono.now += 2700; // part-way through a second
        QVERIFY(sw.setTime("10:00"));
        QCOMPARE(sw.elapsedMs(), 600000);
        QVERIFY(sw.running());
    }

    void parsesTheUsualForms()
    {
        QCOMPARE(Stopwatch::parse("45"), 45);
        QCOMPARE(Stopwatch::parse("2:03"), 123);
        QCOMPARE(Stopwatch::parse("1:02:03"), 3723);
        QCOMPARE(Stopwatch::parse("90:00"), 5400);
        QCOMPARE(Stopwatch::parse(" 1h 30m "), 5400);
        QCOMPARE(Stopwatch::parse("45m"), 2700);
        QCOMPARE(Stopwatch::parse("1h5s"), 3605);
        QCOMPARE(Stopwatch::parse("1:60"), -1);
        QCOMPARE(Stopwatch::parse(""), -1);
        QCOMPARE(Stopwatch::parse("h"), -1);
        QCOMPARE(Stopwatch::parse("1:2:3:4"), -1);
    }

    void formatsPastADay()
    {
        QCOMPARE(Stopwatch::format(0), QString("00:00:00"));
        QCOMPARE(Stopwatch::format(36000), QString("10:00:00"));
        QCOMPARE(Stopwatch::format(100 * 3600 + 61), QString("100:01:01"));
    }

    void adjustsButNeverBelowZero()
    {
        QTemporaryDir dir;
        FakeClock mono, wall;
        Stopwatch sw(dir.filePath("state.json"), mono.fn(), wall.fn());
        sw.adjust(60);
        QCOMPARE(sw.seconds(), 60);
        sw.adjust(-3600);
        QCOMPARE(sw.seconds(), 0);
        sw.undo();
        QCOMPARE(sw.seconds(), 60);
    }

    void undoingAResetKeepsTheTimeThatPassed()
    {
        QTemporaryDir dir;
        FakeClock mono, wall;
        Stopwatch sw(dir.filePath("state.json"), mono.fn(), wall.fn());
        sw.start();
        mono.now += 3600000;
        sw.reset();
        QCOMPARE(sw.seconds(), 0);
        QVERIFY(!sw.running());
        mono.now += 30000;
        sw.undo();
        QVERIFY(sw.running());
        QCOMPARE(sw.seconds(), 3630);
        sw.redo();
        QCOMPARE(sw.seconds(), 0);
        QVERIFY(!sw.running());
    }

    void undoingASetKeepsCounting()
    {
        QTemporaryDir dir;
        FakeClock mono, wall;
        Stopwatch sw(dir.filePath("state.json"), mono.fn(), wall.fn());
        sw.start();
        mono.now += 10000;
        QVERIFY(sw.setTime("2:00:00"));
        mono.now += 5000;
        sw.undo();
        QCOMPARE(sw.seconds(), 15);
        sw.redo();
        QCOMPARE(sw.display(), QString("02:00:00"));
    }

    void keepsCountingAcrossARestart()
    {
        QTemporaryDir dir;
        FakeClock mono, wall;
        {
            Stopwatch sw(dir.filePath("state.json"), mono.fn(), wall.fn());
            QVERIFY(sw.setTime("1:00:00"));
            sw.start();
            mono.now += 2000;
            wall.now += 2000;
        }
        wall.now += 60000; // closed for a minute
        mono.now = 5;      // and the machine rebooted
        Stopwatch again(dir.filePath("state.json"), mono.fn(), wall.fn());
        QVERIFY(again.running());
        QCOMPARE(again.seconds(), 3662);
    }

    void aPausedWatchStaysPausedAcrossARestart()
    {
        QTemporaryDir dir;
        FakeClock mono, wall;
        {
            Stopwatch sw(dir.filePath("state.json"), mono.fn(), wall.fn());
            QVERIFY(sw.setTime("5:00"));
        }
        wall.now += 600000;
        Stopwatch again(dir.filePath("state.json"), mono.fn(), wall.fn());
        QVERIFY(!again.running());
        QCOMPARE(again.seconds(), 300);
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Reset").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Reset": "Repor", "Pause": "Pausa"})");
        write(dir.path() + "/pt-BR.json", R"({"Reset": "Zerar"})");
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "Reset"), QString("Zerar"));
        QCOMPARE(translator.translate("Main", "Pause"), QString("Pausa"));
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(StopwatchTests)
#include "stopwatch_tests.moc"
