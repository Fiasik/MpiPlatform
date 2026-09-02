#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QHash>
#include <QStringList>

#include "qtrpt.h"

namespace {

// Реальный шаблон акта (experiments/qtrpt-spike/act_report/act.xml),
// сгенерированный gen_act_xml.py — правишь .py (позиции полей/бэндов)
// или сам XML, перезапускаешь QtRptSpike, смотришь act.pdf.
//
// ЗИП (bandNo=1) и комментарий (bandNo=2) — теперь настоящие MasterData
// бэнды: количество строк ЗИП и высота комментария не зашиты в XML,
// а сообщаются движку через onSetDSInfo(). Если данных больше, чем
// влезает на страницу, QtRPT сам уходит на следующую страницу и
// повторяет шапку (см. qtrpt.cpp::processMasterData) — раньше вместо
// этого либо тихо пропадали лишние позиции ЗИП (в XML было ровно 10
// полей), либо длинный комментарий наезжал на блок подписей.
class ActDataSource : public QObject
{
    Q_OBJECT
public:
    ActDataSource()
    {
        m_values[QStringLiteral("valvePosition")] = QStringLiteral("12-FV-034");
        m_values[QStringLiteral("objectTag")] = QStringLiteral("УППН-3");

        m_values[QStringLiteral("objectName")] = QStringLiteral("Установка первичной переработки нефти");
        m_values[QStringLiteral("objectShop")] = QStringLiteral("Цех №3");
        m_values[QStringLiteral("objectUnit")] = QStringLiteral("Компрессорная станция");
        m_values[QStringLiteral("objectInstall")] = QStringLiteral("Компрессорная станция");  // tshr.xml: "Данные по объекту"/"Установка"
        m_values[QStringLiteral("objectFIO")] = QStringLiteral("Иванов И. И.");

        m_values[QStringLiteral("matBody")] = QStringLiteral("Сталь");
        m_values[QStringLiteral("matCover")] = QStringLiteral("Сталь");
        m_values[QStringLiteral("matSeat")] = QStringLiteral("Cv/Kv — 12.4/15.5");
        m_values[QStringLiteral("matBall")] = QStringLiteral("—");
        m_values[QStringLiteral("matDisc")] = QStringLiteral("—");
        m_values[QStringLiteral("matPlug")] = QStringLiteral("Сталь нержавеющая");
        m_values[QStringLiteral("matShaft")] = QStringLiteral("—");
        m_values[QStringLiteral("matStem")] = QStringLiteral("Сталь нержавеющая");
        m_values[QStringLiteral("matBushing")] = QStringLiteral("PTFE");

        m_values[QStringLiteral("specPosition")] = QStringLiteral("12-FV-034");
        m_values[QStringLiteral("specSerial")] = QStringLiteral("SN-2024-1187");
        m_values[QStringLiteral("specModel")] = QStringLiteral("V200");
        m_values[QStringLiteral("specVendor")] = QStringLiteral("Valtek");
        m_values[QStringLiteral("specDnPn")] = QStringLiteral("DN50/PN40");
        m_values[QStringLiteral("specPositioner")] = QStringLiteral("SVI II AP");
        m_values[QStringLiteral("specSupplyPressure")] = QStringLiteral("4.5");
        m_values[QStringLiteral("specInputSignal")] = QStringLiteral("4-20 мА");
        m_values[QStringLiteral("specActuator")] = QStringLiteral("39-71361");
        m_values[QStringLiteral("specMotion")] = QStringLiteral("линейное");
        m_values[QStringLiteral("specPacking")] = QStringLiteral("PTFE");

        m_values[QStringLiteral("resDynErrorValue")] = QStringLiteral("1.2");
        m_values[QStringLiteral("resDynErrorNote")] = QStringLiteral("соответствует");
        m_values[QStringLiteral("resStrokeValue")] = QStringLiteral("24.8");
        m_values[QStringLiteral("resStrokeRef")] = QStringLiteral("25.0");
        m_values[QStringLiteral("resStrokeNote")] = QStringLiteral("не соответствует");
        m_values[QStringLiteral("resSpringRange")] = QStringLiteral("0.4–1.2");
        m_values[QStringLiteral("resSpringRangeRef")] = QStringLiteral("0.4–1.2");
        m_values[QStringLiteral("resSpringNote")] = QStringLiteral("соответствует");
        m_values[QStringLiteral("resPressureRange")] = QStringLiteral("0.6–3.8");
        m_values[QStringLiteral("resFrictionPct")] = QStringLiteral("4.1");
        m_values[QStringLiteral("resFrictionAvg")] = QStringLiteral("186");
        m_values[QStringLiteral("resSeatingZone")] = QStringLiteral("без видимых дефектов");
        m_values[QStringLiteral("resPositionerAdaptation")] = QStringLiteral("удовлетворительно");
        m_values[QStringLiteral("resAuxEquipment")] = QStringLiteral("удовлетворительно");
        m_values[QStringLiteral("resFrictionGraph")] = QStringLiteral("зоны повышенного трения отсутствуют");
        m_values[QStringLiteral("resGlandLeak")] = QStringLiteral("видимая течь не наблюдается");
        m_values[QStringLiteral("resTimeForward")] = QStringLiteral("3.2");
        m_values[QStringLiteral("resTimeBackward")] = QStringLiteral("3.4");

        m_values[QStringLiteral("testDate")] = QStringLiteral("06.08.2026");

        m_values[QStringLiteral("conclusionText")] =
            QStringLiteral("Работа клапана удовлетворительная. Ремонт клапана не требуется.");

        m_spareParts = {
            QStringLiteral("сальниковое уплотнение (согласно исполнению)"),
            QStringLiteral("комплект мягких уплотнений"),
            QStringLiteral("диск/уплотнение диска"),
            QStringLiteral("вал (при необходимости)"),
            QStringLiteral("шток/сборка шток-плунжер/штифт (при необходимости)"),
            QStringLiteral("сальниковое уплотнение (согласно исполнению)"),
        };

        m_testComment =
            QStringLiteral("Люфт в узле обратной связи позиционера в пределах нормы, дополнительная ревизия не требуется.");
    }

public slots:
    void onSetValue(const int recNo, const QString paramName, QVariant& paramValue, const int reportPage)
    {
        Q_UNUSED(reportPage);

        if (paramName == QLatin1String("sparePartText")) {
            if (recNo >= 0 && recNo < m_spareParts.size())
                paramValue = m_spareParts.at(recNo);
            return;
        }
        if (paramName == QLatin1String("testCommentText")) {
            paramValue = m_testComment;
            return;
        }

        const auto it = m_values.constFind(paramName);
        if (it != m_values.constEnd())
            paramValue = it.value();
    }

