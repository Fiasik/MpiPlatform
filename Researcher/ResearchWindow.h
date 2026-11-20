#pragma once
#include <QMainWindow>
#include <QJsonObject>
#include <QMap>

class QListWidget;
class QListWidgetItem;
class QThread;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QLabel;
class QTimer;
class QSplitter;
class PressureAnalysis;
namespace Labeler { class ChartFrame; }
namespace Gui::Style { class ThemeManager; }

namespace Researcher {
class ResearchWindow : public QMainWindow {
    Q_OBJECT
public:
    ResearchWindow(Gui::Style::ThemeManager* theme, const QString& workspace, QWidget* parent = nullptr);
    ~ResearchWindow() override;
    void openPath(const QString& path);
    void openArchive(const QString& folder);
    bool restoreArchive();
    bool saveObservation();
    bool exportCase(const QString& output);
    QString currentRecordPath() const;
    QJsonObject currentRevision() const;
signals:
    void archiveProgress(int generation, int count);
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    void openRun();
    void buildObservationPage();
    void changed();
    void filterSessions();
    void activateSession(QListWidgetItem* item);
    void stepSession(int direction);
    void updateSessionCounter();
    void chooseFolder();
    void expandChart(int index);
    void status(const QString& text);
    Gui::Style::ThemeManager* m_theme;
    QString m_workspace, m_folder, m_run;
    QJsonObject m_sources;
    QListWidget* m_sessions;
    QLineEdit* m_search;
    QLabel* m_sessionCounter;
    QString m_archiveRoot;
    QList<QThread*> m_scanThreads;
    int m_scanGeneration = 0;
    QComboBox* m_runs;
    QLabel* m_heading;
    QLabel* m_saved;
    QWidget* m_content = nullptr;
    QWidget* m_host;
    QSplitter* m_mainSplit;
    PressureAnalysis* m_analysis = nullptr;
    Labeler::ChartFrame* m_frames[3] = {};
    QComboBox* m_quality = nullptr;
    QComboBox* m_visibility = nullptr;
    QLineEdit* m_author = nullptr;
    QPlainTextEdit* m_notes[5] = {};
    QTimer* m_saveTimer;
    bool m_loading = false;
    bool m_dirty = false;
    int m_expanded = -1;
};
}
