#include "render/DiagramRenderer.h"

#include "render/ShapeRegistry.h"

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

QVector<Cell *> paintOrder(const Page &page)
{
    QHash<QString, QVector<Cell *>> byLayer;
    QVector<Cell *> orphans;

    for (Cell *cell : page.cells())
    {
        if (!cell->visible)
        {
            continue;
        }

        // A child whose ancestor is hidden or collapsed must not be painted.
        bool hiddenAncestor = false;

        for (Cell *parent = cell->parent; parent != nullptr; parent = parent->parent)
        {
            if (!parent->visible
                || (parent->collapsed
                    && parent->styleFlag(QStringLiteral("swimlane"), false)))
            {
                hiddenAncestor = true;
                break;
            }
        }

        if (hiddenAncestor)
        {
            continue;
        }

        const QString layerId = layerIdFor(page, cell);

        if (layerId.isEmpty())
        {
            orphans.append(cell);
        }
        else if (layerVisible(page, layerId))
        {
            byLayer[layerId].append(cell);
        }
    }

    QVector<Cell *> result;
    result.reserve(page.cells().size());

    // Paint bottom layer first; page.layers is stored bottom-to-top.
    for (const Layer &layer : page.layers)
    {
        result.append(byLayer.value(layer.id));
    }

    result.append(orphans);
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

QRectF DiagramRenderer::pageSheet(const Page &page)
{
    const double width = page.settings.pageWidth;
    const double height = page.settings.pageHeight;

    if (!page.settings.page || width <= 0.0 || height <= 0.0)
    {
        return {};
    }

    // One page per page-sized cell the content reaches into. Pages that hold
    // nothing are not drawn, so dragging the last shape from page 1 to page 2
    // leaves the second page only.
    const QRectF content = contentBounds(page);
    int firstColumn = 0;
    int lastColumn = 0;
    int firstRow = 0;
    int lastRow = 0;

    if (!content.isNull())
    {
        firstColumn = static_cast<int>(std::floor(content.left() / width));
        lastColumn = static_cast<int>(std::ceil(content.right() / width)) - 1;
        firstRow = static_cast<int>(std::floor(content.top() / height));
        lastRow = static_cast<int>(std::ceil(content.bottom() / height)) - 1;

        lastColumn = qMax(lastColumn, firstColumn);
        lastRow = qMax(lastRow, firstRow);

        // Guard against absurd coordinates in a broken file.
        const int limit = 50;
        firstColumn = qBound(-limit, firstColumn, limit);
        firstRow = qBound(-limit, firstRow, limit);
        lastColumn = qBound(firstColumn, lastColumn, firstColumn + limit);
        lastRow = qBound(firstRow, lastRow, firstRow + limit);
    }

    return QRectF(firstColumn * width, firstRow * height,
                  (lastColumn - firstColumn + 1) * width,
                  (lastRow - firstRow + 1) * height);
}

void DiagramRenderer::paintPageSheet(QPainter &painter, const Page &page, bool dark)
{
    const QRectF sheet = pageSheet(page);

    if (sheet.isEmpty())
    {
        return;
    }

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

    // Page breaks: dashed lines at every page boundary inside the sheet.
    QPen pageBreak(dark ? QColor(0x6A, 0x70, 0x78) : QColor(0xC0, 0xC0, 0xC0));
    pageBreak.setStyle(Qt::DashLine);
    pageBreak.setWidthF(1.0);
    painter.setPen(pageBreak);

    const double width = page.settings.pageWidth;
    const double height = page.settings.pageHeight;

    for (double x = sheet.left() + width; x < sheet.right() - 0.5; x += width)
    {
        painter.drawLine(QPointF(x, sheet.top()), QPointF(x, sheet.bottom()));
    }

    for (double y = sheet.top() + height; y < sheet.bottom() - 0.5; y += height)
    {
        painter.drawLine(QPointF(sheet.left(), y), QPointF(sheet.right(), y));
    }

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

        if (cell->isEdgeChild())
        {
            bounds = EdgeRenderer::childBounds(EdgeRouter::route(page, *cell->parent), *cell);
            // mxShape.augmentBoundingBox grows the box by half the stroke
            // width, not by the full width.
            const double stroke = qMax(1.0, cell->styleNumber(QStringLiteral("strokeWidth"), 1.0));
            bounds = bounds.adjusted(-stroke / 2.0, -stroke / 2.0, stroke / 2.0, stroke / 2.0);
        }
        else if (cell->isVertex())
        {
            // transparentBounds containers are painted at the bounds derived
            // from their children, not at their (empty) stored geometry.
            if (ShapeRegistry::hasTransparentBounds(*cell))
            {
                bounds = ShapeRegistry::visibleRect(*cell);

                // mxShape.augmentBoundingBox / getBoundingBox: a rotated shape
                // contributes its rotated bounding box.
                const double angle = cell->effectiveRotation();

                if (!qFuzzyIsNull(angle))
                {
                    QTransform transform;
                    transform.translate(bounds.center().x(), bounds.center().y());
                    transform.rotate(angle);
                    transform.translate(-bounds.center().x(), -bounds.center().y());
                    bounds = transform.mapRect(bounds);
                }
            }
            else
            {
                bounds = cell->visualRect();
            }

            if (bounds.isNull())
            {
                continue;
            }

            // Some draw.io shapes (callout tails, cloud bubbles) draw outside
            // their geometry box, so include the painted path in the bounds.
            const QRectF local = ShapeRegistry::pathFor(*cell).boundingRect();

            if (!local.isEmpty() && !ShapeRegistry::hasTransparentBounds(*cell))
            {
                bounds = bounds.united(local.translated(cell->absolutePosition()));
            }

            // mxShape.augmentBoundingBox grows the box by half the stroke
            // width, not by the full width.
            const double stroke = qMax(1.0, cell->styleNumber(QStringLiteral("strokeWidth"), 1.0));
            bounds = bounds.adjusted(-stroke / 2.0, -stroke / 2.0, stroke / 2.0, stroke / 2.0);

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
        const QRectF sheet = pageSheet(page);
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
            ? pageSheet(page)
            : diagramBounds(page, options);

        if (!gridArea.isEmpty())
        {
            paintGrid(painter, gridArea, page.settings.gridSize, gridColor(options.dark));
        }
    }

    if (options.clipToPage && page.settings.page)
    {
        painter.setClipRect(pageSheet(page));
    }

    const QVector<Cell *> cells = paintOrder(page);

    // Paint in document order so "bring to front" / "send to back" really
    // changes whether an edge is hidden behind a shape.
    for (Cell *cell : cells)
    {
        if (cell->isEdge())
        {
            EdgeRenderer::paint(painter, page, *cell);
        }
        else if (cell->isVertex() && !cell->isEdgeChild())
        {
            // Children of a container are clipped to it, so a collapsed
            // container hides its rows instead of spilling them below.
            painter.save();

            for (Cell *parent = cell->parent; parent != nullptr; parent = parent->parent)
            {
                if (parent->isVertex() && parent->styleFlag(QStringLiteral("swimlane"), false))
                {
                    painter.setClipRect(ShapeRegistry::visibleRect(*parent), Qt::IntersectClip);
                    break;
                }
            }

            // mxGraphView folds the geometry before painting, so the title,
            // fold icon and children clipping all use the folded size.
            const QRectF savedGeometry = cell->geometry;

            // A zero title size leaves nothing to fold to, so draw.io keeps
            // the container as it is.
            if (cell->collapsed && cell->styleFlag(QStringLiteral("swimlane"), false)
                && cell->styleNumber(QStringLiteral("startSize"), 23.0) > 0.0)
            {
                const double startSize = cell->styleNumber(QStringLiteral("startSize"), 23.0);

                if (cell->styleFlag(QStringLiteral("horizontal"), true))
                {
                    cell->geometry.setHeight(qMin(cell->geometry.height(), startSize));
                }
                else
                {
                    cell->geometry.setWidth(qMin(cell->geometry.width(), startSize));
                }
            }

            ShapeRenderer::paint(painter, *cell);
            cell->geometry = savedGeometry;
            painter.restore();
        }
    }

    painter.restore();
}

} // namespace dq
