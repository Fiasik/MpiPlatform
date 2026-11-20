#include "HelpDialog.h"
#include "ui_HelpDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QTextCursor>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextDocumentFragment>
#include <QTreeWidget>
#include <QUrl>
#include <QSysInfo>
#include <QTextBrowser>
#include <QTimer>

#include "Domain/BuildVariant.h"
#include "Domain/Tests/Option/Response/Limits.h"
#include "Domain/Tests/Option/Resolution/Limits.h"
#include "Gui/Style/RoleStyler.h"
#include "Storage/HistoryDb.h"
#include "Storage/EquipmentCatalog.h"
#include "Utils/AppInfo.h"

namespace {

QString plain(QString text)
{
    return text.remove(QLatin1Char('&')).simplified();
}

}

HelpDialog::HelpDialog(const QList<TabHelp>& tabs, QWidget* parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::HelpDialog>())
{
    ui->setupUi(this);
    Gui::Style::applyRoles(this);

    m_databasePath = QDir::toNativeSeparators(HistoryDb::databasePath());
    m_schemaVersion = HistoryDb::ensureSchema().toVersion;
    m_latestSchemaVersion = HistoryDb::latestSchemaVersion();

    ui->label_helpProgram_Text->setText(AppInfo::name());
    ui->label_helpVersion_Text->setText(AppInfo::version());
    ui->label_helpBuild_Text->setText(Domain::VariantToString(Domain::kVariant));
    ui->label_helpCompany_Text->setText(AppInfo::company());

    ui->label_helpSupportName_Text->setText(AppInfo::supportName());
    ui->label_helpSupportPhone_Text->setText(AppInfo::supportPhone());
    ui->label_helpSupportEmail_Text->setText(AppInfo::supportEmail());

    ui->label_helpQt_Text->setText(QString::fromLatin1(qVersion()));
    ui->label_helpOs_Text->setText(QSysInfo::prettyProductName());
    ui->label_helpDatabase_Text->setText(m_databasePath);
    ui->label_helpSchema_Text->setText(
        tr("%1 (последняя в этой версии — %2)").arg(m_schemaVersion).arg(m_latestSchemaVersion));

    setupArticles(tabs);

    connect(ui->pushButton_helpClose, &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->pushButton_helpCopy, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(supportText());
        ui->pushButton_helpCopy->setText(tr("Скопировано"));
        QTimer::singleShot(2000, this, [this] {
            ui->pushButton_helpCopy->setText(tr("Скопировать для поддержки"));
        });
    });
}

HelpDialog::~HelpDialog() = default;

void HelpDialog::bindArticleLink(QLabel* label, const QString& article, const QString& tooltip,
                                 const std::function<void(const QString&)>& openArticle)
{
    const QString caption = label->text() == QStringLiteral("?")
        ? QString() : label->text().toHtmlEscaped() + QStringLiteral("&nbsp;");
    label->setTextFormat(Qt::RichText);
    label->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    label->setText(caption + QStringLiteral("<a href='%1'>?</a>").arg(article.toHtmlEscaped()));
    label->setToolTip(tooltip);
    QObject::connect(label, &QLabel::linkActivated, label, openArticle);
}

void HelpDialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::PaletteChange && m_browser)
        updateArticleStyle();
}

void HelpDialog::updateArticleStyle()
{
    auto* document = m_browser->document();
    const QString css = QStringLiteral("a { color: %1; } .number { font-family: 'JetBrains Mono'; }")
        .arg(QApplication::palette().color(QPalette::Link).name());
    if (document->defaultStyleSheet() == css)
        return;

    const int scroll = m_browser->verticalScrollBar()->value();
    const QTextCursor previous = m_browser->textCursor();
    const int anchor = previous.anchor();
    const int position = previous.position();
    document->setDefaultStyleSheet(css);
    m_browser->setHtml(m_articleHtml);

    QTextCursor cursor(document);
    const int last = document->characterCount() - 1;
    cursor.setPosition(qBound(0, anchor, last));
    cursor.setPosition(qBound(0, position, last), QTextCursor::KeepAnchor);
    m_browser->setTextCursor(cursor);
    m_browser->verticalScrollBar()->setValue(scroll);
}

