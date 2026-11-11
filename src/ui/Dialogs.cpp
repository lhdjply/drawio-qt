#include "ui/Dialogs.h"

#include "app/I18n.h"

#include "app/AppSettings.h"
#include "core/Document.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QKeySequenceEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QLineEdit>
#include <QPageSize>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace dq {
namespace {

QString formatLabel(const QString &format)
{
    if (format == QLatin1String("png"))
    {
        return QStringLiteral("PNG Image (*.png)");
    }

    if (format == QLatin1String("jpg"))
    {
        return QStringLiteral("JPEG Image (*.jpg)");
    }

    if (format == QLatin1String("svg"))
    {
        return QStringLiteral("SVG Vector Image (*.svg)");
    }

    if (format == QLatin1String("pdf"))
    {
        return QStringLiteral("PDF Document (*.pdf)");
    }

    if (format == QLatin1String("xml"))
    {
        return QStringLiteral("draw.io XML (*.drawio *.xml)");
    }

    return QStringLiteral("HTML Viewer (*.html)");
}

} // namespace

QString exportFilterForFormat(const QString &format)
{
    return formatLabel(format);
}

QString defaultSuffixForFormat(const QString &format)
{
    if (format == QLatin1String("jpg"))
    {
        return QStringLiteral("jpg");
    }

    if (format == QLatin1String("xml"))
    {
        return QStringLiteral("drawio");
    }

    return format;
}

