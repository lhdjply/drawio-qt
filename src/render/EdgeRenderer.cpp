#include "render/EdgeRenderer.h"

#include "render/ShapeRenderer.h"

#include <QLineF>
#include <QPainter>
#include <QPolygonF>
#include <QtMath>

namespace dq {
namespace {

QColor edgeColor(const Cell &edge)
{
    return ShapeRenderer::strokeColor(edge);
}

QPen edgePen(const Cell &edge)
{
    QPen pen(edgeColor(edge));
    pen.setWidthF(qMax(0.5, edge.styleNumber(QStringLiteral("strokeWidth"), 1.0)));
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);

    if (edge.styleFlag(QStringLiteral("dashed")))
    {
        pen.setDashPattern({4.0, 3.0});
    }

    return pen;
}

QPointF normalized(const QPointF &vector)
{
    const double length = std::hypot(vector.x(), vector.y());
    return length < 0.0001 ? QPointF(1, 0) : vector / length;
}

void drawArrowHead(QPainter &painter, const QPointF &tip, const QPointF &direction,
                   const QString &type, const QColor &color, double size,
                   double strokeWidth = 1.0, bool filled = true)
{
    const QPointF dir = normalized(direction);
    const QPointF normal(-dir.y(), dir.x());
    const QString t = type.toLower();
    // mxMarker.createArrow adds the stroke width to the requested size.
    const double effective = size + qMax(0.0, strokeWidth);
    const QPointF back = tip - dir * effective;

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);

    if (t == QLatin1String("none") || type.isEmpty())
    {
        painter.restore();
        return;
    }

    if (t == QLatin1String("open") || t == QLatin1String("openasync"))
    {
        QPen pen(color);
        pen.setWidthF(qMax(1.0, strokeWidth));
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(tip, back + normal * effective * 0.55);
        painter.drawLine(tip, back - normal * effective * 0.55);
        painter.restore();
        return;
    }

    if (t == QLatin1String("oval"))
    {
        painter.setBrush(filled ? QBrush(color) : QBrush(Qt::NoBrush));

        if (!filled)
        {
            QPen pen(color);
            pen.setWidthF(qMax(1.0, strokeWidth));
            painter.setPen(pen);
        }

        painter.drawEllipse(tip - dir * effective * 0.5,
                            effective * 0.5, effective * 0.5);
        painter.restore();
        return;
    }

    if (t == QLatin1String("diamond") || t == QLatin1String("diamondthin"))
    {
        const double halfWidth = effective / (t == QLatin1String("diamondthin") ? 3.4 : 2.0);
        QPolygonF polygon;
        polygon << tip << tip - dir * effective * 0.5 + normal * halfWidth
                << tip - dir * effective
                << tip - dir * effective * 0.5 - normal * halfWidth;
        painter.setBrush(filled ? QBrush(color) : QBrush(Qt::NoBrush));

        if (!filled)
        {
            QPen pen(color);
            pen.setWidthF(qMax(1.0, strokeWidth));
            painter.setPen(pen);
        }

        painter.drawPolygon(polygon);
        painter.restore();
        return;
    }

    if (t == QLatin1String("baseDash"))
    {
        QPen pen(color);
        pen.setWidthF(qMax(1.0, strokeWidth));
        painter.setPen(pen);
        painter.drawLine(tip + normal * effective * 0.5,
                         tip - normal * effective * 0.5);
        painter.restore();
        return;
    }

    if (t == QLatin1String("doubleBlock"))
    {
        const double halfWidth = effective / 2.0;
        QPolygonF first;
        first << tip << back + normal * halfWidth << back - normal * halfWidth;
        QPolygonF second;
        const QPointF secondTip = tip - dir * effective;
        second << secondTip << secondTip - dir * effective + normal * halfWidth
               << secondTip - dir * effective - normal * halfWidth;
        painter.setBrush(filled ? QBrush(color) : QBrush(Qt::NoBrush));

        if (!filled)
        {
            QPen pen(color);
            pen.setWidthF(qMax(1.0, strokeWidth));
            painter.setPen(pen);
        }

        painter.drawPolygon(first);
        painter.drawPolygon(second);
        painter.restore();
        return;
    }