QString HelpDialog::supportText() const
{
    return QStringList{
        QStringLiteral("%1 %2").arg(AppInfo::name(), AppInfo::version()),
        tr("Сборка: %1").arg(Domain::VariantToString(Domain::kVariant)),
        tr("Qt %1, %2").arg(QString::fromLatin1(qVersion()), QSysInfo::prettyProductName()),
        tr("База: %1").arg(m_databasePath),
        tr("Схема базы: %1 (последняя в этой версии — %2)").arg(m_schemaVersion).arg(m_latestSchemaVersion),
    }.join(QLatin1Char('\n'));
}

QString HelpDialog::controlsHtml(const QList<TabHelp>& tabs) const
{
    const auto key = [](const QString& text) {
        return QStringLiteral("<b>%1</b>").arg(text.toHtmlEscaped());
    };

    QString html;
    html += QStringLiteral("<h3>Вкладки</h3>");
    html += tr("<p>Цифра на клавиатуре открывает вкладку по порядку слева направо "
               "(недоступные до инициализации пропускаются):</p><table cellspacing='4'>");
    for (int i = 0; i < tabs.size() && i < 9; ++i)
        html += QStringLiteral("<tr><td>%1</td><td>%2</td></tr>")
                    .arg(key(QString::number(i + 1)), plain(tabs[i].title).toHtmlEscaped());
    html += QStringLiteral("</table>");

    html += tr("<h3>Стрелки ← →</h3>"
               "<p>Листают вложенные вкладки текущей вкладки; если вложенных нет — сами вкладки окна.</p>");
    QString inner;
    for (const TabHelp& tab : tabs) {
        if (tab.innerTabs.isEmpty())
            continue;
        QStringList names;
        for (const QString& name : tab.innerTabs)
            names << plain(name).toHtmlEscaped();
        inner += QStringLiteral("<li><b>%1</b>: %2</li>")
                     .arg(plain(tab.title).toHtmlEscaped(), names.join(QStringLiteral(" · ")));
    }
    if (!inner.isEmpty())
        html += QStringLiteral("<ul>%1</ul>").arg(inner);

    const auto actions = [&](bool shift) {
        QString list;
        for (const TabHelp& tab : tabs) {
            const QString action = shift ? tab.shiftEnterAction : tab.enterAction;
            if (action.isEmpty())
                continue;
            list += QStringLiteral("<li><b>%1</b>: «%2»</li>")
                        .arg(plain(tab.title).toHtmlEscaped(), plain(action).toHtmlEscaped());
        }
        return list;
    };
    html += tr("<h3>Enter</h3><p>Нажимает главную кнопку текущей вкладки (старт или остановка теста, "
               "инициализация, формирование отчёта):</p>");
    html += QStringLiteral("<ul>%1</ul>").arg(actions(false));
    const QString second = actions(true);
    if (!second.isEmpty())
        html += tr("<h3>Shift + Enter</h3><p>Вторая кнопка вкладки:</p><ul>%1</ul>").arg(second);

    html += tr("<h3>Остальное</h3><ul>"
               "<li><b>Tab</b> / <b>Shift + Tab</b> — по полям карточки клапана сверху вниз и обратно; "
               "в окне карточки цифры открывают её вкладки;</li>"
               "<li><b>Delete</b> — убрать выбранную запасную часть из списка;</li>"
               "<li><b>Ctrl + колесо мыши</b> — масштаб в «Предпросмотре» отчёта;</li>"
               "<li><b>F1</b> или кнопка «?» в строке состояния — эта справка;</li>"
               "<li>щелчок по всплывающему уведомлению закрывает его.</li></ul>");
    return html;
}

