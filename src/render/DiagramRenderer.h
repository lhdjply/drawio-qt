#pragma once

#include "core/Page.h"

#include <QColor>
#include <QRectF>

class QPainter;

namespace dq {

struct RenderOptions
{
    bool grid = true;
    bool page = true;
    bool dark = false;
    bool antialias = true;
    bool clipToPage = false;
    QColor background = QColor(Qt::transparent);
};

/// Paints a page into a QPainter using page coordinates. The view applies its
/// own zoom/pan transform before calling this, and the exporters paint onto an
/// image/PDF/SVG device with a matching transform, so one renderer drives the
/// canvas, the thumbnails, print output and file export.
namespace DiagramRenderer {

void paint(QPainter &painter, const Page &page, const RenderOptions &options = {});

/// Bounds of everything drawn on the page, including stroke widths, arrow
/// heads, labels and shadows. Empty when the page has no cells.
QRectF contentBounds(const Page &page);

/// Bounds including the page rectangle when page view is enabled.
QRectF diagramBounds(const Page &page, const RenderOptions &options = {});

void paintGrid(QPainter &painter, const QRectF &area, int gridSize, const QColor &color);
void paintPageSheet(QPainter &painter, const Page &page, bool dark);

QColor canvasColor(bool dark);
QColor pageColor(bool dark);
QColor gridColor(bool dark);

} // namespace DiagramRenderer
} // namespace dq
