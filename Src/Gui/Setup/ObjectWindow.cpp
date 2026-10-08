#include "ObjectWindow.h"
#include "ui_ObjectWindow.h"

#include <QCalendarWidget>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QScrollArea>
#include <QShowEvent>
#include <QStyle>
#include <QTextCharFormat>
#include <QTimer>

#include "Domain/ValveEnums.h"
#include "Gui/Style/Palette.h"
#include "Gui/Style/RoleStyler.h"
#include "Storage/HistoryReader.h"
#include "Storage/Registry.h"
#include "Utils/StyledMessageBox.h"

ObjectWindow::ObjectWindow(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ObjectWindow)
{
    ui->setupUi(this);

    // Правая панель (frame_history_Section и всё внутри неё) целиком
    // описана в .ui — раньше собиралась здесь вручную (новые виджеты +
    // layout-хирургия над готовым layout'ом диалога), и именно оттуда
    // раз за разом вылезали архитектурные баги (пропавшие отступы,
    // унаследованная от диалога sizePolicy Maximum ломала горизонтальный
    // fullscreen) — их источником была сама ручная пересборка, не
    // конкретная строчка. Тут остаётся только то, что Designer выразить
    // не может: подключение к QListWidget::currentItemChanged.
    connect(ui->listWidget_history, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *cur, QListWidgetItem *) {
                showPositionCard(cur ? cur->text() : QString());
            });

    Gui::Style::applyRoles(this);

    ui->dateEdit->setDate(QDate::currentDate());

    if (QCalendarWidget* calendar = ui->dateEdit->calendarWidget()) {
        QTextCharFormat weekendFormat;
        weekendFormat.setForeground(Gui::Style::statusColor(Gui::Style::StatusKind::Error));
        calendar->setWeekdayTextFormat(Qt::Saturday, weekendFormat);
        calendar->setWeekdayTextFormat(Qt::Sunday, weekendFormat);
        calendar->setGridVisible(false);
    }

    QRegularExpression regular;
    regular.setPattern("[^/?*:;{}\\\\]+");
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(regular, this);

    // comboBox_object/manufactory/department — Widgets::Combo::
    // SearchableComboBox (см. .ui), поиск уже встроен в сам виджет.
    ui->comboBox_object->setValidator(validator);
    ui->comboBox_manufactory->setValidator(validator);
    ui->comboBox_department->setValidator(validator);

    /*
        Каскадная система
        при сменене объекта, пересобирается список цехов под него,
        а если сменили цех то список отделений под пару объект+цех.
    */

    connect(ui->comboBox_object, &QComboBox::currentIndexChanged,
            this, &ObjectWindow::reloadManufactories);
    connect(ui->comboBox_manufactory, &QComboBox::currentIndexChanged,
            this, &ObjectWindow::reloadDepartments);

    /*
        Панель справа отражает текущую связку объект / цех / отделение,
        обновляется при смене любого из трёх.
    */
    connect(ui->comboBox_object, &QComboBox::currentIndexChanged,
            this, &ObjectWindow::refreshHistoryPanel);
    connect(ui->comboBox_manufactory, &QComboBox::currentIndexChanged,
            this, &ObjectWindow::refreshHistoryPanel);
    connect(ui->comboBox_department, &QComboBox::currentIndexChanged,
            this, &ObjectWindow::refreshHistoryPanel);

    connect(ui->pushButton_start, &QPushButton::clicked,
            this, &ObjectWindow::ButtonClick);
}

ObjectWindow::~ObjectWindow()
{
    for (QComboBox *combo : {ui->comboBox_object, ui->comboBox_manufactory,
                             ui->comboBox_department})
        combo->disconnect(this);

    ui->listWidget_history->disconnect(this);

    delete ui;
}

void ObjectWindow::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    // showMaximized() тут не годится: на Windows это в итоге ShowWindow(...,
    // SW_MAXIMIZE) в Win32, а у QDialog по умолчанию нет флага
    // Qt::WindowMaximizeButtonHint (нет кнопки максимизации на рамке) —
    // Windows такой запрос на диалог тихо игнорирует, окно остаётся
    // прежнего размера. showFullScreen() — другой режим (Qt::WindowFullScreen,
    // сам меняет стиль окна и растягивает поверх экрана), от наличия
    // maximize-box не зависит, работает и для QDialog.
    //
    // Отложено на singleShot(0), а не вызвано прямо тут же — тем же
    // приёмом, что MainWindow уже применяет в main.cpp: смена
    // window state прямо в первом showEvent() на части платформ
    // применяется не всегда корректно, со следующей итерации цикла
    // событий — уже проверенно рабочий вариант.
    if (!m_fullScreenOnce) {
        m_fullScreenOnce = true;
        QTimer::singleShot(0, this, [this] { showFullScreen(); });
    }
}

