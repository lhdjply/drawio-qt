#pragma once

#include "core/Cell.h"
#include "render/EdgeRouter.h"

#include <QPainterPath>
#include <QRectF>

namespace dq {

class Page;

/// Paints connectors: the routed polyline, rounded corners, arrow heads and
/// the edge label.
namespace EdgeRenderer {

void paint(QPainter &painter, const Page &page, const Cell &edge);

/// Repaints only the arrow heads, after vertices, so markers that land on a
/// shape border stay visible above its fill (draw.io does the same).
void paintMarkers(QPainter &painter, const Page &page, const Cell &edge);

/// Position of an edge label or symbol (a vertex whose parent is the edge).
QPointF childAnchor(const RoutedEdge &route, const Cell &child);
/// Bounds of an edge label or symbol, used by DiagramRenderer::contentBounds.
QRectF childBounds(const RoutedEdge &route, const Cell &child);
QPainterPath pathFor(const RoutedEdge &route, const Cell &edge);

/// Axis aligned bounding box of a routed edge, including arrow heads.
QRectF boundsFor(const RoutedEdge &route, const Cell &edge);

} // namespace EdgeRenderer
} // namespace dq
