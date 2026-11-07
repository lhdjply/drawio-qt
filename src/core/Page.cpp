#include "core/Page.h"

#include <QSet>
#include <QUuid>

#include <algorithm>

namespace dq {

Page::Page(const QString &pageName)
    : name(pageName)
{
    id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    layers.append(Layer{});
}

Page::~Page() = default;

QVector<Cell *> Page::vertices() const
{
    QVector<Cell *> result;
    result.reserve(m_order.size());

    for (Cell *cell : m_order)
    {
        if (cell->isVertex())
        {
            result.append(cell);
        }
    }

    return result;
}

QVector<Cell *> Page::edges() const
{
    QVector<Cell *> result;

    for (Cell *cell : m_order)
    {
        if (cell->isEdge())
        {
            result.append(cell);
        }
    }

    return result;
}

QVector<Cell *> Page::childrenOf(const QString &parentId) const
{
    QVector<Cell *> result;

    for (Cell *cell : m_order)
    {
        if (cell->parentId == parentId)
        {
            result.append(cell);
        }
    }

    return result;
}

QString Page::newId()
{
    // drawio ids are opaque strings. Numeric ids keep exported files readable
    // and match what the web editor produces, so prefer the first free number.
    forever
    {
        const QString candidate = QString::number(m_idCounter++);

        if (!m_index.contains(candidate) && candidate != QLatin1String("0")
            && candidate != QLatin1String("1"))
        {
            return candidate;
        }
    }
}

bool Page::moveCell(Cell *cell, int newIndex)
{
    const int current = m_order.indexOf(cell);

    if (current < 0)
    {
        return false;
    }

    newIndex = qBound(0, newIndex, m_order.size() - 1);

    if (newIndex == current)
    {
        return true;
    }

    m_order.move(current, newIndex);
    return true;
}

Cell *Page::addCell(std::unique_ptr<Cell> cell)
{
    if (!cell || cell->id.isEmpty())
    {
        return nullptr;
    }

    if (m_index.contains(cell->id))
    {
        return nullptr;
    }

    cell->page = this;

    if (cell->parentId.isEmpty())
    {
        cell->parentId = activeLayerId;
    }

    Cell *raw = cell.get();
    m_storage.push_back(std::move(cell));

    if (raw->parentId != QLatin1String("0"))
    {
        if (Cell *parent = m_index.value(raw->parentId, nullptr))
        {
            raw->parent = parent;
            parent->children.append(raw);
        }
    }

    m_index.insert(raw->id, raw);
    m_order.append(raw);

    return raw;
}

Cell *Page::addVertex(const QRectF &rect, const StyleMap &style,
                      const QString &value, const QString &parentId)
{
    return addVertexInternal(rect, style, value,
                             parentId.isEmpty() ? activeLayerId : parentId);
}

Cell *Page::addVertexInternal(const QRectF &rect, const StyleMap &style,
                              const QString &value, const QString &parentId)
{
    auto cell = std::make_unique<Cell>(Cell::Kind::Vertex, newId());
    cell->geometry = rect;
    cell->style = style;
    cell->value = value;
    cell->parentId = parentId;

    return addCell(std::move(cell));
}

Cell *Page::addEdge(const QString &sourceId, const QString &targetId,
                    const StyleMap &style, const QString &value)
{
    auto cell = std::make_unique<Cell>(Cell::Kind::Edge, newId());
    cell->sourceId = sourceId;
    cell->targetId = targetId;
    cell->style = style;
    cell->value = value;
    cell->parentId = activeLayerId;

    return addCell(std::move(cell));
}

void Page::collectRemoval(Cell *cell, QSet<QString> &removedIds, QVector<Cell *> &ordered)
{
    if (cell == nullptr || removedIds.contains(cell->id))
    {
        return;
    }

    removedIds.insert(cell->id);
    ordered.append(cell);

    const QVector<Cell *> snapshot = cell->children;

    for (Cell *child : snapshot)
    {
        collectRemoval(child, removedIds, ordered);
    }
}

int Page::removeCell(Cell *cell)
{
    if (cell == nullptr || !m_index.contains(cell->id))
    {
        return 0;
    }

    QSet<QString> removedIds;
    QVector<Cell *> ordered;
    collectRemoval(cell, removedIds, ordered);

    // Edges attached to a removed vertex go away with it, exactly like the
    // draw.io editor removes the incident edges of a deleted shape.
    for (Cell *candidate : m_order)
    {
        if (candidate->isEdge() && !removedIds.contains(candidate->id)
            && (removedIds.contains(candidate->sourceId)
                || removedIds.contains(candidate->targetId)))
        {
            collectRemoval(candidate, removedIds, ordered);
        }
    }

    for (Cell *removed : ordered)
    {
        if (removed->parent != nullptr)
        {
            removed->parent->children.removeAll(removed);
        }

        m_index.remove(removed->id);
        m_order.removeAll(removed);
    }

    // Destroy the owned cells after the indexes no longer point at them.
    m_storage.erase(std::remove_if(m_storage.begin(), m_storage.end(),
                                   [&removedIds](const std::unique_ptr<Cell> &item)
                                   {
                                       return removedIds.contains(item->id);
                                   }),
                    m_storage.end());

    return ordered.size();
}

void Page::clear()
{
    m_storage.clear();
    m_order.clear();
    m_index.clear();
    m_idCounter = 1;
}

void Page::reindex()
{
    m_index.clear();

    for (auto &cell : m_storage)
    {
        cell->page = this;
        cell->parent = nullptr;
        cell->children.clear();
    }

    m_order.clear();

    for (auto &cell : m_storage)
    {
        m_index.insert(cell->id, cell.get());
        m_order.append(cell.get());
    }

    for (auto &cell : m_storage)
    {
        if (cell->parentId == QLatin1String("0"))
        {
            continue;
        }

        if (Cell *parent = m_index.value(cell->parentId, nullptr))
        {
            cell->parent = parent;
            parent->children.append(cell.get());
        }
    }
}

QRectF Page::pageRect() const
{
    if (!settings.page)
    {
        return contentBounds();
    }

    return QRectF(0, 0, settings.pageWidth, settings.pageHeight);
}

QRectF Page::contentBounds() const
{
    QRectF result;
    bool first = true;

    for (Cell *cell : m_order)
    {
        if (cell->isEdge())
        {
            continue;
        }

        const Layer *cellLayer = layer(cell->parentId);
        if (cellLayer != nullptr && !cellLayer->visible)
        {
            continue;
        }

        QRectF rect = cell->absoluteRect();
        const double stroke = cell->styleNumber(QStringLiteral("strokeWidth"), 1.0);
        const double margin = std::max(stroke, 1.0) / 2.0 + (cell->styleFlag(QStringLiteral("shadow")) ? 3.0 : 0.0);
        rect = rect.adjusted(-margin, -margin, margin, margin);

        result = first ? rect : result.united(rect);
        first = false;
    }

    return result;
}

QRectF Page::boundsForCells(const QVector<Cell *> &cells) const
{
    QRectF result;
    bool first = true;

    for (Cell *cell : cells)
    {
        if (cell == nullptr)
        {
            continue;
        }

        const QRectF rect = cell->absoluteRect();
        result = first ? rect : result.united(rect);
        first = false;
    }

    return result;
}

Layer *Page::layer(const QString &layerId)
{
    for (Layer &entry : layers)
    {
        if (entry.id == layerId)
        {
            return &entry;
        }
    }

    return nullptr;
}

const Layer *Page::layer(const QString &layerId) const
{
    for (const Layer &entry : layers)
    {
        if (entry.id == layerId)
        {
            return &entry;
        }
    }

    return nullptr;
}

} // namespace dq