void ObjectWindow::refreshHistoryPanel()
{
    if (!m_registry || m_repopulating)
        return;

    const QString object = ui->comboBox_object->currentText();
    const QString manufactory = ui->comboBox_manufactory->currentText();
    const QString department = ui->comboBox_department->currentText();

    const QStringList positions =
        HistoryReader::positions(object, manufactory, department);

    ui->listWidget_history->clear();
    ui->listWidget_history->addItems(positions);

    if (positions.isEmpty()) {
        ui->label_history_Title->setText(department.isEmpty()
            ? tr("Выберите объект, цех и отделение")
            : tr("В базе нет позиций по этой связке"));
    } else {
        ui->label_history_Title->setText(
            tr("Позиции в базе — %1 / %2").arg(manufactory, department));
    }

    ui->listWidget_history->setVisible(!positions.isEmpty());

    if (!positions.isEmpty()) {
        const int rowH = qMax(ui->listWidget_history->sizeHintForRow(0),
                              ui->listWidget_history->fontMetrics().height() + 6);
        const int rows = qMin(positions.size(), 7);
        ui->listWidget_history->setFixedHeight(
            rowH * rows + 2 * ui->listWidget_history->frameWidth() + 4);
    }

    showPositionCard(QString());
}

void ObjectWindow::showPositionCard(const QString &position)
{
    if (!m_registry)
        return;

    while (ui->formLayout_card->rowCount() > 0)
        ui->formLayout_card->removeRow(0);

    const QString object = ui->comboBox_object->currentText();
    const QString manufactory = ui->comboBox_manufactory->currentText();
    const QString department = ui->comboBox_department->currentText();

    ValveInfo v;
    const bool found = !position.isEmpty()
        && HistoryReader::valveInfoAt(object, manufactory, department, position, v);

    if (!found) {
        ui->label_card_Title->setText(ui->listWidget_history->count() > 0
            ? tr("Выберите позицию, чтобы посмотреть данные")
            : QString());
        ui->label_card_Title->setProperty("muted", true);
        ui->label_card_Title->style()->polish(ui->label_card_Title);
        ui->scrollArea_card->widget()->setMinimumHeight(0);
        return;
    }

    MaterialsOfComponentParts m;
    HistoryReader::materialsAt(object, manufactory, department, position, m);

    const auto row = [this](const QString &label, const QString &value) {
        if (value.trimmed().isEmpty())
            return;
        auto *key = new QLabel(label);
        key->setProperty("cardRole", "key");
        auto *field = new QLabel(value);
        field->setWordWrap(true);
        field->setTextInteractionFlags(Qt::TextSelectableByMouse);
        field->setProperty("cardRole", "value");
        ui->formLayout_card->addRow(key, field);
        // Динамическое свойство в QSS-селекторе не подхватывается само —
        // виджет уже создан после загрузки темы.
        key->style()->polish(key);
        field->style()->polish(field);
    };

    row(tr("Производитель"), v.manufacturer);
    row(tr("Модель"), v.valveModel);
    row(tr("Зав. №"), v.serialNumber);
    row(tr("DN"), v.DN);
    row(tr("PN"), v.PN);
    row(tr("Ход"), ValveEnums::StrokeMovementToString(v.strokeMovement));
    row(tr("Величина хода"), v.valveStroke);
    row(tr("Безопасное положение"),
        ValveEnums::SafePositionToString(v.safePosition));
    row(tr("Тип привода"), ValveEnums::DriveTypeToString(v.driveType));
    row(tr("Модель привода"), v.driveModel);
    if (v.driveType != DriveType::DoubleActing
        && (v.driveRangeLow != 0.0 || v.driveRangeHigh != 0.0))
        row(tr("Диапазон привода"),
            QStringLiteral("%1–%2").arg(v.driveRangeLow).arg(v.driveRangeHigh));
    row(tr("Тип позиционера"),
        ValveEnums::PositionerTypeToString(v.positionerType));
    row(tr("Модель позиционера"), v.positionerModel);
    row(tr("Шкала позиционера"), ValveEnums::PositionerScaleToString(v.positionerScale));
    row(tr("Модель ЭПВ"), v.solenoidValveModel);
    row(tr("Модель концевика"), v.limitSwitchModel);
    row(tr("Модель датчика положения"), v.positionSensorModel);
    row(tr("Уплотнение сальника"),
        ValveEnums::StuffingBoxSealToString(v.materialStuffingBoxSeal));

    row(tr("Материал плунжера"), m.plunger);
    row(tr("Материал седла"), m.saddle);
    row(tr("Материал корпуса"), m.corpus);
    row(tr("Материал крышки"), m.cap);
    row(tr("Материал шара"), m.ball);
    row(tr("Материал диска"), m.disk);
    row(tr("Материал штока"), m.shaft);
    row(tr("Материал шпинделя"), m.stock);
    row(tr("Направляющая втулка"), m.guideSleeve);
    row(tr("CV"), m.CV);

    ui->label_card_Title->setProperty("muted", false);
    ui->label_card_Title->style()->polish(ui->label_card_Title);
    ui->label_card_Title->setText(tr("Позиция «%1»").arg(position));

    QWidget *body = ui->scrollArea_card->widget();
    body->setMinimumHeight(0);
    body->layout()->activate();
    const int w = ui->scrollArea_card->viewport()->width();
    body->setMinimumHeight(body->layout()->hasHeightForWidth()
                               ? body->layout()->heightForWidth(w)
                               : body->sizeHint().height());
}

