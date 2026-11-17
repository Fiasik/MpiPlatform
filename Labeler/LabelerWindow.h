#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QMainWindow>

#include <atomic>
#include <memory>
#include <optional>

#include "LabelSchema.h"
#include "LabelStore.h"

class QButtonGroup;
class QCheckBox;
class QKeyEvent;
class QThread;
class QToolButton;

namespace Ui {
class LabelerWindow;
}

namespace Gui::Style {
class ThemeManager;
enum class ColorScheme;
}

namespace Labeler {

class ChartFrame;

// Окно разметчика: одна сессия архива = одна карточка (три основных теста
// из .data: задание сверху во всю ширину, трение и давление под ним; справа
// панель меток). Метки — из редактируемого labels.json,
// ответы — в labels.sqlite (LabelStore), архив только читается.
//
// Клавиатура разбирается фильтром событий на всём приложении, а не
// QShortcut: цифры/буквы должны работать в любой раскладке и не должны
// срабатывать, пока пишется комментарий.
class LabelerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LabelerWindow(Gui::Style::ThemeManager* themeManager, QWidget* parent = nullptr);
    ~LabelerWindow() override;

    // Аргумент командной строки — корень архива.
    void openRoot(const QString& path, bool forceRescan = false);
    // Без аргумента — последний открытый корень (labeler_settings.ini).
    void restoreLastSession();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    // База и список меток.
    bool ensureStore(const QString& rootPath);
    bool openStore(const QString& dbPath);
    bool loadLabels(bool showErrors);
    QString labelsJsonPath() const;
    void rebuildLabelPanel();

    // Корень архива.
    void startScan(const QString& rootPath);
    void loadDataset();

    // Карточка.
    void showCard(int pos, int direction = +1);
    void showFinished();
    void updateHeader(const SessionRecord& session, const QueueItem& item);
    void updateResults(const SessionRecord& session);
    void updateProgress();
    // formEnabled — оценка/метки/комментарий/«Далее»/«Пропустить»;
    // navEnabled — «Назад» (работает и после конца очереди).
    void setPanelEnabled(bool formEnabled, bool navEnabled);
    QString sessionDirPath(const SessionRecord& session) const;

    // Форма ответа.
    void clearForm();
    void applyAnswerToForm(const Answer& answer);
    QString selectedVerdict() const;
    void selectVerdict(const QString& key);
    QStringList checkedLabels() const;

    void onLabelToggled(const QString& key, bool checked);
    bool formDiffersFromLoaded() const;
    bool saveCurrent();
    double secondsOnCard() const;

    // Действия.
    void goNext();
    void goBack();
    void skipCard();
    void undoLast();
    void exportCsv();
    void chooseDb();
    void toggleExpanded(int frameIndex);
    bool handleKey(QKeyEvent* event);

    void onSchemeChanged(Gui::Style::ColorScheme scheme);
    void updateThemeToggleIcon(Gui::Style::ColorScheme scheme);

    Ui::LabelerWindow* ui = nullptr;
    Gui::Style::ThemeManager* m_themeManager = nullptr;
    QToolButton* m_themeToggleButton = nullptr;
    QVector<ChartFrame*> m_frames;
    int m_expandedFrame = -1;

    std::unique_ptr<LabelStore> m_store;
    LabelSchema m_schema;
    bool m_schemaOk = false;

    QString m_dataset;
    QString m_rootPath;
    QHash<qint64, SessionRecord> m_sessions;
    bool m_scanning = false;
    QThread* m_scanThread = nullptr;
    std::shared_ptr<std::atomic_bool> m_scanCancel;

    int m_pos = -1;
    std::optional<QueueItem> m_item;
    std::optional<Answer> m_loadedAnswer;

    QButtonGroup* m_verdictGroup = nullptr;
    QHash<QString, QCheckBox*> m_labelBoxes;
    // Галочки ставятся программно (заполнение формы, oneOf) — без этого
    // флага onLabelToggled() переключал бы оценку на «дефект» при загрузке.
    bool m_fillingForm = false;

    // Время на карточке считается только пока окно активно (отошли от
    // компьютера — не в счёт).
    QElapsedTimer m_activeTimer;
    double m_activeSeconds = 0.0;
    QString m_shownAtUtc;
};

}
