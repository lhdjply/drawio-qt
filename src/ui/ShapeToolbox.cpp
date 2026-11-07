#include "ui/ShapeToolbox.h"

#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
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
        setGridSize(QSize(76, 66));
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

    auto *hint = new QLabel(QStringLiteral("拖动形状到画布，或双击插入"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #666; font-size: 11px;"));
    layout->addWidget(hint);

    m_list = new ShapeList(this);
    layout->addWidget(m_list, 1);

    populate();

    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item)
    {
        emitActivated(item);
    });
}

void ShapeToolbox::populate()
{
    for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
    {
        auto *item = new QListWidgetItem(m_list);
        item->setText(definition.name);
        item->setData(Qt::UserRole, definition.key);
        item->setToolTip(QStringLiteral("%1（%2）").arg(definition.name, definition.key));
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);

        QIcon icon(QStringLiteral(":/icons/shapes/%1.svg").arg(definition.key));

        if (icon.isNull())
        {
            icon = QIcon(QStringLiteral(":/icons/shapes/rectangle.svg"));
        }

        item->setIcon(icon);
    }
}

void ShapeToolbox::emitActivated(QListWidgetItem *item)
{
    if (item == nullptr)
    {
        return;
    }

    emit shapeActivated(ShapeRegistry::definition(item->data(Qt::UserRole).toString()));
}

} // namespace dq
