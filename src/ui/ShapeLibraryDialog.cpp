#include "ui/ShapeLibraryDialog.h"

#include "app/I18n.h"

#include "app/AppSettings.h"
#include "render/ShapeCatalog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace dq {

ShapeLibraryDialog::ShapeLibraryDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Shape Libraries"));
    setMinimumSize(420, 560);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Shape Libraries"), this);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(14);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *hint = new QLabel(QStringLiteral("Select the libraries to show in the left panel:"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(hint);

    m_list = new QListWidget(this);
    m_list->setAlternatingRowColors(false);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setStyleSheet(QStringLiteral(
        "QListWidget { border: 1px solid palette(mid); border-radius: 6px;"
        " background: palette(base); padding: 4px; }"
        "QListWidget::item { padding: 7px 10px; border-bottom: 1px solid palette(alternate-base); }"
        "QListWidget::item:selected { background: palette(highlight);"
        " color: palette(highlighted-text); border-radius: 4px; }"
        "QListWidget::indicator { width: 16px; height: 16px; }"));
    layout->addWidget(m_list, 1);

    const QStringList configured = configuredCategories();
    const QVector<ShapeCategory> categories = ShapeCatalog::instance().categories();
    bool imageHeaderAdded = false;

    auto *builtinHeader = new QListWidgetItem(I18n::text(QStringLiteral("Built-in Shapes")), m_list);
    builtinHeader->setFlags(Qt::NoItemFlags);
    QFont builtinFont = builtinHeader->font();
    builtinFont.setBold(true);
    builtinFont.setPointSizeF(10);
    builtinHeader->setFont(builtinFont);
    builtinHeader->setForeground(QBrush(QColor(0x7A, 0x82, 0x8E)));
    builtinHeader->setSizeHint(QSize(0, 30));

    for (const ShapeCategory &category : categories)
    {
        if (category.imageLibrary && !imageHeaderAdded)
        {
            imageHeaderAdded = true;
            auto *header = new QListWidgetItem(I18n::text(QStringLiteral("Image Libraries")), m_list);
            header->setFlags(Qt::NoItemFlags);
            QFont headerFont = header->font();
            headerFont.setBold(true);
            headerFont.setPointSizeF(10);
            header->setFont(headerFont);
            header->setForeground(QBrush(QColor(0x7A, 0x82, 0x8E)));
            header->setSizeHint(QSize(0, 30));
        }

        m_allIds.append(category.id);

        auto *item = new QListWidgetItem(m_list);
        item->setText(QStringLiteral("%1  (%2)")
                          .arg(I18n::categoryName(category.id, category.name))
                          .arg(category.shapes.size()));
        item->setData(Qt::UserRole, category.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setSizeHint(QSize(0, 32));
        const bool enabled = configured.isEmpty() || configured.contains(category.id);
        item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);

        if (category.imageLibrary)
        {
            QFont font = item->font();
            font.setItalic(true);
            item->setFont(font);
        }
    }

    auto *buttonsRow = new QHBoxLayout();
    auto *selectAllButton = new QPushButton(QStringLiteral("Select All"), this);
    auto *selectNoneButton = new QPushButton(QStringLiteral("Deselect All"), this);
    selectAllButton->setObjectName(QStringLiteral("linkButton"));
    selectNoneButton->setObjectName(QStringLiteral("linkButton"));
    buttonsRow->addWidget(selectAllButton);
    buttonsRow->addWidget(selectNoneButton);
    buttonsRow->addStretch(1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto *ok = buttons->button(QDialogButtonBox::Ok);
    auto *cancel = buttons->button(QDialogButtonBox::Cancel);
    ok->setText(QStringLiteral("OK"));
    cancel->setText(QStringLiteral("Cancel"));
    ok->setIcon(QIcon());
    cancel->setIcon(QIcon());
    ok->setObjectName(QStringLiteral("primaryButton"));
    ok->setDefault(true);
    buttonsRow->addWidget(buttons);
    layout->addLayout(buttonsRow);

    setStyleSheet(QStringLiteral(
        "QPushButton { min-height: 32px; min-width: 84px; padding: 0 14px;"
        " border: 1px solid palette(mid); border-radius: 4px; background: palette(button); }"
        "QPushButton:hover { background: palette(midlight); }"
        "QPushButton#primaryButton { background: #1a73e8; color: white;"
        " border: none; font-weight: 600; }"
        "QPushButton#primaryButton:hover { background: #1765cc; }"
        "QPushButton#linkButton { border: none; background: transparent;"
        " color: #1a73e8; min-width: 0; padding: 0 6px; }"
        "QPushButton#linkButton:hover { text-decoration: underline; }"));

    connect(selectAllButton, &QPushButton::clicked, this, [this]()
    {
        selectAll(true);
    });
    connect(selectNoneButton, &QPushButton::clicked, this, [this]()
    {
        selectAll(false);
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ShapeLibraryDialog::selectAll(bool selected)
{
    for (int i = 0; i < m_list->count(); ++i)
    {
        QListWidgetItem *item = m_list->item(i);

        if (item->flags() & Qt::ItemIsUserCheckable)
        {
            item->setCheckState(selected ? Qt::Checked : Qt::Unchecked);
        }
    }
}

QStringList ShapeLibraryDialog::enabledCategories() const
{
    QStringList result;

    for (int i = 0; i < m_list->count(); ++i)
    {
        const QListWidgetItem *item = m_list->item(i);

        if (!item->data(Qt::UserRole).isValid())
        {
            continue;
        }

        if (item->checkState() == Qt::Checked)
        {
            result.append(item->data(Qt::UserRole).toString());
        }
    }

    // Persist the explicit selection; draw.io's default panel only shows the
    // built-in libraries, not every bundled image library.
    if (result.isEmpty())
    {
        return {QStringLiteral("__none__")};
    }

    return result;
}

QStringList ShapeLibraryDialog::defaultCategories()
{
    // Keep the initial palette focused: general and basic shapes, arrows and
    // flowchart (the libraries draw.io shows by default).
    return {QStringLiteral("general"), QStringLiteral("basic"),
            QStringLiteral("arrows"), QStringLiteral("flowchart")};
}

QStringList ShapeLibraryDialog::configuredCategories()
{
    const QStringList configured = AppSettings::instance().enabledShapeLibraries();
    return configured.isEmpty() ? defaultCategories() : configured;
}

void ShapeLibraryDialog::setConfiguredCategories(const QStringList &ids)
{
    AppSettings::instance().setEnabledShapeLibraries(ids);
    AppSettings::instance().sync();
}

} // namespace dq
