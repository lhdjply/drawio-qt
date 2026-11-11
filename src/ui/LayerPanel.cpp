#include "ui/LayerPanel.h"

#include "app/I18n.h"
#include "core/Page.h"
#include "ui/DiagramView.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QScrollBar>
#include <QLineEdit>
#include <QTreeWidget>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>

namespace dq {
namespace {

QString layerIdFor(const Page &page, const Cell *cell)
{
    QString parentId = cell != nullptr ? cell->parentId : QString();

    while (!parentId.isEmpty())
    {
        if (page.layer(parentId) != nullptr)
        {
            return parentId;
        }

        const Cell *parent = page.cell(parentId);

        if (parent == nullptr)
        {
            break;
        }

        parentId = parent->parentId;
    }

    return {};
}

} // namespace

LayerPanel::LayerPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->installEventFilter(this);
    layout->addWidget(m_tree, 1);

    auto *buttons = new QHBoxLayout();
    m_add = new QToolButton(this);
    m_add->setText(QStringLiteral("＋"));
    m_add->setToolTip(QStringLiteral("New Layer"));
    m_remove = new QToolButton(this);
    m_remove->setText(QStringLiteral("－"));
    m_remove->setToolTip(QStringLiteral("Delete Layer"));
    m_up = new QToolButton(this);
    m_up->setText(QStringLiteral("↑"));
    m_up->setToolTip(QStringLiteral("Move Layer Up"));
    m_down = new QToolButton(this);
    m_down->setText(QStringLiteral("↓"));
    m_down->setToolTip(QStringLiteral("Move Layer Down"));
    m_lock = new QToolButton(this);
    m_lock->setText(QStringLiteral("🔒"));
    m_lock->setToolTip(QStringLiteral("Lock Layer"));
    auto *rename = new QToolButton(this);
    rename->setText(QStringLiteral("✎"));
    rename->setToolTip(QStringLiteral("Rename Layer"));

    for (QToolButton *button : {m_add, m_remove, m_up, m_down, m_lock, rename})
    {
        button->setAutoRaise(true);
        buttons->addWidget(button);
    }

    buttons->addStretch(1);
    layout->addLayout(buttons);

    connect(m_add, &QToolButton::clicked, this, &LayerPanel::addLayer);
    connect(m_remove, &QToolButton::clicked, this, &LayerPanel::removeLayer);
    connect(rename, &QToolButton::clicked, this, &LayerPanel::renameLayer);
    connect(m_up, &QToolButton::clicked, this, [this]() { moveLayer(1); });
    connect(m_down, &QToolButton::clicked, this, [this]() { moveLayer(-1); });
    connect(m_lock, &QToolButton::clicked, this, &LayerPanel::toggleLock);

    connect(m_tree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *current, QTreeWidgetItem *)
    {
        if (m_updating || current == nullptr || m_view == nullptr)
        {
            return;
        }

        const QTreeWidgetItem *layerItem = current->parent() != nullptr
            ? current->parent() : current;
        m_view->setActiveLayer(layerItem->data(0, Qt::UserRole).toString());

        if (current->parent() != nullptr)
        {
            const QString layerId = current->parent()->data(0, Qt::UserRole).toString();

            if (!m_view->isLayerLocked(layerId))
            {
                m_view->selectCell(current->data(0, Qt::UserRole).toString());
            }
        }
    });
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item)
    {
        if (!m_updating && item != nullptr && item->parent() == nullptr && m_view != nullptr)
        {
            m_view->setLayerVisible(item->data(0, Qt::UserRole).toString(),
                                    item->checkState(0) == Qt::Checked);
        }
    });
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item)
    {
        if (item == nullptr)
        {
            return;
        }

        if (item->parent() == nullptr)
        {
            renameLayer();
        }
        else if (m_view != nullptr)
        {
            m_view->selectCell(item->data(0, Qt::UserRole).toString());
            m_view->startEditing(item->data(0, Qt::UserRole).toString(), true);
        }
    });

    refresh();
}


