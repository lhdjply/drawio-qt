#include "ui/Dialogs.h"

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
        return QStringLiteral("PNG 图像 (*.png)");
    }

    if (format == QLatin1String("jpg"))
    {
        return QStringLiteral("JPEG 图像 (*.jpg)");
    }

    if (format == QLatin1String("svg"))
    {
        return QStringLiteral("SVG 矢量图 (*.svg)");
    }

    if (format == QLatin1String("pdf"))
    {
        return QStringLiteral("PDF 文档 (*.pdf)");
    }

    if (format == QLatin1String("xml"))
    {
        return QStringLiteral("draw.io XML (*.drawio *.xml)");
    }

    return QStringLiteral("HTML 查看器 (*.html)");
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
    setWindowTitle(QStringLiteral("导出"));
    setMinimumWidth(520);

    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout();
    m_format = new QComboBox(this);
    m_format->addItem(QStringLiteral("PNG 图像"), QStringLiteral("png"));
    m_format->addItem(QStringLiteral("SVG 矢量图"), QStringLiteral("svg"));
    m_format->addItem(QStringLiteral("PDF 文档"), QStringLiteral("pdf"));
    m_format->addItem(QStringLiteral("JPEG 图像"), QStringLiteral("jpg"));
    m_format->addItem(QStringLiteral("draw.io XML"), QStringLiteral("xml"));
    m_format->addItem(QStringLiteral("HTML 查看器"), QStringLiteral("html"));
    form->addRow(QStringLiteral("格式："), m_format);

    auto *pathRow = new QHBoxLayout();
    m_path = new QLineEdit(this);
    auto *browseButton = new QPushButton(QStringLiteral("浏览…"), this);
    pathRow->addWidget(m_path, 1);
    pathRow->addWidget(browseButton);
    form->addRow(QStringLiteral("保存到："), pathRow);

    m_scale = new QDoubleSpinBox(this);
    m_scale->setRange(0.1, 20.0);
    m_scale->setSingleStep(0.1);
    m_scale->setValue(1.0);
    m_scale->setSuffix(QStringLiteral(" x"));
    form->addRow(QStringLiteral("缩放："), m_scale);

    m_border = new QSpinBox(this);
    m_border->setRange(0, 200);
    m_border->setValue(0);
    m_border->setSuffix(QStringLiteral(" px"));
    form->addRow(QStringLiteral("边距："), m_border);

    m_width = new QSpinBox(this);
    m_width->setRange(0, 20000);
    m_width->setSpecialValueText(QStringLiteral("自动"));
    form->addRow(QStringLiteral("目标宽度："), m_width);

    m_height = new QSpinBox(this);
    m_height->setRange(0, 20000);
    m_height->setSpecialValueText(QStringLiteral("自动"));
    form->addRow(QStringLiteral("目标高度："), m_height);

    m_quality = new QSpinBox(this);
    m_quality->setRange(1, 100);
    m_quality->setValue(90);
    form->addRow(QStringLiteral("JPEG 质量："), m_quality);

    m_transparent = new QCheckBox(QStringLiteral("透明背景（PNG/SVG）"), this);
    form->addRow(QString(), m_transparent);

    m_embed = new QCheckBox(QStringLiteral("嵌入图数据（可用 draw.io 重新打开）"), this);
    form->addRow(QString(), m_embed);

    m_allPages = new QCheckBox(QStringLiteral("导出全部页面"), this);
    form->addRow(QString(), m_allPages);

    m_pageView = new QCheckBox(QStringLiteral("按页面尺寸导出（而不是裁剪到图形）"), this);
    form->addRow(QString(), m_pageView);

    layout->addLayout(form);

    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    m_hint->setStyleSheet(QStringLiteral("color: #666;"));
    layout->addWidget(m_hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("导出"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
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
        m_hint->setText(QStringLiteral("XML 导出保留完整页面的 mxGraphModel，可直接用 draw.io 打开。"));
    }
    else if (format == QLatin1String("html"))
    {
        m_hint->setText(QStringLiteral("HTML 导出生成一个通过 diagrams.net 查看器渲染的独立网页。"));
    }
    else if (m_embed->isChecked())
    {
        m_hint->setText(QStringLiteral("图数据将写入文件，可再次用 drawio-qt 或 draw.io 打开以继续编辑。"));
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
            + QStringLiteral("未命名绘图");
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
        this, QStringLiteral("导出"), m_path->text().isEmpty()
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
    setWindowTitle(QStringLiteral("页面设置"));
    setMinimumWidth(420);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    m_preset = new QComboBox(this);
    m_preset->addItem(QStringLiteral("A4 (210 × 297 mm)"), QSizeF(827, 1169));
    m_preset->addItem(QStringLiteral("A3 (297 × 420 mm)"), QSizeF(1169, 1654));
    m_preset->addItem(QStringLiteral("A5 (148 × 210 mm)"), QSizeF(583, 827));
    m_preset->addItem(QStringLiteral("Letter (216 × 279 mm)"), QSizeF(850, 1100));
    m_preset->addItem(QStringLiteral("Legal (216 × 356 mm)"), QSizeF(850, 1400));
    m_preset->addItem(QStringLiteral("自定义"), QSizeF());
    form->addRow(QStringLiteral("纸张："), m_preset);

    m_orientation = new QComboBox(this);
    m_orientation->addItem(QStringLiteral("纵向"), 0);
    m_orientation->addItem(QStringLiteral("横向"), 1);
    form->addRow(QStringLiteral("方向："), m_orientation);

    m_width = new QDoubleSpinBox(this);
    m_width->setRange(50, 20000);
    m_width->setDecimals(0);
    m_width->setSuffix(QStringLiteral(" px"));
    form->addRow(QStringLiteral("宽度："), m_width);

    m_height = new QDoubleSpinBox(this);
    m_height->setRange(50, 20000);
    m_height->setDecimals(0);
    m_height->setSuffix(QStringLiteral(" px"));
    form->addRow(QStringLiteral("高度："), m_height);

    m_showPage = new QCheckBox(QStringLiteral("显示页面边界"), this);
    m_showPage->setChecked(m_original.page);
    form->addRow(QString(), m_showPage);

    m_grid = new QCheckBox(QStringLiteral("显示网格"), this);
    m_grid->setChecked(m_original.grid);
    form->addRow(QString(), m_grid);

    m_gridSize = new QSpinBox(this);
    m_gridSize->setRange(1, 200);
    m_gridSize->setValue(m_original.gridSize);
    form->addRow(QStringLiteral("网格大小："), m_gridSize);

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
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
    setWindowTitle(QStringLiteral("键盘快捷键"));
    resize(520, 620);

    auto *layout = new QVBoxLayout(this);
    auto *table = new QTableWidget(m_settings.size(), 2, this);
    table->setHorizontalHeaderLabels({QStringLiteral("功能"), QStringLiteral("快捷键")});
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
    auto *reset = new QPushButton(QStringLiteral("恢复默认"), this);
    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    auto *ok = new QPushButton(QStringLiteral("确定"), this);
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
            QMessageBox::warning(this, QStringLiteral("快捷键冲突"),
                                 QStringLiteral("“%1”和“%2”都使用了 %3")
                                     .arg(used.value(text), item.label, text));
            return;
        }

        used.insert(text, item.label);
    }

    m_settings = current;
    accept();
}

