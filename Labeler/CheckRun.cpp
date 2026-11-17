#include "CheckRun.h"

#include <QDataStream>
#include <climits>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QRegularExpression>
#include <QTextStream>

#include <algorithm>

#include "ComputedResults.h"
#include "LabelSchema.h"
#include "LabelStore.h"
#include "SessionScanner.h"
#include "ValveCardReader.h"
#include "Widgets/Chart/ChartView.h"

namespace Labeler {

namespace {

QTextStream& out()
{
    static QTextStream stream(stdout);
    return stream;
}

QString sessionDir(const QString& root, const QString& relPath)
{
    return relPath.isEmpty() ? root : root + QLatin1Char('/') + relPath;
}

enum class DataStatus { Ok, Empty, Broken, Unreadable };

// Тот же путь, что ChartFrame::showFile(): новый ChartView, loadFromStream().
DataStatus loadData(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return DataStatus::Unreadable;
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_2);

    Widgets::Chart::ChartView chart;
    chart.loadFromStream(stream);
    if (stream.status() != QDataStream::Ok)
        return DataStatus::Broken;

    for (Widgets::Chart::Series* s : chart.series()) {
        if (!s->points().isEmpty())
            return DataStatus::Ok;
    }
    return DataStatus::Empty;
}

}

int runCheck(const QString& rootPath, const QString& csvPath)
{
    int problems = 0;
    const QString root = QFileInfo(rootPath).absoluteFilePath();
    out() << "Корень: " << root << Qt::endl;
    if (!QFileInfo(root).isDir()) {
        out() << "ОШИБКА: папки нет (диск не подключён?)" << Qt::endl;
        return 2;
    }

    // --- список меток из ресурсов -------------------------------------------
    LabelSchema schema;
    {
        QFile f(QStringLiteral(":/Labeler/labels.json"));
        f.open(QIODevice::ReadOnly);
        QString error;
        if (!schema.parse(f.readAll(), &error)) {
            out() << "labels.json (ресурс): ОШИБКА " << error << Qt::endl;
            return 2;
        }
        out() << "labels.json: версия " << schema.version() << ", оценок " << schema.verdicts().size()
              << ", групп " << schema.groups().size() << ", меток " << schema.labelKeys().size() << Qt::endl;
    }

    // --- обход ----------------------------------------------------------------
    QElapsedTimer timer;
    timer.start();
    const QVector<SessionInfo> sessions = scanSessions(root);
    out() << "\nСессий (папок с Task/Friction/Pressure): " << sessions.size()
          << "  (обход " << timer.elapsed() / 1000.0 << " с)" << Qt::endl;

    int withReport = 0;
    int withCardFile = 0;
    for (const SessionInfo& s : sessions) {
        withReport += s.hasReport ? 1 : 0;
        withCardFile += s.valveCardFile.isEmpty() ? 0 : 1;
    }
    out() << "  с report.xlsx: " << withReport << ", с карточкой клапана: " << withCardFile << Qt::endl;

    for (const QString& kind : chartKinds()) {
        int sessionsWith = 0;
        int files = 0;
        int several = 0;
        for (const SessionInfo& s : sessions) {
            const QStringList list = s.charts.value(kind);
            if (list.isEmpty())
                continue;
            ++sessionsWith;
            files += int(list.size());
            several += list.size() > 1 ? 1 : 0;
        }
        out() << "  " << kind << ": в " << sessionsWith << " сессиях, разных прогонов " << files
              << ", сессий с несколькими разными прогонами " << several << Qt::endl;
    }

    // --- все .data через ChartView --------------------------------------------
    timer.restart();
    QHash<int, int> dataCount;
    QStringList dataFailures;
    for (const SessionInfo& s : sessions) {
        for (auto it = s.charts.cbegin(); it != s.charts.cend(); ++it) {
            for (const QString& name : it.value()) {
                const QString path = sessionDir(root, s.relPath) + QLatin1Char('/') + name;
                const DataStatus st = loadData(path);
                ++dataCount[int(st)];
                if (st == DataStatus::Broken || st == DataStatus::Unreadable)
                    dataFailures << path;
                else if (st == DataStatus::Empty && dataFailures.size() < 40)
                    dataFailures << QStringLiteral("(пустой) ") + path;
            }
        }
    }
    out() << "\n.data через ChartView: ok " << dataCount.value(int(DataStatus::Ok))
          << ", без единой точки " << dataCount.value(int(DataStatus::Empty))
          << ", повреждено " << dataCount.value(int(DataStatus::Broken))
          << ", не открылось " << dataCount.value(int(DataStatus::Unreadable))
          << "  (" << timer.elapsed() / 1000.0 << " с)" << Qt::endl;
    for (const QString& f : dataFailures.mid(0, 15))
        out() << "    " << f << Qt::endl;
    problems += dataCount.value(int(DataStatus::Broken)) + dataCount.value(int(DataStatus::Unreadable));

    // --- карточки клапанов ----------------------------------------------------
    timer.restart();
    QHash<QString, ValveCard> cards;
    QHash<QString, int> cardErrors;
    int cardsOk = 0;
    int remarks = 0;
    int withConclusion = 0;
    int withModel = 0;
    int withPositioner = 0;
    int withFail = 0;
    QHash<QString, int> failShort;
    QVector<int> numberFilled(11, 0);
    QString sampleNumbers;
    for (const SessionInfo& s : sessions) {
        if (s.valveCardFile.isEmpty())
            continue;
        const ValveCard card = readValveCard(sessionDir(root, s.relPath) + QLatin1Char('/') + s.valveCardFile);
        cards.insert(s.relPath, card);
        if (!card.ok) {
            if (cardErrors.value(card.error) < 5)
                out() << "    карточка не прочиталась (" << card.error << "): "
                      << sessionDir(root, s.relPath) << "/" << s.valveCardFile << Qt::endl;
            ++cardErrors[card.error];
            continue;
        }
        ++cardsOk;
        remarks += card.operatorRemark() ? 1 : 0;
        withConclusion += card.conclusion.isEmpty() ? 0 : 1;
        withModel += card.model.isEmpty() ? 0 : 1;
        withPositioner += card.positioner.isEmpty() ? 0 : 1;
        withFail += card.failPosition.isEmpty() ? 0 : 1;
        ++failShort[card.failPositionShort()];
        const QStringList numbers = {card.dynamicError, card.dynamicErrorRec, card.stroke, card.strokeRec,
            card.springRange, card.springRangeRec, card.actuatorPressure, card.frictionCoef,
            card.frictionMean, card.strokeTimeForward, card.strokeTimeBackward};
        for (int k = 0; k < numbers.size(); ++k)
            numberFilled[k] += numbers.at(k).isEmpty() ? 0 : 1;
        if (sampleNumbers.isEmpty() && !card.dynamicError.isEmpty())
            sampleNumbers = numbers.join(QStringLiteral(" | ")) + QStringLiteral("  <- ") + s.relPath;
    }
    out() << "\nКарточки клапанов: прочитано " << cardsOk << " из " << cards.size()
          << "  (" << timer.elapsed() / 1000.0 << " с)" << Qt::endl;
    for (auto it = cardErrors.cbegin(); it != cardErrors.cend(); ++it)
        out() << "    ошибка «" << it.key() << "»: " << it.value() << Qt::endl;
    out() << "  с заключением " << withConclusion << ", с моделью " << withModel
          << ", с позиционером " << withPositioner << ", с положением безопасности " << withFail << Qt::endl;
    out() << "  положение безопасности:";
    for (auto it = failShort.cbegin(); it != failShort.cend(); ++it)
        out() << " «" << it.key() << "» " << it.value() << ";";
    out() << Qt::endl;
    {
        const QStringList names = {"дин.ошибка", "реком.", "ход", "реком.", "пружины", "реком.",
            "давл.в приводе", "коэф.трения", "трение ср.", "время прям.", "время обр."};
        out() << "  числа заполнены:";
        for (int k = 0; k < names.size(); ++k)
            out() << " " << names.at(k) << " " << numberFilled.at(k) << ";";
        out() << Qt::endl << "  пример: " << sampleNumbers << Qt::endl;
    }
    out() << "  с замечанием оператора: " << remarks << " ("
          << (cardsOk ? 100.0 * remarks / cardsOk : 0.0) << " % прочитанных)" << Qt::endl;

    // --- пересчёт из .data и сверка с карточкой ----------------------------------
    {
        timer.restart();
        auto toNumber = [](QString text, bool* ok) {
            text.replace(QLatin1Char(','), QLatin1Char('.'));
            return text.trimmed().toDouble(ok);
        };
        auto splitRange = [&toNumber](const QString& text, double* low, double* high) {
            static const QRegularExpression sep(QStringLiteral("\\s*[-–—]\\s*"));
            const QStringList parts = text.split(sep, Qt::SkipEmptyParts);
            bool a = false, b = false;
            if (parts.size() == 2) {
                *low = toNumber(parts.at(0), &a);
                *high = toNumber(parts.at(1), &b);
            }
            return a && b;
        };
        struct Diff { QString name; QVector<double> abs; int close = 0; };
        QVector<Diff> diffs = {{QStringLiteral("дин. ошибка, %")}, {QStringLiteral("коэф. трения, %")},
                               {QStringLiteral("пружины низ, бар")}, {QStringLiteral("пружины верх, бар")},
                               {QStringLiteral("ход, мм")}};
        auto add = [](Diff& d, double card, double calc) {
            const double e = qAbs(card - calc);
            d.abs << e;
            d.close += e <= qMax(0.1, 0.1 * qAbs(card)) ? 1 : 0;
        };
        int computedOk = 0;
        int computedNoCard = 0;
        QStringList worst;
        for (const SessionInfo& s : sessions) {
            const ComputedResults calc = computeResults(sessionDir(root, s.relPath));
            if (!calc.ok)
                continue;
            ++computedOk;
            const auto card = cards.constFind(s.relPath);
            if (card == cards.cend() || !card->ok) {
                ++computedNoCard;
                continue;
            }
            bool ok = false;
            double v = toNumber(card->dynamicError, &ok);
            if (ok) {
                add(diffs[0], v, calc.dynamicError);
                if (qAbs(v - calc.dynamicError) > qMax(0.5, 0.5 * qAbs(v)) && worst.size() < 5)
                    worst << QStringLiteral("дин.ошибка карточка %1 / расчёт %2  %3")
                                 .arg(v).arg(calc.dynamicError, 0, 'f', 2).arg(s.relPath);
            }
            v = toNumber(card->frictionCoef, &ok);
            if (ok)
                add(diffs[1], v, calc.frictionPercent);
            double low = 0, high = 0;
            if (splitRange(card->springRange, &low, &high)) {
                add(diffs[2], low, calc.springLow);
                add(diffs[3], high, calc.springHigh);
            }
            v = toNumber(card->stroke, &ok);
            if (ok)
                add(diffs[4], v, calc.strokeMm);
        }
        out() << "\nПересчёт из .data: получился в " << computedOk << " сессиях из " << sessions.size()
              << " (без карточки клапана — " << computedNoCard << ")  (" << timer.elapsed() / 1000.0 << " с)" << Qt::endl;
        out() << "  сверка с карточкой (медиана |разницы|; доля в пределах 10 % или 0.1):" << Qt::endl;
        for (Diff& d : diffs) {
            if (d.abs.isEmpty())
                continue;
            std::sort(d.abs.begin(), d.abs.end());
            out() << "    " << d.name << ": сессий " << d.abs.size() << ", медиана " << d.abs.at(d.abs.size() / 2)
                  << ", близко " << (100.0 * d.close / d.abs.size()) << " %" << Qt::endl;
        }
        for (const QString& w : worst)
            out() << "    сильно расходится: " << w << Qt::endl;
    }

    // --- очередь во временной базе --------------------------------------------
    LabelStore store;
    QString error;
    if (!store.open(QStringLiteral(":memory:"), &error)) {
        out() << "\nВременная база: ОШИБКА " << error << Qt::endl;
        return 2;
    }
    store.registerLabelSet(schema, nullptr);
    const QString dataset = QFileInfo(root).fileName();
    const int added = store.syncSessions(dataset, root, sessions, cards);
    const int size = store.ensureQueue(dataset);

    QHash<qint64, int> firstPos;
    QHash<qint64, int> normalCount;
    int repeats = 0;
    int repeatBeforeOriginal = 0;
    int minGap = INT_MAX;
    int maxGap = 0;
    QSet<qint64> flaggedIds;
    for (const SessionRecord& r : store.sessions(dataset)) {
        if (r.card.ok && r.card.operatorRemark())
            flaggedIds.insert(r.id);
    }
    int flaggedHead = 0;
    int flaggedFirst400 = 0;
    for (int pos = 0; pos < size; ++pos) {
        const auto item = store.queueItem(dataset, pos);
        if (!item) {
            out() << "  дыра в очереди на позиции " << pos << Qt::endl;
            ++problems;
            continue;
        }
        if (item->isRepeat) {
            ++repeats;
            if (!firstPos.contains(item->sessionId)) {
                ++repeatBeforeOriginal;
            } else {
                const int gap = pos - firstPos.value(item->sessionId);
                minGap = qMin(minGap, gap);
                maxGap = qMax(maxGap, gap);
            }
            continue;
        }
        ++normalCount[item->sessionId];
        firstPos.insert(item->sessionId, pos);
        if (flaggedIds.contains(item->sessionId)) {
            if (firstPos.size() <= 100)
                ++flaggedHead;
            if (firstPos.size() <= 400)
                ++flaggedFirst400;
        }
    }
    int duplicates = 0;
    for (int c : std::as_const(normalCount))
        duplicates += c > 1 ? 1 : 0;
    const int missing = int(sessions.size()) - int(normalCount.size());

    out() << "\nОчередь (временная база): сессий записано " << added << ", длина очереди " << size
          << ", повторов " << repeats << Qt::endl;
    out() << "  сессий без места в очереди: " << missing << ", попавших дважды: " << duplicates
          << ", повторов раньше оригинала: " << repeatBeforeOriginal << Qt::endl;
    if (repeats > 0)
        out() << "  расстояние до повтора: " << minGap << "…" << maxGap << " карточек" << Qt::endl;
    out() << "  с замечанием оператора: в первых 100 — " << flaggedHead << ", в первых 400 — "
          << flaggedFirst400 << ", всего " << flaggedIds.size() << Qt::endl;
    problems += missing + duplicates + repeatBeforeOriginal;

    // --- пробные ответы и экспорт ---------------------------------------------
    if (!csvPath.isEmpty() && size > 3) {
        const QStringList keys = schema.labelKeys();
        for (int pos = 0; pos < 3; ++pos) {
            const auto item = store.queueItem(dataset, pos);
            NewAnswer a;
            a.sessionId = item->sessionId;
            a.queuePos = pos;
            a.verdict = pos == 0 ? QStringLiteral("normal") : QStringLiteral("defect");
            if (pos > 0)
                a.labels = QStringList{keys.value(pos), keys.value(pos + 5)};
            a.comment = QStringLiteral("пробный ответ; с точкой с запятой и \"кавычками\"");
            a.labelSetVersion = schema.version();
            a.shownAtUtc = nowUtcIso();
            a.secondsOnCard = 1.5 * (pos + 1);
            if (store.addAnswer(a) == 0) {
                out() << "  addAnswer: ОШИБКА " << store.lastError() << Qt::endl;
                ++problems;
            }
        }
        // Правка первой карточки (новая запись) и отмена последнего ответа.
        {
            const auto item = store.queueItem(dataset, 0);
            NewAnswer a;
            a.sessionId = item->sessionId;
            a.queuePos = 0;
            a.verdict = QStringLiteral("unsure");
            a.labelSetVersion = schema.version();
            a.shownAtUtc = nowUtcIso();
            if (store.addAnswer(a) == 0) {
                out() << "  addAnswer: ОШИБКА " << store.lastError() << Qt::endl;
                ++problems;
            }
        }
        const auto revokedPos = store.revokeLastAnswer(dataset);
        const auto current0 = store.answerForCard(*store.queueItem(dataset, 0));
        const Progress p = store.progress(dataset);
        out() << "\nПробные ответы: отменён ответ на позиции " << revokedPos.value_or(-1)
              << ", актуальный ответ карточки 0 — " << (current0 ? current0->verdict : QStringLiteral("нет"))
              << " (ожидается normal), размечено " << p.labeledSessions << " / " << p.totalSessions << Qt::endl;
        if (!current0 || current0->verdict != QStringLiteral("normal"))
            ++problems;

        if (store.exportCsv(csvPath, schema, &error))
            out() << "  экспорт: " << csvPath << Qt::endl;
        else {
            out() << "  экспорт: ОШИБКА " << error << Qt::endl;
            ++problems;
        }
    }

    out() << "\nИтог: " << (problems == 0 ? "без ошибок" : "есть проблемы — см. выше") << Qt::endl;
    return problems == 0 ? 0 : 1;
}

}
