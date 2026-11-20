#pragma once
#include <QJsonObject>
#include <QString>

namespace Researcher {
// One file per folder/run, with atomic writes and an append-only revision list.
QString recordPath(const QString& workspace, const QString& folder, const QString& run);
QJsonObject loadRecord(const QString& path, QString* error);
bool appendRecord(const QString& path, const QJsonObject& revision, QString* error);
QString readableResultPath(const QString& workspace, const QString& folder, const QString& run);
bool refreshReadableResults(const QString& workspace, QString* error);
}
