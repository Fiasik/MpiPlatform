#include "ImageService.h"

#include <QOpenGLWidget>
#include <QPainter>

#include "Manager.h"
#include "ChartView.h"

namespace Widgets::Chart {

ImageService::ImageService(Manager* charts, QObject* parent)
    : QObject(parent)
    , m_charts(charts)
{
}

namespace {
// Ниже этого размера снимок принудительно увеличивают, временно раздувая
// сам виджет перед захватом — ch->grab() берёт ровно столько пикселей,
// сколько виджет занимает на экране, а панель графика в интерфейсе обычно
// заметно меньше, чем нужно для чёткой картинки в отчёте.
constexpr int kMinCaptureWidth = 1600;
constexpr int kMinCaptureHeight = 900;
}

QImage ImageService::captureChart(ChartType chart)
{
    auto backup = hideAuxSeries(chart);

    ChartView* ch = m_charts->chart(chart);
    if (!ch)
        return {};

    const QSize originalSize = ch->size();
    const bool needsUpscale =
        originalSize.width() < kMinCaptureWidth || originalSize.height() < kMinCaptureHeight;

    if (needsUpscale) {
        const qreal scale = qMax(
            qreal(kMinCaptureWidth) / originalSize.width(),
            qreal(kMinCaptureHeight) / originalSize.height());
        // Синхронный resize() сразу вызывает resizeEvent (график и наша
        // легенда перекладываются под новый размер) — никакого кадра с
        // "промежуточным" видом на экране не проскакивает, потому что мы ни
        // разу не возвращаем управление в цикл событий между resize() и
        // grab().
        ch->resize((originalSize.toSizeF() * scale).toSize());
    }

    QPixmap pixmap = ch->grab();

    if (auto* glWidget = ch->findChild<QOpenGLWidget*>()) {
        QPainter painter(&pixmap);
        const QPoint delta =
            glWidget->mapToGlobal(QPoint()) - ch->mapToGlobal(QPoint());
        painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        painter.drawImage(delta, glWidget->grabFramebuffer());
    }

    if (needsUpscale)
        ch->resize(originalSize);

    if (backup)
        restoreSeries(chart, *backup);

    return pixmap.toImage();
}

void ImageService::restoreSeries(ChartType chart, const SeriesVisibilityBackup& b)
{
    auto* ch = m_charts ? m_charts->chart(chart) : nullptr;

    if (!ch)
        return;

    if (chart == ChartType::Task && b.visible.size() == 3) {
        ch->visible(2, b.visible[0]);
        ch->visible(3, b.visible[1]);
        ch->visible(4, b.visible[2]);
        return;
    }

    if (chart == ChartType::Pressure && b.visible.size() == 2) {
        ch->visible(1, b.visible[0]);
        ch->visible(2, b.visible[1]);
        return;
    }
}

std::optional<ImageService::SeriesVisibilityBackup>
ImageService::hideAuxSeries(ChartType chart)
{
    auto* ch = m_charts ? m_charts->chart(chart) : nullptr;

    if (!ch)
        return std::nullopt;

    SeriesVisibilityBackup b;

    if (chart == ChartType::Task) {
        if (ch->series().size() <= 4)
            return std::nullopt;

        b.visible = {
            ch->series()[2]->isVisible(),
            ch->series()[3]->isVisible(),
            ch->series()[4]->isVisible()
        };

        ch->visible(2, false);
        ch->visible(3, false);
        ch->visible(4, false);

        return b;
    }

    if (chart == ChartType::Pressure) {
        // Обе линии регрессии (прямой/обратный ход, см. Program::
        // addRegression() — серии 1 и 2), не только прямой — иначе
        // обратный ход остаётся на сохранённом снимке даже при выключенном
        // чекбоксе регрессии.
        if (ch->series().size() <= 2)
            return std::nullopt;

        b.visible = {
            ch->series()[1]->isVisible(),
            ch->series()[2]->isVisible()
        };

        ch->visible(1, false);
        ch->visible(2, false);

        return b;
    }

    return std::nullopt;
}

}