#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Labeler {

// Общая оценка карточки (ровно одна).
struct VerdictDef
{
    QString key;
    QString title;
    QString hint;
    QString shortcut;
};

// Одна метка-признак. Хранится в базе ТОЛЬКО ключом, поэтому название можно
// править свободно, а переименование/объединение ключей — через "aliases".
struct LabelDef
{
    QString key;
    QString title;
    QString hint;
    QString shortcut;
    // Метки с одинаковым oneOf взаимоисключающие (например, TSO: ошибка «на
    // закрытие» ИЛИ «на открытие»); пусто — независимая галочка.
    QString oneOf;
};

struct LabelGroup
{
    QString key;
    QString title;
    QString hint;
    // false — группа «особенностей», а не дефектов: галочка в ней не
    // переключает общую оценку на «есть дефект».
    bool isDefect = true;
    QVector<LabelDef> labels;
};

// Список меток из редактируемого labels.json (формат — см. образец
// Labeler/labels.json). version обязан расти при любой правке ключей:
// каждый ответ хранит версию списка, а labels.sqlite — снимок каждой версии.
class LabelSchema
{
public:
    // Ключ записи «Пропустить» — зарезервирован, в labels.json его быть не должно.
    static QString skippedVerdictKey() { return QStringLiteral("skipped"); }

    bool parse(const QByteArray& json, QString* error);

    int version() const { return m_version; }
    const QByteArray& rawJson() const { return m_rawJson; }
    const QVector<VerdictDef>& verdicts() const { return m_verdicts; }
    const QVector<LabelGroup>& groups() const { return m_groups; }

    // Все ключи меток текущей версии в порядке файла — столбцы экспорта.
    QStringList labelKeys() const;
    const LabelDef* findLabel(const QString& key) const;
    const LabelGroup* groupOfLabel(const QString& key) const;
    const VerdictDef* findVerdict(const QString& key) const;

    // Старый ключ -> актуальный по цепочке "aliases" (циклы обрываются).
    QString resolveAlias(const QString& key) const;

    // Горячая клавиша не зависит от раскладки: буква с той же физической
    // клавиши в ЙЦУКЕН приводится к QWERTY ("т" -> "n"), регистр не важен.
    static QString normalizeShortcut(const QString& text);

private:
    int m_version = 0;
    QByteArray m_rawJson;
    QVector<VerdictDef> m_verdicts;
    QVector<LabelGroup> m_groups;
    QHash<QString, QString> m_aliases;
};

}