    if (t == QLatin1String("manyOptional"))
    {
        QPen pen(color);
        pen.setWidthF(qMax(1.0, strokeWidth));
        painter.setPen(pen);
        painter.setBrush(filled ? QBrush(color) : QBrush(Qt::NoBrush));
        painter.drawEllipse(tip - dir * effective * 1.5,
                            effective * 0.5, effective * 0.5);
        painter.drawLine(tip, tip - dir * effective);
        painter.drawLine(tip - dir * effective + normal * effective * 0.5,
                         tip - dir * effective);
        painter.drawLine(tip - dir * effective - normal * effective * 0.5,
                         tip - dir * effective);
        painter.restore();
        return;
    }

    if (t == QLatin1String("block") || t == QLatin1String("blockthin"))
    {
        const double halfWidth = effective * (t == QLatin1String("blockthin") ? 1.0 / 3.0 : 0.5);
        QPolygonF polygon;
        polygon << tip << back + normal * halfWidth << back - normal * halfWidth;
        painter.setBrush(filled ? QBrush(color) : QBrush(Qt::NoBrush));

        if (!filled)
        {
            QPen pen(color);
            pen.setWidthF(qMax(1.0, strokeWidth));
            painter.setPen(pen);
        }

        painter.drawPolygon(polygon);
        painter.restore();
        return;
    }

    // classic: a concave triangle with a 3/4 notch on the back edge.
    const double widthFactor = t == QLatin1String("classicthin") ? 3.0 : 2.0;
    const double halfWidth = effective / widthFactor;
    const QPointF notch = tip - dir * effective * 0.75;
    QPolygonF polygon;
    polygon << tip << back + normal * halfWidth << notch << back - normal * halfWidth;
    painter.setBrush(filled ? QBrush(color) : QBrush(Qt::NoBrush));

    if (!filled)
    {
        QPen pen(color);
        pen.setWidthF(qMax(1.0, strokeWidth));
        painter.setPen(pen);
    }

    painter.drawPolygon(polygon);
    painter.restore();
}

double arrowSize(const Cell &edge, const QString &prefix)
{
    const double size = edge.styleNumber(prefix + QStringLiteral("Size"), 6.0);
    return qBound(3.0, size, 40.0);
}

void paintLabel(QPainter &painter, const Page &page, const Cell &edge, const RoutedEdge &route)
{
    Q_UNUSED(page);

    if (edge.value.isEmpty())
    {
        return;
    }

    const double fontSize = qMax(1.0, edge.styleNumber(QStringLiteral("fontSize"), 11.0));
    const double position = edge.styleNumber(QStringLiteral("labelPosition"), 0.5);
    const QPointF anchor = route.pointAt(position);

    // Size the label box to the text: a full-width white box would cover
    // neighbouring shapes, which is not what draw.io does.
    const QString plain = edge.plainLabel();
    const QFont font = LabelPainter::fontFor(edge);
    const QFontMetricsF metrics(font);
    double textWidth = 0.0;

    for (const QString &line : plain.split(QLatin1Char('\n')))
    {
        textWidth = qMax(textWidth, metrics.horizontalAdvance(line));
    }

    const int lineCount = qMax(1, plain.count(QLatin1Char('\n')) + 1);
    const double width = qBound(16.0, textWidth + 8.0, qMax(40.0, route.length()));
    const double height = qMax(fontSize * 1.5 + 4.0, metrics.height() * lineCount + 4.0);

    const QString background = edge.styleValue(QStringLiteral("labelBackgroundColor"),
                                               QStringLiteral("#ffffff"));

    painter.save();
    painter.setPen(Qt::NoPen);

    if (!background.isEmpty() && !Style::isNoneColor(background))
    {
        painter.setBrush(QColor(background));
        painter.drawRoundedRect(QRectF(anchor.x() - width / 2.0, anchor.y() - height / 2.0,
                                       width, height),
                                2, 2);
    }

    painter.setBrush(Qt::NoBrush);
    painter.restore();

    const QRectF labelBox(anchor.x() - width / 2.0, anchor.y() - height / 2.0, width, height);

    // Edge labels are centred horizontally; the label painter handles the rest.
    // Cell is not copyable, so build a lightweight label carrier.
    Cell labelCell(Cell::Kind::Vertex, QString());
    labelCell.style = edge.style;
    labelCell.value = edge.value;
    painter.save();
    LabelPainter::paint(painter, labelCell, labelBox);
    painter.restore();
}

} // namespace