ExportDialog::ExportDialog(const Document *document, QWidget *parent)
    : QDialog(parent)
    , m_document(document)
{
    setWindowTitle(QStringLiteral("Export"));
    setMinimumWidth(520);

    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout();
    m_format = new QComboBox(this);
    m_format->addItem(QStringLiteral("PNG Image"), QStringLiteral("png"));
    m_format->addItem(QStringLiteral("SVG Vector Image"), QStringLiteral("svg"));
    m_format->addItem(QStringLiteral("PDF Document"), QStringLiteral("pdf"));
    m_format->addItem(QStringLiteral("JPEG Image"), QStringLiteral("jpg"));
    m_format->addItem(QStringLiteral("draw.io XML"), QStringLiteral("xml"));
    m_format->addItem(QStringLiteral("HTML Viewer"), QStringLiteral("html"));
    form->addRow(QStringLiteral("Format:"), m_format);

    auto *pathRow = new QHBoxLayout();
    m_path = new QLineEdit(this);
    auto *browseButton = new QPushButton(QStringLiteral("Browse…"), this);
    pathRow->addWidget(m_path, 1);
    pathRow->addWidget(browseButton);
    form->addRow(QStringLiteral("Save to:"), pathRow);

    m_scale = new QDoubleSpinBox(this);
    m_scale->setRange(0.1, 20.0);
    m_scale->setSingleStep(0.1);
    m_scale->setValue(1.0);
    m_scale->setSuffix(QStringLiteral(" x"));
    form->addRow(QStringLiteral("Zoom:"), m_scale);

    m_border = new QSpinBox(this);
    m_border->setRange(0, 200);
    m_border->setValue(0);
    m_border->setSuffix(QStringLiteral(" px"));
    form->addRow(QStringLiteral("Margin:"), m_border);

    m_width = new QSpinBox(this);
    m_width->setRange(0, 20000);
    m_width->setSpecialValueText(QStringLiteral("Auto"));
    form->addRow(QStringLiteral("Target Width:"), m_width);

    m_height = new QSpinBox(this);
    m_height->setRange(0, 20000);
    m_height->setSpecialValueText(QStringLiteral("Auto"));
    form->addRow(QStringLiteral("Target Height:"), m_height);

    m_quality = new QSpinBox(this);
    m_quality->setRange(1, 100);
    m_quality->setValue(90);
    form->addRow(QStringLiteral("JPEG Quality:"), m_quality);

    m_transparent = new QCheckBox(QStringLiteral("Transparent background (PNG/SVG)"), this);
    form->addRow(QString(), m_transparent);

    m_embed = new QCheckBox(QStringLiteral("Embed diagram data (reopen with draw.io)"), this);
    form->addRow(QString(), m_embed);

    m_allPages = new QCheckBox(QStringLiteral("Export All Pages"), this);
    form->addRow(QString(), m_allPages);

    m_pageView = new QCheckBox(QStringLiteral("Export at page size instead of cropping to the drawing"), this);
    form->addRow(QString(), m_pageView);

    layout->addLayout(form);

    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    m_hint->setStyleSheet(QStringLiteral("color: #666;"));
    layout->addWidget(m_hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Export"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancel"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]()
    {
        if (m_path->text().trimmed().isEmpty())
        {
            browse();

            if (m_path->text().trimmed().isEmpty())
            {
                return;
            }
        }

        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(browseButton, &QPushButton::clicked, this, &ExportDialog::browse);
    connect(m_format, &QComboBox::currentIndexChanged, this, [this]()
    {
        updateFormatUi();
        updateDefaultPath();
    });
    connect(m_path, &QLineEdit::textEdited, this, [this]()
    {
        m_pathEdited = true;
    });
    connect(m_embed, &QCheckBox::toggled, this, [this]()
    {
        updateFormatUi();
    });

    updateFormatUi();
    updateDefaultPath();
}

QString ExportDialog::selectedFormat() const
{
    return m_format->currentData().toString();
}

void ExportDialog::updateFormatUi()
{
    const QString format = selectedFormat();
    const bool image = format == QLatin1String("png") || format == QLatin1String("jpg")
        || format == QLatin1String("svg") || format == QLatin1String("pdf");

    m_scale->setEnabled(image);
    m_border->setEnabled(image);
    m_transparent->setEnabled(format == QLatin1String("png") || format == QLatin1String("svg"));
    m_width->setEnabled(image);
    m_height->setEnabled(image);
    m_quality->setEnabled(format == QLatin1String("jpg"));
    m_allPages->setEnabled(format == QLatin1String("pdf") || format == QLatin1String("html")
                           || format == QLatin1String("xml"));
    m_pageView->setEnabled(format == QLatin1String("png") || format == QLatin1String("jpg")
                           || format == QLatin1String("svg"));

    if (format == QLatin1String("xml") || format == QLatin1String("html"))
    {
        m_embed->setChecked(false);
        m_embed->setEnabled(false);
        m_scale->setValue(1.0);
    }
    else
    {
        m_embed->setEnabled(true);
    }

    if (format == QLatin1String("jpg"))
    {
        m_transparent->setChecked(false);
    }

    if (format == QLatin1String("xml"))
    {
        m_hint->setText(QStringLiteral("The XML export keeps the full-page mxGraphModel and opens directly in draw.io."));
    }
    else if (format == QLatin1String("html"))
    {
        m_hint->setText(QStringLiteral("The HTML export creates a standalone page rendered by the diagrams.net viewer."));
    }
    else if (m_embed->isChecked())
    {
        m_hint->setText(QStringLiteral("Diagram data is embedded and can be reopened in drawio-qt or draw.io."));
    }
    else
    {
        m_hint->clear();
    }
}

void ExportDialog::updateDefaultPath()
{
    if (m_document == nullptr || m_pathEdited)
    {
        return;
    }

    const QString format = selectedFormat();
    const QString suffix = defaultSuffixForFormat(format);
    QString base = m_document->filePath();

    if (base.isEmpty())
    {
        base = AppSettings::instance().lastExportDir() + QLatin1Char('/')
            + QStringLiteral("Untitled Drawing");
    }
    else
    {
        base = QFileInfo(base).absolutePath() + QLatin1Char('/')
            + QFileInfo(base).completeBaseName();
    }

    m_path->setText(base + QLatin1Char('.') + suffix);
}

void ExportDialog::browse()
{
    const QString format = selectedFormat();
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export"), m_path->text().isEmpty()
            ? AppSettings::instance().lastExportDir() : m_path->text(),
        exportFilterForFormat(format));

    if (!path.isEmpty())
    {
        if (QFileInfo(path).suffix().isEmpty())
        {
            path += QLatin1Char('.') + defaultSuffixForFormat(format);
        }

        m_path->setText(path);
        m_pathEdited = true;
    }
}

ExportOptions ExportDialog::options() const
{
    ExportOptions result;
    result.format = selectedFormat();
    result.scale = m_scale->value();
    result.border = m_border->value();
    result.transparent = m_transparent->isChecked();
    result.embedXml = m_embed->isChecked();
    result.allPages = m_allPages->isChecked();
    result.pageView = m_pageView->isChecked();
    result.width = m_width->value();
    result.height = m_height->value();
    result.quality = m_quality->value();
    return result;
}

