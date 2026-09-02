#include "CommentsSource.h"

#include "Report/Context.h"
#include "Report/Html/CommentCard.h"

#include <QDateTime>

namespace Report::Pdf {

CommentsSource::CommentsSource(const Report::Context& ctx, QObject* parent)
    : QObject(parent)
    , m_ctx(ctx)
{
    buildEntries();
}

void CommentsSource::buildEntries()
{
    using Report::Html::CommentSlot;

    static const CommentSlot order[] = {
        CommentSlot::Manual,
        CommentSlot::MainTest,
        CommentSlot::ResponseTest,
        CommentSlot::ResolutionTest,
        CommentSlot::StepTest,
    };

    for (CommentSlot slot : order) {
        const QString text = m_ctx.testComments.value(slot).trimmed();

        if (text.isEmpty())
            continue;

        m_entries.push_back({Report::Html::titleFor(slot), text});
    }
}

void CommentsSource::onSetValue(const int recNo, const QString paramName, QVariant& paramValue, const int reportPage)
{
    Q_UNUSED(reportPage);

    if (paramName == QLatin1String("valvePosition")) {
        paramValue = m_ctx.valve.positionNumber;
        return;
    }

    if (paramName == QLatin1String("commentsDate")) {
        paramValue = QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy HH:mm"));
        return;
    }

    if (recNo < 0 || recNo >= m_entries.size())
        return;

    if (paramName == QLatin1String("commentTitle")) {
        paramValue = m_entries.at(recNo).title;
    } else if (paramName == QLatin1String("commentText")) {
        paramValue = m_entries.at(recNo).text;
    }
}

void CommentsSource::onSetDSInfo(DataSetInfo& dsInfo)
{
    if (dsInfo.dataSetNo == 1)
        dsInfo.recordCount = m_entries.size();
}

}
