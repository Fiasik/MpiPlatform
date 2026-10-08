#pragma once

#include <QtGlobal>
#include <QString>

class Registry;
struct CrossingStatus;
namespace Domain::Tests::Stroke { struct Result; }
namespace Domain::Tests::Option::Step { struct Result; }
namespace Domain::Tests::Option::Response { struct Result; }
namespace Domain::Tests::Option::Resolution { struct Result; }

// Пишет в Database/mpiplatform.db сайт/позицию/сессию, снимок карточки
// клапана (+ материалы) В МОМЕНТ начала теста (вызывается из main.cpp сразу
// после ObjectWindow+ValveWindow, до показа MainWindow), и итог проверки
// допусков — В МОМЕНТ его завершения (см. writeCrossingResults(),
// MainWindow::endTest()) — см. Database/schema.sql.
//
// Никогда не бросает и не блокирует основной флоу теста: любая ошибка БД
// просто логируется через qWarning() и возвращает -1/false — история
// вспомогательная, а не критичная часть системы.
class HistoryWriter
{
public:
    static qint64 beginSession(const Registry& registry, const QString& variant, const QString& pattern);

    // sessionId — то, что вернул beginSession() в начале этой же сессии.
    // Вызывается из MainWindow::endTest() при каждом завершении теста
    // (Main/Stroke/Step/...), не только основного — INSERT OR REPLACE по
    // session_id делает повторные вызовы безопасными (перезапишут теми же
    // значениями, если crossingStatus между ними не менялся).
    static bool writeCrossingResults(qint64 sessionId, const CrossingStatus& status);

    // Те же вызовы из MainWindow::endTest(), по одному на каждый из 4
    // опциональных типов теста — но только если соответствующий
    // m_telemetry.testX уже заполнен (endTest() не знает, какой именно тест
    // только что завершился, см. Program::testFinished()), поэтому вызывающая
    // сторона сама проверяет std::optional::has_value() перед вызовом.
    static bool writeStrokeResults(qint64 sessionId, const Domain::Tests::Stroke::Result& result);
    static bool writeStepResults(qint64 sessionId, const Domain::Tests::Option::Step::Result& result);
    static bool writeResponseResults(qint64 sessionId, const Domain::Tests::Option::Response::Result& result);
    static bool writeResolutionResults(qint64 sessionId, const Domain::Tests::Option::Resolution::Result& result);

    // Флаг "нуждается в ремонте" — переключается оператором в "Истории" на
    // уже завершённой сессии (checkBox_history_needsRepair), а не во время
    // самого теста, поэтому вызывается не из endTest(), а напрямую из
    // HistoryPresenter::onNeedsRepairToggled().
    static bool setNeedsRepair(qint64 sessionId, bool needsRepair);
};
