#include "TelemetryUiMapper.h"
#include "ui_MainWindow.h"

#include "Gui/MainWindow/BalanceSeesawWidget.h"
#include "Gui/Style/Palette.h"
#include "Domain/Tests/Option/Response/Limits.h"
#include "Utils/TimeUtils.h"

#include <QAbstractItemView>
#include <QLabel>
#include <QTableWidgetItem>
#include <QTime>
#include <algorithm>
#include <cmath>
#include <optional>

void TelemetryUiMapper::updateInit(const InitState& init,
                                   const ValveStrokeRecord& strokeResult)
{
    const QColor okColor = Gui::Style::statusColor(Gui::Style::StatusKind::Success);
    const QColor errorColor = Gui::Style::statusColor(Gui::Style::StatusKind::Error);
    const QColor pendingColor = Gui::Style::statusColor(Gui::Style::StatusKind::Warning);

    m_ui->label_deviceStatusResult->setText(
        init.deviceConnected ? init.devicePortName : QStringLiteral("Не подключен"));
    m_ui->label_deviceStatusResult->setStyleSheet(
        "color:" + (init.deviceConnected ? okColor : errorColor).name(QColor::HexRgb));

    if (init.deviceConnected) {
        m_ui->label_deviceInitResult->setText(
            init.initSucceeded ? QStringLiteral("Успешна") : QStringLiteral("Ошибка"));
        m_ui->label_deviceInitResult->setStyleSheet(
            "color:" + (init.initSucceeded ? okColor : errorColor).name(QColor::HexRgb));
    } else {
        // "--", не пустая строка — иначе на неудачной попытке (порт не
        // найден) вся строка "Инициализация" выглядит стёртой, а не "ещё не
        // проводилась"/"нет данных", как остальные "--"-плейсхолдеры в
        // этом блоке статуса.
        m_ui->label_deviceInitResult->setText(QStringLiteral("--"));
        m_ui->label_deviceInitResult->setStyleSheet(QString());
    }

    if (init.deviceConnected && init.initSucceeded) {
        QString sensorsText;
        QColor sensorsColor;
        if (init.sensorCount == 0) {
            sensorsText = QStringLiteral("Не обнаружены");
            sensorsColor = errorColor;
        } else if (init.sensorCount == 1) {
            sensorsText = QStringLiteral("1 датчик");
            sensorsColor = pendingColor;
        } else {
            sensorsText = QStringLiteral("%1 датчика").arg(init.sensorCount);
            sensorsColor = okColor;
        }
        m_ui->label_connectedSensorsNumber->setText(sensorsText);
        m_ui->label_connectedSensorsNumber->setStyleSheet(
            "color:" + sensorsColor.name(QColor::HexRgb));
    } else {
        m_ui->label_connectedSensorsNumber->setText(QStringLiteral("--"));
        m_ui->label_connectedSensorsNumber->setStyleSheet(QString());
    }

    // "Измерение" имеет смысл только пока реально идёт опрос датчика
    // (устройство подключено и инициализация прошла) — иначе (порт не
    // найден/инициализация упала) значение так и останется nullopt
    // НАВСЕГДА, и "Измерение" вводит в заблуждение, будто что-то всё ещё
    // происходит. См. ts.init = {} в начале Program::initialization() —
    // startingPosition/finalPosition сбрасываются в nullopt на КАЖДОЙ
    // попытке, в том числе неудачной.
    const bool measuring = init.deviceConnected && init.initSucceeded;
    auto formatPosition = [&](const std::optional<qreal>& value, QLabel* label) {
        if (value) {
            label->setText(QString::number(*value, 'f', 2));
            label->setStyleSheet("color:" + okColor.name(QColor::HexRgb));
        } else if (measuring) {
            label->setText(QStringLiteral("Измерение"));
            label->setStyleSheet("color:" + pendingColor.name(QColor::HexRgb));
        } else {
            label->setText(QStringLiteral("--"));
            label->setStyleSheet(QString());
        }
    };

    formatPosition(init.startingPosition, m_ui->label_startingPosition);
    m_ui->label_startingPosition_Unit->setVisible(init.startingPosition.has_value());

    if (strokeResult.real) {
        m_ui->frame_startingPosition_Card->setVisible(true);
    }

    formatPosition(init.finalPosition, m_ui->label_finalPosition);

    m_ui->label_valveStroke_range_Value->setText(strokeResult.range);
    m_ui->label_crossingLimits_range_Value->setText(strokeResult.range);
}

