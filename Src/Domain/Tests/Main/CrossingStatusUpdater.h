#pragma once

#include "Storage/Telemetry.h"
#include "Domain/DeviceConfig.h"

namespace Domain::Tests::Main {

// Пересчёт "прошёл/не прошёл" по уже сохранённому в telemetry.testMain
// результату. Вынесено из Scenario в свободную функцию, чтобы им мог
// пользоваться не только сам сценарий на завершении теста, но и
// Program::recalculateRegression() при ручной корректировке регрессии,
// когда Scenario уже уничтожен.
void updateCrossingStatus(Telemetry& telemetry, const DeviceConfig& config);

}
