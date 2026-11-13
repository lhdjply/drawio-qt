#include "render/EdgeRouter.h"

#include "core/Page.h"
#include "render/ShapeRegistry.h"

#include <QLineF>
#include <QPainterPath>
#include <QPolygonF>
#include <QtMath>
#include <QTransform>

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
    QPointF point(rect.left() + rect.width() * exitX,
                  rect.top() + rect.height() * exitY);
    const double angle = cell.effectiveRotation();

    if (!qFuzzyIsNull(angle))
    {
        QTransform transform;
        transform.translate(rect.center().x(), rect.center().y());
        transform.rotate(angle);
        transform.translate(-rect.center().x(), -rect.center().y());
        point = transform.map(point);
    }

    return point;
}

/// shape=partialRectangle paints only the sides selected by top/right/bottom/
/// left. draw.io connects to the painted line (the mindmap's Branch cells are
/// a single bottom line), so the terminal is the point of that line nearest to
/// the other end of the edge.
bool partialRectanglePoint(const Cell &cell, const QPointF &toward, QPointF *point)
{
    if (ShapeRegistry::canonicalKey(cell.style) != QLatin1String("partialRectangle"))
    {
        return false;
    }

    const QRectF rect = cell.absoluteRect();
    const StyleMap style = Style::withDefaults(cell.style);
    const auto flag = [&style](const char *key)
    {
        return Style::number(style, QString::fromLatin1(key), 1.0) != 0.0;
    };

    QVector<QLineF> segments;

    if (flag("top"))
    {
        segments.append(QLineF(rect.topLeft(), rect.topRight()));
    }

    if (flag("bottom"))
    {
        segments.append(QLineF(rect.bottomLeft(), rect.bottomRight()));
    }

    if (flag("left"))
    {
        segments.append(QLineF(rect.topLeft(), rect.bottomLeft()));
    }

    if (flag("right"))
    {
        segments.append(QLineF(rect.topRight(), rect.bottomRight()));
    }

    QPointF best;
    double bestDistance = std::numeric_limits<double>::max();

    for (const QLineF &segment : segments)
    {
        const QPointF delta = segment.p2() - segment.p1();
        const double lengthSquared = delta.x() * delta.x() + delta.y() * delta.y();
        double t = 0.0;

        if (lengthSquared > 0.0)
        {
            t = ((toward.x() - segment.p1().x()) * delta.x()
                 + (toward.y() - segment.p1().y()) * delta.y()) / lengthSquared;
            t = qBound(0.0, t, 1.0);
        }

        const QPointF candidate = segment.p1() + delta * t;
        const double distance = QLineF(candidate, toward).length();

        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = candidate;
        }
    }

    if (bestDistance == std::numeric_limits<double>::max())
    {
        return false;
    }

    *point = best;
    return true;
}

