#pragma once

// Данные отчёта ПРОШЛОЙ сессии — то же, что ReportGenerationPresenter::
// collectInput() снимает с окна для текущей, но из базы (карточка, объект,
// материалы, заключение, ЗИП, правки оператора, тест хода, давление питания)
// и папки сессии (графики основного теста: по ним тот же Analyzer, что у
// живого теста, и Pass|Fail по допускам из карточки ЭТОЙ сессии, а не
// текущей). Окно и базу не меняет. Нужен «Предпросмотру» при просмотре
// архива из «Истории» и эксперименту сравнения отчётов (отсюда namespace).
//
// Чего в базе нет и потому нет в акте прошлой сессии: ФИО, комментарии к
// тестам. Собирается только Акт ТО — результаты ТШР/ТЧР/ТРС из базы в акты
// пока не поднимаются.

#include <QDateTime>
#include <QString>
#include <QVector>

#include <optional>

#include "Domain/DeviceConfig.h"
#include "Gui/MainWindow/ReportGenerationWorker.h"

namespace ReportCompare {

struct PastSession {
    qint64 id = -1;
    QDateTime ts;
    QString folder; // абсолютная папка сессии
    int run = 0;    // испытание в папке, по которому собирается акт (ArchivedTest::pickRun)
};

// Прошлые сессии позиции, новые сверху, у которых в папке есть графики
// основного теста (без них акт ТО не собрать). excludeId — текущая сессия.
// skipped — сколько сессий позиции пропущено из-за отсутствия графиков.
QVector<PastSession> pastSessions(const ObjectInfo& object, const QString& positionNumber,
                                  qint64 excludeId, int* skipped = nullptr);

// Настройки устройства из карточки сессии (допуски Pass|Fail, диаметр
// привода, шкала) — как Program берёт их из карточки текущей.
Domain::DeviceConfig deviceConfig(const ValveInfo& valve);

// Карточка клапана сессии и порог T её шаговой реакции — из базы, без
// графиков: по ним просмотр архива считает полный ход, ШР, ЧР и РС той же
// настройкой, что была у живого теста (loadInput() для сессии без основного
// теста не годится — ему нужны его графики). nullopt — сессии нет в базе.
struct SessionCard {
    ValveInfo valve;
    std::optional<double> stepTestValue; // step_results.test_value, % — нет теста ШР в базе
};
std::optional<SessionCard> loadCard(qint64 sessionId);

// nullopt — сессии нет в базе или графики не читаются; причина — в error.
std::optional<ReportGenerationWorker::Input> loadInput(const PastSession& session, QString* error = nullptr);

}
