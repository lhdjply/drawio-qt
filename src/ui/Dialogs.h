#pragma once

#include "core/Page.h"
#include "io/Exporter.h"

#include <QDialog>
#include <QLineEdit>
#include <QKeySequence>
#include <QVector>
#include <QSizeF>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSpinBox;


namespace dq {

/// Shortcut recorder that always displays Qt's portable English key names
/// ("Ctrl+Shift+Down"), independent of the interface language.
class ShortcutEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit ShortcutEdit(QWidget *parent = nullptr);

    QKeySequence keySequence() const { return m_sequence; }
    void setKeySequence(const QKeySequence &sequence);

signals:
    void keySequenceChanged();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QKeySequence m_sequence;
};


class Document;

/// The export dialog: format, destination, scale, border, transparency,
/// embedded diagram and multi-page options.
class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportDialog(const Document *document, QWidget *parent = nullptr);

    ExportOptions options() const;
    QString filePath() const;
    void setPresetFormat(const QString &format);

private:
    void browse();
    void updateFormatUi();
    void updateDefaultPath();
    QString selectedFormat() const;

    const Document *m_document;
    QComboBox *m_format = nullptr;
    QLineEdit *m_path = nullptr;
    QDoubleSpinBox *m_scale = nullptr;
    QSpinBox *m_border = nullptr;
    QSpinBox *m_quality = nullptr;
    QSpinBox *m_width = nullptr;
    QSpinBox *m_height = nullptr;
    QCheckBox *m_transparent = nullptr;
    QCheckBox *m_embed = nullptr;
    QCheckBox *m_allPages = nullptr;
    QCheckBox *m_pageView = nullptr;
    QLabel *m_hint = nullptr;
    bool m_pathEdited = false;
};

/// Page size and page view settings for the current page.
class PageSetupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PageSetupDialog(const Page &page, QWidget *parent = nullptr);

    Page::Settings settings() const;

private:
    void applyPreset(int index);
    void updateEnabled();

    QComboBox *m_preset = nullptr;
    QComboBox *m_orientation = nullptr;
    QDoubleSpinBox *m_width = nullptr;
    QDoubleSpinBox *m_height = nullptr;
    QCheckBox *m_showPage = nullptr;
    QCheckBox *m_grid = nullptr;
    QSpinBox *m_gridSize = nullptr;
    Page::Settings m_original;
};

struct ShortcutSetting
{
    QString id;
    QString label;
    QKeySequence sequence;
    QKeySequence defaultSequence;
};

/// Editable shortcut table with reset and conflict checking.
class ShortcutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShortcutDialog(const QVector<ShortcutSetting> &settings,
                            QWidget *parent = nullptr);

    QVector<ShortcutSetting> settings() const;

private:
    void restoreDefaults();
    void acceptIfValid();

    QVector<ShortcutSetting> m_settings;
    QVector<ShortcutEdit *> m_editors;
};

/// Unified settings dialog: appearance and keyboard shortcuts.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    SettingsDialog(const QVector<ShortcutSetting> &shortcuts, const QString &theme,
                   const QString &language, QWidget *parent = nullptr);

    QVector<ShortcutSetting> shortcutSettings() const;
    QString theme() const;
    QString language() const;

private:
    void restoreDefaults();
    void acceptIfValid();

    QVector<ShortcutSetting> m_shortcuts;
    QVector<ShortcutEdit *> m_editors;
    class QComboBox *m_theme = nullptr;
    class QComboBox *m_language = nullptr;
};

void showAboutDialog(QWidget *parent);

QString exportFilterForFormat(const QString &format);
QString defaultSuffixForFormat(const QString &format);

} // namespace dq
