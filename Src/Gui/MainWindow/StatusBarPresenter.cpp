#include "StatusBarPresenter.h"

#include "Gui/MainWindow/BalanceSeesawWidget.h"
#include "Gui/MainWindow/TelemetryUiMapper.h"
#include "Gui/Style/ThemeManager.h"
#include "Widgets/Chart/Manager.h"
#include "ui_MainWindow.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolButton>

StatusBarPresenter::StatusBarPresenter(Ui::MainWindow* ui, TelemetryUiMapper* mapper,
                                       Widgets::Chart::Manager* chartManager, QWidget* parent)
    : QObject(parent)
    , m_ui(ui)
    , m_mapper(mapper)
    , m_chartManager(chartManager)
{
    build();
}

void StatusBarPresenter::build()
{
    auto* panel = new QWidget(m_ui->statusbar);
    auto* layout = new QHBoxLayout(panel);
    layout->setContentsMargins(0, 0, 4, 0);
    layout->setSpacing(18);

    // Цвет красится не через theme.qss (эти лейблы создаются кодом, а не
    // из .ui), а точечным setStyleSheet() на каждый — поэтому при смене
    // темы их нужно перекрашивать вручную, см. updatePalette() (вызывается
    // и отсюда один раз при первой сборке, и из schemeChanged при
    // переключении).
    auto addField = [&](const QString& caption) {
        auto* wrap = new QWidget(panel);
        auto* wrapLayout = new QHBoxLayout(wrap);
        wrapLayout->setContentsMargins(0, 0, 0, 0);
        wrapLayout->setSpacing(4);

        auto* captionLabel = new QLabel(caption, wrap);
        m_statusCaptionLabels.append(captionLabel);
        wrapLayout->addWidget(captionLabel);

        auto* valueLabel = new QLabel(QStringLiteral("--"), wrap);
        wrapLayout->addWidget(valueLabel);

        layout->addWidget(wrap);
        return valueLabel;
    };

    m_statusStateValue = addField(tr("Статус:"));
    m_statusStateValue->setText(tr("Готов к работе"));

    m_statusTimeValue = addField(tr("Осталось:"));
    m_statusTimeValue->setText(QStringLiteral("--:--:--"));

    m_statusPositionValue = addField(tr("Позиция:"));

    m_statusPressureCaption = new QLabel(tr("Давление:"), panel);
    m_statusCaptionLabels.append(m_statusPressureCaption);
    layout->addWidget(m_statusPressureCaption);

    auto* pressureValues = new QWidget(panel);
    auto* pressureLayout = new QHBoxLayout(pressureValues);
    pressureLayout->setContentsMargins(0, 0, 0, 0);
    pressureLayout->setSpacing(8);

    m_statusPressure1Value = new QLabel(QStringLiteral("--"), pressureValues);
    pressureLayout->addWidget(m_statusPressure1Value);

    m_statusPressure2Value = new QLabel(QStringLiteral("--"), pressureValues);
    pressureLayout->addWidget(m_statusPressure2Value);

    m_statusPressure3Value = new QLabel(QStringLiteral("--"), pressureValues);
    pressureLayout->addWidget(m_statusPressure3Value);

    layout->addWidget(pressureValues);
    updatePalette();

    m_statusBalanceSeesaw = new BalanceSeesawWidget(panel);
    layout->addWidget(m_statusBalanceSeesaw);

    m_mapper->setStatusPanelWidgets(m_statusPositionValue, m_statusPressure1Value,
                                    m_statusPressure2Value, m_statusPressure3Value,
                                    m_statusBalanceSeesaw);

    // У QStatusBar нет нативного "центра" — permanent-виджеты всегда
    // прижаты к правому краю. Эмулируем центрирование растяжками по бокам
    // внутри отдельной обёртки, которую добавляем со stretch=1, чтобы она
    // заняла всю доступную permanent-область.
    m_statusPanelWrapper = new QWidget(m_ui->statusbar);
    auto* wrapperLayout = new QHBoxLayout(m_statusPanelWrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    wrapperLayout->addStretch(1);
    wrapperLayout->addWidget(panel);
    wrapperLayout->addStretch(1);

    m_ui->statusbar->addPermanentWidget(m_statusPanelWrapper, 1);

    // На "Ручном управлении" те же данные уже крупно показаны на самой
    // странице — дублировать их в статус-баре незачем, прячем панель там.
    auto updatePanelVisibility = [this](int index) {
        if (!m_statusPanelWrapper)
            return;
        const bool onManualTab = m_ui->tabWidget_main->widget(index) == m_ui->tab_manual;
        m_statusPanelWrapper->setVisible(!onManualTab);
    };

    connect(m_ui->tabWidget_main, &QTabWidget::currentChanged,
            this, updatePanelVisibility);

    updatePanelVisibility(m_ui->tabWidget_main->currentIndex());
}

void StatusBarPresenter::updatePalette()
{
    const Gui::Style::Palette& palette = m_themeManager
        ? m_themeManager->currentPalette()
        : Gui::Style::Palette::forScheme(Gui::Style::ColorScheme::Light);

    const QString captionStyle =
        QStringLiteral("color: %1; font-size: 9pt;").arg(palette.textMuted2);
    const QString valueStyle =
        QStringLiteral("color: %1; font-family: 'JetBrains Mono';").arg(palette.textStrong);

    for (QLabel* label : std::as_const(m_statusCaptionLabels)) {
        if (label)
            label->setStyleSheet(captionStyle);
    }

    // Позиция и давления сюда намеренно не входят: их цвет — это цвет
    // датчика (MainWindow::applyPressureSensorColors(), из Registry::
    // sensorColors()), абсолютный hex, не завязанный на светлую/тёмную
    // схему. Если красить их тут тоже, каждое обновление палитры (в т.ч.
    // переключение темы кнопкой в статус-баре) стирало бы цвет датчика
    // обратно на общий textStrong — ровно так и было до этого комментария.
    const QList<QLabel*> valueLabels = { m_statusStateValue, m_statusTimeValue };
    for (QLabel* label : valueLabels) {
        if (label)
            label->setStyleSheet(valueStyle);
    }
}

void StatusBarPresenter::setThemeManager(Gui::Style::ThemeManager* themeManager)
{
    m_themeManager = themeManager;
    if (!m_themeManager)
        return;

    if (!m_themeToggleButton)
        buildThemeToggle();

    updateThemeToggleIcon(m_themeManager->currentScheme());
    updatePalette();
    m_chartManager->applyPalette(m_themeManager->currentPalette());

    connect(m_themeManager, &Gui::Style::ThemeManager::schemeChanged, this, [this](Gui::Style::ColorScheme scheme) {
        updateThemeToggleIcon(scheme);
        updatePalette();
        m_chartManager->applyPalette(m_themeManager->currentPalette());
    });
}

void StatusBarPresenter::buildThemeToggle()
{
    m_themeToggleButton = new QToolButton(m_ui->statusbar);
    m_themeToggleButton->setCursor(Qt::PointingHandCursor);
    m_ui->statusbar->addPermanentWidget(m_themeToggleButton);

    connect(m_themeToggleButton, &QToolButton::clicked, this, [this] {
        if (m_themeManager)
            m_themeManager->toggle();
    });
}

void StatusBarPresenter::updateThemeToggleIcon(Gui::Style::ColorScheme scheme)
{
    if (!m_themeToggleButton)
        return;

    const bool dark = scheme == Gui::Style::ColorScheme::Dark;
    m_themeToggleButton->setIcon(QIcon(dark
        ? QStringLiteral(":/Src/Img/ThemeSun.svg")
        : QStringLiteral(":/Src/Img/ThemeMoon.svg")));
    m_themeToggleButton->setToolTip(dark
        ? tr("Переключить на светлую тему")
        : tr("Переключить на тёмную тему"));
}