QPainterPath EdgeRenderer::pathFor(const RoutedEdge &route, const Cell &edge)
{
    QPainterPath path;

    if (route.points.isEmpty())
    {
        return path;
    }

    path.moveTo(route.points.first());

    const bool rounded = edge.styleFlag(QStringLiteral("rounded"));
    double radius = rounded
        ? qBound(0.0, edge.styleNumber(QStringLiteral("arcSize"), 20.0) / 100.0 * 30.0 + 4.0, 30.0)
        : 0.0;

    for (int i = 1; i < route.points.size(); ++i)
    {
        const QPointF &previous = route.points.at(i - 1);
        const QPointF &current = route.points.at(i);

        if (i < route.points.size() - 1 && radius > 0.0)
        {
            const QPointF &next = route.points.at(i + 1);
            const QPointF in = normalized(previous - current);
            const QPointF out = normalized(next - current);
            const double inLength = QLineF(current, previous).length();
            const double outLength = QLineF(current, next).length();
            const double r = qMin(radius, qMin(inLength, outLength) / 2.0);

            path.lineTo(current + in * r);
            path.quadTo(current, current + out * r);
        }
        else
        {
            path.lineTo(current);
        }
    }

    return path;
}

namespace {

void paintFlexArrow(QPainter &painter, const Cell &edge, const RoutedEdge &route)
{
    if (route.points.size() < 2)
    {
        return;
    }

    // draw.io's default shape fill is white: flexArrow edges without an explicit
    // fillColor are outlined white arrows, not coloured ones.
    const QColor fill = edge.styleColor(QStringLiteral("fillColor"), QColor(Qt::white));
    const QColor stroke = edgeColor(edge);

    // draw.io's flexArrow has a solid shaft and a wide triangular head; its
    // size is independent of the generic arrow markers.
    // Matches draw.io's default flexArrow proportions: the head is almost as
    // wide as the available gap, while the shaft is roughly one third of it.
    double shaft = edge.styleNumber(QStringLiteral("arrowWidth"), 11.0);

    if (shaft < 2.0)
    {
        shaft = 11.0; // style may store a fraction; fall back to the default
    }

    // draw.io: endSize (default 6) is multiplied by three for flexArrow.
    const double headLength = qBound(8.0,
                                     edge.styleNumber(QStringLiteral("endSize"), 6.0) * 3.0,
                                     60.0);
    const double headWidth = qMax(headLength, 30.0);
    shaft = qBound(4.0, shaft, 40.0);

    const QPointF tip = route.points.last();
    const QPointF previous = route.points.at(route.points.size() - 2);
    const QPointF direction = normalized(tip - previous);
    const QPointF normal(-direction.y(), direction.x());
    const QPointF base = tip - direction * headLength;

    // A straight flexArrow is a single closed polygon. Building it from a
    // stroked line plus a head leaves the shaft's cap edge visible inside the
    // head, which draw.io does not show.
    if (route.points.size() == 2)
    {
        const QPointF start = route.points.first();
        const double shaftHalf = shaft / 2.0;
        const double headHalf = qMax(headWidth / 2.0, shaftHalf);
        const double length = QLineF(start, tip).length();
        const double head = qMin(headLength, length / 2.0);
        const QPointF startBase = start + direction * head;
        const QPointF endBase = tip - direction * head;

        const QString startArrow = edge.styleValue(QStringLiteral("startArrow"),
                                                   QStringLiteral("none"));
        const QString endArrow = edge.styleValue(QStringLiteral("endArrow"),
                                                 QStringLiteral("classic"));
        const bool startHead = startArrow.compare(QLatin1String("none"), Qt::CaseInsensitive) != 0
            && !startArrow.isEmpty();
        const bool endHead = endArrow.compare(QLatin1String("none"), Qt::CaseInsensitive) != 0
            && !endArrow.isEmpty();

        QPolygonF polygon;

        if (startHead)
        {
            polygon << start
                    << startBase + normal * headHalf
                    << startBase + normal * shaftHalf;
        }
        else
        {
            polygon << start + normal * shaftHalf;
        }

        if (endHead)
        {
            polygon << endBase + normal * shaftHalf
                    << endBase + normal * headHalf
                    << tip
                    << endBase - normal * headHalf
                    << endBase - normal * shaftHalf;
        }
        else
        {
            polygon << tip + normal * shaftHalf
                    << tip - normal * shaftHalf;
        }

        if (startHead)
        {
            polygon << startBase - normal * shaftHalf
                    << startBase - normal * headHalf;
        }
        else
        {
            polygon << start - normal * shaftHalf;
        }

        QPainterPath outline;
        outline.addPolygon(polygon);
        outline.closeSubpath();

        painter.save();
        painter.setPen(QPen(stroke, 1.0));
        painter.setBrush(fill);
        painter.drawPath(outline);
        painter.restore();
        return;
    }

    QPainterPath line;
    line.moveTo(route.points.first());

    for (int i = 1; i < route.points.size() - 1; ++i)
    {
        line.lineTo(route.points.at(i));
    }

    line.lineTo(base);

    QPainterPathStroker stroker;
    stroker.setWidth(shaft);
    stroker.setCapStyle(Qt::FlatCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    QPainterPath shaftPath = stroker.createStroke(line);

    QPainterPath headPath;
    headPath.moveTo(tip);
    headPath.lineTo(base + normal * (headWidth / 2.0));
    headPath.lineTo(base - normal * (headWidth / 2.0));
    headPath.closeSubpath();

    QPainterPath shape = shaftPath.united(headPath);

    painter.save();
    painter.setPen(QPen(stroke, 1.0));
    painter.setBrush(fill);
    painter.drawPath(shape);
    painter.restore();
}

} // namespace

QRectF EdgeRenderer::boundsFor(const RoutedEdge &route, const Cell &edge)
{
    QRectF bounds;

    if (route.points.isEmpty())
    {
        return bounds;
    }

    bounds = QRectF(route.points.first(), QSizeF(0, 0));

    for (const QPointF &point : route.points)
    {
        bounds |= QRectF(point, QSizeF(0, 0));
    }

    double margin = qMax(1.0, edge.styleNumber(QStringLiteral("strokeWidth"), 1.0))
        + arrowSize(edge, QStringLiteral("end"));

    if (edge.styleValue(QStringLiteral("shape")).compare(QLatin1String("flexArrow"),
                                                         Qt::CaseInsensitive) == 0)
    {
        margin = qMax(margin, 18.0);
    }

    return bounds.adjusted(-margin, -margin, margin, margin);
}

void EdgeRenderer::paint(QPainter &painter, const Page &page, const Cell &edge)
{
    const RoutedEdge route = EdgeRouter::route(page, edge);

    if (route.points.size() < 2)
    {
        return;
    }

    if (edge.styleValue(QStringLiteral("shape")).compare(QLatin1String("flexArrow"),
                                                         Qt::CaseInsensitive) == 0)
    {
        paintFlexArrow(painter, edge, route);
        paintLabel(painter, page, edge, route);
        return;
    }

    painter.save();

    const QColor color = edgeColor(edge);
    QPen pen = edgePen(edge);
    pen.setColor(color);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const QPainterPath path = pathFor(route, edge);

    if (color.alpha() > 0)
    {
        painter.drawPath(path);
    }

    // Arrow heads use the adjacent segment orientation.
    const QPointF endDirection = route.points.last() - route.points.at(route.points.size() - 2);
    const QString endArrow = edge.styleValue(QStringLiteral("endArrow"), QStringLiteral("classic"));
    const double strokeWidth = edge.styleNumber(QStringLiteral("strokeWidth"), 1.0);
    const bool endFilled = edge.styleValue(QStringLiteral("endFill"), QStringLiteral("1"))
        != QLatin1String("0");
    drawArrowHead(painter, route.points.last(), endDirection, endArrow, color,
                  arrowSize(edge, QStringLiteral("end")), strokeWidth, endFilled);

    const QString startArrow = edge.styleValue(QStringLiteral("startArrow"), QStringLiteral("none"));

    if (startArrow != QLatin1String("none") && !startArrow.isEmpty())
    {
        const QPointF startDirection = route.points.first() - route.points.at(1);
        const bool startFilled = edge.styleValue(QStringLiteral("startFill"), QStringLiteral("1"))
            != QLatin1String("0");
        drawArrowHead(painter, route.points.first(), startDirection, startArrow, color,
                      arrowSize(edge, QStringLiteral("start")), strokeWidth, startFilled);
    }

    painter.restore();

    paintLabel(painter, page, edge, route);
}

} // namespace dq
