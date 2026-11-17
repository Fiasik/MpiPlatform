#pragma once

#include <QHash>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

#include "SessionScanner.h"
#include "ValveCardReader.h"

namespace Labeler {

class LabelSchema;

// Сессия, как она лежит в labels.sqlite (без повторного обхода архива).
struct SessionRecord
{
    qint64 id = 0;
    QString dataset;
    QString relPath;
    QString filesHash;
    QMap<QString, QStringList> charts;
    QString valveCardFile;
    ValveCard card;
    bool hasReport = false;
};

struct QueueItem
{
    int pos = -1;
    qint64 sessionId = 0;
    bool isRepeat = false;
};

struct Answer
{
    qint64 id = 0;
    QString verdict;
    QStringList labels;
    QString comment;
};

// Что пишется на каждый ответ (одна новая строка answers, старые не
// трогаются — история правок).
struct NewAnswer
{
    qint64 sessionId = 0;
    int queuePos = -1;
    bool isRepeat = false;
    QString verdict;
    QStringList labels;
    QString comment;
    int labelSetVersion = 0;
    QString filesHash;
    QString shownAtUtc;
    double secondsOnCard = 0.0;
};

struct Progress
{
    int labeledSessions = 0;   // сессий с актуальным ответом (не пропуск)
    int totalSessions = 0;
    int repeatsAnswered = 0;
};

// labels.sqlite — отдельный от mpiplatform.db файл. Один файл на все корни
// (архив «рез», 30 примеров с флешки и т.д.): сессии различаются полем
// dataset = имя папки корня. Ничего не перезаписывает: правка ответа —
// новая строка answers, отмена (Ctrl+Z) — отметка revoked_at.
class LabelStore
{
public:
    LabelStore();
    ~LabelStore();

    LabelStore(const LabelStore&) = delete;
    LabelStore& operator=(const LabelStore&) = delete;

    // ":memory:" — временная база (режим --check, ничего не пишет на диск).
    bool open(const QString& path, QString* error);
    QString path() const { return m_path; }
    QString lastError() const { return m_lastError; }

    // Снимок labels.json этой версии. Если версия уже есть, а текст другой,
    // снимок не перезаписывается, а в warning — просьба поднять version.
    bool registerLabelSet(const LabelSchema& schema, QString* warning);

    // Результат обхода корня -> таблица sessions (новые добавляются, старые
    // обновляются, исчезнувшие помечаются present=0). Возвращает число новых.
    int syncSessions(const QString& dataset, const QString& rootPath,
                     const QVector<SessionInfo>& sessions,
                     const QHash<QString, ValveCard>& cardsByRelPath);

    QString rootPath(const QString& dataset) const;
    QVector<SessionRecord> sessions(const QString& dataset) const;

    // Очередь показа. Строится один раз на корень: первые 100 — случайно,
    // дальше перемешанно с повышенной долей сессий, где оператор записал
    // замечание; ~10 % карточек повторно через 30–60 позиций. Новые сессии
    // после пересканирования дописываются в конец. Возвращает размер очереди.
    int ensureQueue(const QString& dataset);
    int queueSize(const QString& dataset) const;
    std::optional<QueueItem> queueItem(const QString& dataset, int pos) const;
    // «Пропустить»: карточка ещё раз в конце очереди (если её там уже нет).
    void requeueAtEnd(const QString& dataset, qint64 sessionId, int afterPos);

    int cursor(const QString& dataset) const;
    void setCursor(const QString& dataset, int pos);

    // Ответ, которым заполнить форму карточки: для обычной — актуальный ответ
    // по сессии; для повтора — ответ именно на этот повтор (позиция очереди).
    std::optional<Answer> answerForCard(const QueueItem& item) const;
    qint64 addAnswer(const NewAnswer& answer);
    // Отменяет последний неотменённый ответ корня; возвращает его позицию в
    // очереди, чтобы вернуться к той карточке.
    std::optional<int> revokeLastAnswer(const QString& dataset);

    Progress progress(const QString& dataset) const;

    // Все неотменённые ответы всех корней: строка на ответ, столбец 0/1 на
    // каждую метку текущей версии (старые ключи — через aliases), поля
    // карточки клапана (заключение оператора, числа) — для сравнения.
    bool exportCsv(const QString& filePath, const LabelSchema& schema, QString* error) const;

private:
    QString meta(const QString& key) const;
    void setMeta(const QString& key, const QString& value);
    void buildQueue(const QString& dataset);
    void appendNewSessionsToQueue(const QString& dataset);
    bool exec(const QString& sql);

    QString m_connectionName;
    QString m_path;
    mutable QString m_lastError;
};

QString nowUtcIso();

}