void TelemetryUiMapper::updateStrokeTest(const Domain::Tests::Stroke::Result& result)
{
    const QString forwardTime = TimeUtils::msToText(result.forwardTimeMs);
    const QString backwardTime = TimeUtils::msToText(result.backwardTimeMs);

    m_ui->label_strokeTest_forwardTime_Value->setText(forwardTime);

    m_ui->label_strokeTest_respondingReactionForwardTime_Value->setText(
        TimeUtils::msToText(result.forwardSignalDelayMs));

    m_ui->lineEdit_resultsTable_strokeTest_forwardTime->setText(forwardTime);

    m_ui->label_strokeTest_backwardTime_Value->setText(backwardTime);

    m_ui->lineEdit_resultsTable_strokeTest_backwardTime->setText(backwardTime);

    m_ui->label_strokeTest_respondingReactionBackwardTime_Value->setText(
        TimeUtils::msToText(result.backwardSignalDelayMs));
}

void TelemetryUiMapper::updateMainTest(
    const Domain::Tests::Main::Result& mainResult,
    const ValveStrokeRecord& strokeResult)
{
    m_ui->label_pressureDifference_Value->setText(
        QString("%1").arg(mainResult.pressureDiff, 0, 'f', 3));

    m_ui->label_frictionForce_Value->setText(
        QString("%1").arg(mainResult.frictionForce, 0, 'f', 3));

    m_ui->label_frictionPercent_Value->setText(
        QString("%1").arg(mainResult.frictionPercent, 0, 'f', 2));

    m_ui->lineEdit_resultsTable_frictionForceValue->setText(
        QString("%1").arg(mainResult.frictionForce, 0, 'f', 3));

    m_ui->lineEdit_resultsTable_frictionPercentValue->setText(
        QString("%1").arg(mainResult.frictionPercent, 0, 'f', 2));

    m_ui->label_dynamicErrorMeanPercent_Value->setText(
        QString("%1").arg(mainResult.dynamicErrorMeanPercent, 0, 'f', 2));

    m_ui->label_dynamicErrorMean_Value->setText(
        QString("%1").arg(mainResult.dynamicErrorMean, 0, 'f', 3));

    m_ui->label_dynamicErrorMaxPercent_Value->setText(
        QString("%1")
            .arg(mainResult.dynamicErrorMaxPercent, 0, 'f', 2)
        );

    m_ui->label_dynamicErrorMax_Value->setText(
        QString("%1").arg(mainResult.dynamicErrorMax, 0, 'f', 3));

    m_ui->lineEdit_resultsTable_dynamicErrorReal->setText(
        QString("%1").arg(mainResult.dynamicErrorReal, 0, 'f', 2));

    m_ui->lineEdit_resultsTable_strokeReal->setText(
        QString("%1").arg(strokeResult.real, 0, 'f', 2));

    m_ui->label_lowLimit_Value->setText(
        QString("%1").arg(mainResult.lowLimitPressure, 0, 'f', 2));

    m_ui->label_highLimit_Value->setText(
        QString("%1").arg(mainResult.highLimitPressure, 0, 'f', 2));

    m_ui->lineEdit_resultsTable_rangePressure->setText(
        QString("%1–%2")
            .arg(mainResult.lowLimitPressure, 0, 'f', 2)
            .arg(mainResult.highLimitPressure, 0, 'f', 2)
        );

    m_ui->lineEdit_resultsTable_driveRangeReal->setText(
        QString("%1–%2")
            .arg(mainResult.springLow, 0, 'f', 2)
            .arg(mainResult.springHigh, 0, 'f', 2)
        );

    m_ui->label_valveStroke_range_Value->setText(strokeResult.range);
}

