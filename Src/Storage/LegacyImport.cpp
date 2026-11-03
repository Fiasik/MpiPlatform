#include "LegacyImport.h"
#include "HistoryDb.h"
#include "HistoryWriter.h"
#include "Registry.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVector>

namespace {

bool importAlreadyDone()
{
    bool done = false;
    HistoryDb::withConnection(QStringLiteral("mpiplatform_legacy_flag"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT 1 FROM app_state WHERE key = 'legacy_import_done'"));
        done = q.exec() && q.next();
    });
    return done;
}

bool hasAnyMaterial(const MaterialsOfComponentParts& m)
{
    return !(m.plunger.isEmpty() && m.saddle.isEmpty() && m.cap.isEmpty()
             && m.corpus.isEmpty() && m.ball.isEmpty() && m.disk.isEmpty()
             && m.shaft.isEmpty() && m.stock.isEmpty() && m.guideSleeve.isEmpty()
             && m.stuffingBoxSeal.isEmpty() && m.CV.isEmpty());
}

QVector<LegacyCard> collectCards()
{
    QVector<LegacyCard> cards;
    Registry registry;

    for (const QString& object : registry.objects()) {
        for (const QString& manufactory : registry.manufactories(object)) {
            for (const QString& department : registry.departments(object, manufactory)) {
                for (const QString& position : registry.positions(object, manufactory, department)) {
                    LegacyCard card;
                    card.site.object = object;
                    card.site.manufactory = manufactory;
                    card.site.department = department;

                    if (!registry.valveInfoAt(object, manufactory, department, position, card.valve))
                        continue;

                    card.hasMaterials = registry.materialsAt(object, manufactory, department,
                                                             position, card.materials)
                                        && hasAnyMaterial(card.materials);
                    cards.append(card);
                }
            }
        }
    }
    return cards;
}

}

LegacyImport::Result LegacyImport::runOnce()
{
    Result result;

    if (importAlreadyDone())
        return result;

    result.ran = true;

    const QVector<LegacyCard> cards = collectCards();

    // Флаг выставляется и при нуле карточек (чистая машина без реестра):
    // иначе каждый запуск пересканировал бы пустое дерево настроек.
    const int imported = HistoryWriter::importLegacyCards(cards, &result.error);
    if (imported >= 0)
        result.imported = imported;
    return result;
}
