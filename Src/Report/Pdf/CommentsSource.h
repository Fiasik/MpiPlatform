#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVector>

#include "qtrpt.h"

namespace Report {
struct Context;
}

namespace Report::Pdf {

// Источник данных для Comments.xml — заменяет собой Report::Html::Document
// (HTML->PDF рендер, отдельный движок только ради одного документа).
// Печатает только реально заполненные слоты m_ctx.testComments, порядок —
// Manual/MainTest/ResponseTest/ResolutionTest/StepTest (тот же, что
// Report::Html::CommentSlot).
class CommentsSource : public QObject
{
    Q_OBJECT
public:
    explicit CommentsSource(const Report::Context& ctx, QObject* parent = nullptr);

    int commentCount() const { return m_entries.size(); }

public slots:
    void onSetValue(const int recNo, const QString paramName, QVariant& paramValue, const int reportPage);
    void onSetDSInfo(DataSetInfo& dsInfo);

private:
    struct Entry {
        QString title;
        QString text;
    };

    void buildEntries();

    const Report::Context& m_ctx;
    QVector<Entry> m_entries;
};

}
