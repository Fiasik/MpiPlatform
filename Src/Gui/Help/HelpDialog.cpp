#include "HelpDialog.h"
#include "ui_HelpDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QSysInfo>
#include <QTextBrowser>
#include <QTimer>

#include "Domain/BuildVariant.h"
#include "Domain/Tests/Option/Response/Limits.h"
#include "Domain/Tests/Option/Resolution/Limits.h"
#include "Gui/Style/RoleStyler.h"
#include "Storage/HistoryDb.h"
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

    ui->textBrowser_helpControls->setHtml(controlsHtml(tabs));
    // Справка по тестам: общий текст + по вложенной вкладке на каждый тест.
    // testsHtml() режется по заголовкам <h3> — заголовок становится названием
    // вкладки, текст до первого заголовка — вкладкой "Общее".
    {
        const QStringList parts = testsHtml().split(QStringLiteral("<h3>"));
        for (int i = 0; i < parts.size(); ++i) {
            QString title = tr("Общее");
            QString body = parts.at(i);
            if (i > 0) {
                const int end = body.indexOf(QStringLiteral("</h3>"));
                title = body.left(end);
                body = body.mid(end + 5);
            }
            if (body.trimmed().isEmpty())
                continue;
            auto* browser = new QTextBrowser(ui->tabWidget_helpTests);
            browser->setOpenExternalLinks(false);
            browser->setHtml(body);
            ui->tabWidget_helpTests->addTab(browser, title);
        }
    }

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

QString HelpDialog::testsHtml() const
{
    using namespace Domain::Tests::Option;
    const auto limits = [](double (*allowed)(int)) {
        QStringList parts;
        for (int i = 0; i < 4; ++i)
            parts << QString::number(allowed(i), 'f', 0);
        return parts.join(QStringLiteral(" / "));
    };

    QString html;
    html += tr("<p>Для каждого теста: зачем он, как идёт, что считается и как читать результат. "
               "Допуски задаются в карточке клапана (вкладка Pass | Fail); здесь они названы "
               "по полям карточки.</p>");

    // Линейность — экспериментальная (MPIPLATFORM_EXPERIMENTAL_LINEARITY).
    QString linearityHelp;
#ifdef MPIPLATFORM_EXPERIMENTAL_LINEARITY
    linearityHelp = tr("<li><b>Линейность</b> — наибольшее отклонение характеристики «давление → положение» "
                       "от прямой, в % рабочего окна 10–90 % хода, худший из двух ходов;</li>");
#endif

    html += tr("<h3>Основной тест</h3>"
               "<p><b>Зачем.</b> Главная диагностика привода и клапана: трение, пружина, "
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
               "<li><b>Ток страгивания</b> — минимальное абсолютное задание (мА), при котором "
               "положение ушло от исходного больше чем на 0,2 % хода, отдельно для прямого и обратного "
               "хода. Допуска нет — значения для наблюдения. Большое значение или сильная разница "
               "ходов указывает на «залипание» штока.</li></ul>"
               "<p><b>Как проверить.</b> Сравните значения с допусками карточки (трение, "
               "динамическая ошибка, ход, пружина — подсвечены цветом). Серые полосы на графике "
               "давления — края хода вне окна 10–90 %, они в расчёт не входят. Для спорного "
               "результата перетащите линии регрессии на графике — значения пересчитаются.</p>").arg(linearityHelp);

    html += tr("<h3>Ход клапана (Stroke)</h3>"
               "<p><b>Зачем.</b> Время полного хода и время реакции в обе стороны.</p>"
               "<p><b>Как идёт.</b> Задание сразу переключается на противоположный край и обратно; "
               "по графику положения ищется начало и конец движения.</p>"
               "<p><b>Как проверить.</b> Сравните время прямого и обратного хода с требованием к "
               "клапану; метки начала и конца движения отмечены на графике — убедитесь, что они "
               "стоят там, где движение действительно началось. Нули означают, что события на "
               "графике не найдены.</p>");

    html += tr("<h3>Ступенчатый тест (Step)</h3>"
               "<p><b>Зачем.</b> Поведение позиционера на малых скачках задания: время достижения "
               "порога T и перерегулирование.</p>"
               "<p><b>Как идёт.</b> Серия ступеней задания вверх и вниз; для каждой ступени фиксируется "
               "момент, когда положение достигло порога, и максимальный перелёт.</p>"
               "<p><b>Как проверить.</b> Большое время T — медленный позиционер/привод; большое "
               "перерегулирование — колебательная настройка. Ступени, до которых клапан не доехал, "
               "показывают 0:00.000 — это не ошибка расчёта, а отсутствие достижения.</p>");

    html += tr("<h3>ТРС (Response)</h3>"
               "<p><b>Зачем.</b> Насколько точно клапан отрабатывает ступенчатые изменения задания.</p>"
               "<p><b>Как идёт.</b> Базовый уровень, затем ступени вверх и вниз по четыре в каждую "
               "сторону.</p>"
               "<p><b>Что считается.</b> Для каждой ступени — отклонение фактического уровня от идеала "
               "(идеал = предыдущий уровень ± размер ступени), в процентах ступени. Фактический "
               "уровень — среднее по полке без первого перескока: в начале каждой ступени позиционер "
               "с неверно откалиброванной функцией shutoff даёт один выброс; он отрезается (до тех пор, "
               "пока положение не устоялось рядом с медианой полки) и закрашен красноватым на графике. "
               "Остальная полка идёт в расчёт целиком.</p>"
               "<p><b>Допуск по номеру ступени, %:</b> %1.</p>"
               "<p><b>Как проверить.</b> Зелёная точка — ступень в допуске, красная — превышен. "
               "Много серого в середине полки означает, что клапан долго не мог устояться — "
               "смотрите на механику, а не только на число.</p>")
            .arg(limits(Response::allowedErrorForStep));

    html += tr("<h3>ТЧР (Resolution)</h3>"
               "<p><b>Зачем.</b> Наименьший шаг задания, который клапан ещё отрабатывает (разрешающая "
               "способность).</p>"
               "<p><b>Как идёт и что считается.</b> Расчёт тот же, что у ТРС, но ступени отсчитываются от "
               "базового уровня, а не от предыдущей ступени; уровень также считается как среднее без "
               "первого перескока (красноватым на графике).</p>"
               "<p><b>Допуск по номеру ступени, %:</b> %1.</p>"
               "<p><b>Как проверить.</b> Ступени малого размера, на которые клапан не откликнулся, "
               "дают большую ошибку — это и есть мёртвая зона. Сравните число с допуском в таблице "
               "результатов.</p>")
            .arg(limits(Resolution::allowedErrorForStep));

    html += tr("<h3>Циклические тесты</h3>"
               "<p>Есть не во всех сборках. Многократное перемещение клапана для оценки износа и "
               "стабильности: результат сравнивается между сессиями на вкладке «История».</p>");

    html += tr("<h3>Проверка датчиков</h3>"
               "<p>Раз в 60 дней при запуске программа напоминает проверить датчики стенда (давление, "
               "положение, ток). Нажмите «Проверено сегодня», когда проверка выполнена; «Напомнить "
               "завтра» переносит напоминание.</p>");
    return html;
}