QList<HelpDialog::Article> HelpDialog::testArticles() const
{
    using namespace Domain::Tests::Option;
    const auto limits = [](double (*allowed)(int)) {
        QStringList parts;
        for (int i = 0; i < 4; ++i)
            parts << QString::number(allowed(i), 'f', 0);
        return parts.join(QStringLiteral(" / "));
    };

    QList<Article> articles;

    // Линейность — экспериментальная (MPIPLATFORM_EXPERIMENTAL_LINEARITY).
    QString linearityHelp;
#ifdef MPIPLATFORM_EXPERIMENTAL_LINEARITY
    linearityHelp = tr("<li><b>Линейность</b> — наибольшее отклонение характеристики «давление → положение» "
                       "от прямой, в % рабочего окна 10–90 % хода, худший из двух ходов;</li>");
#endif

    articles.append(Article{QStringLiteral("main-test"), tr("Основной тест"),
        tr("<p><b>Зачем.</b> Главная диагностика привода и клапана: трение, пружина, "
               "динамическая ошибка, ток страгивания.</p>"
               "<p><b>Как идёт.</b> Задание плавно проходит весь диапазон вперёд и назад; на каждом "
               "шаге записываются давление, положение и задание.</p>"
               "<p><b>Что считается.</b></p><ul>"
               "<li><b>Трение</b> — разность давлений прямого и обратного ходов на середине хода "
               "(сила — в Н, коэффициент — в %). Растёт при износе уплотнений, загрязнении, "
               "перетянутом сальнике;</li>"
               "<li><b>Пружина</b> — диапазон давлений, в котором клапан проходит ход; сравнивается "
               "с паспортом привода. Уход вниз/вверх — усталость или неверная настройка пружины;</li>"
               "%1"
               "<li><b>Динамическая ошибка</b> — разность заданий прямого и обратного ходов при одном "
               "положении (мА и %). Показывает гистерезис привода вместе с позиционером;</li>"
               "<li><b>Ток страгивания</b> — задание в момент обнаружения начала движения, "
               "отдельно для прямого и обратного хода.</li></ul>"
               "<p><b>Как проверить.</b> Сравните значения с допусками карточки (трение, "
               "динамическая ошибка, ход, пружина — подсвечены цветом). Серые полосы на графике "
               "давления — края хода вне окна 10–90 %, они в расчёт не входят. Для спорного "
               "результата перетащите линии регрессии на графике — значения пересчитаются.</p>").arg(linearityHelp) + tr("<p>Расчёты: <a href='friction'>трение</a> · "
                   "<a href='dynamic-error'>динамическая ошибка</a> · "
                   "<a href='breakaway'>ток страгивания</a></p>")});

    articles.append(Article{QStringLiteral("stroke"), tr("Ход клапана (Stroke)"),
        tr("<p><b>Зачем.</b> Время полного хода и время реакции в обе стороны.</p>"
               "<p><b>Как идёт.</b> Задание сразу переключается на противоположный край и обратно; "
               "по графику положения ищется начало и конец движения.</p>"
               "<p><b>Как проверить.</b> Сравните время прямого и обратного хода с требованием к "
               "клапану; метки начала и конца движения отмечены на графике — убедитесь, что они "
               "соответствуют началу и концу движения. Нули означают, что события на "
               "графике не найдены.</p><p><a href='reaction-time'>Как рассчитывается время</a></p>")});

    articles.append(Article{QStringLiteral("step"), tr("Ступенчатый тест (Step)"),
        tr("<p><b>Зачем.</b> Поведение позиционера на малых скачках задания: время достижения "
               "порога T и перерегулирование.</p>"
               "<p><b>Как идёт.</b> Серия ступеней задания вверх и вниз; для каждой ступени фиксируется "
               "момент, когда положение достигло порога, и максимальный перелёт.</p>"
               "<p><b>Как проверить.</b> Большое время T — медленный позиционер/привод; большое "
               "перерегулирование — колебательная настройка. Ступени, до которых клапан не доехал, "
               "показывают 0:00.000.</p><p><a href='overshoot'>Формулы и пример расчёта</a></p>")});

    articles.append(Article{QStringLiteral("response"), tr("ТРС (Response)"),
        tr("<p><b>Зачем.</b> Насколько точно клапан отрабатывает ступенчатые изменения задания.</p>"
               "<p><b>Как идёт.</b> Базовый уровень, затем ступени вверх и вниз по четыре в каждую "
               "сторону.</p>"
               "<p><a href='step-error'>Ошибка на шаг</a> считается относительно предыдущей "
               "рассчитанной ступени или базовой полки.</p>"
               "<p><b>Допуск для шагов 1–4 в каждом направлении, %:</b> %1.</p>"
               "<p>Зелёная точка означает, что ошибка в допуске. Красная показывает превышение.</p>")
            .arg(limits(Response::allowedErrorForStep))});

    articles.append(Article{QStringLiteral("resolution"), tr("ТЧР (Resolution)"),
        tr("<p><b>Зачем.</b> Наименьший шаг задания, который клапан ещё отрабатывает (разрешающая "
               "способность).</p>"
               "<p><a href='step-error'>Ошибка на шаг</a> для каждой ступени считается "
               "относительно исходной базовой полки.</p>"
               "<p><b>Допуск для шагов 1–4 в каждом направлении, %:</b> %1.</p>"
               "<p>Сравните ошибку с допуском в таблице результатов.</p>")
            .arg(limits(Resolution::allowedErrorForStep))});

    if (Domain::kSkiVariant) {
        articles.append(Article{QStringLiteral("cyclic"), tr("Циклические тесты"),
            tr("<p>Клапан многократно проходит заданную последовательность. Перед запуском "
               "проверьте диапазон испытаний и заданное количество циклов. Счётчик выполненных "
               "циклов показывает ход испытания.</p>"
               "<ul><li><b>Регулирующая арматура.</b> Для каждого диапазона результат содержит "
               "крайние достигнутые положения и номера циклов, в которых они наблюдались.</li>"
               "<li><b>Запорная арматура.</b> В результат входят счётчики срабатывания концевых "
               "выключателей и переключений дискретных выходов.</li></ul>"
               "<p>Для сравнения испытаний откройте сохранённые сессии на вкладке «История».</p>")});
    }

    articles.append(Article{QStringLiteral("sensor-check"), tr("Проверка датчиков"),
        tr("<p>Раз в 60 дней при запуске программа напоминает проверить датчики стенда (давление, "
               "положение, ток). Нажмите «Проверено сегодня», когда проверка выполнена; «Напомнить "
               "завтра» переносит напоминание.</p>")});

    return articles;
}

