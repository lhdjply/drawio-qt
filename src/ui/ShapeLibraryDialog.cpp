#include "ui/ShapeLibraryDialog.h"

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
    setWindowTitle(QStringLiteral("形状库"));
    setMinimumSize(380, 520);

    auto *layout = new QVBoxLayout(this);

    auto *hint = new QLabel(QStringLiteral("勾选要在形状面板中显示的库："), this);
    layout->addWidget(hint);

    m_list = new QListWidget(this);
    m_list->setAlternatingRowColors(true);
    layout->addWidget(m_list, 1);

    const QStringList configured = configuredCategories();
    const QVector<ShapeCategory> categories = ShapeCatalog::instance().categories();

    for (const ShapeCategory &category : categories)
    {
        m_allIds.append(category.id);

        auto *item = new QListWidgetItem(m_list);
        item->setText(QStringLiteral("%1  (%2)").arg(category.name)
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
    auto *selectAllButton = new QPushButton(QStringLiteral("全选"), this);
    auto *selectNoneButton = new QPushButton(QStringLiteral("全不选"), this);
    buttonsRow->addWidget(selectAllButton);
    buttonsRow->addWidget(selectNoneButton);
    buttonsRow->addStretch(1);
    layout->addLayout(buttonsRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
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
        m_list->item(i)->setCheckState(selected ? Qt::Checked : Qt::Unchecked);
    }
}

QStringList ShapeLibraryDialog::enabledCategories() const
{
    QStringList result;

    for (int i = 0; i < m_list->count(); ++i)
    {
        const QListWidgetItem *item = m_list->item(i);

        if (item->checkState() == Qt::Checked)
        {
            result.append(item->data(Qt::UserRole).toString());
        }
    }

    // Everything checked is the default state and needs no persistence.
    return result.size() == m_allIds.size() ? QStringList() : result;
}

QStringList ShapeLibraryDialog::configuredCategories()
{
    return AppSettings::instance().enabledShapeLibraries();
}

void ShapeLibraryDialog::setConfiguredCategories(const QStringList &ids)
{
    AppSettings::instance().setEnabledShapeLibraries(ids);
    AppSettings::instance().sync();
}

} // namespace dq