SettingsDialog::SettingsDialog(const QVector<ShortcutSetting> &shortcuts,
                               const QString &theme, QWidget *parent)
    : QDialog(parent), m_shortcuts(shortcuts)
{
    setWindowTitle(QStringLiteral("设置"));
    resize(620, 680);

    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);

    auto *general = new QWidget(tabs);
    auto *form = new QFormLayout(general);
    m_theme = new QComboBox(general);
    m_theme->addItem(QStringLiteral("浅色"), QStringLiteral("light"));
    m_theme->addItem(QStringLiteral("深色"), QStringLiteral("dark"));
    m_theme->addItem(QStringLiteral("跟随系统"), QStringLiteral("system"));

    const int themeIndex = m_theme->findData(theme);
    m_theme->setCurrentIndex(themeIndex >= 0 ? themeIndex : 2);
    form->addRow(QStringLiteral("外观"), m_theme);
    form->addRow(QString(), new QLabel(QStringLiteral("快捷键可在“快捷键”页逐项修改。"), general));
    tabs->addTab(general, QStringLiteral("常规"));

    auto *shortcutPage = new QWidget(tabs);
    auto *shortcutLayout = new QVBoxLayout(shortcutPage);
    auto *table = new QTableWidget(m_shortcuts.size(), 2, shortcutPage);
    table->setHorizontalHeaderLabels({QStringLiteral("功能"), QStringLiteral("快捷键")});
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

    auto *reset = new QPushButton(QStringLiteral("恢复默认快捷键"), shortcutPage);
    auto *resetRow = new QHBoxLayout();
    resetRow->addWidget(reset);
    resetRow->addStretch(1);
    shortcutLayout->addLayout(resetRow);

    tabs->addTab(shortcutPage, QStringLiteral("快捷键"));
    layout->addWidget(tabs, 1);

    auto *buttons = new QHBoxLayout();
    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    auto *ok = new QPushButton(QStringLiteral("确定"), this);
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
            QMessageBox::warning(this, QStringLiteral("快捷键冲突"),
                                 QStringLiteral("“%1”和“%2”都使用了 %3")
                                     .arg(used.value(text), item.label, text));
            return;
        }

        used.insert(text, item.label);
    }

    m_shortcuts = current;
    accept();
}

