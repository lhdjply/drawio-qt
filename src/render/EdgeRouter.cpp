#include "render/EdgeRouter.h"

#include "core/Page.h"
#include "render/ShapeRegistry.h"

#include <QLineF>
#include <QPainterPath>
#include <QPolygonF>
#include <QtMath>

#include <algorithm>
#include <limits>

namespace dq {

double RoutedEdge::length() const
{
    double total = 0.0;

    for (int i = 1; i < points.size(); ++i)
    {
        total += QLineF(points.at(i - 1), points.at(i)).length();
    }

    return total;
}

QPointF RoutedEdge::pointAt(double fraction) const
{
    if (points.isEmpty())
    {
        return {};
    }

    if (points.size() == 1)
    {
        return points.first();
    }

    const double target = qBound(0.0, fraction, 1.0) * length();
    double walked = 0.0;

    for (int i = 1; i < points.size(); ++i)
    {
        const QPointF a = points.at(i - 1);
        const QPointF b = points.at(i);
        const double segment = QLineF(a, b).length();

        if (walked + segment >= target && segment > 0.0)
        {
            const double t = (target - walked) / segment;
            return a + (b - a) * t;
        }

        walked += segment;
    }

    return points.last();
}

namespace EdgeRouter {
namespace {

QPointF axisDirection(const QPointF &vector)
{
    if (qAbs(vector.x()) >= qAbs(vector.y()))
    {
        return QPointF(vector.x() >= 0 ? 1.0 : -1.0, 0.0);
    }

    return QPointF(0.0, vector.y() >= 0 ? 1.0 : -1.0);
}

QPointF perimeterPoint(const Cell &cell, double exitX, double exitY)
{
    const QRectF rect = cell.absoluteRect();
    return QPointF(rect.left() + rect.width() * exitX,
                   rect.top() + rect.height() * exitY);
}

QPointF shapeIntersection(const Cell &cell, const QPointF &toward, QPointF *direction)
{
    const QRectF rect = cell.absoluteRect();
    const QPointF center = rect.center();

    if (rect.width() <= 0.0 || rect.height() <= 0.0)
    {
        if (direction != nullptr)
        {
            *direction = axisDirection(toward - center);
        }
        return center;
    }

    const QPointF localToward = toward - rect.topLeft();
    QPainterPath path = ShapeRegistry::pathFor(cell);
    const QPolygonF polygon = path.toFillPolygon();

    QPointF best = center;
    double bestDistance = std::numeric_limits<double>::max();

    if (polygon.size() >= 2)
    {
        const QPointF localCenter = rect.center() - rect.topLeft();
        QLineF ray(localCenter, localToward);

        for (int i = 0; i < polygon.size(); ++i)
        {
            const QPointF a = polygon.at(i);
            const QPointF b = polygon.at((i + 1) % polygon.size());
            QLineF segment(a, b);
            QPointF intersection;

            if (ray.intersects(segment, &intersection) == QLineF::BoundedIntersection)
            {
                const double distance = QLineF(localCenter, intersection).length();

                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = intersection + rect.topLeft();
                }
            }
        }

        if (bestDistance == std::numeric_limits<double>::max())
        {
            // Fall back to the bounding box intersection.
            const double angle = std::atan2(localToward.y() - localCenter.y(),
                                            localToward.x() - localCenter.x());
            const double dx = std::cos(angle);
            const double dy = std::sin(angle);
            const double halfW = rect.width() / 2.0;
            const double halfH = rect.height() / 2.0;
            const double scale = 1.0 / std::max(qAbs(dx) / std::max(halfW, 0.001),
                                                qAbs(dy) / std::max(halfH, 0.001));
            best = center + QPointF(dx, dy) * scale;
        }
    }

    if (direction != nullptr)
    {
        *direction = axisDirection(best - center);
    }