void TelemetryUiMapper::updateCrossingValues(
    const Domain::Tests::Main::Result& mainResult,
    const ValveStrokeRecord& strokeResult)
{
    m_ui->label_crossingLimits_dynamicError_Value->setText(
        QString::number(mainResult.dynamicErrorReal, 'f', 2));
    m_ui->label_crossingLimits_dynamicError_Value->setText(
        QString::number(mainResult.dynamicErrorReal, 'f', 2));

    m_ui->label_crossingLimits_linearCharacteristic_Value->setText(
        QString::number(mainResult.linearityError, 'f', 2));
    m_ui->label_crossingLimits_linearCharacteristic_Value->setText(
        QString::number(mainResult.linearityError, 'f', 2));

    m_ui->label_crossingLimits_range_Value->setText(
        QString::number(strokeResult.real, 'f', 2));
    m_ui->label_crossingLimits_range_Value->setText(
        QString::number(strokeResult.real, 'f', 2));

    m_ui->label_crossingLimits_spring_Value->setText(
        QString("%1–%2")
            .arg(mainResult.springLow, 0, 'f', 2)
            .arg(mainResult.springHigh, 0, 'f', 2));
    m_ui->label_crossingLimits_spring_Value->setText(
        QString("%1–%2")
            .arg(mainResult.springLow, 0, 'f', 2)
            .arg(mainResult.springHigh, 0, 'f', 2));

    m_ui->label_crossingLimits_coefficientFriction_Value->setText(
        QString::number(mainResult.frictionPercent, 'f', 2));
}

void TelemetryUiMapper::updateStepTest(const Domain::Tests::Option::Step::Result& result)
{
    const auto& steps = result.steps;

    m_ui->tableWidget_stepResults->clearContents();

    m_ui->tableWidget_stepResults->setColumnCount(2);
    m_ui->tableWidget_stepResults->setHorizontalHeaderLabels({
        QObject::tr("T%1").arg(result.testValue),
        QObject::tr("Перерегулирование")
    });

    m_ui->tableWidget_stepResults->setRowCount(steps.size());

    QStringList rowNames;
    rowNames.reserve(steps.size());

    for (int i = 0; i < steps.size(); ++i)
    {
        const auto& step = steps.at(i);

        const QString time = step.T_value == 0
                                 ? QObject::tr("Ошибка")
                                 : QTime(0, 0)
                                       .addMSecs(step.T_value)
                                       .toString("mm:ss.zzz");

        const QString overshoot =
            QString("%1%").arg(step.overshoot, 4, 'f', 2);

        const QString rowName =
            QString("%1-%2").arg(step.from).arg(step.to);

        m_ui->tableWidget_stepResults->setItem(
            i, 0, new QTableWidgetItem(time));

        m_ui->tableWidget_stepResults->setItem(
            i, 1, new QTableWidgetItem(overshoot));

        rowNames << rowName;
    }

    m_ui->tableWidget_stepResults->setVerticalHeaderLabels(rowNames);
    m_ui->tableWidget_stepResults->resizeColumnsToContents();
}

void TelemetryUiMapper::setupOptionCell(QTableWidget* table,
                                        int row,
                                        int col,
                                        const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);

    if (text == "Восходящее движение") {
        item->setBackground(QColor(Qt::yellow));
    } else if (text == "Нисходящее движение" ||
               text == "Ошибка по положению, %") {
        item->setBackground(QColor(QStringLiteral("#fd7d13")));
    } else if (text == "Задание, %" ||
               text == "Факт, %" ||
               text == "Допустимая ошибка, %" ||
               text == "Ошибка на шаг, %") {
        item->setBackground(QColor(Qt::green));
    }

    table->setItem(row, col, item);
}

void TelemetryUiMapper::setupColoredOptionCell(QTableWidget* table,
                                               int row,
                                               int col,
                                               const QString& text,
                                               const QColor& color)
{
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    item->setBackground(color);
    table->setItem(row, col, item);
}

