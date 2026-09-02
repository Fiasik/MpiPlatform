#include "ObjectWindow.h"
#include "ui_ObjectWindow.h"

#include <QCalendarWidget>
#include <QMessageBox>
#include <QScreen>
#include <QTextCharFormat>

#include "Gui/Style/Palette.h"
#include "Gui/Style/RoleStyler.h"
#include "Utils/StyledMessageBox.h"

ObjectWindow::ObjectWindow(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ObjectWindow)
{
    ui->setupUi(this);

    Gui::Style::applyRoles(this);

    QRect scr = QApplication::primaryScreen()->geometry();
    move(scr.center() - rect().center());

    ui->dateEdit->setDate(QDate::currentDate());

    // Qt красит Сб/Вс в календаре собственным QTextCharFormat — это
    // относится и к заголовку "Сб"/"Вс", и к самим числам в этих
    // столбцах, и QSS этот цвет не достаёт (не часть темы, задаётся
    // только так). Берём тот же акцентный красный, что и статусы
    // ошибок в остальном интерфейсе — вместо стандартного системного
    // красного, который со светлой/тёмной темой заново не сочетается.
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

    ui->lineEdit_object->setValidator(validator);
    ui->lineEdit_manufactory->setValidator(validator);
    ui->lineEdit_department->setValidator(validator);

    connect(ui->pushButton_start, &QPushButton::clicked,
            this, &ObjectWindow::ButtonClick);
}

void ObjectWindow::LoadFromReg(Registry *registry)
{
    m_registry = registry;

    m_registry->loadObjectInfo();

    const auto& o = m_registry->objectInfo();
    ui->lineEdit_object->setText(o.object);
    ui->lineEdit_manufactory->setText(o.manufactory);
    ui->lineEdit_department->setText(o.department);
    ui->lineEdit_FIO->setText(o.FIO);
}

void ObjectWindow::saveToRegistry()
{
    auto& o = m_registry->objectInfo();
    o.object = ui->lineEdit_object->text();
    o.manufactory = ui->lineEdit_manufactory->text();
    o.department = ui->lineEdit_department->text();
    o.FIO = ui->lineEdit_FIO->text();

    m_registry->saveObjectInfo();
}

void ObjectWindow::ButtonClick()
{
    if (ui->lineEdit_object->text().isEmpty()) {
        StyledMessageBox::warning(this, "Ошибка", "Введите наименование объекта");
        return;
    }
    if (ui->lineEdit_manufactory->text().isEmpty()) {
        StyledMessageBox::warning(this, "Ошибка", "Введите наименование цеха");
        return;
    }
    if (ui->lineEdit_department->text().isEmpty()) {
        StyledMessageBox::warning(this, "Ошибка", "Введите наименование отделение/установки");
        return;
    }

    // Проверки в БД (QSettings)
    if (!m_registry->checkObject(ui->lineEdit_object->text())) {
        auto button = StyledMessageBox::question(this, "Предупреждение",
                                            "Объекта нет в базе. Вы действительно хотите продолжить?");
        if (button == QMessageBox::Yes) {
            saveToRegistry();
            accept();
        }
        return;
    }

    // чтобы checkManufactory/checkDepartment смотрели в правильные группы
    m_registry->objectInfo().object = ui->lineEdit_object->text();

    if (!m_registry->checkManufactory(ui->lineEdit_manufactory->text())) {
        auto button = StyledMessageBox::question(this, "Предупреждение",
                                            "Цеха нет в базе. Вы действительно хотите продолжить?");
        if (button == QMessageBox::Yes) {
            saveToRegistry();
            accept();
        }
        return;
    }

    m_registry->objectInfo().manufactory = ui->lineEdit_manufactory->text();

    if (!m_registry->checkDepartment(ui->lineEdit_department->text())) {
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

