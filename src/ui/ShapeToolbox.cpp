#include "ui/ShapeToolbox.h"

#include "render/ShapeCatalog.h"
#include "ui/ShapeLibraryDialog.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace dq {
namespace {

constexpr const char *kShapeMimeType = "application/x-drawio-shape";

/// Adds the shape key to the drag payload so DiagramView can rebuild the
/// definition on drop.
class ShapeList : public QListWidget
{
public:
    explicit ShapeList(QWidget *parent = nullptr)
        : QListWidget(parent)
    {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setViewMode(QListView::IconMode);
        setResizeMode(QListView::Adjust);
        setMovement(QListView::Static);
        setIconSize(QSize(36, 36));
        setGridSize(QSize(82, 72));
        setSpacing(2);
        setWordWrap(true);
        setUniformItemSizes(true);
    }

protected:
    QMimeData *mimeData(const QList<QListWidgetItem *> &items) const override
    {
        auto *data = new QMimeData();

        if (!items.isEmpty())
        {
            data->setData(QLatin1String(kShapeMimeType),
                          items.first()->data(Qt::UserRole).toString().toUtf8());
        }

        return data;
    }
};

} // namespace

ShapeToolbox::ShapeToolbox(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("搜索形状…"));
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    auto *categoryRow = new QHBoxLayout();
    categoryRow->setSpacing(4);

    m_category = new QComboBox(this);
    m_category->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    categoryRow->addWidget(m_category, 1);

    auto *manage = new QToolButton(this);
    manage->setText(QStringLiteral("…"));
    manage->setToolTip(QStringLiteral("选择要显示的形状库"));
    manage->setAutoRaise(true);
    categoryRow->addWidget(manage);

    layout->addLayout(categoryRow);

    m_list = new ShapeList(this);
    layout->addWidget(m_list, 1);

    m_count = new QLabel(this);
    m_count->setStyleSheet(QStringLiteral("color: #8a9099; font-size: 11px;"));
    layout->addWidget(m_count);

    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(180);

    populateCategories();
    populateList();

    connect(m_category, &QComboBox::currentIndexChanged, this, [this](int)
    {
        populateList();
    });
    connect(m_search, &QLineEdit::textChanged, this, [this]()
    {
        m_searchTimer->start();
    });
    connect(m_searchTimer, &QTimer::timeout, this, [this]()
    {
        populateList();
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item)
    {
        emitActivated(item);
    });
    connect(manage, &QToolButton::clicked, this, &ShapeToolbox::manageLibraries);
}

QStringList ShapeToolbox::enabledCategories() const
{
    return ShapeLibraryDialog::configuredCategories();
}

void ShapeToolbox::populateCategories()
{
    const QString previous = m_category->currentData().toString();
    m_category->blockSignals(true);
    m_category->clear();

    m_category->addItem(QStringLiteral("全部内置形状"), QStringLiteral("builtin:all"));

    const QStringList enabled = enabledCategories();

    for (const ShapeCategory &category : ShapeCatalog::instance().builtinCategories())
    {
        if (!enabled.isEmpty() && !enabled.contains(category.id))
        {
            continue;
        }

        m_category->addItem(category.name, category.id);
    }

    bool separatorAdded = false;

    for (const ShapeCategory &category : ShapeCatalog::instance().imageCategories())
    {
        if (!enabled.isEmpty() && !enabled.contains(category.id))
        {
            continue;
        }

        if (!separatorAdded)
        {
            m_category->insertSeparator(m_category->count());
            separatorAdded = true;
        }

        m_category->addItem(category.name, category.id);
    }

    const int index = m_category->findData(previous);

    if (index >= 0)
    {
        m_category->setCurrentIndex(index);
    }

    m_category->blockSignals(false);
}

void ShapeToolbox::populateList()
{
    if (m_list == nullptr)
    {
        return;
    }

    const QString query = m_search->text().trimmed();
    const QStringList enabled = enabledCategories();
    const QString categoryId = m_category->currentData().toString();

    QVector<ShapeDefinition> shapes;

    if (!query.isEmpty())
    {
        for (const ShapeDefinition &definition : ShapeCatalog::instance().search(query, 240))
        {
            if (enabled.isEmpty() || enabled.contains(definition.category))
            {
                shapes.append(definition);
            }
        }
    }
    else if (categoryId == QLatin1String("builtin:all"))
    {
        for (const ShapeCategory &category : ShapeCatalog::instance().builtinCategories())
        {
            if (enabled.isEmpty() || enabled.contains(category.id))
            {
                shapes.append(category.shapes);
            }
        }
    }
    else
    {
        for (const ShapeCategory &category : ShapeCatalog::instance().categories())
        {
            if (category.id == categoryId)
            {
                shapes = category.shapes;
                break;
            }
        }
    }

    m_list->clear();

    for (const ShapeDefinition &definition : shapes)
    {
        auto *item = new QListWidgetItem(m_list);
        item->setText(definition.name);
        item->setData(Qt::UserRole, definition.key);
        item->setToolTip(QStringLiteral("%1\n%2").arg(definition.name, definition.style));
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        item->setIcon(ShapeCatalog::instance().iconFor(definition, 36));
    }

    m_count->setText(query.isEmpty()
                         ? QStringLiteral("%1 个形状").arg(shapes.size())
                         : QStringLiteral("找到 %1 个形状").arg(shapes.size()));
}

void ShapeToolbox::manageLibraries()
{
    ShapeLibraryDialog dialog(this);

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    ShapeLibraryDialog::setConfiguredCategories(dialog.enabledCategories());
    reload();
}

void ShapeToolbox::reload()
{
    populateCategories();
    populateList();
}

void ShapeToolbox::emitActivated(QListWidgetItem *item)
{
    if (item == nullptr)
    {
        return;
    }

    const ShapeDefinition definition =
        ShapeCatalog::instance().definition(item->data(Qt::UserRole).toString());

    if (!definition.key.isEmpty())
    {
        emit shapeActivated(definition);
    }
}

} // namespace dq
