#pragma once

#include <QDialog>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <memory>
#include <functional>

namespace Ui { class HelpDialog; }

class QTreeWidget;
class QTreeWidgetItem;
class QTextBrowser;
class QLabel;

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
    void showArticle(const QString& id);
    static void bindArticleLink(QLabel* label, const QString& article, const QString& tooltip,
                                const std::function<void(const QString&)>& openArticle);

protected:
    void changeEvent(QEvent* event) override;

private:
    QString controlsHtml(const QList<TabHelp>& tabs) const;
    struct Article {
        QString id;
        QString title;
        QString html;
    };

    void setupArticles(const QList<TabHelp>& tabs);
    void updateArticleStyle();
    QList<Article> testArticles() const;
    QString sensorHtml() const;

    std::unique_ptr<Ui::HelpDialog> ui;
    QTreeWidget* m_tree = nullptr;
    QTextBrowser* m_browser = nullptr;
    QString m_articleHtml;
    QHash<QString, QTreeWidgetItem*> m_articles;
    QString m_databasePath;
    int m_schemaVersion = 0;
    int m_latestSchemaVersion = 0;
};
