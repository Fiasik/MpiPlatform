#include "ThemeManager.h"

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QStyleHints>
#include <QDebug>

namespace Gui::Style {

namespace {
QString themeQssDiskPath()
{
    return QStringLiteral(MPISDK_SOURCE_DIR "/Src/Style/theme.qss");
}
// Встроенный в .qrc (style.qrc) — единственное, что реально доступно на
// машине, где нет дерева исходников (например, у конечного пользователя
// на Windows): MPISDK_SOURCE_DIR — абсолютный путь на диске сборочной
// машины, зашитый на этапе компиляции, и на любой другой машине его
// просто не существует.
QString themeQssResourcePath()
{
    return QStringLiteral(":/Src/Style/theme.qss");
}
}

ThemeManager::ThemeManager(QApplication& app, QObject* parent)
    : QObject(parent)
    , m_app(app)
    , m_preference(m_settings.themePreference())
{
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this](Qt::ColorScheme) {
        if (m_preference == ThemePreference::System)
            resolveAndApply();
    });

    // Горячая перезагрузка имеет смысл только для файла на диске дерева
    // исходников (правка в редакторе применяется на лету) — на машине,
    // где его нет (см. themeQssResourcePath() выше), просто используем
    // вкомпилированную версию без слежения за несуществующим путём.
    //
    // Некоторые редакторы (напр. VS Code по умолчанию) сохраняют файл через
    // атомарную замену (rename), а не запись в тот же inode — после такого
    // QFileSystemWatcher молча перестаёт следить за путём, поэтому путь
    // нужно добавлять в watcher заново при каждом срабатывании.
    if (QFileInfo::exists(themeQssDiskPath())) {
        connect(&m_watcher, &QFileSystemWatcher::fileChanged,
                this, [this](const QString& changedPath) {
            applyStylesheet();
            if (!m_watcher.files().contains(changedPath))
                m_watcher.addPath(changedPath);
        });
        m_watcher.addPath(themeQssDiskPath());
    }

    resolveAndApply();
}

ColorScheme ThemeManager::resolve(ThemePreference pref, Qt::ColorScheme osScheme)
{
    if (pref == ThemePreference::Light)
        return ColorScheme::Light;
    if (pref == ThemePreference::Dark)
        return ColorScheme::Dark;
    return osScheme == Qt::ColorScheme::Dark ? ColorScheme::Dark : ColorScheme::Light;
}

void ThemeManager::setPreference(ThemePreference pref)
{
    m_preference = pref;
    m_settings.setThemePreference(pref);
    resolveAndApply();
}

void ThemeManager::toggle()
{
    const bool nowDark = m_resolvedScheme == ColorScheme::Dark;
    setPreference(nowDark ? ThemePreference::Light : ThemePreference::Dark);
}

void ThemeManager::resolveAndApply()
{
    const ColorScheme scheme = resolve(m_preference, QGuiApplication::styleHints()->colorScheme());
    const bool changed = scheme != m_resolvedScheme;
    m_resolvedScheme = scheme;

    applyWidgetPalette();
    applyStylesheet();

    if (changed)
        emit schemeChanged(m_resolvedScheme);
}

void ThemeManager::applyWidgetPalette()
{
    const Palette& p = Palette::forScheme(m_resolvedScheme);

    QPalette pal = m_app.palette();
    pal.setColor(QPalette::Window, QColor(p.bgDialog));
    pal.setColor(QPalette::WindowText, QColor(p.textBase));
    pal.setColor(QPalette::Base, QColor(p.bgSurface));
    pal.setColor(QPalette::AlternateBase, QColor(p.bgRecessed));
    pal.setColor(QPalette::Text, QColor(p.textStrong));
    pal.setColor(QPalette::Button, QColor(p.bgSurface));
    pal.setColor(QPalette::ButtonText, QColor(p.textStrong));
    pal.setColor(QPalette::Highlight, QColor(p.accentPrimary));
    pal.setColor(QPalette::HighlightedText, QColor(p.bgSurface));
    pal.setColor(QPalette::ToolTipBase, QColor(p.bgSurface));
    pal.setColor(QPalette::ToolTipText, QColor(p.textStrong));
    pal.setColor(QPalette::PlaceholderText, QColor(p.borderHover));

    pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(p.borderHover));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor(p.borderHover));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(p.borderHover));

    m_app.setPalette(pal);
}

void ThemeManager::applyStylesheet()
{
    QFile file(themeQssDiskPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // На машине без дерева исходников (обычная ситуация для
        // собранного .exe у конечного пользователя) диска просто нет —
        // это ожидаемо, не предупреждение, а тихий переход на встроенный
        // ресурс. Настоящая ошибка — если не открылся и ресурс тоже.
        file.setFileName(themeQssResourcePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Не удалось открыть QSS ни с диска, ни из ресурсов:"
                       << themeQssDiskPath() << themeQssResourcePath();
            return;
        }
    }

    const QString rendered = renderTemplate(QString::fromUtf8(file.readAll()),
                                             Palette::forScheme(m_resolvedScheme));
    m_app.setStyleSheet(rendered);
}

QString ThemeManager::renderTemplate(const QString& qssTemplate, const Palette& palette)
{
    QString result = qssTemplate;
    const QHash<QString, QString> tokens = palette.tokens();
    for (auto it = tokens.constBegin(); it != tokens.constEnd(); ++it)
        result.replace(QStringLiteral("@{%1}").arg(it.key()), it.value());

    static const QRegularExpression kLeftoverToken(QStringLiteral("@\\{[^}]*\\}"));
    const QRegularExpressionMatch leftover = kLeftoverToken.match(result);
    if (leftover.hasMatch())
        qWarning() << "theme.qss: нераспознанный токен" << leftover.captured();

    return result;
}

}