    void onSetDSInfo(DataSetInfo& dsInfo)
    {
        // dataSetNo соответствует bandNo из XML: 1 — заключение, 2 — ЗИП,
        // 3 — комментарий. У заключения и комментария всегда ровно
        // 1 "запись" (заголовок+текст целиком, см. gen_act_xml.py) — так
        // они переезжают на след. страницу единым блоком при переполнении.
        if (dsInfo.dataSetNo == 1)
            dsInfo.recordCount = 1;
        else if (dsInfo.dataSetNo == 2)
            dsInfo.recordCount = m_spareParts.size();
        else if (dsInfo.dataSetNo == 3)
            dsInfo.recordCount = 1;
    }

private:
    QHash<QString, QString> m_values;
    QStringList m_spareParts;
    QString m_testComment;
};

// Источник данных для tchr.xml/trs.xml (gen_tchr_xml.py/gen_trs_xml.py) —
// значения объекта/спецификации не выдуманы, а взяты из уже заполненного
// боевого отчёта пользователя ("report Илим.xlsx", тест от 08.05.2026),
// как максимально правдоподобный референс. Сами таблицы результатов там
// статичные (см. докстринг gen_tchr_xml.py) — этому источнику нужны
// только общие плейсхолдеры шапки, тот же набор, что и у ActDataSource.
class IlimDataSource : public QObject
{
    Q_OBJECT
public:
    IlimDataSource()
    {
        m_values[QStringLiteral("valvePosition")] = QStringLiteral("1");
        m_values[QStringLiteral("objectTag")] = QStringLiteral("123");

        m_values[QStringLiteral("objectName")] = QStringLiteral("Лаборатория");
        m_values[QStringLiteral("objectShop")] = QStringLiteral("Лаборатория");
        m_values[QStringLiteral("objectInstall")] = QStringLiteral("123");

        m_values[QStringLiteral("specPosition")] = QStringLiteral("1");
        m_values[QStringLiteral("specSerial")] = QStringLiteral("35112");
        m_values[QStringLiteral("specModel")] = QStringLiteral("35000");
        m_values[QStringLiteral("specVendor")] = QStringLiteral("Masoneilan");
        m_values[QStringLiteral("specDnPn")] = QStringLiteral("40/30");
        m_values[QStringLiteral("specPositioner")] = QStringLiteral("12");
        m_values[QStringLiteral("specSupplyPressure")] = QStringLiteral("0.00");
        m_values[QStringLiteral("specInputSignal")] = QStringLiteral("Нормально закрыт");
        m_values[QStringLiteral("specActuator")] = QStringLiteral("мод прив");
        m_values[QStringLiteral("specMotion")] = QStringLiteral("Возвратно-поступательное");
        m_values[QStringLiteral("specPacking")] = QStringLiteral("PTFE");

        m_values[QStringLiteral("testDate")] = QStringLiteral("08.05.2026");
    }

public slots:
    void onSetValue(const int recNo, const QString paramName, QVariant& paramValue, const int reportPage)
    {
        Q_UNUSED(recNo);
        Q_UNUSED(reportPage);

        const auto it = m_values.constFind(paramName);
        if (it != m_values.constEnd())
            paramValue = it.value();
    }

private:
    QHash<QString, QString> m_values;
};

}