void LayerPanel::setView(DiagramView *view)
{
    if (m_view == view)
    {
        refresh();
        return;
    }

    if (m_view != nullptr)
    {
        disconnect(m_view, nullptr, this, nullptr);

        if (m_view->undoStack() != nullptr)
        {
            disconnect(m_view->undoStack(), nullptr, this, nullptr);
        }
    }

    m_view = view;

    if (m_view != nullptr)
    {
        connect(m_view, &DiagramView::selectionChanged, this, [this]()
        {
            syncSelection();
        });
        connect(m_view, &DiagramView::modified, this, [this]()
        {
            refresh();
            syncSelection();
        });
    }

    refresh();
    syncSelection();
}

bool LayerPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_tree && event->type() == QEvent::KeyPress)
    {
        auto *keyEvent = static_cast<QKeyEvent *>(event);

        if (keyEvent->key() == Qt::Key_Delete && m_view != nullptr)
        {
            QTreeWidgetItem *item = m_tree->currentItem();

            if (item == nullptr)
            {
                return false;
            }

            if (item->parent() == nullptr)
            {
                const QString layerId = item->data(0, Qt::UserRole).toString();
                const Layer *layer = m_view->page() != nullptr
                    ? m_view->page()->layer(layerId) : nullptr;

                if (layer != nullptr && !layer->locked)
                {
                    removeLayer();
                }
            }
            else if (!m_view->isLayerLocked(item->parent()->data(0, Qt::UserRole).toString()))
            {
                m_view->selectCell(item->data(0, Qt::UserRole).toString());
                m_view->deleteSelection();
                refresh();
            }

            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

QString LayerPanel::objectLabel(const QString &cellId) const
{
    if (m_view == nullptr || m_view->page() == nullptr)
    {
        return cellId;
    }

    const Cell *cell = m_view->page()->cell(cellId);

    if (cell == nullptr)
    {
        return cellId;
    }

    QString text = cell->plainLabel().simplified();

    if (text.isEmpty())
    {
        text = I18n::text(cell->isEdge() ? QStringLiteral("Connector")
                                         : QStringLiteral("Shape"));
    }

    if (text.size() > 28)
    {
        text = text.left(27) + QChar(0x2026);
    }

    return text;
}

void LayerPanel::refresh()
{
    QScrollBar *scrollBar = m_tree->verticalScrollBar();
    const int scrollPosition = scrollBar != nullptr ? scrollBar->value() : 0;

    m_updating = true;
    m_tree->clear();

    if (m_view == nullptr || m_view->page() == nullptr)
    {
        m_updating = false;
        return;
    }

    Page *page = m_view->page();
    const QVector<Layer> &layers = page->layers;
    const QSet<QString> selection = m_view->selection();

    // Topmost layer first, like draw.io's Layers panel.
    for (int i = layers.size() - 1; i >= 0; --i)
    {
        const Layer &layer = layers.at(i);
        auto *layerItem = new QTreeWidgetItem(m_tree);
        const QString layerName = layer.name.isEmpty()
            ? I18n::text(QStringLiteral("Layer %1")).arg(i + 1) : layer.name;
        const QString translatedName = I18n::text(layerName);
        layerItem->setText(0, layer.locked ? QStringLiteral("🔒 ") + translatedName
                                           : translatedName);
        layerItem->setData(0, Qt::UserRole, layer.id);
        layerItem->setFlags(layerItem->flags() | Qt::ItemIsUserCheckable);
        layerItem->setCheckState(0, layer.visible ? Qt::Checked : Qt::Unchecked);

        // Objects are listed topmost first inside each layer, matching the
        // z-order shown by draw.io.
        const QVector<Cell *> &cells = page->cells();

        for (int c = cells.size() - 1; c >= 0; --c)
        {
            Cell *cell = cells.at(c);

            if (layerIdFor(*page, cell) != layer.id)
            {
                continue;
            }

            auto *item = new QTreeWidgetItem(layerItem);
            item->setText(0, objectLabel(cell->id));
            item->setData(0, Qt::UserRole, cell->id);

            if (selection.contains(cell->id))
            {
                m_tree->setCurrentItem(item);
            }
        }

        if (layer.id == page->activeLayerId)
        {
            QFont font = layerItem->font(0);
            font.setBold(true);
            layerItem->setFont(0, font);

            if (m_tree->currentItem() == nullptr
                || m_tree->currentItem()->parent() == nullptr)
            {
                m_tree->setCurrentItem(layerItem);
            }
        }

        layerItem->setExpanded(true);
    }

    const Layer *currentLayer = page->layer(currentLayerId());
    m_lock->setText(currentLayer != nullptr && currentLayer->locked
                        ? QStringLiteral("🔓") : QStringLiteral("🔒"));
    m_lock->setToolTip(currentLayer != nullptr && currentLayer->locked
                           ? QStringLiteral("Unlock Layer") : QStringLiteral("Lock Layer"));
    m_remove->setEnabled(layers.size() > 1
                         && (currentLayer == nullptr || !currentLayer->locked));
    m_up->setEnabled(layers.size() > 1);
    m_down->setEnabled(layers.size() > 1);
    m_updating = false;

    if (scrollBar != nullptr)
    {
        scrollBar->setValue(scrollPosition);
    }
}

void LayerPanel::syncSelection()
{
    if (m_view == nullptr || m_view->page() == nullptr)
    {
        return;
    }

    const QSet<QString> selection = m_view->selection();
    QTreeWidgetItem *target = nullptr;
    QTreeWidgetItem *activeLayerItem = nullptr;

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *layerItem = m_tree->topLevelItem(i);

        if (layerItem->data(0, Qt::UserRole).toString() == m_view->page()->activeLayerId)
        {
            activeLayerItem = layerItem;
        }

        for (int j = 0; j < layerItem->childCount(); ++j)
        {
            QTreeWidgetItem *child = layerItem->child(j);

            if (selection.contains(child->data(0, Qt::UserRole).toString()))
            {
                target = child;
                break;
            }
        }

        if (target != nullptr)
        {
            break;
        }
    }

    m_updating = true;

    if (target != nullptr)
    {
        target->parent()->setExpanded(true);
        m_tree->setCurrentItem(target);
        m_view->setActiveLayer(target->parent()->data(0, Qt::UserRole).toString());
    }
    else if (activeLayerItem != nullptr)
    {
        m_tree->setCurrentItem(activeLayerItem);
    }

    m_updating = false;
}

QString LayerPanel::currentLayerId() const
{
    const QTreeWidgetItem *item = m_tree->currentItem();

    if (item == nullptr)
    {
        return {};
    }

    return item->parent() != nullptr ? item->parent()->data(0, Qt::UserRole).toString()
                                     : item->data(0, Qt::UserRole).toString();
}

void LayerPanel::addLayer()
{
    if (m_view != nullptr)
    {
        m_view->addLayer();
        refresh();
    }
}

void LayerPanel::removeLayer()
{
    if (m_view != nullptr)
    {
        m_view->removeLayer(currentLayerId());
        refresh();
    }
}

void LayerPanel::renameLayer()
{
    if (m_view == nullptr || m_view->page() == nullptr)
    {
        return;
    }

    const QString id = currentLayerId();
    const Layer *layer = m_view->page()->layer(id);

    if (layer == nullptr)
    {
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Rename Layer"),
                                               QStringLiteral("Layer name:"), QLineEdit::Normal,
                                               layer->name, &ok);

    if (ok && !name.trimmed().isEmpty())
    {
        m_view->renameLayer(id, name.trimmed());
        refresh();
    }
}

void LayerPanel::toggleLock()
{
    if (m_view == nullptr || m_view->page() == nullptr)
    {
        return;
    }

    const QString id = currentLayerId();
    const Layer *layer = m_view->page()->layer(id);

    if (layer != nullptr)
    {
        m_view->setLayerLocked(id, !layer->locked);
        refresh();
    }
}

void LayerPanel::moveLayer(int delta)
{
    if (m_view == nullptr)
    {
        return;
    }

    QTreeWidgetItem *item = m_tree->currentItem();

    if (item != nullptr && item->parent() != nullptr)
    {
        m_view->moveObject(item->data(0, Qt::UserRole).toString(), delta);
    }
    else
    {
        m_view->moveLayer(currentLayerId(), delta);
    }

    refresh();
}

} // namespace dq