void TelemetryUiMapper::updateResolutionTest(
    const Domain::Tests::Option::Resolution::Result& result)
{
    auto* table = m_ui->tableWidget_resolutionResults;

    const int steps = std::max(result.upward.size(),
                               result.downward.size());

    const int cols = steps + 2;

    const int upTitleRow = 0;
    const int upHeaderRow = 1;
    const int upBaseRow = 2;
    const int upErrorRow = upBaseRow + 2;

    const int downTitleRow = 5;
    const int downHeaderRow = downTitleRow + 1;
    const int downBaseRow = downHeaderRow + 1;
    const int downErrorRow = downBaseRow + 2;

    const int rows = downErrorRow + 1;

    table->clear();
    table->setRowCount(rows);
    table->setColumnCount(cols);
    table->setSelectionMode(QAbstractItemView::NoSelection);

    table->setSpan(upTitleRow, 0, 1, cols);
    setupOptionCell(table, upTitleRow, 0, "Восходящее движение");

    setupOptionCell(table, upHeaderRow, 0, "Положение хода клапана");

    for (int i = 0; i <= steps; ++i)
    {
        const QString header =
            i == 0
                ? "Задание"
                : QString("Ступенчатый шаг %1").arg(i);

        setupOptionCell(table, upHeaderRow, i + 1, header);
    }

    setupOptionCell(table, upBaseRow, 0, "Задание, %");
    setupOptionCell(table, upBaseRow + 1, 0, "Факт, %");
    setupOptionCell(table, upErrorRow, 0, "Ошибка на шаг, %");

    table->setSpan(downTitleRow, 0, 1, cols);
    setupOptionCell(table, downTitleRow, 0, "Нисходящее движение");

    setupOptionCell(table, downHeaderRow, 0, "Положение хода клапана");

    for (int i = 0; i <= steps; ++i)
    {
        const QString header =
            i == 0
                ? "Задание"
                : QString("Ступенчатый шаг %1").arg(i);

        setupOptionCell(table, downHeaderRow, i + 1, header);
    }

    setupOptionCell(table, downBaseRow, 0, "Задание, %");
    setupOptionCell(table, downBaseRow + 1, 0, "Факт, %");
    setupOptionCell(table, downErrorRow, 0, "Ошибка на шаг, %");

    auto fillDirection =
        [&](const auto& data, int baseRow)
    {
        const int errorRow = baseRow + 2;

        if (std::isfinite(result.baseTask)) {
            setupOptionCell(table,
                            baseRow,
                            1,
                            QString::number(result.baseTask, 'f', 2));
        }

        if (std::isfinite(result.baseActual)) {
            setupOptionCell(table,
                            baseRow + 1,
                            1,
                            QString::number(result.baseActual, 'f', 2));
        }

        for (int i = 0; i < data.size(); ++i)
        {
            const auto& step = data.at(i);
            const int col = i + 2;

            setupOptionCell(table, baseRow, col,
                            QString::number(step.selectedPercent, 'f', 2));

            setupOptionCell(table, baseRow + 1, col,
                            QString::number(step.actualPercent, 'f', 2));

            setupOptionCell(table, errorRow, col,
                            QString::number(step.stepErrorPercent, 'f', 2));
        }
    };

    fillDirection(result.upward, upBaseRow);
    fillDirection(result.downward, downBaseRow);

    table->resizeColumnsToContents();
}

