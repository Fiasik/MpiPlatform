#include "ResearchStore.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QDateTime>
#include <QTimeZone>
#include <QUrl>
#include <QLocale>
#include <QMap>
#include <QRegularExpression>
#include <algorithm>

namespace Researcher {
namespace {
QString safeName(QString name) {
    name.replace(QRegularExpression(QStringLiteral("[\\x00-\\x1f<>:\"/\\\\|?*]")), "_");
    name = name.left(75).trimmed();
    while (name.endsWith('.') || name.endsWith(' ')) name.chop(1);
    return name.isEmpty() || name == "." || name == ".." ? QStringLiteral("Сессия") : name;
}
QString sessionTitle(const QString& folder) {
    const QFileInfo info(folder);
    return info.dir().dirName() + QStringLiteral(" / ") + info.fileName();
}
QString cell(QString text) { return text.replace('|', "\\|").replace('\n', ' ').replace('\r', ' '); }
QString number(const QJsonValue& value) {
    return value.isDouble() ? QLocale(QLocale::Russian).toString(value.toDouble(), 'f', 4) : QStringLiteral("—");
}
QString timeText(const QJsonValue& value) {
    const auto time = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
    return time.isValid() ? time.toTimeZone(QTimeZone("Europe/Moscow")).toString("dd.MM.yyyy HH:mm:ss") + " мск" : value.toString();
}
QString quality(const QJsonValue& value) {
    const QMap<QString, QString> names{{"unchecked", "Не проверено"}, {"usable", "Пригодно для анализа"},
        {"uncertain", "Есть сомнения"}, {"unusable", "Непригодно"}};
    return names.value(value.toString(), QStringLiteral("Не проверено"));
}
QString visibility(const QJsonValue& value) {
    const QMap<QString, QString> names{{"unrated", "Не оценено"}, {"none", "Не различаю"},
        {"slight", "Слабая особенность"}, {"pronounced", "Выраженная особенность"}, {"uncertain", "Не уверен"}};
    return names.value(value.toString(), QStringLiteral("Не оценено"));
}
QString observationsText(const QJsonObject& notes) {
    QString text = QStringLiteral("Испытание: **%1**. Визуальная оценка: **%2**.\n\n")
        .arg(quality(notes.value("quality")), visibility(notes.value("visibility")));
    const QStringList keys{"pressure", "task", "friction", "hypothesis", "confirmation"};
    const QStringList titles{QStringLiteral("Pressure — что видно"), QStringLiteral("Task — что видно"),
        QStringLiteral("Friction — что видно"), QStringLiteral("Предположение и вопросы"), QStringLiteral("Подтверждение и качество испытания")};
    for (int i = 0; i < keys.size(); ++i) {
        const auto value = notes.value(keys[i]).toString();
        text += QStringLiteral("### %1\n\n%2\n\n").arg(titles[i], value.trimmed().isEmpty() ? QStringLiteral("Пока не заполнено.") : value);
    }
    return text;
}
bool writeText(const QString& path, const QString& text, QString* error) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { *error = "Не удалось создать папку: " + QFileInfo(path).absolutePath(); return false; }
    QSaveFile file(path);
    const auto bytes = text.toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        *error = path + ": " + file.errorString(); return false;
    }
    return true;
}
QString reportText(const QJsonObject& revision, int revisionCount) {
    const auto notes = revision.value("observations").toObject();
    const auto a = revision.value("analysis").toObject();
    const auto folder = revision.value("folder").toString();
    QString text = QStringLiteral("# %1\n\nПрогон **%2**. Сохранено: %3.\n\nНаблюдатель: %4.\n\n")
        .arg(sessionTitle(folder), revision.value("run").toString(), timeText(revision.value("created_utc")),
             notes.value("author").toString().isEmpty() ? QStringLiteral("Не указан") : notes.value("author").toString());
    text += QStringLiteral("Файл обновляется автоматически. Наблюдения редактируются во вкладке приложения «Наблюдения».\n\n## Расчёт\n\n");
    if (!a.value("valid").toBool()) text += QStringLiteral("Расчёт недоступен для выбранных данных и настроек.\n\n");
    else {
        const bool local = a.value("method").toInt() >= 2;
        const auto unit = a.value("position_unit").toString() + "/" + a.value("pressure_unit").toString();
        text += QStringLiteral("Метод: **%1**. Режим: **%2**.\n\nХод, определивший итог: **%3**.\n\n")
            .arg(a.value("method_title").toString(), a.value("basis_title").toString(), a.value("stroke_title").toString());
        text += QStringLiteral("| Величина | Значение |\n|---|---:|\n");
        text += QStringLiteral("| Итог по выбранному методу | %1 % |\n| Общий наклон k | %2 %3 |\n| Рабочий диапазон | %4…%5 %6 |\n\n")
            .arg(number(a.value("result_percent")), number(a.value("global_slope")), cell(unit),
                 number(a.value("working_low")), number(a.value("working_high")), cell(a.value("position_unit").toString()));
        text += local ? QStringLiteral("Проценты рассчитаны относительно модуля общего наклона.\n\n")
                      : QStringLiteral("Проценты рассчитаны относительно ширины рабочей части по положению.\n\n");
        if (a.contains("physical_deviation")) text += QStringLiteral("%1: **%2 %3**.\n\n")
            .arg(a.value("physical_deviation_title").toString(), number(a.value("physical_deviation")), cell(unit));
        if (a.contains("worst_slope")) {
            text += QStringLiteral("Худший локальный наклон: **%1 %2**; разность с общим: **%3 %2 (%4 %)**.\n\n")
                .arg(number(a.value("worst_slope")), cell(unit), number(a.value("worst_delta")), number(a.value("worst_delta_percent")));
        }
        if (a.contains("worst_residual")) text += QStringLiteral("Наибольшее отклонение от прямой со знаком: %1 %2.\n\n")
            .arg(number(a.value("worst_residual")), a.value("position_unit").toString());
        const auto window = a.value("selected_window").toObject();
        if (!window.isEmpty()) {
            text += QStringLiteral("### Выбранный участок\n\nУчасток № %1: **%2…%3 %4**.\n\nЛокальный наклон: **%5 %6**. Разность с общим: **%7 %6 (%8 %)**.\n\n")
                .arg(a.value("selected_row").toInt()+1).arg(number(window.value("low")), number(window.value("high")),
                    a.value("position_unit").toString(), number(window.value("slope")), cell(unit), number(window.value("delta")), number(window.value("delta_percent")));
            text += QStringLiteral("Итог метода приведён выше; выбранная строка может относиться к другому участку.\n\n");
        }
        const auto windows = a.value("windows").toArray();
        if (!windows.isEmpty()) {
            text += QStringLiteral("### Все пригодные участки выбранного хода\n\nПоложение — %1; наклоны и их разность — %2.\n\n")
                .arg(a.value("position_unit").toString(), unit);
            text += QStringLiteral("| № | От | До | Локальный k | Δk | Δk, % |\n|---:|---:|---:|---:|---:|---:|\n");
            for (int i = 0; i < windows.size(); ++i) {
                const auto w = windows[i].toObject();
                text += QStringLiteral("| %1 | %2 | %3 | %4 | %5 | %6 |\n").arg(i+1)
                    .arg(number(w.value("low")), number(w.value("high")), number(w.value("slope")), number(w.value("delta")), number(w.value("delta_percent")));
            }
            text += '\n';
        }
    }
    text += QStringLiteral("## Наблюдения\n\n") + observationsText(notes);
    text += QStringLiteral("## Настройки и исходные файлы\n\nПапка испытания:\n\n```text\n%1\n```\n\n").arg(folder);
    text += QStringLiteral("Рабочая часть: 10–90 % размаха. Для локальных методов: окно 10 %, шаг 2,5 %, минимум 8 точек.\n\n");
    const auto sources = revision.value("sources").toObject();
    for (const auto& kind : {"Pressure", "Task", "Friction"}) {
        const auto source = sources.value(kind).toObject();
        text += QStringLiteral("- %1: %2 — %3.\n").arg(kind, QFileInfo(source.value("path").toString()).fileName(),
            source.value("loaded").toBool() ? QStringLiteral("прочитан") : QStringLiteral("отсутствует или не читается"));
    }
    text += QStringLiteral("\nСохранений в истории: %1. [История правок](История_правок.md).\n\n") .arg(revisionCount);
    return text;
}
bool refreshUnlocked(const QString& workspace, QString* error) {
    struct Entry { QJsonObject latest; QJsonArray history; QString report; };
    QList<Entry> entries;
    const QDir records(QDir(workspace).filePath("observations"));
    for (const auto& name : records.entryList({"*.json"}, QDir::Files)) {
        const auto record = loadRecord(records.filePath(name), error);
        if (!error->isEmpty()) return false;
        const auto history = record.value("revisions").toArray();
        if (history.isEmpty()) continue;
        const auto latest = history.last().toObject();
        if (latest.value("folder").toString().isEmpty()) continue;
        entries.append({latest, history, readableResultPath(workspace, latest.value("folder").toString(), latest.value("run").toString())});
    }
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        return a.latest.value("created_utc").toString() > b.latest.value("created_utc").toString();
    });
    QString index = QStringLiteral("# Результаты DataResearcher\n\nСохранённых карточек с учётом прогонов: %1.\n\n") .arg(entries.size());
    index += QStringLiteral("Здесь перечислены открытые и сохранённые карточки. Нажмите на название сессии, чтобы открыть расчёт и наблюдения. Файлы обновляются автоматически; наблюдения редактируются в приложении.\n\n");
    index += QStringLiteral("| Сессия | Прогон | Испытание | Метод | Режим расчёта | Итог, % | Сохранено |\n|---|---:|---|---|---|---:|---|\n");
    for (const auto& entry : entries) {
        if (!writeText(entry.report, reportText(entry.latest, entry.history.size()), error)) return false;
        QString historyText = QStringLiteral("# История правок\n\n%1 · прогон %2\n\n")
            .arg(sessionTitle(entry.latest.value("folder").toString()), entry.latest.value("run").toString());
        for (int i = 0; i < entry.history.size(); ++i) {
            const auto revision = entry.history[i].toObject();
            const auto a = revision.value("analysis").toObject();
            const auto notes = revision.value("observations").toObject();
            historyText += QStringLiteral("## Сохранение %1 от %2\n\nНаблюдатель: %3.\n\nМетод: %4. Режим: %5. Итог: %6. Выбранный участок: %7.\n\n")
                .arg(i+1).arg(timeText(revision.value("created_utc")), notes.value("author").toString(),
                    a.value("method_title").toString(), a.value("basis_title").toString(),
                    a.value("valid").toBool() ? number(a.value("result_percent"))+" %" : QStringLiteral("Расчёт недоступен"),
                    a.value("selected_window").isObject() ? QString::number(a.value("selected_row").toInt()+1) : QStringLiteral("Не выбран"));
            historyText += observationsText(notes);
        }
        if (!writeText(QFileInfo(entry.report).dir().filePath(QStringLiteral("История_правок.md")), historyText, error)) return false;
        const auto a = entry.latest.value("analysis").toObject();
        const auto relative = QDir(workspace).relativeFilePath(entry.report);
        const auto link = QString::fromLatin1(QUrl::toPercentEncoding(relative, "/"));
        index += QStringLiteral("| [%1](%2) | %3 | %4 | %5 | %6 | %7 | %8 |\n")
            .arg(cell(sessionTitle(entry.latest.value("folder").toString())), link, cell(entry.latest.value("run").toString()),
                quality(entry.latest.value("observations").toObject().value("quality")), cell(a.value("method_title").toString()),
                cell(a.value("basis_title").toString()), a.value("valid").toBool() ? number(a.value("result_percent")) : QStringLiteral("Недоступен"),
                timeText(entry.latest.value("created_utc")));
    }
    if (!writeText(QDir(workspace).filePath(QStringLiteral("Сводка.md")), index, error)) return false;
    return writeText(QDir(workspace).filePath(QStringLiteral("Как_читать_результаты.md")), QStringLiteral(
        "# Результаты DataResearcher\n\n"
        "Откройте **Сводка.md**: в ней собраны ссылки на сохранённые сессии и прогоны.\n\n"
        "В папке **Сессии** для каждого прогона находятся:\n\n"
        "- **Результаты.md** — текущий метод, ход, итог, выбранный участок, таблица окон и ваши наблюдения.\n"
        "- **История_правок.md** — предыдущие сохранения с расчётом и текстами наблюдений.\n\n"
        "Результат открытой карточки сохраняется автоматически после короткой паузы и перед переключением. "
        "Чтобы изменить наблюдения, используйте вкладку «Наблюдения» в приложении: эти файлы обновляются автоматически.\n\n"
        "Кнопка «Сохранить разбор…» дополнительно сохраняет увеличенные снимки трёх графиков, общий вид и исходные .data в выбранную новую папку.\n\n"
        "Папка **observations** содержит полную историю с точными числами, настройками и контрольными суммами исходных файлов. "
        "Она нужна приложению для восстановления карточек; для обычного чтения используйте файлы выше.\n"), error);
}
}
QString readableResultPath(const QString& workspace, const QString& folder, const QString& run) {
    const auto id = QFileInfo(recordPath(workspace, folder, QString())).completeBaseName().left(12);
    const auto name = safeName(QFileInfo(folder).dir().dirName()) + "__" + safeName(QFileInfo(folder).fileName()) + "__" + id;
    return QDir(workspace).filePath(QStringLiteral("Сессии/%1/Прогон_%2/Результаты.md").arg(name, safeName(run)));
}
bool refreshReadableResults(const QString& workspace, QString* error) {
    error->clear();
    if (!QDir().mkpath(workspace)) { *error = QStringLiteral("Не удалось создать папку результатов"); return false; }
    QLockFile lock(QDir(workspace).filePath("results.lock"));
    if (!lock.tryLock(0)) { *error = QStringLiteral("Результаты сохраняет другой экземпляр приложения"); return false; }
    return refreshUnlocked(workspace, error);
}
QString recordPath(const QString& workspace, const QString& folder, const QString& run)
{
    const auto key = QFileInfo(folder).absoluteFilePath().toUtf8() + '\0' + run.toUtf8();
    return QDir(workspace).filePath(QStringLiteral("observations/%1.json")
        .arg(QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex())));
}
QJsonObject loadRecord(const QString& path, QString* error)
{
    error->clear();
    QFile file(path);
    if (!file.exists()) return {};
    if (!file.open(QIODevice::ReadOnly)) { *error = file.errorString(); return {}; }
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
    const auto obj = doc.object();
    if (parse.error != QJsonParseError::NoError || !doc.isObject()
        || obj.value("schema").toInt() != 1 || !obj.value("revisions").isArray()) {
        *error = QStringLiteral("Неизвестный или повреждённый формат записи: %1").arg(path);
        return {};
    }
    return obj;
}
bool appendRecord(const QString& path, const QJsonObject& revision, QString* error)
{
    error->clear();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        *error = QStringLiteral("Не удалось создать папку наблюдений"); return false;
    }
    const auto workspace = QDir::cleanPath(QFileInfo(path).dir().filePath(".."));
    QLockFile resultsLock(QDir(workspace).filePath("results.lock"));
    if (!resultsLock.tryLock(0)) { *error = QStringLiteral("Результаты сохраняет другой экземпляр приложения"); return false; }
    QLockFile lock(path + QStringLiteral(".lock"));
    if (!lock.tryLock(0)) { *error = QStringLiteral("Запись открыта другим экземпляром приложения"); return false; }
    auto record = loadRecord(path, error);
    if (!error->isEmpty()) return false;
    auto revisions = record.value("revisions").toArray();
    auto withoutTime = [](QJsonObject value) { value.remove("created_utc"); return value; };
    if (revisions.isEmpty() || withoutTime(revisions.last().toObject()) != withoutTime(revision)) revisions.append(revision);
    record.insert("schema", 1);
    record.insert("revisions", revisions);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { *error = file.errorString(); return false; }
    const auto bytes = QJsonDocument(record).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit()) { *error = file.errorString(); return false; }
    if (!refreshUnlocked(workspace, error)) {
        *error = QStringLiteral("История сохранена, но не удалось обновить читаемые результаты: ") + *error;
        return false;
    }
    return true;
}
}
