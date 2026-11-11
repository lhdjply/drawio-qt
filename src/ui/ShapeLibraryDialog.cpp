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
    setMinimumSize(380, 520);

    auto *layout = new QVBoxLayout(this);

    auto *hint = new QLabel(QStringLiteral("Select the libraries to show in the left panel:"), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_list = new QListWidget(this);
    m_list->setAlternatingRowColors(true);
    layout->addWidget(m_list, 1);

    const QStringList configured = configuredCategories();
    const QVector<ShapeCategory> categories = ShapeCatalog::instance().categories();
    bool imageHeaderAdded = false;

    auto *builtinHeader = new QListWidgetItem(I18n::text(QStringLiteral("Built-in Shapes")), m_list);
    builtinHeader->setFlags(Qt::NoItemFlags);
    QFont builtinFont = builtinHeader->font();
    builtinFont.setBold(true);
    builtinHeader->setFont(builtinFont);

    for (const ShapeCategory &category : categories)
    {
        if (category.imageLibrary && !imageHeaderAdded)
        {
            imageHeaderAdded = true;
            auto *header = new QListWidgetItem(I18n::text(QStringLiteral("Image Libraries")), m_list);
            header->setFlags(Qt::NoItemFlags);
            QFont headerFont = header->font();
            headerFont.setBold(true);
            header->setFont(headerFont);
        }

        m_allIds.append(category.id);

        auto *item = new QListWidgetItem(m_list);
        item->setText(QStringLiteral("%1  (%2)")
                          .arg(I18n::categoryName(category.id, category.name))
                          .arg(category.shapes.size()));
        item->setData(Qt::UserRole, category.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
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
    buttonsRow->addWidget(selectAllButton);
    buttonsRow->addWidget(selectNoneButton);
    buttonsRow->addStretch(1);
    layout->addLayout(buttonsRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("OK"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancel"));
    layout->addWidget(buttons);

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
    // Keep the initial palette focused: basic shapes, arrows and flowchart.
    return {QStringLiteral("basic"), QStringLiteral("arrows"),
            QStringLiteral("flowchart")};
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
