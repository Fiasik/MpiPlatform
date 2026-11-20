#include <QtTest>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include "ResearchStore.h"
#include "ResearchWindow.h"
#include "PressureAnalysis.h"
#include "ChartFrame.h"
#include "Widgets/Chart/ChartView.h"

using namespace Researcher;
class ResearcherTest : public QObject {
    Q_OBJECT
    QString fixture = QString::fromUtf8("/media/vladislav-b/Tree/Library/рез/ЛУКОЙЛ-УНП/ВИСБРЕКИНГ/-/PV-2223/03_10_2024/");
    QByteArray bytes(const QString& path) { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
private slots:
    void readableReportsPreserveObservationsAndSelection() {
        QTemporaryDir dir;
        const auto folder = QStringLiteral("/archive/Plant/Valve-A/16_11_2026");
        const auto path = recordPath(dir.path(), folder, "1");
        const QJsonObject window{{"low", 10.0}, {"high", 20.0}, {"slope", 85.0}, {"delta", -5.0}, {"delta_percent", -5.5556}};
        QJsonObject analysis{{"valid", true}, {"method", 2}, {"method_title", "Крутизна · максимум"},
            {"basis_title", "Прямой ход"}, {"stroke_title", "прямой ход"}, {"result_percent", 20.0},
            {"global_slope", 90.0}, {"worst_slope", 72.0}, {"worst_delta", -18.0}, {"worst_delta_percent", -20.0},
            {"working_low", 10.0}, {"working_high", 90.0}, {"position_unit", "mm"}, {"pressure_unit", "bar"},
            {"selected_row", 0}, {"selected_window", window}, {"windows", QJsonArray{window}}};
        QJsonObject notes{{"quality", "uncertain"}, {"visibility", "slight"}, {"pressure", "Первое наблюдение"}};
        QJsonObject revision{{"created_utc", "2026-11-19T12:00:00.000Z"}, {"folder", folder}, {"run", "1"},
            {"analysis", analysis}, {"observations", notes}};
        QString error;
        QVERIFY2(appendRecord(path, revision, &error), qPrintable(error));
        const auto readable = readableResultPath(dir.path(), folder, "1");
        const auto report = QString::fromUtf8(bytes(readable));
        QVERIFY(report.contains("20,0000 %"));
        QVERIFY(report.contains("-5,5556 %"));
        QVERIFY(report.contains("mm/bar"));
        QVERIFY(report.contains("Первое наблюдение"));
        QVERIFY(report.contains("Есть сомнения"));
        QVERIFY(report.contains("19.11.2026 15:00:00 мск"));
        const auto index = QString::fromUtf8(bytes(dir.filePath("Сводка.md")));
        QVERIFY(index.contains("Valve-A / 16_11_2026"));
        notes.insert("pressure", "Уточнённое наблюдение");
        revision.insert("observations", notes);
        revision.insert("created_utc", "2026-11-19T13:00:00.000Z");
        QVERIFY2(appendRecord(path, revision, &error), qPrintable(error));
        const auto historyPath = QFileInfo(readable).dir().filePath("История_правок.md");
        const auto history = QString::fromUtf8(bytes(historyPath));
        QVERIFY(history.contains("Первое наблюдение"));
        QVERIFY(history.contains("Уточнённое наблюдение"));
        revision.insert("created_utc", "2026-11-19T14:00:00.000Z");
        QVERIFY(appendRecord(path, revision, &error));
        QCOMPARE(loadRecord(path, &error).value("revisions").toArray().size(), 2);
        const auto secondRun = readableResultPath(dir.path(), folder, "2");
        QCOMPARE(QDir::cleanPath(QFileInfo(readable).dir().absoluteFilePath("..")),
                 QDir::cleanPath(QFileInfo(secondRun).dir().absoluteFilePath("..")));
        QVERIFY(readable != readableResultPath(dir.path(), "/other/Plant/Valve-A/16_11_2026", "1"));
        revision.insert("analysis", QJsonObject{{"valid", false}});
        QVERIFY(appendRecord(path, revision, &error));
        const auto unavailable = QString::fromUtf8(bytes(readable));
        QVERIFY(unavailable.contains("Расчёт недоступен"));
        QVERIFY(!unavailable.contains("| Итог по выбранному методу |"));
    }
    void nestedArchiveNavigationAndRestore() {
        if (!QFile::exists(fixture+"Pressure_1.data")) QSKIP("Local archive unavailable");
        QTemporaryDir dir;
        const auto archive = dir.filePath("archive");
        const auto first = archive + "/site/deep/Valve-A/2026_11_01";
        const auto second = archive + "/other/deeper/Valve-B/2026_11_02";
        for (const auto& folder : {first, second}) {
            QVERIFY(QDir().mkpath(folder));
            for (const auto& kind : {"Pressure", "Task", "Friction"}) {
                const auto file = QString(kind)+"_1.data";
                QVERIFY(QFile::copy(fixture+file, QDir(folder).filePath(file)));
            }
        }
        QDir().mkpath(archive+"/not-a-main-session");
        QVERIFY(QFile::copy(fixture+"Task_1.data", archive+"/not-a-main-session/Stroke_1.data"));
        const auto workspace = dir.filePath("notes");
        QString selected;
        {
            ResearchWindow window(nullptr, workspace);
            window.openArchive(archive);
            auto* list = window.findChild<QListWidget*>("researchSessions");
            QTRY_COMPARE_WITH_TIMEOUT(list->count(), 2, 10000);
            QTRY_VERIFY(window.findChild<PressureAnalysis*>());
            const auto initial = window.currentRevision().value("folder").toString();
            window.findChild<QPlainTextEdit*>("note_pressure")->setPlainText("Keep this observation");
            window.findChild<QPushButton*>("nextSession")->click();
            selected = window.currentRevision().value("folder").toString();
            QVERIFY(selected != initial);
            window.findChild<QPushButton*>("previousSession")->click();
            QCOMPARE(window.currentRevision().value("folder").toString(), initial);
            QCOMPARE(window.findChild<QPlainTextEdit*>("note_pressure")->toPlainText(), "Keep this observation");
            auto* search = window.findChild<QLineEdit*>("sessionSearch");
            search->setText("Valve-A");
            int visible = 0;
            for (int i = 0; i < list->count(); ++i) visible += !list->item(i)->isHidden();
            QCOMPARE(visible, 1);
            search->clear();
            window.findChild<QPushButton*>("nextSession")->click();
            QCOMPARE(window.currentRevision().value("folder").toString(), selected);
        }
        {
            ResearchWindow restored(nullptr, workspace);
            QVERIFY(restored.restoreArchive());
            auto* list = restored.findChild<QListWidget*>("researchSessions");
            QTRY_COMPARE_WITH_TIMEOUT(list->count(), 2, 10000);
            QTRY_VERIFY(restored.findChild<PressureAnalysis*>());
            QCOMPARE(restored.currentRevision().value("folder").toString(), selected);
            const auto empty = dir.filePath("empty");
            QVERIFY(QDir().mkpath(empty));
            restored.openArchive(empty);
            QTRY_COMPARE_WITH_TIMEOUT(list->count(), 0, 10000);
            QVERIFY(!restored.findChild<PressureAnalysis*>());
            QVERIFY(restored.currentRevision().value("folder").toString().isEmpty());
        }
    }
    void historyAndCorruption() {
        QTemporaryDir dir;
        QString error;
        const auto path = recordPath(dir.path(), "/test/valve", "1");
        QVERIFY(appendRecord(path, {{"text", "first"}}, &error));
        QVERIFY(appendRecord(path, {{"text", "second"}}, &error));
        const auto record = loadRecord(path, &error);
        QCOMPARE(record.value("revisions").toArray().size(), 2);
        QCOMPARE(record.value("revisions").toArray()[0].toObject().value("text").toString(), "first");
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("broken"); f.close();
        QVERIFY(!appendRecord(path, {{"text", "third"}}, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(bytes(path), QByteArray("broken"));
    }
    void realCaseRestoreAndExport() {
        if (!QFile::exists(fixture+"Pressure_1.data")) QSKIP("Local archive unavailable");
        QTemporaryDir dir;
        const auto source = dir.filePath("valve"); QDir().mkpath(source);
        QMap<QString, QByteArray> originals;
        for (const auto& kind : {"Pressure", "Task", "Friction"}) {
            const auto name = QString(kind)+"_1.data";
            QVERIFY(QFile::copy(fixture+name, QDir(source).filePath(name)));
            originals.insert(name, bytes(QDir(source).filePath(name)));
        }
        const auto workspace = dir.filePath("notes");
        QString recordPath;
        {
            ResearchWindow window(nullptr, workspace);
            window.openPath(source);
            window.show();
            auto* analysis = window.findChild<PressureAnalysis*>();
            QVERIFY(analysis);
            QVERIFY(analysis->snapshot().value("valid").toBool());
            const auto frames = window.findChildren<Labeler::ChartFrame*>();
            QCOMPARE(frames.size(), 3);
            for (auto* f : frames) { QVERIFY(f->chart()); QVERIFY(!f->chart()->annotations().isEmpty()); }
            auto* basis = window.findChild<QComboBox*>("analysisBasis");
            basis->setCurrentIndex(1);
            auto* table = window.findChild<QTableWidget*>("analysisWindows");
            QVERIFY(table->rowCount()>1);
            table->setCurrentCell(1, 0);
            QCOMPARE(analysis->snapshot().value("selected_row").toInt(), 1);
            const auto selected = analysis->snapshot().value("selected_window").toObject();
            QVERIFY(!selected.isEmpty());
            auto* note = window.findChild<QPlainTextEdit*>("note_pressure");
            note->setPlainText(QString::fromUtf8("Вижу изменение наклона, причина неизвестна."));
            QVERIFY(window.saveObservation());
            recordPath = window.currentRecordPath();
            QVERIFY(window.exportCase(dir.filePath("export")));
            for (const auto& name : {"Pressure.png", "Task.png", "Friction.png", "Overview.png", "calculation.json"})
                QVERIFY2(QFileInfo::exists(dir.filePath("export/")+name), name);
            QVERIFY(bytes(dir.filePath(QString::fromUtf8("export/Разбор.md"))).contains(note->toPlainText().toUtf8()));
        }
        {
            ResearchWindow window(nullptr, workspace);
            window.openPath(source);
            QCOMPARE(window.findChild<QPlainTextEdit*>("note_pressure")->toPlainText(), QString::fromUtf8("Вижу изменение наклона, причина неизвестна."));
            QCOMPARE(window.findChild<PressureAnalysis*>()->snapshot().value("selected_row").toInt(), 1);
            auto* runs = window.findChild<QComboBox*>("researchRun");
            QCOMPARE(runs->count(), 1);
        }
        QVERIFY(QFileInfo::exists(recordPath));
        for (auto i = originals.cbegin(); i != originals.cend(); ++i) QCOMPARE(bytes(QDir(source).filePath(i.key())), i.value());
    }
    void missingCompanionNeverUsesOtherRun() {
        if (!QFile::exists(fixture+"Pressure_1.data")) QSKIP("Local archive unavailable");
        QTemporaryDir dir;
        QDir().mkpath(dir.filePath("valve"));
        QVERIFY(QFile::copy(fixture+"Pressure_1.data", dir.filePath("valve/Pressure_2.data")));
        QVERIFY(QFile::copy(fixture+"Task_1.data", dir.filePath("valve/Task_1.data")));
        ResearchWindow window(nullptr, dir.filePath("notes"));
        window.openPath(dir.filePath("valve/Pressure_2.data"));
        const auto sources = window.currentRevision().value("sources").toObject();
        QVERIFY(sources.value("Pressure").toObject().value("loaded").toBool());
        QVERIFY(!sources.value("Task").toObject().value("loaded").toBool());
        window.findChild<QComboBox*>("researchRun")->setCurrentIndex(0);
        QVERIFY(!window.findChild<PressureAnalysis*>());
        QVERIFY(!window.currentRevision().value("analysis").toObject().value("valid").toBool());
    }
};
QTEST_MAIN(ResearcherTest)
#include "ResearcherTest.moc"