void TelemetryUiMapper::updateResponseTest(
    const Domain::Tests::Option::Response::Result& result)
{
    auto* table = m_ui->tableWidget_responseResults;

    const int steps = std::max(result.upward.size(),
                               result.downward.size());

    const int cols = steps + 2;

    const int upTitleRow = 0;
    const int upHeaderRow = 1;
    const int upBaseRow = 2;
    const int upAllowedErrorRow = upBaseRow + 2;
    const int upErrorRow = upBaseRow + 3;

    const int downTitleRow = 6;
    const int downHeaderRow = downTitleRow + 1;
    const int downBaseRow = downHeaderRow + 1;
    const int downAllowedErrorRow = downBaseRow + 2;
    const int downErrorRow = downBaseRow + 3;

    const int gisRow = downErrorRow + 1;
    const int rows = gisRow + 1;

    table->clear();
    table->setRowCount(rows);
    table->setColumnCount(cols);
    table->setSelectionMode(QAbstractItemView::NoSelection);

    table->setSpan(upTitleRow, 0, 1, cols);
    setupOptionCell(table, upTitleRow, 0, "Восходящее движение");

    setupOptionCell(table, upHeaderRow, 0, "Положение хода клапана");

    for (int i = 0; i <= steps; ++i)
    {
        const QString header =
            i == 0
                ? "Задание"
                : QString("Задание + шаг %1").arg(i);

        setupOptionCell(table, upHeaderRow, i + 1, header);
    }

    setupOptionCell(table, upBaseRow, 0, "Задание, %");
    setupOptionCell(table, upBaseRow + 1, 0, "Факт, %");
    setupOptionCell(table, upAllowedErrorRow, 0, "Допустимая ошибка, %");
    setupOptionCell(table, upErrorRow, 0, "Ошибка на шаг, %");

    table->setSpan(downTitleRow, 0, 1, cols);
    setupOptionCell(table, downTitleRow, 0, "Нисходящее движение");

    setupOptionCell(table, downHeaderRow, 0, "Положение хода клапана");

    for (int i = 0; i <= steps; ++i)
    {
        const QString header =
            i == 0
                ? "Задание"
                : QString("Задание - шаг %1").arg(i);

        setupOptionCell(table, downHeaderRow, i + 1, header);
    }

    setupOptionCell(table, downBaseRow, 0, "Задание, %");
    setupOptionCell(table, downBaseRow + 1, 0, "Факт, %");
    setupOptionCell(table, downAllowedErrorRow, 0, "Допустимая ошибка, %");
    setupOptionCell(table, downErrorRow, 0, "Ошибка на шаг, %");

    setupOptionCell(table, gisRow, 0, "Ошибка по положению, %");

    if (std::isfinite(result.baseActual) &&
        std::isfinite(result.baseActualTwo))
    {
        const double gis =
            qAbs(result.baseActual - result.baseActualTwo);

        setupOptionCell(table,
                        gisRow,
                        1,
                        QString::number(gis, 'f', 2));

        table->setSpan(gisRow, 2, 1, cols - 2);
    }

    auto fillDirection =
        [&](const auto& data,
            int baseRow,
            double baseActualValue)
    {
        const int allowedErrorRow = baseRow + 2;
        const int errorRow = baseRow + 3;

        if (std::isfinite(result.baseTask)) {
            setupOptionCell(table, baseRow, 1,
                            QString::number(result.baseTask, 'f', 2));
        }

        if (std::isfinite(baseActualValue)) {
            setupOptionCell(table, baseRow + 1, 1,
                            QString::number(baseActualValue, 'f', 2));
        }

        for (int i = 0; i < data.size(); ++i)
        {
            const auto& step = data.at(i);
            const int col = i + 2;

            QString selectedText;

            if (std::isfinite(result.baseTask)) {
                const double delta =
                    step.selectedPercent - result.baseTask;

                const QChar sign =
                    delta >= 0.0 ? '+' : '-';

                selectedText = QString("%1 %2 %3")
                                   .arg(result.baseTask, 0, 'f', 2)
                                   .arg(sign)
                                   .arg(std::abs(delta), 0, 'f', 2);
            }
            else {
                selectedText =
                    QString::number(step.selectedPercent, 'f', 2);
            }

            setupOptionCell(table, baseRow, col, selectedText);

            setupOptionCell(table,
                            baseRow + 1,
                            col,
                            QString::number(step.actualPercent, 'f', 2));

            const double allowedError =
                Domain::Tests::Option::Response::allowedErrorForStep(i);

            setupOptionCell(table, allowedErrorRow, col,
                            std::isfinite(allowedError)
                                ? QString::number(allowedError, 'f', 2)
                                : "");

            const bool isOk =
                std::isfinite(allowedError) &&
                std::isfinite(step.stepErrorPercent) &&
                qAbs(step.stepErrorPercent) <= allowedError;

            setupColoredOptionCell(table, errorRow, col,
                                   std::isfinite(step.stepErrorPercent)
                                       ? QString::number(step.stepErrorPercent, 'f', 2)
                                       : "",
                                   isOk
                                       ? Gui::Style::passFillColor()
                                       : Gui::Style::failFillColor());
        }
    };

    fillDirection(result.upward,
                  upBaseRow,
                  result.baseActual);

    fillDirection(result.downward,
                  downBaseRow,
                  result.baseActualTwo);

    table->resizeColumnsToContents();
}