QString HelpDialog::sensorHtml() const
{
    return tr(
        "<h3>Выбор датчика перемещения</h3>"
        "<p>Для более точных диагностических графиков выбирайте датчик с меньшим диапазоном, "
        "который покрывает полный ход клапана с запасом для установки.</p>"
        "<ul><li><b>0–50 мм:</b> линейный ход до 40 мм; поворотный шкив А — до 90°, "
        "шкив В — до 50°.</li>"
        "<li><b>0–125 мм:</b> линейный ход от 45 до 100 мм; шкив В — до 90°, "
        "шкив С — до 50°.</li>"
        "<li><b>0–225+ мм:</b> линейный ход свыше 100 мм; шкив С — до 90°.</li></ul>"
        "<p>Для хода между 40 и 45 мм отдельная рекомендация в исходном материале не задана. "
        "Проверьте, что выбранный датчик покрывает весь ход с установочным запасом.</p>"
        "<h3>Установка тросика</h3>"
        "<p>При движении клапана тросик может вытягиваться из датчика или втягиваться в него. "
        "В обоих случаях на всём ходе показания должны оставаться в допустимом диапазоне.</p>"
        "<p>При установке для вытягивания исходный выход тросика — не более 5 мм "
        "между защитным резиновым кожухом и гнездом. Пример: датчик 0–50 мм, "
        "ход клапана 40 мм, исходный выход не более 5 мм; полный выход не должен превышать 50 мм.</p>"
        "<p>При установке для втягивания исходный выход тросика должен превышать ход клапана "
        "на 5 мм и оставаться в диапазоне датчика. Пример: датчик 0–125 мм, ход 100 мм; "
        "исходный выход — более 105 мм, но не более 125 мм.</p>"
        "<p>Запас механического хода датчика за пределами измерительного диапазона защищает "
        "тросик от повреждения при ошибке установки; для измерений этот запас не используется.</p>"
        "<h3>Калибровочные точки</h3>"
        "<p>В «Ручном управлении» показаны крайние точки хода клапана, определённые "
        "при инициализации. В исходной справке приведены примеры корректной установки:</p>"
        "<ul><li>Ход клапана 15 мм: начальная точка 41,50 мм, конечная 26,11 мм.</li>"
        "<li>Ход клапана 40 мм: начальная точка 6 мм, конечная 47 мм.</li></ul>"
        "<p>Для поворотных клапанов сохраняется тот же принцип установки датчика.</p>");
}

