#include <QtTest>
#include <QComboBox>
#include <QDataStream>
#include <QFile>
#include <QLabel>
#include <QTableWidget>
#include <QToolBar>
#include <QPushButton>
#include <QTreeView>
#include <QTemporaryDir>
#include <QDir>
#include <QTabWidget>
#include <QGraphicsRectItem>
#include "PressureAnalysis.h"
#include "ViewerWindow.h"
#include "Widgets/Chart/ChartView.h"

using Widgets::Chart::ChartView;
using namespace Domain::Tests::Main::Linearity;

class PressureAnalysisTest : public QObject
{
    Q_OBJECT
private slots:
    void realFile_data()
    {
        QTest::addColumn<QString>("relative");
        QTest::newRow("pv2223") << QString::fromUtf8("ЛУКОЙЛ-УНП/ВИСБРЕКИНГ/-/PV-2223/03_10_2024/Pressure_1.data");
        QTest::newRow("small-reference") << QString::fromUtf8("2024/ЛУКРЙЛ-УНП/ППНН/АТ-1/pм 7254a/16_05_2024/Pressure_1.data");
        QTest::newRow("missing-windows") << QString::fromUtf8("2023/ООО ИНК/УКПГ-3/ГПА/15 часть 16 ед/3-SDV-3402/30_08_2023/Pressure_1.data");
    }
    void realFile()
    {
        QFETCH(QString, relative);
        const QString path = QString::fromUtf8("/media/vladislav-b/Tree/Library/рез/")+relative;
        if (!QFile::exists(path)) QSKIP("Local archive fixture unavailable");
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray original = file.readAll();
        file.seek(0);
        QDataStream stream(&file);
        stream.setVersion(QDataStream::Qt_6_2);
        auto* chart = new ChartView;
        chart->setLiveRendering(false);
        chart->loadFromStream(stream);
        QCOMPARE(stream.status(), QDataStream::Ok);
        const auto points = chart->getPoints(0);
        const auto regression = chart->getPoints(1);
        PressureAnalysis panel(chart);
        panel.resize(1400, 900);
        auto* method = panel.findChild<QComboBox*>("analysisMethod");
        auto* basis = panel.findChild<QComboBox*>("analysisBasis");
        auto* table = panel.findChild<QTableWidget*>("analysisWindows");
        auto* summary = panel.findChild<QLabel*>("analysisSummary");
        QVERIFY(method && basis && table && summary);
        QVERIFY(table->rowCount() > 0);
        QVERIFY(!chart->annotations().isEmpty());
        QCOMPARE(chart->getPoints(1), regression);
        if (!regression.first.isEmpty() || !regression.second.isEmpty())
            QVERIFY(chart->isSeriesVisible(1));
        QCOMPARE(chart->getPoints(0), points);
        for (int m = 0; m < method->count(); ++m) {
            method->setCurrentIndex(m);
            for (int b = 0; b < basis->count(); ++b) {
                basis->setCurrentIndex(b);
                const auto expected = computeLinearity(LinearityMethod(method->currentData().toInt()),
                    points.second, points.first, StrokeBasis(basis->currentData().toInt()));
                if (!expected.valid) {
                    QVERIFY(summary->text().contains(QString::fromUtf8("Нет пригодного результата")));
                    QCOMPARE(chart->getPoints(0), points);
                    continue;
                }
                QVERIFY(summary->text().contains(QLocale(QLocale::Russian).toString(expected.value, 'f', 4)));
                QCOMPARE(chart->getPoints(0), points);
                if (table->rowCount()) {
                    table->setCurrentCell(0, 0);
                    const auto& window = expected.strokes[expected.worstStroke].windows.first();
                    QCOMPARE(chart->annotations().first().from, window.low);
                    QCOMPARE(chart->annotations().first().to, window.high);
                }
            }
        }
        if (relative.startsWith(QString::fromUtf8("ЛУКОЙЛ")) && qEnvironmentVariableIsSet("MPIPLATFORM_VIEWER_SCREENSHOT")) {
            method->setCurrentIndex(2);
            basis->setCurrentIndex(1);
            panel.show();
            QTest::qWait(100);
            QVERIFY(panel.grab().save(qEnvironmentVariable("MPIPLATFORM_VIEWER_SCREENSHOT")));
        }
        file.seek(0);
        QCOMPARE(file.readAll(), original);
    }
    void invalidIsNotZero()
    {
        auto* chart = new ChartView;
        chart->setLiveRendering(false);
        chart->addAxis("%.2f mm");
        chart->addSeries(0, "Перемещение от давления", Qt::blue);
        chart->replacePoints(0, {{1, 2}, {1, 3}});
        PressureAnalysis panel(chart);
        auto* summary = panel.findChild<QLabel*>("analysisSummary");
        QVERIFY(summary->text().contains(QString::fromUtf8("Нет пригодного результата")));
        QVERIFY(chart->annotations().isEmpty());
    }
    void compactWindow()
    {
        ViewerWindow window(nullptr);
        QVERIFY(window.findChildren<QToolBar*>().isEmpty());
        auto* button = window.findChild<QPushButton*>("button_openFolder");
        auto* tree = window.findChild<QTreeView*>("treeView_files");
        QVERIFY(button && tree);
        const QString path = QString::fromUtf8("/media/vladislav-b/Tree/Library/рез/ЛУКОЙЛ-УНП/ВИСБРЕКИНГ/-/PV-2223/03_10_2024/Pressure_1.data");
        if (!QFile::exists(path)) QSKIP("Local archive fixture unavailable");
        window.openPath(path);
        window.resize(1600, 900);
        window.show();
        QTest::qWait(100);
        QVERIFY(button->mapTo(&window, QPoint()).y() > tree->mapTo(&window, QPoint()).y()+tree->height());
        QVERIFY(window.findChild<PressureAnalysis*>());
        if (qEnvironmentVariableIsSet("MPIPLATFORM_VIEWER_SCREENSHOT"))
            QVERIFY(window.grab().save(qEnvironmentVariable("MPIPLATFORM_VIEWER_SCREENSHOT")+QStringLiteral(".window.png")));
    }
    void linkedChartsShareSelection()
    {
        const QString fixture = QString::fromUtf8("/media/vladislav-b/Tree/Library/рез/ЛУКОЙЛ-УНП/ВИСБРЕКИНГ/-/PV-2223/03_10_2024/");
        if (!QFile::exists(fixture+"Task_1.data")) QSKIP("Local archive fixture unavailable");
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        for (const auto& name : {"Pressure_1.data", "Task_1.data", "Friction_1.data"})
            QVERIFY(QFile::copy(fixture+name, temp.filePath(name)));
        ViewerWindow window(nullptr);
        window.resize(1600, 900);
        window.openPath(temp.path());
        PressureAnalysis *pressure = nullptr, *task = nullptr, *friction = nullptr;
        for (auto* panel : window.findChildren<PressureAnalysis*>()) {
            const auto type = panel->displayedChart()->getname();
            if (type == "Pressure") pressure = panel;
            if (type == "Task") task = panel;
            if (type == "Friction") friction = panel;
        }
        QVERIFY(pressure && task && friction);
        pressure->findChild<QComboBox*>("analysisBasis")->setCurrentIndex(1);
        auto* table = pressure->findChild<QTableWidget*>("analysisWindows");
        table->setCurrentCell(5, 0);
        QCOMPARE(pressure->selection(), task->selection());
        QCOMPARE(pressure->selection(), friction->selection());
        const auto p = pressure->sourceChart()->annotations().first();
        QCOMPARE(p.rangeStyle, ChartView::ChartAnnotation::RangeStyle::Band);
        QCOMPARE(p.bandAxis, Qt::Vertical);
        QVERIFY(!task->displayedChart()->annotations().isEmpty());
        for (const auto& a : task->displayedChart()->annotations()) {
            QCOMPARE(a.rangeStyle, ChartView::ChartAnnotation::RangeStyle::Band);
            QCOMPARE(a.bandAxis, Qt::Horizontal);
            QCOMPARE(a.seriesIndex, 1);
            QVERIFY(a.from.y() >= p.from.y() && a.from.y() <= p.to.y());
            QVERIFY(a.to.y() >= p.from.y() && a.to.y() <= p.to.y());
        }
        const auto f = friction->displayedChart()->annotations();
        QVERIFY(!f.isEmpty());
        QCOMPARE(f.first().from.x(), p.from.y());
        QCOMPARE(f.first().to.x(), p.to.y());
        // A selection in a companion tab must propagate back to pressure.
        task->findChild<QTableWidget*>("analysisWindows")->setCurrentCell(10, 0);
        QCOMPARE(pressure->selection(), task->selection());
        QCOMPARE(pressure->selection(), friction->selection());
        auto* tabs = window.findChild<QTabWidget*>("tabWidget_charts");
        tabs->setCurrentWidget(pressure);
        window.show();
        QTest::qWait(100);
        bool foundBand = false;
        for (auto* item : pressure->sourceChart()->chart()->scene()->items()) {
            auto* rect = qgraphicsitem_cast<QGraphicsRectItem*>(item);
            if (!rect || rect->brush().color() != QColor(184,64,154,34)) continue;
            foundBand = true;
            const auto plot = pressure->sourceChart()->chart()->plotArea();
            QVERIFY(std::abs(rect->rect().width()-plot.width()) < .01);
            QVERIFY(rect->rect().height() > 0 && rect->rect().height() < plot.height());
        }
        QVERIFY(foundBand);
        // Same file name in another directory is a different test session.
        QDir(temp.path()).mkdir("other");
        QVERIFY(QFile::copy(fixture+"Pressure_1.data", temp.filePath("other/Pressure_1.data")));
        window.openPath(temp.filePath("other/Pressure_1.data"));
        auto* other = qobject_cast<PressureAnalysis*>(tabs->currentWidget());
        QVERIFY(other);
        const auto unchanged = other->selection();
        table->setCurrentCell(12, 0);
        QCOMPARE(other->selection(), unchanged);
        if (qEnvironmentVariableIsSet("MPIPLATFORM_VIEWER_SCREENSHOT")) {
            for (auto* panel : {pressure, task, friction}) {
                tabs->setCurrentWidget(panel);
                QApplication::processEvents();
                QVERIFY(window.grab().save(qEnvironmentVariable("MPIPLATFORM_VIEWER_SCREENSHOT")+"."+panel->displayedChart()->getname()+".png"));
            }
        }
    }
    void missingCompanionDoesNotInventAnalysis()
    {
        const QString fixture = QString::fromUtf8("/media/vladislav-b/Tree/Library/рез/ЛУКОЙЛ-УНП/ВИСБРЕКИНГ/-/PV-2223/03_10_2024/Task_1.data");
        if (!QFile::exists(fixture)) QSKIP("Local archive fixture unavailable");
        QTemporaryDir temp;
        QVERIFY(QFile::copy(fixture, temp.filePath("Task_1.data")));
        ViewerWindow window(nullptr);
        window.openPath(temp.path());
        QVERIFY(window.findChildren<PressureAnalysis*>().isEmpty());
    }
};
QTEST_MAIN(PressureAnalysisTest)
#include "PressureAnalysisTest.moc"