QString ExportDialog::filePath() const
{
    return m_path->text().trimmed();
}

void ExportDialog::setPresetFormat(const QString &format)
{
    const int index = m_format->findData(format);

    if (index >= 0)
    {
        m_pathEdited = false;
        m_format->setCurrentIndex(index);
        updateFormatUi();
        updateDefaultPath();
    }
}

// ---------------------------------------------------------------------------

PageSetupDialog::PageSetupDialog(const Page &page, QWidget *parent)
    : QDialog(parent)
    , m_original(page.settings)
{
    setWindowTitle(QStringLiteral("Page Setup"));
    setMinimumWidth(420);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    m_preset = new QComboBox(this);
    m_preset->addItem(QStringLiteral("A4 (210 × 297 mm)"), QSizeF(827, 1169));
    m_preset->addItem(QStringLiteral("A3 (297 × 420 mm)"), QSizeF(1169, 1654));
    m_preset->addItem(QStringLiteral("A5 (148 × 210 mm)"), QSizeF(583, 827));
    m_preset->addItem(QStringLiteral("Letter (216 × 279 mm)"), QSizeF(850, 1100));
    m_preset->addItem(QStringLiteral("Legal (216 × 356 mm)"), QSizeF(850, 1400));
    m_preset->addItem(QStringLiteral("Custom"), QSizeF());
    form->addRow(QStringLiteral("Paper:"), m_preset);

    m_orientation = new QComboBox(this);
    m_orientation->addItem(QStringLiteral("Portrait"), 0);
    m_orientation->addItem(QStringLiteral("Landscape"), 1);
    form->addRow(QStringLiteral("Direction:"), m_orientation);

    m_width = new QDoubleSpinBox(this);
    m_width->setRange(50, 20000);
    m_width->setDecimals(0);
    m_width->setSuffix(QStringLiteral(" px"));
    form->addRow(QStringLiteral("Width:"), m_width);

    m_height = new QDoubleSpinBox(this);
    m_height->setRange(50, 20000);
    m_height->setDecimals(0);
    m_height->setSuffix(QStringLiteral(" px"));
    form->addRow(QStringLiteral("Height:"), m_height);

    m_showPage = new QCheckBox(QStringLiteral("Show Page Bounds"), this);
    m_showPage->setChecked(m_original.page);
    form->addRow(QString(), m_showPage);

    m_grid = new QCheckBox(QStringLiteral("Show Grid"), this);
    m_grid->setChecked(m_original.grid);
    form->addRow(QString(), m_grid);

    m_gridSize = new QSpinBox(this);
    m_gridSize->setRange(1, 200);
    m_gridSize->setValue(m_original.gridSize);
    form->addRow(QStringLiteral("Grid Size:"), m_gridSize);

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("OK"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancel"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_preset, &QComboBox::currentIndexChanged, this, &PageSetupDialog::applyPreset);
    connect(m_orientation, &QComboBox::currentIndexChanged, this, &PageSetupDialog::updateEnabled);

    m_width->setValue(m_original.pageWidth);
    m_height->setValue(m_original.pageHeight);

    // Select the preset matching the current page size, if any.
    int presetIndex = -1;
    for (int i = 0; i < m_preset->count() - 1; ++i)
    {
        const QSizeF size = m_preset->itemData(i).toSizeF();
        const double w = m_original.pageWidth;
        const double h = m_original.pageHeight;

        if (qFuzzyCompare(size.width(), qMin(w, h)) && qFuzzyCompare(size.height(), qMax(w, h)))
        {
            presetIndex = i;
            break;
        }
    }
    m_preset->setCurrentIndex(presetIndex >= 0 ? presetIndex : m_preset->count() - 1);

    if (m_original.pageWidth > m_original.pageHeight)
    {
        m_orientation->setCurrentIndex(1);
    }
}

void PageSetupDialog::applyPreset(int index)
{
    const QSizeF size = m_preset->itemData(index).toSizeF();

    if (size.isEmpty())
    {
        return;
    }

    if (m_orientation->currentIndex() == 1)
    {
        m_width->setValue(qMax(size.width(), size.height()));
        m_height->setValue(qMin(size.width(), size.height()));
    }
    else
    {
        m_width->setValue(qMin(size.width(), size.height()));
        m_height->setValue(qMax(size.width(), size.height()));
    }
}

void PageSetupDialog::updateEnabled()
{
    // Orientations swap width and height in place.
    if (m_preset->currentIndex() >= 0)
    {
        const double width = m_width->value();
        const double height = m_height->value();

        if (m_orientation->currentIndex() == 1 && width < height)
        {
            m_width->setValue(height);
            m_height->setValue(width);
        }
        else if (m_orientation->currentIndex() == 0 && width > height)
        {
            m_width->setValue(height);
            m_height->setValue(width);
        }
    }
}

Page::Settings PageSetupDialog::settings() const
{
    Page::Settings result = m_original;
    result.pageWidth = m_width->value();
    result.pageHeight = m_height->value();
    result.page = m_showPage->isChecked();
    result.grid = m_grid->isChecked();
    result.gridSize = m_gridSize->value();
    return result;
}

ShortcutDialog::ShortcutDialog(const QVector<ShortcutSetting> &settings, QWidget *parent)
    : QDialog(parent), m_settings(settings)
{
    setWindowTitle(QStringLiteral("Keyboard Shortcuts"));
    resize(520, 620);

    auto *layout = new QVBoxLayout(this);
    auto *table = new QTableWidget(m_settings.size(), 2, this);
    table->setHorizontalHeaderLabels({QStringLiteral("Action"), QStringLiteral("Shortcut")});
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    for (int row = 0; row < m_settings.size(); ++row)
    {
        const ShortcutSetting &item = m_settings.at(row);
        table->setItem(row, 0, new QTableWidgetItem(item.label));
        auto *edit = new QKeySequenceEdit(item.sequence, table);
        table->setCellWidget(row, 1, edit);
        m_editors.append(edit);
    }

    layout->addWidget(table, 1);

    auto *buttons = new QHBoxLayout();
    auto *reset = new QPushButton(QStringLiteral("Restore Defaults"), this);
    auto *cancel = new QPushButton(QStringLiteral("Cancel"), this);
    auto *ok = new QPushButton(QStringLiteral("OK"), this);
    ok->setDefault(true);
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    layout->addLayout(buttons);

    connect(reset, &QPushButton::clicked, this, &ShortcutDialog::restoreDefaults);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok, &QPushButton::clicked, this, &ShortcutDialog::acceptIfValid);
}

