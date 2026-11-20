#include <QtTest>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include "Report/Saver.h"
#include "Shared/Chart/ChartType.h"

// Links Report without Gui: persisting a chart must not require ChartView.
class SaverDataTest : public QObject
{
    Q_OBJECT
private slots:
    void savesSerializedChartUnchanged()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        Report::Saver saver;
        saver.setBasePath(temporary.path());
        QImage image(2, 2, QImage::Format_ARGB32);
        image.fill(Qt::red);
        const QByteArray data = QByteArray::fromHex("000000ff01020300fe");

        saver.saveChartSnapshot(Shared::Chart::ChartType::Task, image, data);

        const auto files = saver.directory().entryList({QStringLiteral("*.data")}, QDir::Files);
        QCOMPARE(files.size(), 1);
        QFile saved(saver.directory().filePath(files.first()));
        QVERIFY(saved.open(QIODevice::ReadOnly));
        QCOMPARE(saved.readAll(), data);
        const auto images = saver.directory().entryList({QStringLiteral("*.png")}, QDir::Files);
        QCOMPARE(images.size(), 1);
        QCOMPARE(QImage(saver.directory().filePath(images.first())), image);
    }
};

QTEST_GUILESS_MAIN(SaverDataTest)
#include "SaverDataTest.moc"
