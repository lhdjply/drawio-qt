#pragma once

#include "core/Cell.h"

#include <QPointF>
#include <QVector>

namespace dq {

class Page;

/// Result of routing one edge: absolute page coordinates plus the directions
/// used to leave the source and enter the target, which drive the arrow heads.
struct RoutedEdge
{
    QVector<QPointF> points;
    QPointF sourceDirection;
    QPointF targetDirection;
    const Cell *sourceCell = nullptr;
    const Cell *targetCell = nullptr;

    QPointF start() const { return points.isEmpty() ? QPointF() : points.first(); }
    QPointF end() const { return points.isEmpty() ? QPointF() : points.last(); }
    double length() const;
    QPointF pointAt(double fraction) const;
};

namespace EdgeRouter {

/// Connects an edge to work out its polyline. Handles free points, fixed
/// exit/entry constraints (`exitX`, `exitY`, `entryX`, `entryY`) and the
/// routing styles used by draw.io: orthogonal (default), elbow, entity
/// relation and straight/segment.
RoutedEdge route(const Page &page, const Cell &edge);

/// Intersection of the ray from the cell centre towards `toward` with the
/// cell outline. `direction` receives the outward normal used at that point.
QPointF connectionPoint(const Cell &cell, const QPointF &toward,
                        QPointF *direction = nullptr);

/// Straightens a polyline: removes duplicate and collinear points.
QVector<QPointF> simplify(const QVector<QPointF> &points);

} // namespace EdgeRouter
} // namespace dq