QVector<ShortcutSetting> ShortcutDialog::settings() const
{
    QVector<ShortcutSetting> result = m_settings;

    for (int i = 0; i < result.size() && i < m_editors.size(); ++i)
    {
        result[i].sequence = m_editors.at(i)->keySequence();
    }

    return result;
}

void ShortcutDialog::restoreDefaults()
{
    for (int i = 0; i < m_settings.size() && i < m_editors.size(); ++i)
    {
        m_editors.at(i)->setKeySequence(m_settings.at(i).defaultSequence);
    }
}

void ShortcutDialog::acceptIfValid()
{
    const QVector<ShortcutSetting> current = settings();
    QHash<QString, QString> used;

    for (const ShortcutSetting &item : current)
    {
        const QString text = item.sequence.toString(QKeySequence::PortableText);

        if (text.isEmpty())
        {
            continue;
        }

        if (used.contains(text))
        {
            QMessageBox::warning(this, QStringLiteral("Shortcut Conflict"),
                                 I18n::text(QStringLiteral("“%1” and “%2” both use %3"))
                                     .arg(used.value(text), item.label, text));
            return;
        }

        used.insert(text, item.label);
    }

    m_settings = current;
    accept();
}

SettingsDialog::SettingsDialog(const QVector<ShortcutSetting> &shortcuts,
                               const QString &theme, const QString &language,
                               QWidget *parent)
    : QDialog(parent), m_shortcuts(shortcuts)
{
    setWindowTitle(QStringLiteral("Settings"));
    resize(620, 680);

    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);

    auto *general = new QWidget(tabs);
    auto *form = new QFormLayout(general);
    m_theme = new QComboBox(general);
    m_theme->addItem(QStringLiteral("Light"), QStringLiteral("light"));
    m_theme->addItem(QStringLiteral("Dark"), QStringLiteral("dark"));
    m_theme->addItem(QStringLiteral("Follow System"), QStringLiteral("system"));

    const int themeIndex = m_theme->findData(theme);
    m_theme->setCurrentIndex(themeIndex >= 0 ? themeIndex : 2);
    form->addRow(QStringLiteral("Appearance"), m_theme);

    m_language = new QComboBox(general);
    m_language->addItem(QStringLiteral("Simplified Chinese"), QStringLiteral("zh"));
    m_language->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_language->addItem(QStringLiteral("Follow System"), QStringLiteral("system"));

    const int languageIndex = m_language->findData(language);
    m_language->setCurrentIndex(languageIndex >= 0 ? languageIndex : 2);
    form->addRow(QStringLiteral("Language"), m_language);
    form->addRow(QString(), new QLabel(QStringLiteral("Shortcuts can be edited on the Shortcuts tab."), general));
    tabs->addTab(general, QStringLiteral("General"));

    auto *shortcutPage = new QWidget(tabs);
    auto *shortcutLayout = new QVBoxLayout(shortcutPage);
    auto *table = new QTableWidget(m_shortcuts.size(), 2, shortcutPage);
    table->setHorizontalHeaderLabels({QStringLiteral("Action"), QStringLiteral("Shortcut")});
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    for (int row = 0; row < m_shortcuts.size(); ++row)
    {
        const ShortcutSetting &item = m_shortcuts.at(row);
        table->setItem(row, 0, new QTableWidgetItem(item.label));
        auto *edit = new QKeySequenceEdit(item.sequence, table);
        table->setCellWidget(row, 1, edit);
        m_editors.append(edit);
    }

    shortcutLayout->addWidget(table, 1);

    auto *reset = new QPushButton(QStringLiteral("Restore Default Shortcuts"), shortcutPage);
    auto *resetRow = new QHBoxLayout();
    resetRow->addWidget(reset);
    resetRow->addStretch(1);
    shortcutLayout->addLayout(resetRow);

    tabs->addTab(shortcutPage, QStringLiteral("Shortcut"));
    layout->addWidget(tabs, 1);

    auto *buttons = new QHBoxLayout();
    auto *cancel = new QPushButton(QStringLiteral("Cancel"), this);
    auto *ok = new QPushButton(QStringLiteral("OK"), this);
    ok->setDefault(true);
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    layout->addLayout(buttons);

    connect(reset, &QPushButton::clicked, this, &SettingsDialog::restoreDefaults);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok, &QPushButton::clicked, this, &SettingsDialog::acceptIfValid);
}