void TelemetryUiMapper::setStatusPanelWidgets(QLabel* positionValue,
                                              QLabel* pressure1Value,
                                              QLabel* pressure2Value,
                                              QLabel* pressure3Value,
                                              BalanceSeesawWidget* balanceSeesaw)
{
    m_statusPositionValue = positionValue;
    m_statusPressure1Value = pressure1Value;
    m_statusPressure2Value = pressure2Value;
    m_statusPressure3Value = pressure3Value;
    m_statusBalanceSeesaw = balanceSeesaw;
}

void TelemetryUiMapper::updateSensorReadings(const Domain::Measurement::Sample& s)
{
    if (!qIsNaN(s.positionValue))
        m_ui->label_sensorLinear_Value->setText(QString::number(s.positionValue, 'f', 2));

    if (!qIsNaN(s.positionPercent)) {
        m_ui->label_sensorLinearPercent_Value->setText(QString::number(s.positionPercent, 'f', 2));
        if (m_statusPositionValue)
            m_statusPositionValue->setText(QString("%1%").arg(s.positionPercent, 0, 'f', 1));
    }

    if (!qIsNaN(s.pressure1)) {
        m_ui->label_pressureSensor_1_Value->setText(QString::number(s.pressure1, 'f', 2));
        if (m_statusPressure1Value)
            m_statusPressure1Value->setText(QString::number(s.pressure1, 'f', 2));
    }

    if (!qIsNaN(s.pressure2)) {
        m_ui->label_pressureSensor_2_Value->setText(QString::number(s.pressure2, 'f', 2));
        if (m_statusPressure2Value)
            m_statusPressure2Value->setText(QString::number(s.pressure2, 'f', 2));
    }

    if (!qIsNaN(s.pressure3)) {
        m_ui->label_pressureSensor_3_Value->setText(QString::number(s.pressure3, 'f', 2));
        if (m_statusPressure3Value)
            m_statusPressure3Value->setText(QString::number(s.pressure3, 'f', 2));
    }

    if (!qIsNaN(s.pressure1) && !qIsNaN(s.pressure3)) {
        const double diff = qAbs(s.pressure1 - s.pressure3);
        const double base = qMax(s.pressure1, s.pressure3);
        const double percent = (base > 0.0) ? (diff / base * 100.0) : 0.0;

        m_ui->label_calcPressureBalancePercent_Value->setText(
            QString("%1").arg(percent, 0, 'f', 2));

        m_ui->label_calcPressureBalance->setText(
            QString("%1").arg(diff, 0, 'f', 2));

        constexpr double kEdgeTolerancePercent = 1.0;
        constexpr double kBalanceThresholdPercent = 17.0;

        if (!qIsNaN(s.positionPercent)) {
            const bool nearEdge =
                s.positionPercent <= kEdgeTolerancePercent ||
                s.positionPercent >= (100.0 - kEdgeTolerancePercent);

            QColor color;
            if (nearEdge)
                color = Gui::Style::statusColor(Gui::Style::StatusKind::Warning);
            else if (percent > kBalanceThresholdPercent)
                color = Gui::Style::statusColor(Gui::Style::StatusKind::Error);
            else
                color = Gui::Style::statusColor(Gui::Style::StatusKind::Success);

            m_ui->label_calcPressureBalancePercent_Value->setStyleSheet(
                QStringLiteral("color: %1;").arg(color.name()));

            if (m_statusBalanceSeesaw) {
                const double signedPercent =
                    (base > 0.0)
                        ? ((s.pressure1 - s.pressure3) / base * 100.0)
                        : 0.0;
                m_statusBalanceSeesaw->setBalance(signedPercent, color);
            }
        }
    }

    if (!qIsNaN(s.feedbackCurrent))
        m_ui->label_sensorFeedback_Value->setText(QString::number(s.feedbackCurrent, 'f', 2));
}