#include "main.moc"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    const QDir dir(QCoreApplication::applicationDirPath());
    const QString reportFile = dir.filePath(QStringLiteral("act_report/act.xml"));
    const QString outputFile = dir.filePath(QStringLiteral("act.pdf"));

    QtRPT report;

    if (!report.loadReport(reportFile)) {
        qWarning() << "Не удалось загрузить отчёт:" << reportFile;
        return 1;
    }

    ActDataSource dataSource;
    QObject::connect(&report, &QtRPT::setValue, &dataSource, &ActDataSource::onSetValue);
    QObject::connect(&report, &QtRPT::setDSInfo, &dataSource, &ActDataSource::onSetDSInfo);

    report.printPDF(outputFile, false);

    qDebug() << "Написано:" << outputFile;

    // ТШР ("Акт шагового испытания клапана", gen_tshr_xml.py) — второй,
    // независимый отчёт. Та же ActDataSource: и ТО, и ТШР используют один
    // и тот же набор плейсхолдеров объекта/спецификации (см. память
    // report_types_shared_blocks — эти блоки специально сделаны общими),
    // отдельный dataSource заводить незачем.
    const QString tshrReportFile = dir.filePath(QStringLiteral("act_report/tshr.xml"));
    const QString tshrOutputFile = dir.filePath(QStringLiteral("tshr.pdf"));

    QtRPT tshrReport;
    if (!tshrReport.loadReport(tshrReportFile)) {
        qWarning() << "Не удалось загрузить отчёт:" << tshrReportFile;
        return 1;
    }
    QObject::connect(&tshrReport, &QtRPT::setValue, &dataSource, &ActDataSource::onSetValue);
    tshrReport.printPDF(tshrOutputFile, false);
    qDebug() << "Написано:" << tshrOutputFile;

    // ТЧР ("Акт теста на чувствительность реагирования", gen_tchr_xml.py) и
    // ТРС ("Акт теста на разрешающую способность", gen_trs_xml.py) —
    // отдельный IlimDataSource с реальными значениями объекта/спецификации
    // из report Илим.xlsx (см. его докстринг).
    IlimDataSource ilimDataSource;

    const QString tchrReportFile = dir.filePath(QStringLiteral("act_report/tchr.xml"));
    const QString tchrOutputFile = dir.filePath(QStringLiteral("tchr.pdf"));

    QtRPT tchrReport;
    if (!tchrReport.loadReport(tchrReportFile)) {
        qWarning() << "Не удалось загрузить отчёт:" << tchrReportFile;
        return 1;
    }
    QObject::connect(&tchrReport, &QtRPT::setValue, &ilimDataSource, &IlimDataSource::onSetValue);
    tchrReport.printPDF(tchrOutputFile, false);
    qDebug() << "Написано:" << tchrOutputFile;

    const QString trsReportFile = dir.filePath(QStringLiteral("act_report/trs.xml"));
    const QString trsOutputFile = dir.filePath(QStringLiteral("trs.pdf"));

    QtRPT trsReport;
    if (!trsReport.loadReport(trsReportFile)) {
        qWarning() << "Не удалось загрузить отчёт:" << trsReportFile;
        return 1;
    }
    QObject::connect(&trsReport, &QtRPT::setValue, &ilimDataSource, &IlimDataSource::onSetValue);
    trsReport.printPDF(trsOutputFile, false);
    qDebug() << "Написано:" << trsOutputFile;

    return 0;
}