// Заполняет combo списком и выбирает первый пункт (или очищает поле,
// если истории нет). Прежнее значение не сохраняем: сменили родителя —
// старая связка больше не действительна, подставляем первую доступную.
static void fillFromHistory(QComboBox *combo, const QStringList &items)
{
    combo->clear();
    combo->addItems(items);

    if (combo->count() > 0)
        combo->setCurrentIndex(0);
    else
        combo->clearEditText();
}

void ObjectWindow::reloadManufactories()
{
    if (m_repopulating || !m_registry)
        return;

    m_repopulating = true;
    fillFromHistory(ui->comboBox_manufactory,
                    HistoryReader::manufactories(ui->comboBox_object->currentText()));
    m_repopulating = false;

    reloadDepartments();
}

void ObjectWindow::reloadDepartments()
{
    if (m_repopulating || !m_registry)
        return;

    m_repopulating = true;
    fillFromHistory(ui->comboBox_department,
                    HistoryReader::departments(ui->comboBox_object->currentText(),
                                               ui->comboBox_manufactory->currentText()));
    m_repopulating = false;
}

void ObjectWindow::LoadFromReg(Registry *registry)
{
    m_registry = registry;

    m_registry->loadObjectInfo();

    const auto& o = m_registry->objectInfo();

    // Списки собираем из истории в БД (Database/mpiplatform.db, общая на
    // все стенды — см. HistoryReader), текущее значение выставляем поверх —
    // оно может быть и новым, которого в истории ещё нет.
    m_repopulating = true;

    ui->comboBox_object->clear();
    ui->comboBox_object->addItems(HistoryReader::objects());
    ui->comboBox_object->setCurrentText(o.object);

    ui->comboBox_manufactory->clear();
    ui->comboBox_manufactory->addItems(HistoryReader::manufactories(o.object));
    ui->comboBox_manufactory->setCurrentText(o.manufactory);

    ui->comboBox_department->clear();
    ui->comboBox_department->addItems(
        HistoryReader::departments(o.object, o.manufactory));
    ui->comboBox_department->setCurrentText(o.department);

    m_repopulating = false;

    ui->lineEdit_FIO->setText(o.FIO);

    refreshHistoryPanel();
}

void ObjectWindow::saveToRegistry()
{
    auto& o = m_registry->objectInfo();
    o.object = ui->comboBox_object->currentText();
    o.manufactory = ui->comboBox_manufactory->currentText();
    o.department = ui->comboBox_department->currentText();
    o.FIO = ui->lineEdit_FIO->text();

    m_registry->saveObjectInfo();
}

void ObjectWindow::ButtonClick()
{
    if (ui->comboBox_object->currentText().isEmpty()) {
        StyledMessageBox::warning(this, "Ошибка", "Введите наименование объекта");
        return;
    }
    if (ui->comboBox_manufactory->currentText().isEmpty()) {
        StyledMessageBox::warning(this, "Ошибка", "Введите наименование цеха");
        return;
    }
    if (ui->comboBox_department->currentText().isEmpty()) {
        StyledMessageBox::warning(this, "Ошибка", "Введите наименование отделение/установки");
        return;
    }

    // Проверки в БД (Database/mpiplatform.db, общая на все стенды)
    if (!HistoryReader::checkObject(ui->comboBox_object->currentText())) {
        auto button = StyledMessageBox::question(this, "Предупреждение",
                                            "Объекта нет в базе. Вы действительно хотите продолжить?");
        if (button == QMessageBox::Yes) {
            saveToRegistry();
            accept();
        }
        return;
    }

    if (!HistoryReader::checkManufactory(ui->comboBox_object->currentText(),
                                         ui->comboBox_manufactory->currentText())) {
        auto button = StyledMessageBox::question(this, "Предупреждение",
                                            "Цеха нет в базе. Вы действительно хотите продолжить?");
        if (button == QMessageBox::Yes) {
            saveToRegistry();
            accept();
        }
        return;
    }

    if (!HistoryReader::checkDepartment(ui->comboBox_object->currentText(),
                                        ui->comboBox_manufactory->currentText(),
                                        ui->comboBox_department->currentText())) {
        auto button = StyledMessageBox::question(this, "Предупреждение",
                                            "Отделения/установки нет в базе. Вы действительно хотите продолжить?");
        if (button == QMessageBox::Yes) {
            saveToRegistry();
            accept();
        }
        return;
    }

    auto& other = m_registry->otherParameters();
    other.date = ui->dateEdit->date().toString("dd.MM.yyyy");

    saveToRegistry();
    accept();
}
