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
QPainterPath pathFor(const RoutedEdge &route, const Cell &edge);

/// Axis aligned bounding box of a routed edge, including arrow heads.
QRectF boundsFor(const RoutedEdge &route, const Cell &edge);

} // namespace EdgeRenderer
} // namespace dq
