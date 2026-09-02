#pragma once

#include <QString>
#include <optional>

#include "Domain/Tests/Stroke/Result.h"
#include "Domain/Tests/Main/Result.h"
#include "Domain/Tests/Option/Step/Result.h"
#include "Domain/Tests/Option/Response/Result.h"
#include "Domain/Tests/Option/Resolution/Result.h"
#include "Domain/Tests/Cyclic/Regulatory/Result.h"
#include "Domain/Tests/Cyclic/Shutoff/Result.h"

// Только факты о ходе инициализации устройства — ни текста, ни цвета.
// Русский текст и цветовая семантика ("успех" -> зелёный) — решение
// презентационного слоя, не домена; см. Gui/MainWindow/TelemetryUiMapper::
// updateInit(), которая единственная форматирует это для UI.
struct InitState {
    bool deviceConnected = false;
    QString devicePortName; // валидно только при deviceConnected

    bool initSucceeded = false; // валидно только при deviceConnected

    quint8 sensorCount = 0;

    // nullopt = ещё измеряется ("Измерение..."), значение — итог измерения
    std::optional<qreal> startingPosition;
    std::optional<qreal> finalPosition;
};

struct CrossingStatus {
    enum class State {
        Unknown,
        Ok,
        Fail
    };

    State frictionPercent = State::Unknown;
    State valveStroke = State::Unknown;
    State dynamicError = State::Unknown;
    State spring = State::Unknown;
    State linearCharacteristic = State::Unknown;
};

struct ValveStrokeRecord {
    QString range = "";
    qreal real= qQNaN();
};

struct SupplyRecord {
    double pressure_bar = qQNaN();
};

class Telemetry {
public:
    InitState init;
    ValveStrokeRecord valveStrokeRecord;
    SupplyRecord supplyRecord;
    CrossingStatus crossingStatus;

    std::optional<Domain::Tests::Stroke::Result> testStroke;
    std::optional<Domain::Tests::Main::Result> testMain;
    std::optional<Domain::Tests::Option::Step::Result> testStep;
    std::optional<Domain::Tests::Option::Response::Result> testResponse;
    std::optional<Domain::Tests::Option::Resolution::Result> testResolution;
    std::optional<Domain::Tests::Cyclic::Regulatory::Result> testCyclicRegulatory;
    std::optional<Domain::Tests::Cyclic::Shutoff::Result> testCyclicShutoff;

    Telemetry() = default;

    void clearAll() {
        init = {};
        testMain.reset();
        testStroke.reset();
        testStep.reset();
        testResponse.reset();
        testResolution.reset();
        testCyclicRegulatory.reset();
        testCyclicShutoff.reset();
        valveStrokeRecord = {};
        supplyRecord = {};
    }
};
