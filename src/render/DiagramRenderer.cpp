#include "render/DiagramRenderer.h"

#include "render/EdgeRenderer.h"
#include "render/EdgeRouter.h"
#include "render/ShapeRenderer.h"

#include <QPainter>
#include <QtMath>

#include <algorithm>

namespace dq {
namespace {

bool layerVisible(const Page &page, const QString &layerId)
{
    const Layer *layer = page.layer(layerId);

    if (layer == nullptr)
    {
        // Cells parented to a group inherit visibility through the group; only
        // layer cells are looked up here.
        return true;
    }

    return layer->visible;
}

QVector<Cell *> paintOrder(const Page &page)
{
    QVector<Cell *> vertices;
    QVector<Cell *> edges;

    for (Cell *cell : page.cells())
    {
        if (!cell->visible)
        {
            continue;
        }

        // A child whose ancestor is hidden must not be painted.
        bool hiddenAncestor = false;

        for (Cell *parent = cell->parent; parent != nullptr; parent = parent->parent)
        {
            if (!parent->visible)
            {
                hiddenAncestor = true;
                break;
            }
        }

        if (hiddenAncestor)
        {
            continue;
        }

        if (cell->isVertex())
        {
            if (!layerVisible(page, cell->parentId) && page.layer(cell->parentId) != nullptr)
            {
                continue;
            }

            vertices.append(cell);
        }
        else
        {
            edges.append(cell);
        }
    }

    std::stable_sort(vertices.begin(), vertices.end(), [](Cell *a, Cell *b)
    {
        return a->depth() < b->depth();
    });

    QVector<Cell *> result;
    result.reserve(vertices.size() + edges.size());

    // Draw groups before their children, then edges above the shapes.
    for (Cell *cell : vertices)
    {
        result.append(cell);
    }

    for (Cell *cell : edges)
    {
        result.append(cell);
    }

    return result;
}

} // namespace

QColor DiagramRenderer::canvasColor(bool dark)
{
    return dark ? QColor(0x1B, 0x1D, 0x1E) : QColor(0xF1, 0xF3, 0xF4);
}

QColor DiagramRenderer::pageColor(bool dark)
{
    return dark ? QColor(0x24, 0x27, 0x2B) : QColor(Qt::white);
}

QColor DiagramRenderer::gridColor(bool dark)
{
    return dark ? QColor(0x3A, 0x3E, 0x43) : QColor(0xE3, 0xE6, 0xE8);
}

void DiagramRenderer::paintGrid(QPainter &painter, const QRectF &area, int gridSize,
                                const QColor &color)
{
    if (gridSize <= 0)
    {
        return;
    }

    painter.save();
    QPen pen(color);
    pen.setWidthF(0.5);
    painter.setPen(pen);

    const double left = std::floor(area.left() / gridSize) * gridSize;
    const double top = std::floor(area.top() / gridSize) * gridSize;

    for (double x = left; x <= area.right(); x += gridSize)
    {
        painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
    }

    for (double y = top; y <= area.bottom(); y += gridSize)
    {
        painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    }

    painter.restore();
}

void DiagramRenderer::paintPageSheet(QPainter &painter, const Page &page, bool dark)
{
    if (!page.settings.page)
    {
        return;
    }

    const QRectF sheet(0, 0, page.settings.pageWidth, page.settings.pageHeight);

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, dark ? 90 : 28));
    painter.drawRect(sheet.translated(4, 4));
    painter.setBrush(pageColor(dark));
    painter.drawRect(sheet);
    painter.setBrush(Qt::NoBrush);
    QPen border(dark ? QColor(0x4A, 0x4F, 0x55) : QColor(0xD0, 0xD4, 0xD8));
    border.setWidthF(0.6);
    painter.setPen(border);
    painter.drawRect(sheet);
    painter.restore();
}

QRectF DiagramRenderer::contentBounds(const Page &page)
{
    QRectF result;
    bool first = true;

    for (Cell *cell : page.cells())
    {
        if (!cell->visible)
        {
            continue;
        }

        QRectF bounds;

        if (cell->isVertex())
        {
            bounds = cell->absoluteRect();
            const double stroke = qMax(1.0, cell->styleNumber(QStringLiteral("strokeWidth"), 1.0));
            bounds = bounds.adjusted(-stroke, -stroke, stroke, stroke);

            if (cell->styleFlag(QStringLiteral("shadow")))
            {
                bounds = bounds.adjusted(0, 0, 4, 4);
            }
        }
        else
        {
            if (cell->sourceId.isEmpty() && cell->targetId.isEmpty()
                && !cell->sourcePoint.isNull() && !cell->targetPoint.isNull())
            {
                bounds = QRectF(cell->sourcePoint, cell->targetPoint).normalized();
            }
            else
            {
                bounds = EdgeRenderer::boundsFor(EdgeRouter::route(page, *cell), *cell);
            }
        }

        if (bounds.isNull() && !cell->isEdge())
        {
            continue;
        }

        result = first ? bounds : result.united(bounds);
        first = false;
    }

    return result;
}

QRectF DiagramRenderer::diagramBounds(const Page &page, const RenderOptions &options)
{
    QRectF bounds = contentBounds(page);

    if (options.page && page.settings.page)
    {
        const QRectF sheet(0, 0, page.settings.pageWidth, page.settings.pageHeight);
        bounds = bounds.isNull() ? sheet : bounds.united(sheet);
    }

    return bounds;
}

void DiagramRenderer::paint(QPainter &painter, const Page &page, const RenderOptions &options)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, options.antialias);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (options.background.alpha() > 0)
    {
        painter.fillRect(painter.clipBoundingRect().isEmpty()
                             ? QRectF(0, 0, page.settings.pageWidth, page.settings.pageHeight)
                             : painter.clipBoundingRect(),
                         options.background);
    }

    if (options.page && page.settings.page)
    {
        paintPageSheet(painter, page, options.dark);
    }

    if (options.grid && page.settings.grid)
    {
        const QRectF gridArea = options.page && page.settings.page
            ? QRectF(0, 0, page.settings.pageWidth, page.settings.pageHeight)
            : diagramBounds(page, options);

        if (!gridArea.isEmpty())
        {
            paintGrid(painter, gridArea, page.settings.gridSize, gridColor(options.dark));
        }
    }

    if (options.clipToPage && page.settings.page)
    {
        painter.setClipRect(QRectF(0, 0, page.settings.pageWidth, page.settings.pageHeight));
    }

    const QVector<Cell *> cells = paintOrder(page);

    for (Cell *cell : cells)
    {
        if (cell->isVertex())
        {
            ShapeRenderer::paint(painter, *cell);
        }
    }

    for (Cell *cell : cells)
    {
        if (cell->isEdge())
        {
            EdgeRenderer::paint(painter, page, *cell);
        }
    }

    painter.restore();
}

} // namespace dq