QPointF shapeIntersection(const Cell &cell, const QPointF &toward, QPointF *direction)
{
    const QRectF rect = cell.visualRect();
    const QPointF center = rect.center();

    QPointF linePoint;

    if (partialRectanglePoint(cell, toward, &linePoint))
    {
        if (direction != nullptr)
        {
            *direction = axisDirection(toward - linePoint);
        }

        return linePoint;
    }

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

/// mxEdgeStyle.IsometricConnector: the route follows the two isometric base
/// directions (30 degrees above the horizontal). With elbow=horizontal the
/// first segment is horizontal, otherwise vertical.
QVector<QPointF> isometricRoute(const QPointF &start, const QPointF &end,
                                bool horizontalElbow)
{
    const QPointF a(std::cos(qDegreesToRadians(-30.0)),
                    std::sin(qDegreesToRadians(-30.0)));
    const QPointF b(std::cos(qDegreesToRadians(-150.0)),
                    std::sin(qDegreesToRadians(-150.0)));
    const double a1 = a.x();
    const double a2 = a.y();
    const double b1 = b.x();
    const double b2 = b.y();
    const double denominator = a1 * b2 - a2 * b1;

    QVector<QPointF> points;
    points.append(start);
    QPointF last = start;

    const auto isoLineTo = [&](const QPointF &target, bool ignoreFirst)
    {
        const double c1 = target.x() - last.x();
        const double c2 = target.y() - last.y();

        if (qFuzzyIsNull(denominator))
        {
            return;
        }

        const double h = (b2 * c1 - b1 * c2) / denominator;
        const double v = (a2 * c1 - a1 * c2) / (a2 * b1 - a1 * b2);

        if (horizontalElbow)
        {
            if (ignoreFirst)
            {
                last = QPointF(last.x() + a1 * h, last.y() + a2 * h);
                points.append(last);
            }

            last = QPointF(last.x() + b1 * v, last.y() + b2 * v);
            points.append(last);
        }
        else
        {
            if (ignoreFirst)
            {
                last = QPointF(last.x() + b1 * v, last.y() + b2 * v);
                points.append(last);
            }

            last = QPointF(last.x() + a1 * h, last.y() + a2 * h);
            points.append(last);
        }
    };

    isoLineTo(QPointF((start.x() + end.x()) / 2.0, (start.y() + end.y()) / 2.0), true);
    isoLineTo(end, false);

    if (points.isEmpty() || QLineF(points.last(), end).length() > 0.001)
    {
        points.append(end);
    }

    return points;
}

/// mxEdgeStyle.ElbowConnector for floating terminals: SideToSide (horizontal)
/// or TopToBottom (vertical) with the bend in the middle.
QVector<QPointF> elbowRoute(const QPointF &start, const QPointF &end, bool horizontal)
{
    QVector<QPointF> points;
    points.append(start);

    if (horizontal)
    {
        const double x = qRound((start.x() + end.x()) / 2.0);
        points.append(QPointF(x, start.y()));
        points.append(QPointF(x, end.y()));
    }
    else
    {
        const double y = qRound((start.y() + end.y()) / 2.0);
        points.append(QPointF(start.x(), y));
        points.append(QPointF(end.x(), y));
    }

    points.append(end);
    return simplify(points);
}

/// mxEdgeStyle.EntityRelation: a horizontal S-curve between the facing sides
/// of the two terminal cells with a `segment` long stub on each side.
QVector<QPointF> entityRelationRoute(const QRectF &source, const QRectF &target,
                                     double segment)
{
    const bool sourceLeft = target.right() < source.left();
    const bool targetLeft = source.right() < target.left();

    const double x0 = sourceLeft ? source.left() : source.right();
    const double y0 = source.center().y();
    const double xe = targetLeft ? target.left() : target.right();
    const double ye = target.center().y();

    const QPointF dep(x0 + (sourceLeft ? -segment : segment), y0);
    const QPointF arr(xe + (targetLeft ? -segment : segment), ye);

    QVector<QPointF> points;

    if (sourceLeft == targetLeft)
    {
        const double x = sourceLeft ? qMin(x0, xe) - segment : qMax(x0, xe) + segment;
        points.append(QPointF(x, y0));
        points.append(QPointF(x, ye));
    }
    else if ((dep.x() < arr.x()) == sourceLeft)
    {
        const double midY = y0 + (ye - y0) / 2.0;
        points.append(dep);
        points.append(QPointF(dep.x(), midY));
        points.append(QPointF(arr.x(), midY));
        points.append(arr);
    }
    else
    {
        points.append(dep);
        points.append(arr);
    }

    return points;
}

/// The port constraint of an edge (mxConstants.STYLE_SOURCE_PORT_CONSTRAINT /
/// STYLE_TARGET_PORT_CONSTRAINT) pins the terminal to one axis: eastwest
/// leaves and enters through the left or right side, northsouth through the
/// top or bottom side. Returns the side point and its outward direction.
bool portConstraintPoint(const Cell &cell, const QString &constraint,
                         const QPointF &toward, QPointF *point, QPointF *direction)
{
    const QRectF rect = ShapeRegistry::visibleRect(cell);

    if (constraint == QLatin1String("eastwest"))
    {
        if (toward.x() < rect.center().x())
        {
            *point = QPointF(rect.left(), rect.center().y());
            *direction = QPointF(-1, 0);
        }
        else
        {
            *point = QPointF(rect.right(), rect.center().y());
            *direction = QPointF(1, 0);
        }

        return true;
    }

    if (constraint == QLatin1String("northsouth"))
    {
        if (toward.y() < rect.center().y())
        {
            *point = QPointF(rect.center().x(), rect.top());
            *direction = QPointF(0, -1);
        }
        else
        {
            *point = QPointF(rect.center().x(), rect.bottom());
            *direction = QPointF(0, 1);
        }

        return true;
    }

    return false;
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

    const QString sourceConstraint = edge.styleValue(
        QStringLiteral("sourcePortConstraint"));
    const QString targetConstraint = edge.styleValue(
        QStringLiteral("targetPortConstraint"));

    if (result.sourceCell != nullptr && !sourceConstraint.isEmpty()
        && portConstraintPoint(*result.sourceCell, sourceConstraint,
                               edge.targetPoint.isNull() ? result.targetCell != nullptr
                                                               ? result.targetCell->center()
                                                               : QPointF()
                                                         : edge.targetPoint,
                               &start, &sourceDir))
    {
        // Fixed side port: skip the perimeter intersection below.
    }
    else if (result.sourceCell != nullptr)
    {
        if (!exitX.isEmpty() && !exitY.isEmpty())
        {
            start = perimeterPoint(*result.sourceCell, exitX.toDouble(), exitY.toDouble());
            sourceDir = axisDirection(start - result.sourceCell->center());

            // Explicit side constraints dominate: exitX=1 means the wire leaves
            // to the right even when the cell itself is rotated.
            const double ex = exitX.toDouble();
            const double ey = exitY.toDouble();

            if (qFuzzyCompare(ex + 1.0, 1.0))
            {
                sourceDir = QPointF(-1, 0);
            }
            else if (qFuzzyCompare(ex, 1.0))
            {
                sourceDir = QPointF(1, 0);
            }
            else if (qFuzzyCompare(ey + 1.0, 1.0))
            {
                sourceDir = QPointF(0, -1);
            }
            else if (qFuzzyCompare(ey, 1.0))
            {
                sourceDir = QPointF(0, 1);
            }
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

    if (result.targetCell != nullptr && !targetConstraint.isEmpty()
        && portConstraintPoint(*result.targetCell, targetConstraint,
                               edge.sourcePoint.isNull() ? result.sourceCell != nullptr
                                                               ? result.sourceCell->center()
                                                               : QPointF()
                                                         : edge.sourcePoint,
                               &end, &targetDir))
    {
        // Fixed side port.
    }
    else if (result.targetCell != nullptr)
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

    const QString edgeStyle = edge.styleValue(QStringLiteral("edgeStyle")).toLower();
    const bool elbowStyle = edgeStyle == QLatin1String("elbowedgestyle");
    const bool orthogonalStyle = edgeStyle == QLatin1String("orthogonaledgestyle");

    // mxEdgeStyle.ElbowConnector / OrthConnector connect at the centre of the
    // side the wire leaves from (getRoutingCenterX/Y), not at the perimeter
    // intersection: a vertical elbow enters the target at its top centre.
    if ((elbowStyle || orthogonalStyle)
        && exitX.isEmpty() && exitY.isEmpty()
        && entryX.isEmpty() && entryY.isEmpty()
        && sourceConstraint.isEmpty() && targetConstraint.isEmpty())
    {
        const QRectF sourceRect = result.sourceCell != nullptr
            ? ShapeRegistry::visibleRect(*result.sourceCell) : QRectF();
        const QRectF targetRect = result.targetCell != nullptr
            ? ShapeRegistry::visibleRect(*result.targetCell) : QRectF();

        const QPointF sourceCenter = result.sourceCell != nullptr
            ? sourceRect.center() : start;
        const QPointF targetCenter = result.targetCell != nullptr
            ? targetRect.center() : end;

        bool vertical = elbowStyle
            ? edge.styleValue(QStringLiteral("elbow")) != QLatin1String("horizontal")
            : qAbs(targetCenter.y() - sourceCenter.y()) >= qAbs(targetCenter.x() - sourceCenter.x());

        if (vertical)
        {
            if (result.sourceCell != nullptr)
            {
                start = QPointF(sourceCenter.x(), targetCenter.y() >= sourceCenter.y()
                                                      ? sourceRect.bottom() : sourceRect.top());
                sourceDir = QPointF(0, targetCenter.y() >= sourceCenter.y() ? 1 : -1);
            }

            if (result.targetCell != nullptr)
            {
                end = QPointF(targetCenter.x(), sourceCenter.y() <= targetCenter.y()
                                                  ? targetRect.top() : targetRect.bottom());
                targetDir = QPointF(0, sourceCenter.y() <= targetCenter.y() ? -1 : 1);
            }
        }
        else
        {
            if (result.sourceCell != nullptr)
            {
                start = QPointF(targetCenter.x() >= sourceCenter.x()
                                    ? sourceRect.right() : sourceRect.left(), sourceCenter.y());
                sourceDir = QPointF(targetCenter.x() >= sourceCenter.x() ? 1 : -1, 0);
            }

            if (result.targetCell != nullptr)
            {
                end = QPointF(sourceCenter.x() <= targetCenter.x()
                                  ? targetRect.left() : targetRect.right(), targetCenter.y());
                targetDir = QPointF(sourceCenter.x() <= targetCenter.x() ? -1 : 1, 0);
            }
        }
    }

    result.sourceDirection = sourceDir;
    result.targetDirection = targetDir;

    const bool floating = result.sourceCell == nullptr && result.targetCell == nullptr;
    const bool routedStyle = !edgeStyle.isEmpty()
        && edgeStyle != QLatin1String("none");

    // Free floating edges without a routing style (or with explicit
    // waypoints) already store the exact polyline. Using it verbatim keeps
    // the wire lengths identical to draw.io.
    if (floating && (!routedStyle || !edge.waypoints.isEmpty()))
    {
        QVector<QPointF> given;

        if (!edge.sourcePoint.isNull())
        {
            given.append(edge.sourcePoint);
        }

        given.append(edge.waypoints);

        if (!edge.targetPoint.isNull())
        {
            given.append(edge.targetPoint);
        }

        if (given.size() >= 2)
        {
            result.points = simplify(given);

            if (result.points.size() >= 2)
            {
                return result;
            }
        }
    }

    const QVector<QPointF> waypoints = edge.waypoints;

    // mxEdgeStyle.EntityRelation routes between the facing sides of the two
    // terminals (the mindmap's curved connectors).
    if (waypoints.isEmpty() && edgeStyle == QLatin1String("entityrelationedgestyle")
        && result.sourceCell != nullptr && result.targetCell != nullptr)
    {
        const double segment = Style::number(Style::withDefaults(edge.style),
                                             QStringLiteral("segment"), 10.0);
        const QRectF sourceRect = ShapeRegistry::visibleRect(*result.sourceCell);
        const QRectF targetRect = ShapeRegistry::visibleRect(*result.targetCell);
        const QVector<QPointF> middle = entityRelationRoute(sourceRect, targetRect, segment);

        start = connectionPoint(*result.sourceCell,
                                middle.isEmpty() ? targetRect.center() : middle.first(),
                                &sourceDir);
        end = connectionPoint(*result.targetCell,
                              middle.isEmpty() ? sourceRect.center() : middle.last(),
                              &targetDir);

        QVector<QPointF> routed{start};
        routed.append(middle);
        routed.append(end);

        result.sourceDirection = sourceDir;
        result.targetDirection = targetDir;
        result.points = simplify(routed);

        if (result.points.size() >= 2)
        {
            return result;
        }
    }

    QVector<QPointF> points;
    points.append(start);

    if (waypoints.isEmpty())
    {
        // draw.io routes an edge without an edgeStyle as a straight polyline
        // (mxEdgeStyle.Straight); only the explicit styles bend the wire.
        const bool orthogonal = edgeStyle == QLatin1String("orthogonaledgestyle")
            || edgeStyle == QLatin1String("entityrelationedgestyle");

        // draw.io routes an edge whose other end is a floating point with the
        // same edge style: only the connection point of the terminal cell is
        // fixed, the direction comes from the style (or from the relative
        // position of the two anchors).
        const bool oneTerminal = (result.sourceCell == nullptr)
            != (result.targetCell == nullptr);

        if (floating && edgeStyle == QLatin1String("isometricedgestyle"))
        {
            points = isometricRoute(start, end,
                edge.styleValue(QStringLiteral("elbow")) != QLatin1String("vertical"));
        }
        else if (edgeStyle == QLatin1String("elbowedgestyle")
                 && (floating || oneTerminal || (result.sourceCell != nullptr
                                                 && result.targetCell != nullptr)))
        {
            points = elbowRoute(start, end,
                edge.styleValue(QStringLiteral("elbow")) != QLatin1String("vertical"));
        }
        else if (floating && (edgeStyle == QLatin1String("segmentedgestyle")
                              || orthogonal))
        {
            // Graph.isOrthogonal() includes SegmentConnector: for two fixed
            // points the route bends at the target x and source y.
            const double dx = qAbs(end.x() - start.x());
            const double dy = qAbs(end.y() - start.y());
            QVector<QPointF> given{start};

            if (dx >= dy)
            {
                given.append(QPointF(end.x(), start.y()));
            }
            else
            {
                given.append(QPointF(start.x(), end.y()));
            }

            given.append(end);
            points = simplify(given);
        }
        else if (orthogonal && (result.sourceCell != nullptr
                                || result.targetCell != nullptr))
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