void HelpDialog::setupArticles(const QList<TabHelp>& tabs)
{
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    auto* navigation = new QWidget(splitter);
    auto* layout = new QVBoxLayout(navigation);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* search = new QLineEdit(navigation);
    search->setPlaceholderText(tr("Поиск по справочнику"));
    search->setAccessibleName(search->placeholderText());
    search->setClearButtonEnabled(true);
    layout->addWidget(search);

    m_tree = new QTreeWidget(navigation);
    m_tree->setObjectName(QStringLiteral("helpNavigation"));
    m_tree->setAccessibleName(tr("Разделы справочника"));
    m_tree->setHeaderHidden(true);
    layout->addWidget(m_tree);

    auto* pages = new QStackedWidget(splitter);
    auto* browser = m_browser = new QTextBrowser(pages);
    browser->setObjectName(QStringLiteral("helpArticle"));
    browser->setOpenLinks(false);
    browser->document()->setDocumentMargin(16);
    pages->addWidget(browser);
    updateArticleStyle();

    auto* about = new QScrollArea(pages);
    about->setWidgetResizable(true);
    ui->verticalLayout_help->removeWidget(ui->tab_helpAbout);
    about->setWidget(ui->tab_helpAbout);
    pages->addWidget(about);

    splitter->addWidget(navigation);
    splitter->addWidget(pages);
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({260, 640});
    ui->verticalLayout_help->insertWidget(0, splitter, 1);

    constexpr int idRole = Qt::UserRole;
    constexpr int htmlRole = Qt::UserRole + 1;
    constexpr int searchRole = Qt::UserRole + 2;
    const auto add = [&](const Article& article, QTreeWidgetItem* parent = nullptr) {
        auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
        item->setText(0, article.title);
        item->setData(0, idRole, article.id);
        item->setData(0, htmlRole, article.html);
        item->setData(0, searchRole,
            (article.title + QLatin1Char(' ') +
             QTextDocumentFragment::fromHtml(article.html).toPlainText()).simplified());
        m_articles.insert(article.id, item);
        return item;
    };

    add({QStringLiteral("home"), tr("Главная"),
         tr("<p><b>%1</b> управляет стендом диагностики трубопроводной арматуры, "
            "записывает графики испытаний и формирует отчёты.</p>"
            "<h2>Быстрые ссылки</h2>"
            "<ul><li><a href='getting-started'>Подготовить стенд</a></li>"
            "<li><a href='tests'>Выбрать испытание</a></li>"
            "<li><a href='calculations'>Посмотреть формулы и примеры</a></li>"
            "<li><a href='equipment'>Найти оборудование</a></li>"
            "<li><a href='about'>Поддержка и сведения о программе</a></li></ul>"
            "<p>Разработчик — %2.</p>").arg(AppInfo::name().toHtmlEscaped(),
                                                    AppInfo::company().toHtmlEscaped())});

    auto* start = add({QStringLiteral("getting-started"), tr("Начало работы"),
                       tr("<ol><li><a href='connection'>Подключите стенд</a> и проверьте статус порта.</li>"
                          "<li>Оформите карточку клапана и проверьте данные привода.</li>"
                          "<li><a href='sensor'>Выберите датчик перемещения</a>.</li>"
                          "<li>Выполните <a href='initialization'>инициализацию</a> и проверьте точки хода.</li>"
                          "<li>Выберите <a href='tests'>испытание</a>.</li></ol>")});
    add({QStringLiteral("connection"), tr("Подключение стенда"),
         tr("<p>Подключите МПИ к компьютеру согласно документации стенда. В программе "
            "выберите последовательный порт устройства и откройте «Ручное управление».</p>"
            "<p>В блоке «Статус устройства» проверьте поле «Порт» и сведения о подключённых "
            "датчиках. Если порт отсутствует в списке, проверьте, что компьютер распознал "
            "устройство. Если порт есть, но связи нет, проверьте выбранный порт и закройте "
            "другие приложения, которые могут его использовать.</p>"
            "<p><a href='initialization'>Инициализация</a></p>")}, start);
    add({QStringLiteral("sensor"), tr("Датчик перемещения"), sensorHtml()}, start);
    add({QStringLiteral("initialization"), tr("Инициализация"),
         tr("<p>Перед запуском проверьте данные в карточке клапана и состояние датчиков. "
            "Во время инициализации клапан перемещается по ходу.</p>"
            "<p>В «Ручном управлении» запустите инициализацию и дождитесь результата в "
            "блоке «Статус устройства». Сверьте «Калибровочные точки» с ожидаемыми крайними "
            "положениями клапана и рабочим диапазоном датчика.</p>"
            "<p>Если инициализация не выполнена, проверьте <a href='connection'>связь с МПИ</a> "
            "и <a href='sensor'>показания датчика перемещения</a>. После отключения устройства "
            "выполните инициализацию повторно.</p>")}, start);
    if (!tabs.isEmpty())
        add({QStringLiteral("controls"), tr("Горячие клавиши"), controlsHtml(tabs)}, start);

    auto* tests = add({QStringLiteral("tests"), tr("Испытания"),
                       tr("<p>Выберите испытание в дереве слева. Допуски, которые настраиваются "
                          "для конкретного клапана, находятся на вкладке Pass | Fail "
                          "его карточки.</p>")});
    for (const Article& article : testArticles())
        add(article, article.id == QStringLiteral("sensor-check") ? start : tests);

    auto* calculations = add({QStringLiteral("calculations"), tr("Характеристики и расчёты"),
                             tr("<ul><li><a href='friction'>Сила и коэффициент трения</a></li>"
                                "<li><a href='dynamic-error'>Динамическая ошибка</a></li>"
                                "<li><a href='breakaway'>Ток страгивания</a></li>"
                                "<li><a href='reaction-time'>Время реакции и полного хода</a></li>"
                                "<li><a href='overshoot'>Порог T и перерегулирование</a></li>"
                                "<li><a href='step-error'>Ошибка на шаг</a></li></ul>")});
    Q_INIT_RESOURCE(help_articles);
    const auto addCalculation = [&](const QString& id, const QString& title) {
        QFile file(QStringLiteral(":/help/%1.html").arg(id));
        const QString html = file.open(QIODevice::ReadOnly)
            ? QString::fromUtf8(file.readAll())
            : tr("<p>Статья недоступна.</p>");
        add({id, title, html}, calculations);
    };
    addCalculation(QStringLiteral("friction"), tr("Сила и коэффициент трения"));
    addCalculation(QStringLiteral("dynamic-error"), tr("Динамическая ошибка"));
    addCalculation(QStringLiteral("breakaway"), tr("Ток страгивания"));
    addCalculation(QStringLiteral("reaction-time"), tr("Время реакции и полного хода"));
    addCalculation(QStringLiteral("overshoot"), tr("Порог T и перерегулирование"));
    addCalculation(QStringLiteral("step-error"), tr("Ошибка на шаг"));

    add({QStringLiteral("glossary"), tr("Глоссарий"),
         tr("<dl><dt><b>Позиционер</b></dt><dd>Устройство, которое регулирует положение "
            "клапана по управляющему сигналу.</dd>"
            "<dt><b>Привод</b></dt><dd>Механизм, который перемещает шток или поворачивает вал клапана.</dd>"
            "<dt><b>Гистерезис</b></dt><dd>Различие характеристик прямого и обратного хода. "
            "См. <a href='main-test'>основной тест</a>.</dd>"
            "<dt><b>Перерегулирование</b></dt><dd>Величина, на которую положение выходит за "
            "целевой уровень после ступени. См. <a href='overshoot'>перерегулирование</a>.</dd>"
            "<dt><b>Ток страгивания</b></dt><dd>Задание в мА, при котором программа фиксирует "
            "начало движения. См. <a href='breakaway'>расчёт тока страгивания</a>.</dd>"
            "<dt><b>Калибровочные точки</b></dt><dd>Измеренные при инициализации крайние положения "
            "клапана. См. <a href='initialization'>инициализацию</a>.</dd>"
            "<dt><b>ТРС</b></dt><dd>Тест реакции на ступенчатые изменения задания. "
            "См. <a href='response'>Response</a>.</dd>"
            "<dt><b>ТЧР</b></dt><dd>Тест чувствительности и разрешающей способности. "
            "См. <a href='resolution'>Resolution</a>.</dd></dl>")});

    auto* catalog = add({QStringLiteral("equipment"), tr("Каталог оборудования"),
                         tr("<p>Введите модель позиционера, привода или производителя в поиск слева.</p>"
                            "<ul><li><a href='positioners'>Позиционеры</a></li>"
                            "<li><a href='drives'>Приводы</a></li>"
                            "<li><a href='manufacturers'>Производители арматуры</a></li>"
                            "<li><a href='valve-models'>Модели клапанов</a></li></ul>")});
    const auto catalogPage = [&](EquipmentCatalog::Kind kind) {
        QString html;
        QString maker;
        for (const auto& entry : EquipmentCatalog::entries(kind)) {
            if (maker != entry.manufacturer) {
                if (!maker.isEmpty())
                    html += QStringLiteral("</ul>");
                maker = entry.manufacturer;
                html += QStringLiteral("<h2>%1</h2><ul>").arg(maker.toHtmlEscaped());
            }
            html += QStringLiteral("<li>%1</li>").arg(entry.name.toHtmlEscaped());
        }
        return html.isEmpty() ? tr("<p>В каталоге нет записей.</p>")
                              : html + QStringLiteral("</ul>");
    };
    add({QStringLiteral("positioners"), tr("Позиционеры"),
         catalogPage(EquipmentCatalog::Kind::Positioner)}, catalog);
    add({QStringLiteral("drives"), tr("Приводы"),
         catalogPage(EquipmentCatalog::Kind::Drive)}, catalog);

    QString manufacturers = QStringLiteral("<ul>");
    for (const QString& manufacturer : EquipmentCatalog::manufacturers())
        manufacturers += QStringLiteral("<li>%1</li>").arg(manufacturer.toHtmlEscaped());
    manufacturers += QStringLiteral("</ul>");
    add({QStringLiteral("manufacturers"), tr("Производители арматуры"), manufacturers}, catalog);

    add({QStringLiteral("valve-models"), tr("Модели клапанов"),
         tr("<p>Выберите <a href='manufacturers'>производителя арматуры</a> в карточке. "
            "Модель клапана указывается вручную в поле «Модель клапана». "
            "Возьмите обозначение модели с маркировки или из паспорта клапана.</p>")}, catalog);

    auto* support = add({QStringLiteral("about"), tr("Поддержка и о программе"),
                         tr("Контакты, телефон, электронная почта, версия, сборка, Qt, ОС, путь к базе данных")});
    add({QStringLiteral("registration"), tr("Государственная регистрация"),
         tr("<p>СКИ «Арматура-Контроль»</p>"
            "<p>Свидетельство о государственной регистрации программы для ЭВМ № 2025691376.</p>"
            "<p>Дата регистрации — 13 ноября 2025 года.</p>"
            "<p>Правообладатель — ООО «РДК Системс».</p>"
            "<p>Авторы — Еремин Павел Валерьевич, Чеботарёв Алексей Дмитриевич, "
            "Балагин Владислав Андреевич.</p>"
            "<p>Заявка № 2025690631 от 5 ноября 2025 года.</p>")}, support);

    connect(m_tree, &QTreeWidget::currentItemChanged, this,
            [this, pages, browser, about, search](QTreeWidgetItem* item) {
        const bool supportPage = item && item->data(0, idRole).toString() == QStringLiteral("about");
        ui->pushButton_helpCopy->setVisible(supportPage);
        pages->setCurrentWidget(supportPage ? static_cast<QWidget*>(about) : browser);
        if (item && !supportPage) {
            m_articleHtml = QStringLiteral("<h1>%1</h1>%2")
                .arg(item->text(0).toHtmlEscaped(), item->data(0, htmlRole).toString());
            browser->setHtml(m_articleHtml);
            const QString query = search->text().simplified();
            if (!query.isEmpty())
                browser->find(query);
        } else if (!item) {
            m_articleHtml = tr("<p>По этому запросу ничего не найдено.</p>");
            browser->setHtml(m_articleHtml);
        }
    });
    connect(browser, &QTextBrowser::anchorClicked, this, [this, search](const QUrl& link) {
        const QString id = link.toString();
        if (m_articles.contains(id)) {
            search->clear();
            showArticle(id);
        }
    });
    connect(search, &QLineEdit::textChanged, this, [this, browser](const QString& text) {
        const QString query = text.simplified();
        QTreeWidgetItem* firstMatch = nullptr;
        QTreeWidgetItem* previous = m_tree->currentItem();
        // Сначала проверяем потомков, чтобы оставить путь к найденной статье.
        const auto filter = [&](const auto& self, QTreeWidgetItem* item) -> bool {
            bool childMatched = false;
            for (int i = 0; i < item->childCount(); ++i)
                childMatched = self(self, item->child(i)) || childMatched;
            const bool matches = item->data(0, searchRole).toString().contains(query, Qt::CaseInsensitive);
            if (matches && !firstMatch)
                firstMatch = item;
            item->setHidden(!matches && !childMatched);
            return matches || childMatched;
        };
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
            filter(filter, m_tree->topLevelItem(i));
        if (!query.isEmpty())
            m_tree->expandAll();
        if (query.isEmpty() && !previous)
            showArticle(QStringLiteral("home"));
        else if (!query.isEmpty() &&
                 (!previous || !previous->data(0, searchRole).toString().contains(query, Qt::CaseInsensitive)))
            m_tree->setCurrentItem(firstMatch);
        else {
            QTextCursor cursor(browser->document());
            browser->setTextCursor(cursor);
            if (!query.isEmpty())
                browser->find(query);
        }
    });
    showArticle(QStringLiteral("home"));
}

void HelpDialog::showArticle(const QString& id)
{
    auto* item = m_articles.value(id, m_articles.value(QStringLiteral("home")));
    for (auto* parent = item->parent(); parent; parent = parent->parent())
        parent->setExpanded(true);
    m_tree->setCurrentItem(item);
    m_tree->scrollToItem(item);
}
