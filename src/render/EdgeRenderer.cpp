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
                   const QString &type, const QColor &color, double size)
{
    const QPointF dir = normalized(direction);
    const QPointF normal(-dir.y(), dir.x());
    const QPointF back = tip - dir * size;

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);

    if (type == QLatin1String("none") || type.isEmpty())
    {
        painter.restore();
        return;
    }

    if (type == QLatin1String("open") || type == QLatin1String("openasync"))
    {
        QPen pen(color);
        pen.setWidthF(qMax(1.0, size / 5.0));
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(tip, back + normal * size * 0.55);
        painter.drawLine(tip, back - normal * size * 0.55);
        painter.restore();
        return;
    }

    if (type == QLatin1String("oval"))
    {
        painter.drawEllipse(tip - dir * size * 0.5, size * 0.45, size * 0.45);
        painter.restore();
        return;
    }

    if (type == QLatin1String("diamond") || type == QLatin1String("diamondthin"))
    {
        const QPointF middle = tip - dir * size * 0.5;
        const double halfWidth = size * (type == QLatin1String("diamondthin") ? 0.28 : 0.42);
        QPolygonF polygon;
        polygon << tip << middle + normal * halfWidth
                << tip - dir * size << middle - normal * halfWidth;
        painter.drawPolygon(polygon);
        painter.restore();
        return;
    }

    // classic, block, async and unknown types.
    const double halfWidth = size * (type == QLatin1String("block") ? 0.5 : 0.4);
    QPolygonF polygon;
    polygon << tip << back + normal * halfWidth << back - normal * halfWidth;
    painter.drawPolygon(polygon);
    painter.restore();
}

double arrowSize(const Cell &edge, const QString &prefix)
{
    const double size = edge.styleNumber(prefix + QStringLiteral("Size"), 8.0);
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
    const double shaft = qBound(4.0, edge.styleNumber(QStringLiteral("arrowWidth"), 10.0), 40.0);
    const double headLength = qBound(8.0, edge.styleNumber(QStringLiteral("arrowSize"), 18.0), 60.0);
    const double headWidth = qMax(headLength, 24.0);

    const QPointF tip = route.points.last();
    const QPointF previous = route.points.at(route.points.size() - 2);
    const QPointF direction = normalized(tip - previous);
    const QPointF normal(-direction.y(), direction.x());
    const QPointF base = tip - direction * headLength;

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
    drawArrowHead(painter, route.points.last(), endDirection, endArrow, color,
                  arrowSize(edge, QStringLiteral("end")));

    const QString startArrow = edge.styleValue(QStringLiteral("startArrow"), QStringLiteral("none"));

    if (startArrow != QLatin1String("none") && !startArrow.isEmpty())
    {
        const QPointF startDirection = route.points.first() - route.points.at(1);
        drawArrowHead(painter, route.points.first(), startDirection, startArrow, color,
                      arrowSize(edge, QStringLiteral("start")));
    }

    painter.restore();

    paintLabel(painter, page, edge, route);
}

} // namespace dq