QVector<ShortcutSetting> SettingsDialog::shortcutSettings() const
{
    QVector<ShortcutSetting> result = m_shortcuts;

    for (int i = 0; i < result.size() && i < m_editors.size(); ++i)
    {
        result[i].sequence = m_editors.at(i)->keySequence();
    }

    return result;
}

QString SettingsDialog::theme() const
{
    return m_theme != nullptr ? m_theme->currentData().toString() : QStringLiteral("system");
}

QString SettingsDialog::language() const
{
    return m_language != nullptr ? m_language->currentData().toString() : QStringLiteral("system");
}

void SettingsDialog::restoreDefaults()
{
    for (int i = 0; i < m_shortcuts.size() && i < m_editors.size(); ++i)
    {
        m_editors.at(i)->setKeySequence(m_shortcuts.at(i).defaultSequence);
    }
}

void SettingsDialog::acceptIfValid()
{
    const QVector<ShortcutSetting> current = shortcutSettings();
    QHash<QString, QString> used;

    for (const ShortcutSetting &item : current)
    {
        const QString text = item.sequence.toString(QKeySequence::PortableText);

        if (text.isEmpty())
        {
            continue;
        }

        if (used.contains(text))
        {
            QMessageBox::warning(this, QStringLiteral("Shortcut Conflict"),
                                 I18n::text(QStringLiteral("“%1” and “%2” both use %3"))
                                     .arg(used.value(text), item.label, text));
            return;
        }

        used.insert(text, item.label);
    }

    m_shortcuts = current;
    accept();
}

void showAboutDialog(QWidget *parent)
{
    const QString html = QStringLiteral("<h3>drawio-qt %1</h3>")
        + I18n::text(QStringLiteral("about.body"))
              .arg(QStringLiteral(DRAWIO_QT_VERSION), QString::fromLatin1(qVersion()));

    QMessageBox box(parent);
    box.setWindowTitle(QStringLiteral("About drawio-qt"));
    box.setTextFormat(Qt::RichText);
    box.setText(html);
    box.exec();
}

} // namespace dq
