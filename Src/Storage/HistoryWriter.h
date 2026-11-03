#pragma once

#include <QtGlobal>
#include <QString>
#include <QVector>

#include "Registry.h"
#include "FieldOverrides.h"

// Карточка клапана, накопленная до перехода на БД (QSettings/реестр
// Windows): объект/цех/отделение + позиция + материалы. См.
// HistoryWriter::importLegacyCards().
struct LegacyCard {
    ObjectInfo site;
    ValveInfo valve;
    MaterialsOfComponentParts materials;
    bool hasMaterials = false;
};

// Заключение оператора по клапану (комбобоксы и итоговый текст вкладки
// "Формирование отчёта"). Хранятся ТЕКСТЫ ответов — как в схеме БД (см.
// комментарий у таблицы conclusions в Database/migrations/001_baseline.sql).
struct ConclusionRecord {
    QString seatingZone;
    QString positionerAdaptation;
    QString auxiliaryEquipment;
    QString frictionGraph;
    QString glandLeak;
    QString summaryTemplate;   // текст выбранного шаблона заключения; пусто — "Ручной ввод"
    QString additionalNotes;   // итоговый текст заключения (шаблон или набранный вручную)
};

struct SparePartRecord {
    QString text;
    bool freeText = false;     // набрано вручную, а не выбрано из каталога
};

struct CrossingStatus;
struct CrossingLimits;
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

    // Сохраняет карточку клапана (+ материалы + допуски Pass/Fail) после
    // правки по "Назад": обновляет снимок ТОЙ ЖЕ сессии, если объект и номер
    // позиции остались теми же; если сменились — результаты прежних тестов
    // принадлежат другой позиции, поэтому заводится новая сессия. Возвращает
    // id сессии, в которой теперь лежит карточка (-1 — не удалось). Если
    // sessionId < 0 — просто beginSession().
    static qint64 saveCard(qint64 sessionId, const Registry& registry,
                           const QString& variant, const QString& pattern);

    // Связывает сессию с папкой на диске (sessions.folder_path), когда Saver
    // её создал. absoluteDir приводится к виду для БД (см. SessionFolders).
    static bool setSessionFolder(qint64 sessionId, const QString& absoluteDir);

    // Что оператор сделал на вкладке отчёта: заключение, список ЗИП и ручные
    // правки значений (посчитанное + исправленное). Вызывается при каждом
    // формировании отчёта и каждый раз ПОЛНОСТЬЮ заменяет прежнее содержимое
    // этих трёх таблиц для сессии: убранная правка или позиция ЗИП не должна
    // оставаться в истории.
    static bool writeReportExtras(qint64 sessionId, const ConclusionRecord& conclusion,
                                  const QVector<SparePartRecord>& spareParts,
                                  const QVector<FieldOverrides::Record>& overrides);

    struct DeleteResult {
        bool ok = false;
        QString error;
        int deleted = 0;
        // Папки сессий (абсолютные пути), на которые после удаления НЕ ссылается
        // ни одна оставшаяся сессия — их можно предложить удалить с диска.
        QStringList orphanFolders;
    };
    // Безвозвратно удаляет сессии со всеми их результатами (таблицы с
    // session_id находятся по схеме, новые попадут сами), одной транзакцией.
    // Позиции и связки объект/цех/отделение, в которых не осталось ни одной
    // сессии, тоже убираются — иначе ошибочно введённое название оставалось бы
    // в списках автодополнения навсегда. Файлы на диске НЕ трогает.
    static DeleteResult deleteSessions(const QVector<qint64>& sessionIds);

    // Разовый перенос старых карточек: каждая становится сессией source=
    // 'legacy' БЕЗ тестов (в историю позиции не попадает — там JOIN по
    // crossing_results) и служит только источником карточки/материалов для
    // окна клапана. Время сессии подбирается так, чтобы настоящие сессии
    // позиции её перекрывали: на секунду раньше самой ранней 'live', а если
    // таких нет — "сейчас" (новее архивных 'harvest'). Всё в одной
    // транзакции вместе с флагом legacy_import_done в app_state; возвращает
    // число перенесённых карточек или -1 (error заполнен).
    static int importLegacyCards(const QVector<LegacyCard>& cards, QString* error);

    // sessionId — то, что вернул beginSession() в начале этой же сессии.
    // Вызывается из MainWindow::endTest() при каждом завершении теста
    // (Main/Stroke/Step/...), не только основного — INSERT OR REPLACE по
    // session_id делает повторные вызовы безопасными (перезапишут теми же
    // значениями, если crossingStatus между ними не менялся). limits — какие
    // проверки были включены на момент расчёта: пишутся рядом с результатом,
    // потому что снимок карточки могут поправить уже после теста.
    static bool writeCrossingResults(qint64 sessionId, const CrossingStatus& status,
                                     const CrossingLimits& limits);

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