void showShortcutsDialog(QWidget *parent)
{
    const QString html = QStringLiteral(
        "<h3>键盘快捷键</h3>"
        "<table cellspacing='6'>"
        "<tr><td>Ctrl + N</td><td>新建</td></tr>"
        "<tr><td>Ctrl + O</td><td>打开</td></tr>"
        "<tr><td>Ctrl + S</td><td>保存</td></tr>"
        "<tr><td>Ctrl + Shift + S</td><td>另存为</td></tr>"
        "<tr><td>Ctrl + Z / Ctrl + Shift + Z</td><td>撤销 / 重做</td></tr>"
        "<tr><td>Ctrl + C / X / V / D</td><td>复制 / 剪切 / 粘贴 / 复制副本</td></tr>"
        "<tr><td>Delete</td><td>删除所选</td></tr>"
        "<tr><td>Enter / F2</td><td>编辑标签</td></tr>"
        "<tr><td>Ctrl + 拖动</td><td>从形状拉出连接线</td></tr>"
        "<tr><td>Shift + 单击</td><td>加选 / 减选</td></tr>"
        "<tr><td>拖动空白处</td><td>框选</td></tr>"
        "<tr><td>Ctrl + 滚轮</td><td>缩放</td></tr>"
        "<tr><td>中键拖动 / 空格 + 拖动</td><td>平移画布</td></tr>"
        "<tr><td>Ctrl + G / Ctrl + Shift + U</td><td>组合 / 取消组合</td></tr>"
        "<tr><td>方向键 / Shift + 方向键</td><td>移动所选（1px / 10px）</td></tr>"
        "<tr><td>Alt + 拖动</td><td>临时关闭对齐参考线与网格吸附</td></tr>"
        "<tr><td>F11</td><td>全屏</td></tr>"
        "</table>");

    QMessageBox box(parent);
    box.setWindowTitle(QStringLiteral("快捷键"));
    box.setTextFormat(Qt::RichText);
    box.setText(html);
    box.exec();
}

void showAboutDialog(QWidget *parent)
{
    const QString html = QStringLiteral(
        "<h3>drawio-qt %1</h3>"
        "<p>draw.io 桌面版（Electron）的原生 Qt6 / C++ 移植版本。</p>"
        "<p>使用 Qt Widgets 自行实现图形编辑器：mxGraph 数据模型、形状库、"
        "正交连线路由、撤销重做、PNG/SVG/PDF/HTML/XML 导出与打印。</p>"
        "<p>文件格式与 draw.io 兼容，可直接打开和保存 <code>.drawio</code> 文件。</p>"
        "<p>本程序基于上游 draw.io 桌面版的项目结构重新实现，"
        "遵循 GNU General Public License v3.0。</p>"
        "<p>版本：%1　Qt：%2</p>")
        .arg(QStringLiteral(DRAWIO_QT_VERSION), QString::fromLatin1(qVersion()));

    QMessageBox box(parent);
    box.setWindowTitle(QStringLiteral("关于 drawio-qt"));
    box.setTextFormat(Qt::RichText);
    box.setText(html);
    box.exec();
}

} // namespace dq
