#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

class QSettings;

namespace dq {

/// Thin wrapper around QSettings for the values the desktop application keeps
/// between runs (window geometry, recent files, last used folders...). It also
/// exposes the directories used for drafts and auto-save recovery files.
class AppSettings
{
public:
    static AppSettings &instance();

    QString lastOpenDir() const;
    void setLastOpenDir(const QString &dir);

    QString lastSaveDir() const;
    void setLastSaveDir(const QString &dir);

    QString lastExportDir() const;
    void setLastExportDir(const QString &dir);

    QStringList recentFiles() const;
    void addRecentFile(const QString &path);
    void removeRecentFile(const QString &path);
    void clearRecentFiles();

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geometry);

    QByteArray windowState() const;
    void setWindowState(const QByteArray &state);

    bool gridVisible() const;
    void setGridVisible(bool visible);

    bool pageVisible() const;
    void setPageVisible(bool visible);

    bool guidesVisible() const;
    void setGuidesVisible(bool visible);

    bool autoSaveEnabled() const;
    void setAutoSaveEnabled(bool enabled);

    /// Check for updates once on startup. Enabled by default; failures are
    /// silent, only a real update shows a dialog.
    bool autoCheckUpdate() const;
    void setAutoCheckUpdate(bool enabled);

    QString theme() const;
    void setTheme(const QString &theme);

    QString language() const;
    void setLanguage(const QString &language);

    double zoom() const;
    void setZoom(double zoom);

    /// Shape library categories enabled in the palette. An empty list means
    /// the built-in defaults (standard libraries only, no image libraries).
    QStringList enabledShapeLibraries() const;
    void setEnabledShapeLibraries(const QStringList &ids);

    void sync();

    // --- locations -------------------------------------------------------
    static QString dataDirectory();
    static QString draftDirectory();
    static QString configFilePath();

    static void ensureDirectories();

private:
    AppSettings();
    ~AppSettings();
    AppSettings(const AppSettings &) = delete;
    AppSettings &operator=(const AppSettings &) = delete;

    QSettings *m_settings = nullptr;
};

} // namespace dq
