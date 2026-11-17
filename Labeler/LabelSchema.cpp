#include "LabelSchema.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace Labeler {

QString LabelSchema::normalizeShortcut(const QString& text)
{
    // Одна и та же физическая клавиша в ЙЦУКЕН и QWERTY.
    static const QString cyrillic = QStringLiteral("йцукенгшщзхъфывапролджэячсмитьбюё");
    static const QString latin = QStringLiteral("qwertyuiop[]asdfghjkl;'zxcvbnm,.`");

    const QString lower = text.trimmed().toLower();
    if (lower.size() != 1)
        return lower;
    const int idx = cyrillic.indexOf(lower.at(0));
    return idx >= 0 ? QString(latin.at(idx)) : lower;
}

bool LabelSchema::parse(const QByteArray& json, QString* error)
{
    auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return false;
    };

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (doc.isNull())
        return fail(QStringLiteral("JSON не разобран: %1 (смещение %2)")
                        .arg(parseError.errorString()).arg(parseError.offset));

    const QJsonObject root = doc.object();
    const int version = root.value(QStringLiteral("version")).toInt();
    if (version < 1)
        return fail(QStringLiteral("нет \"version\" (целое ≥ 1)"));

    QSet<QString> keys;
    QSet<QString> shortcuts;
    auto takeKey = [&](const QString& key, QString* problem) {
        if (key.isEmpty()) {
            *problem = QStringLiteral("пустой \"key\"");
            return false;
        }
        if (key == skippedVerdictKey()) {
            *problem = QStringLiteral("ключ \"%1\" зарезервирован").arg(key);
            return false;
        }
        if (keys.contains(key)) {
            *problem = QStringLiteral("ключ \"%1\" повторяется").arg(key);
            return false;
        }
        keys.insert(key);
        return true;
    };
    auto takeShortcut = [&](const QString& raw, QString* problem) {
        const QString sc = normalizeShortcut(raw);
        if (sc.isEmpty())
            return true;
        if (sc.size() != 1) {
            *problem = QStringLiteral("клавиша \"%1\": нужна одна буква или цифра").arg(raw);
            return false;
        }
        if (shortcuts.contains(sc)) {
            *problem = QStringLiteral("клавиша \"%1\" назначена дважды").arg(raw);
            return false;
        }
        shortcuts.insert(sc);
        return true;
    };

    QVector<VerdictDef> verdicts;
    for (const QJsonValue& v : root.value(QStringLiteral("verdicts")).toArray()) {
        const QJsonObject o = v.toObject();
        VerdictDef d;
        d.key = o.value(QStringLiteral("key")).toString();
        d.title = o.value(QStringLiteral("title")).toString(d.key);
        d.hint = o.value(QStringLiteral("hint")).toString();
        d.shortcut = o.value(QStringLiteral("shortcut")).toString();
        QString problem;
        if (!takeKey(d.key, &problem) || !takeShortcut(d.shortcut, &problem))
            return fail(QStringLiteral("verdicts: %1").arg(problem));
        verdicts << d;
    }
    if (verdicts.isEmpty())
        return fail(QStringLiteral("пустой список \"verdicts\""));

    QVector<LabelGroup> groups;
    for (const QJsonValue& g : root.value(QStringLiteral("groups")).toArray()) {
        const QJsonObject go = g.toObject();
        LabelGroup group;
        group.key = go.value(QStringLiteral("key")).toString();
        group.title = go.value(QStringLiteral("title")).toString(group.key);
        group.hint = go.value(QStringLiteral("hint")).toString();
        group.isDefect = go.value(QStringLiteral("isDefect")).toBool(true);

        for (const QJsonValue& l : go.value(QStringLiteral("labels")).toArray()) {
            const QJsonObject lo = l.toObject();
            LabelDef d;
            d.key = lo.value(QStringLiteral("key")).toString();
            d.title = lo.value(QStringLiteral("title")).toString(d.key);
            d.hint = lo.value(QStringLiteral("hint")).toString();
            d.shortcut = lo.value(QStringLiteral("shortcut")).toString();
            d.oneOf = lo.value(QStringLiteral("oneOf")).toString();
            QString problem;
            if (!takeKey(d.key, &problem) || !takeShortcut(d.shortcut, &problem))
                return fail(QStringLiteral("группа \"%1\": %2").arg(group.title, problem));
            group.labels << d;
        }
        groups << group;
    }

    QHash<QString, QString> aliases;
    const QJsonObject aliasObj = root.value(QStringLiteral("aliases")).toObject();
    for (auto it = aliasObj.begin(); it != aliasObj.end(); ++it)
        aliases.insert(it.key(), it.value().toString());

    m_version = version;
    m_rawJson = json;
    m_verdicts = verdicts;
    m_groups = groups;
    m_aliases = aliases;
    return true;
}

QStringList LabelSchema::labelKeys() const
{
    QStringList keys;
    for (const LabelGroup& g : m_groups) {
        for (const LabelDef& l : g.labels)
            keys << l.key;
    }
    return keys;
}

const LabelDef* LabelSchema::findLabel(const QString& key) const
{
    for (const LabelGroup& g : m_groups) {
        for (const LabelDef& l : g.labels) {
            if (l.key == key)
                return &l;
        }
    }
    return nullptr;
}

const LabelGroup* LabelSchema::groupOfLabel(const QString& key) const
{
    for (const LabelGroup& g : m_groups) {
        for (const LabelDef& l : g.labels) {
            if (l.key == key)
                return &g;
        }
    }
    return nullptr;
}

const VerdictDef* LabelSchema::findVerdict(const QString& key) const
{
    for (const VerdictDef& v : m_verdicts) {
        if (v.key == key)
            return &v;
    }
    return nullptr;
}

QString LabelSchema::resolveAlias(const QString& key) const
{
    QString current = key;
    // Ограничение глубины — защита от цикла "a"->"b"->"a" в ручной правке.
    for (int i = 0; i < 16 && m_aliases.contains(current); ++i)
        current = m_aliases.value(current);
    return current;
}

}
