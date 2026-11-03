#pragma once

#include <QString>
#include <QVector>

#include <optional>

// Правки оператора перед печатью отчёта: значения в таблице результатов
// (lineEdit_resultsTable_*) можно поправить руками, и в отчёт идёт уже
// правка. В БД (таблица field_overrides) сохраняются ОБА значения —
// посчитанное и исправленное: расхождение само по себе сигнал (и для
// разбора, и как разметка для датасета). См. ReportGenerationPresenter.
namespace FieldOverrides {

struct Record {
    QString field;            // ключ из kFriction... ниже (стабилен, в БД пишется он)
    double computed = 0.0;
    double corrected = 0.0;
};

// Ключи полей. Не менять после выпуска — по ним читается история.
inline const QString kFrictionForce      = QStringLiteral("frictionForce");
inline const QString kFrictionPercent    = QStringLiteral("frictionPercent");
inline const QString kDynamicErrorReal   = QStringLiteral("dynamicErrorReal");
inline const QString kPressureLow        = QStringLiteral("pressureLow");
inline const QString kPressureHigh       = QStringLiteral("pressureHigh");
inline const QString kSpringLow          = QStringLiteral("springLow");
inline const QString kSpringHigh         = QStringLiteral("springHigh");
inline const QString kValveStrokeReal    = QStringLiteral("valveStrokeReal");
inline const QString kStrokeForwardMs    = QStringLiteral("strokeForwardTimeMs");
inline const QString kStrokeBackwardMs   = QStringLiteral("strokeBackwardTimeMs");

// Человекочитаемое название для истории/отчётов; для неизвестного ключа — он сам.
QString title(const QString& key);

// Единица измерения поля ("" — безразмерное/не нужна), для вывода рядом со значениями.
QString unit(const QString& key);

// Правкой считается только то, чего приложение само бы не показало: текст в
// поле, отличающийся от того же значения, отформатированного так же, как это
// делает TelemetryUiMapper (decimals знаков после запятой / mm:ss.zzz).
// Простое округление при показе правкой не является. nullopt — не правили
// или в поле нечитаемый текст (в отчёт тогда идёт посчитанное).
std::optional<Record> detectNumber(const QString& key, double computed, int decimals, const QString& uiText);

// "lo–hi": по записи на каждое изменённое число (до двух).
QVector<Record> detectRange(const QString& keyLow, const QString& keyHigh,
                            double computedLow, double computedHigh,
                            int decimals, const QString& uiText);

std::optional<Record> detectDuration(const QString& key, quint32 computedMs, const QString& uiText);

}
