#pragma once

#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>

#include <memory>

namespace Ui { class HelpDialog; }

// Справка (F1 / «?» в статус-баре): «О программе» — версия, сборка, разработчик,
// контакты поддержки (заглушки в Utils/AppInfo.h), сведения о системе и кнопка
// «Скопировать для поддержки»; «Управление» — клавиатура.
//
// Раздел про клавиатуру строится из ФАКТИЧЕСКИХ вкладок окна (их передаёт
// MainWindow: видимые вкладки по порядку, вложенные вкладки, кнопки Enter), а
// не пишется вручную — цифры и действия Enter зависят от сборки (в Ilim нет
// циклического теста) и не должны разойтись со справкой.
class HelpDialog : public QDialog
{
    Q_OBJECT

public:
    struct TabHelp {
        QString title;            // название вкладки
        QStringList innerTabs;    // вложенные вкладки: ←/→ листают их; пусто — стрелки листают сами вкладки
        QString enterAction;      // текст кнопки, которую нажимает Enter; пусто — Enter ничего не делает
        QString shiftEnterAction; // то же для Shift+Enter
    };

    // tabs — видимые вкладки главного окна в порядке слева направо.
    explicit HelpDialog(const QList<TabHelp>& tabs, QWidget* parent = nullptr);
    ~HelpDialog() override;

    // Те же сведения, что на вкладке «О программе», одним текстом (без
    // контактов — они у получателя уже есть).
    QString supportText() const;

private:
    QString controlsHtml(const QList<TabHelp>& tabs) const;
    QString testsHtml() const;

    std::unique_ptr<Ui::HelpDialog> ui;
    QString m_databasePath;
    int m_schemaVersion = 0;
    int m_latestSchemaVersion = 0;
};