    return best;
}

/// Builds a Manhattan polyline between two anchors, honouring their exit and
/// entry directions. This mirrors draw.io's orthogonalEdgeStyle output for the
/// common cases (same or perpendicular sides).
QVector<QPointF> orthogonalBetween(const QPointF &start, const QPointF &startDir,
                                   const QPointF &end, const QPointF &endDir)
{
    QVector<QPointF> points;
    points.append(start);

    const bool startHorizontal = qAbs(startDir.x()) > qAbs(startDir.y());
    const bool endHorizontal = qAbs(endDir.x()) > qAbs(endDir.y());

    const double epsilon = 0.5;

    if (startHorizontal && endHorizontal)
    {
        const double midX = (start.x() + end.x()) / 2.0;

        if (qAbs(start.y() - end.y()) > epsilon)
        {
            points.append(QPointF(midX, start.y()));
            points.append(QPointF(midX, end.y()));
        }
    }
    else if (!startHorizontal && !endHorizontal)
    {
        const double midY = (start.y() + end.y()) / 2.0;

        if (qAbs(start.x() - end.x()) > epsilon)
        {
            points.append(QPointF(start.x(), midY));
            points.append(QPointF(end.x(), midY));
        }
    }
    else if (startHorizontal && !endHorizontal)
    {
        points.append(QPointF(end.x(), start.y()));
    }
    else
    {
        points.append(QPointF(start.x(), end.y()));
    }

    points.append(end);
    return points;
}

} // namespace

QPointF connectionPoint(const Cell &cell, const QPointF &toward, QPointF *direction)
{
    return shapeIntersection(cell, toward, direction);
}

QVector<QPointF> simplify(const QVector<QPointF> &points)
{
    QVector<QPointF> result;

    for (const QPointF &point : points)
    {
        if (result.isEmpty() || QLineF(result.last(), point).length() > 0.01)
        {
            result.append(point);
        }
    }

    if (result.size() < 3)
    {
        return result;
    }

    QVector<QPointF> simplified;
    simplified.append(result.first());

    for (int i = 1; i < result.size() - 1; ++i)
    {
        const QPointF &previous = simplified.last();
        const QPointF &current = result.at(i);
        const QPointF &next = result.at(i + 1);

        // Only drop points that continue in the same direction; a point where
        // the polyline reverses (a waypoint fed through a detour) is required.
        const bool collinearHorizontal = qAbs(previous.y() - current.y()) < 0.01
            && qAbs(current.y() - next.y()) < 0.01
            && (current.x() - previous.x()) * (next.x() - current.x()) >= 0.0;
        const bool collinearVertical = qAbs(previous.x() - current.x()) < 0.01
            && qAbs(current.x() - next.x()) < 0.01
            && (current.y() - previous.y()) * (next.y() - current.y()) >= 0.0;

        if (!collinearHorizontal && !collinearVertical)
        {
            simplified.append(current);
        }
    }

    simplified.append(result.last());
    return simplified;
}

RoutedEdge route(const Page &page, const Cell &edge)
{
    RoutedEdge result;
    result.sourceCell = page.cell(edge.sourceId);
    result.targetCell = page.cell(edge.targetId);

    QPointF sourceDir(0, -1);
    QPointF targetDir(0, 1);
    QPointF start;
    QPointF end;

    QPointF targetReference = edge.targetPoint;
    QPointF sourceReference = edge.sourcePoint;

    if (result.targetCell != nullptr)
    {
        targetReference = result.targetCell->center();
    }

    if (result.sourceCell != nullptr)
    {
        sourceReference = result.sourceCell->center();
    }

    const QString exitX = edge.styleValue(QStringLiteral("exitX"));
    const QString exitY = edge.styleValue(QStringLiteral("exitY"));
    const QString entryX = edge.styleValue(QStringLiteral("entryX"));
    const QString entryY = edge.styleValue(QStringLiteral("entryY"));

    if (result.sourceCell != nullptr)
    {
        if (!exitX.isEmpty() && !exitY.isEmpty())
        {
            start = perimeterPoint(*result.sourceCell, exitX.toDouble(), exitY.toDouble());
            sourceDir = axisDirection(start - result.sourceCell->center());
        }
        else
        {
            start = connectionPoint(*result.sourceCell, targetReference, &sourceDir);
        }
    }
    else
    {
        start = edge.sourcePoint;
        sourceDir = axisDirection(targetReference - start);
    }

    if (result.targetCell != nullptr)
    {
        if (!entryX.isEmpty() && !entryY.isEmpty())
        {
            end = perimeterPoint(*result.targetCell, entryX.toDouble(), entryY.toDouble());
            targetDir = axisDirection(result.targetCell->center() - end);
        }
        else
        {
            end = connectionPoint(*result.targetCell, sourceReference, &targetDir);
        }
    }
    else
    {
        end = edge.targetPoint;
        targetDir = axisDirection(end - sourceReference);
    }

    result.sourceDirection = sourceDir;
    result.targetDirection = targetDir;

    QVector<QPointF> points;
    points.append(start);

    const QVector<QPointF> waypoints = edge.waypoints;

    if (waypoints.isEmpty())
    {
        const QString edgeStyle = edge.styleValue(QStringLiteral("edgeStyle")).toLower();
        const bool orthogonal = edgeStyle == QLatin1String("orthogonaledgestyle")
            || edgeStyle == QLatin1String("elbowedgestyle")
            || edgeStyle == QLatin1String("entityrelationedgestyle")
            || edgeStyle.isEmpty();

        if (orthogonal)
        {
            points = orthogonalBetween(start, sourceDir, end, targetDir);
        }
        else
        {
            points.append(end);
        }
    }
    else
    {
        bool horizontal = qAbs(sourceDir.x()) > qAbs(sourceDir.y());
        QPointF current = start;

        for (const QPointF &waypoint : waypoints)
        {
            if (horizontal)
            {
                points.append(QPointF(waypoint.x(), current.y()));
                current = QPointF(waypoint.x(), current.y());
            }
            else
            {
                points.append(QPointF(current.x(), waypoint.y()));
                current = QPointF(current.x(), waypoint.y());
            }

            points.append(waypoint);
            current = waypoint;
            horizontal = !horizontal;
        }

        const bool endHorizontal = qAbs(targetDir.x()) > qAbs(targetDir.y());

        if (endHorizontal)
        {
            points.append(QPointF(current.x(), end.y()));
        }
        else
        {
            points.append(QPointF(end.x(), current.y()));
        }

        points.append(end);
    }

    result.points = simplify(points);

    if (result.points.size() < 2)
    {
        result.points = {start, end};
    }

    return result;
}

} // namespace EdgeRouter
} // namespace dq
