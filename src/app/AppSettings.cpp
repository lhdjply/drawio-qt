#include "app/AppSettings.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace dq {
namespace {

constexpr int kMaxRecentFiles = 15;

QString normalized(const QString &path)
{
    return QFileInfo(path).absoluteFilePath();
}

} // namespace

AppSettings::AppSettings()
{
    ensureDirectories();
    m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                               QStringLiteral("drawio-qt"), QStringLiteral("drawio-qt"));
}

AppSettings::~AppSettings()
{
    delete m_settings;
}

AppSettings &AppSettings::instance()
{
    static AppSettings settings;
    return settings;
}

QString AppSettings::lastOpenDir() const
{
    return m_settings->value(QStringLiteral("paths/lastOpen"),
                             QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
        .toString();
}

void AppSettings::setLastOpenDir(const QString &dir)
{
    m_settings->setValue(QStringLiteral("paths/lastOpen"), dir);
}

QString AppSettings::lastSaveDir() const
{
    return m_settings->value(QStringLiteral("paths/lastSave"), lastOpenDir()).toString();
}

void AppSettings::setLastSaveDir(const QString &dir)
{
    m_settings->setValue(QStringLiteral("paths/lastSave"), dir);
}

QString AppSettings::lastExportDir() const
{
    return m_settings->value(QStringLiteral("paths/lastExport"), lastOpenDir()).toString();
}

void AppSettings::setLastExportDir(const QString &dir)
{
    m_settings->setValue(QStringLiteral("paths/lastExport"), dir);
}

QStringList AppSettings::recentFiles() const
{
    QStringList files = m_settings->value(QStringLiteral("recent/files")).toStringList();
    QStringList existing;

    for (const QString &file : files)
    {
        if (QFileInfo::exists(file))
        {
            existing.append(file);
        }
    }

    return existing;
}

void AppSettings::addRecentFile(const QString &path)
{
    if (path.isEmpty())
    {
        return;
    }

    const QString absolute = normalized(path);
    QStringList files = m_settings->value(QStringLiteral("recent/files")).toStringList();
    files.removeAll(absolute);
    files.prepend(absolute);

    while (files.size() > kMaxRecentFiles)
    {
        files.removeLast();
    }

    m_settings->setValue(QStringLiteral("recent/files"), files);
}

void AppSettings::removeRecentFile(const QString &path)
{
    const QString absolute = normalized(path);
    QStringList files = m_settings->value(QStringLiteral("recent/files")).toStringList();
    files.removeAll(absolute);
    m_settings->setValue(QStringLiteral("recent/files"), files);
}

void AppSettings::clearRecentFiles()
{
    m_settings->remove(QStringLiteral("recent/files"));
}

QByteArray AppSettings::windowGeometry() const
{
    return m_settings->value(QStringLiteral("window/geometry")).toByteArray();
}

void AppSettings::setWindowGeometry(const QByteArray &geometry)
{
    m_settings->setValue(QStringLiteral("window/geometry"), geometry);
}

QByteArray AppSettings::windowState() const
{
    return m_settings->value(QStringLiteral("window/state")).toByteArray();
}

void AppSettings::setWindowState(const QByteArray &state)
{
    m_settings->setValue(QStringLiteral("window/state"), state);
}

bool AppSettings::gridVisible() const
{
    return m_settings->value(QStringLiteral("view/grid"), true).toBool();
}

void AppSettings::setGridVisible(bool visible)
{
    m_settings->setValue(QStringLiteral("view/grid"), visible);
}

bool AppSettings::pageVisible() const
{
    return m_settings->value(QStringLiteral("view/page"), true).toBool();
}

void AppSettings::setPageVisible(bool visible)
{
    m_settings->setValue(QStringLiteral("view/page"), visible);
}

bool AppSettings::autoSaveEnabled() const
{
    return m_settings->value(QStringLiteral("editor/autoSave"), true).toBool();
}

void AppSettings::setAutoSaveEnabled(bool enabled)
{
    m_settings->setValue(QStringLiteral("editor/autoSave"), enabled);
}

QString AppSettings::theme() const
{
    return m_settings->value(QStringLiteral("view/theme"), QStringLiteral("auto")).toString();
}

void AppSettings::setTheme(const QString &theme)
{
    m_settings->setValue(QStringLiteral("view/theme"), theme);
}

double AppSettings::zoom() const
{
    return m_settings->value(QStringLiteral("view/zoom"), 1.0).toDouble();
}

void AppSettings::setZoom(double zoom)
{
    m_settings->setValue(QStringLiteral("view/zoom"), zoom);
}

void AppSettings::sync()
{
    m_settings->sync();
}

QString AppSettings::dataDirectory()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    if (base.isEmpty())
    {
        return QDir::homePath() + QStringLiteral("/.drawio-qt");
    }

    return base;
}

QString AppSettings::draftDirectory()
{
    return dataDirectory() + QStringLiteral("/drafts");
}

QString AppSettings::configFilePath()
{
    return dataDirectory() + QStringLiteral("/drawio-qt.ini");
}

void AppSettings::ensureDirectories()
{
    QDir().mkpath(dataDirectory());
    QDir().mkpath(draftDirectory());
}

} // namespace dq
