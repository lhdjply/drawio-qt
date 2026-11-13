#include "render/EdgeRenderer.h"

#include "render/ShapeRenderer.h"

#include <QLineF>
#include <QLinearGradient>
#include <QPainter>
#include <QTextDocument>
#include <QPolygonF>
#include <QtMath>

#include <cmath>
#include <functional>

namespace dq {
namespace {

StyleMap edgeStyle(const Cell &edge)
{
    return Style::withDefaults(edge.style);
}

QColor edgeColor(const Cell &edge)
{
    return ShapeRenderer::strokeColor(edge);
}

double strokeWidthFor(const Cell &edge)
{
    return qMax(0.5, Style::number(edgeStyle(edge), QStringLiteral("strokeWidth"), 1.0));
}

QPen edgePen(const Cell &edge)
{
    const StyleMap style = edgeStyle(edge);
    QPen pen(edgeColor(edge));
    const double strokeWidth = strokeWidthFor(edge);
    pen.setWidthF(strokeWidth);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);

    if (Style::flag(style, QStringLiteral("dashed")))
    {
        const QString dashPattern = Style::value(style, QStringLiteral("dashPattern"));
        QVector<qreal> dashes;
        const QStringList parts = dashPattern.split(QLatin1Char(' '), Qt::SkipEmptyParts);

        for (const QString &part : parts)
        {
            bool ok = false;
            const double value = part.toDouble(&ok);

            if (ok && value > 0.0)
            {
                dashes.append(value);
            }
        }

        if (dashes.isEmpty())
        {
            // mxAbstractCanvas2D.setDashed() default pattern.
            dashes = {3.0, 3.0};
        }

        // Qt scales dash patterns by the pen width, draw.io by the stroke
        // width, so the raw style values are used unchanged.
        pen.setDashPattern(dashes);
    }

    return pen;
}

QPointF normalized(const QPointF &vector)
{
    const double length = std::hypot(vector.x(), vector.y());
    return length < 0.0001 ? QPointF(1, 0) : vector / length;
}

/// draw.io's half pixel offset for odd effective stroke widths
/// (mxImageExport.getSvgShapeOffset).
double crispOffsetFor(QPainter &painter, const Cell &edge)
{
    const double strokeWidth = strokeWidthFor(edge);

    if (!edge.hasStroke())
    {
        return 0.0;
    }

    const QTransform transform = painter.transform();
    const double scale = std::hypot(transform.m11(), transform.m12());
    const int deviceWidth = qMax(1, qRound(strokeWidth * scale));

    return (deviceWidth % 2 == 1) ? 0.5 : 0.0;
}

double markerSize(const Cell &edge, const QString &prefix)
{
    const double size = Style::number(edgeStyle(edge), prefix + QStringLiteral("Size"), 6.0);
    return qBound(3.0, size, 40.0);
}

bool markerPresent(const Cell &edge, const QString &key, bool defaultValue)
{
    const QString value = Style::value(edgeStyle(edge), key);
    return defaultValue ? value.compare(QLatin1String("none"), Qt::CaseInsensitive) != 0
                        : (!value.isEmpty()
                           && value.compare(QLatin1String("none"), Qt::CaseInsensitive) != 0);
}

/// Returns the direction and the point where the line must stop so that the
/// marker sits on the endpoint exactly like mxMarker.createArrow().
QPointF shortenedForMarker(const QPointF &tip, const QPointF &direction,
                           const QString &type, double size, double strokeWidth)
{
    const double endOffset = strokeWidth * 1.118;
    const double length = size + strokeWidth;
    const QString t = type.toLower();
    const double factor = (t == QLatin1String("classic")
                           || t == QLatin1String("classicthin")) ? 0.75 : 1.0;

    if (t == QLatin1String("open") || t == QLatin1String("openasync")
        || t == QLatin1String("openthin"))
    {
        return tip - direction * (2.0 * endOffset);
    }

    return tip - direction * (length * factor + endOffset);
}

void drawClassicMarker(QPainter &painter, const QPointF &tip, const QPointF &direction,
                       const QString &type, double size, double strokeWidth,
                       const QColor &color, bool filled)
{
    const QPointF unit = direction * (size + strokeWidth);
    const QPointF normal(-direction.y(), direction.x());
    const QPointF pt = tip - direction * (strokeWidth * 1.118);
    const double widthFactor = type.compare(QLatin1String("classicThin"), Qt::CaseInsensitive) == 0
        || type.compare(QLatin1String("blockThin"), Qt::CaseInsensitive) == 0 ? 3.0 : 2.0;
    const double halfWidth = (size + strokeWidth) / widthFactor;

    QPolygonF polygon;
    polygon << pt
            << pt - unit - normal * halfWidth
            << pt - unit * 0.75
            << pt + normal * halfWidth - unit;

    painter.save();
    painter.setPen(filled ? QPen(color, strokeWidth) : QPen(color, qMax(1.0, strokeWidth)));

    if (filled)
    {
        painter.setBrush(color);
        painter.drawPolygon(polygon);
    }
    else
    {
        painter.setBrush(Qt::NoBrush);
        painter.drawPolyline(QPolygonF() << polygon.at(0) << polygon.at(1) << polygon.at(2));
        painter.drawPolyline(QPolygonF() << polygon.at(2) << polygon.at(3));
    }

    painter.restore();
}

void drawOpenMarker(QPainter &painter, const QPointF &tip, const QPointF &direction,
                    double size, double strokeWidth, const QColor &color)
{
    const QPointF unit = direction * (size + strokeWidth);
    const QPointF normal(-direction.y(), direction.x());
    const QPointF pt = tip - direction * (strokeWidth * 1.118);
    const double halfWidth = (size + strokeWidth) / 2.0;

    painter.save();
    QPen pen(color, qMax(1.0, strokeWidth));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPolyline(QPolygonF() << (pt - unit - normal * halfWidth) << pt
                                     << (pt + normal * halfWidth - unit));
    painter.restore();
}

void drawArrowHead(QPainter &painter, const QPointF &tip, const QPointF &direction,
                   const QString &type, const QColor &color, double size,
                   double strokeWidth = 1.0, bool filled = true)
{
    const QPointF dir = normalized(direction);
    const QPointF normal(-dir.y(), dir.x());
    const QString t = type.toLower();
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

    if (t == QLatin1String("classic") || t == QLatin1String("classicthin"))
    {
        painter.restore();
        drawClassicMarker(painter, tip, dir, type, size, strokeWidth, color, filled);
        return;
    }

    if (t == QLatin1String("block") || t == QLatin1String("blockthin"))
    {
        painter.restore();
        drawClassicMarker(painter, tip, dir, type, size, strokeWidth, color, filled);
        return;
    }

    if (t == QLatin1String("open") || t == QLatin1String("openasync")
        || t == QLatin1String("openthin"))
    {
        painter.restore();
        drawOpenMarker(painter, tip, dir, size, strokeWidth, color);
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

    painter.restore();
}

/// Port of mxMarker.createArrow() for the classic family: the line is
/// shortened by the marker before it is painted.
QPointF applyMarker(QPainter &painter, const Cell &edge, const QPointF &tip,
                    const QPointF &direction, bool isStart, bool paint)
{
    const QString type = Style::value(edgeStyle(edge),
        isStart ? QStringLiteral("startArrow") : QStringLiteral("endArrow"),
        isStart ? QStringLiteral("none") : QStringLiteral("classic"));

    if (type.isEmpty() || type.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0)
    {
        return tip;
    }

    const double size = markerSize(edge, isStart ? QStringLiteral("start")
                                                : QStringLiteral("end"));
    const double strokeWidth = strokeWidthFor(edge);
    const QString fillKey = isStart ? QStringLiteral("startFill") : QStringLiteral("endFill");
    const bool filled = Style::value(edgeStyle(edge), fillKey, QStringLiteral("1"))
        != QLatin1String("0");

    if (paint)
    {
        drawArrowHead(painter, tip, direction, type, edgeColor(edge), size, strokeWidth, filled);
    }

    const QString t = type.toLower();

    if (t == QLatin1String("classic") || t == QLatin1String("classicthin")
        || t == QLatin1String("block") || t == QLatin1String("blockthin")
        || t == QLatin1String("open") || t == QLatin1String("openasync")
        || t == QLatin1String("openthin"))
    {
        return shortenedForMarker(tip, normalized(direction), type, size, strokeWidth);
    }

    return tip;
}

/// TaperedArrowShape.paintTaperedLine + the block head that is part of the
/// band outline: a polygon whose width goes from startWidth to endWidth.
QPolygonF taperedArrowPath(const Cell &edge, const QVector<QPointF> &input)
{
    QVector<QPointF> points;

    for (const QPointF &point : input)
    {
        if (points.isEmpty() || QLineF(points.last(), point).length() > 0.0001)
        {
            points.append(point);
        }
    }

    if (points.size() < 2)
    {
        return {};
    }

    const StyleMap style = edgeStyle(edge);
    const double startWidth = qMax(0.0, Style::number(style, QStringLiteral("startWidth"), 12.0));
    const double endWidth = qMax(0.0, Style::number(style, QStringLiteral("endWidth"), 2.0));
    const double strokeWidth = strokeWidthFor(edge);

    QVector<QPointF> normals;
    QVector<double> lengths{0.0};

    for (int i = 1; i < points.size(); ++i)
    {
        const QPointF delta = points.at(i) - points.at(i - 1);
        const double distance = std::hypot(delta.x(), delta.y());
        normals.append(distance > 0.0001
                           ? QPointF(-delta.y() / distance, delta.x() / distance)
                           : QPointF(0, 1));
        lengths.append(lengths.last() + distance);
    }

    const double total = lengths.last();

    if (total <= 0.0001)
    {
        return {};
    }

    // The marker shortens the last point by its own length.
    const QString arrow = Style::value(style, QStringLiteral("endArrow")).toLower();
    const bool hasHead = !arrow.isEmpty() && arrow != QLatin1String("none");
    const double markerSize = Style::number(style, QStringLiteral("endSize"), 6.0);
    const double headLength = hasHead ? markerSize + endWidth : 0.0;
    const double headOffset = hasHead ? strokeWidth * 1.118 : 0.0;
    QPointF lastDirection(1, 0);

    if (hasHead)
    {
        const QPointF delta = points.last() - points.at(points.size() - 2);
        const double distance = std::hypot(delta.x(), delta.y());

        if (distance > 0.0001)
        {
            lastDirection = delta / distance;
            points.last() = points.last() - lastDirection * (headLength + headOffset);
            lengths.last() = qMax(0.0, total - headLength - headOffset);
        }
    }

    QVector<QPointF> left;
    QVector<QPointF> right;

    for (int i = 0; i < points.size(); ++i)
    {
        const double width = (startWidth + (endWidth - startWidth)
                              * (total > 0.0 ? lengths.at(i) / total : 0.0)) / 2.0;
        const QPointF n0 = normals.at(qMax(0, i - 1));
        const QPointF n1 = normals.at(qMin(normals.size() - 1, i));
        QPointF normal = n0 + n1;
        const double length = std::hypot(normal.x(), normal.y());
        const double cosine = length == 0.0 ? 0.0 : length / 2.0;
        const double factor = width / qMax(0.5, cosine);
        normal = length == 0.0 ? n1 : normal / length;

        left.append(points.at(i) + normal * factor);
        right.append(points.at(i) - normal * factor);
    }

    QPolygonF polygon;

    for (const QPointF &point : left)
    {
        polygon << point;
    }

    if (hasHead)
    {
        const double half = headLength / 2.0;
        const QPointF normal(-lastDirection.y(), lastDirection.x());
        const QPointF base = points.last();
        polygon << base + normal * half;
        polygon << base + lastDirection * headLength;
        polygon << base - normal * half;
    }

    for (int i = right.size() - 1; i >= 0; --i)
    {
        polygon << right.at(i);
    }

    return polygon;
}

/// mxArrowConnector.paintEdgeShape(): the outline of flexArrow and link./// mxArrowConnector.paintEdgeShape(): the outline of flexArrow and link.
/// `openEnded` strokes two parallel lines instead of a closed outline.
QPainterPath arrowConnectorPath(const Cell &edge, const QVector<QPointF> &input,
                                bool openEnded, double defaultWidth,
                                double defaultArrowWidth)
{
    QPainterPath path;

    if (input.size() < 2)
    {
        return path;
    }

    const StyleMap style = edgeStyle(edge);
    const double strokeWidth = strokeWidthFor(edge);
    const double edgeWidth = Style::number(style, QStringLiteral("width"), defaultWidth)
        + qMax(0.0, strokeWidth - 1.0);
    const double startArrowWidth = edgeWidth
        + Style::number(style, QStringLiteral("startWidth"), defaultArrowWidth);
    const double endArrowWidth = edgeWidth
        + Style::number(style, QStringLiteral("endWidth"), defaultArrowWidth);
    const double startWidth = startArrowWidth + strokeWidth;
    const double endWidth = endArrowWidth + strokeWidth;
    const bool markerStart = markerPresent(edge, QStringLiteral("startArrow"), false);
    const bool markerEnd = markerPresent(edge, QStringLiteral("endArrow"), true);
    const double spacing = openEnded ? 0.0 : 0.5 * strokeWidth;
    const double startSize = Style::number(style, QStringLiteral("startSize"), 6.0) * 3.0
        + strokeWidth;
    const double endSize = Style::number(style, QStringLiteral("endSize"), 6.0) * 3.0
        + strokeWidth;

    const QVector<QPointF> &pts = input;
    const QPointF pe = pts.last();
    QPointF base = pts.at(1) - pts.first();
    double dist = std::hypot(base.x(), base.y());

    if (dist == 0.0)
    {
        return path;
    }

    double nx = base.x() / dist;
    double ny = base.y() / dist;
    double nx1 = nx;
    double ny1 = ny;
    double orthx = edgeWidth * ny;
    double orthy = -edgeWidth * nx;
    QVector<std::function<void()>> deferred;

    const auto appendMarker = [&](const QPointF &point, const QPointF &n, double size,
                                  double arrowWidth, bool initialMove)
    {
        const double ratio = edgeWidth / arrowWidth;
        const QPointF orth(edgeWidth * n.y() / 2.0, -edgeWidth * n.x() / 2.0);
        const QPointF space = n * (spacing + size);
        const QPointF p1 = point - orth + space;
        const QPointF p2 = point - orth / ratio + space;
        const QPointF p3 = point + n * spacing;
        const QPointF p4 = point + orth / ratio + space;
        const QPointF p5 = point + orth + space;

        if (initialMove)
        {
            path.moveTo(p1);
        }
        else
        {
            path.lineTo(p1);
        }

        path.lineTo(p2);
        path.lineTo(p3);
        path.lineTo(p4);
        path.lineTo(p5);
    };

    if (markerStart && !openEnded)
    {
        appendMarker(pts.first(), QPointF(nx, ny), startSize, startWidth, true);
    }
    else
    {
        const QPointF outStart = pts.first() + QPointF(orthx / 2.0 + spacing * nx,
                                                      orthy / 2.0 + spacing * ny);
        const QPointF inEnd = pts.first() + QPointF(-orthx / 2.0 + spacing * nx,
                                                    -orthy / 2.0 + spacing * ny);

        if (openEnded)
        {
            path.moveTo(outStart);
            deferred.append([&path, inEnd]() { path.lineTo(inEnd); });
        }
        else
        {
            path.moveTo(inEnd);
            path.lineTo(outStart);
        }
    }

    for (int i = 0; i < pts.size() - 2; ++i)
    {
        const QPointF delta = pts.at(i + 2) - pts.at(i + 1);
        const double segment = std::hypot(delta.x(), delta.y());

        if (segment == 0.0)
        {
            continue;
        }

        nx1 = delta.x() / segment;
        ny1 = delta.y() / segment;
        const double dot = nx * nx1 + ny * ny1;
        const double tmp = qMax(std::sqrt(qMax(0.0, (dot + 1.0) / 2.0)), 0.04);

        QPointF normalSum(nx + nx1, ny + ny1);
        const double normalLength = std::hypot(normalSum.x(), normalSum.y());

        if (normalLength == 0.0)
        {
            continue;
        }

        normalSum /= normalLength;
        const double angleFactor = qMax(tmp, 0.06);

        const QPointF centre = pts.at(i + 1);
        const QPointF outPoint = centre + QPointF(normalSum.y() * edgeWidth / 2.0 / angleFactor,
                                                 -normalSum.x() * edgeWidth / 2.0 / angleFactor);
        const QPointF inPoint = centre + QPointF(-normalSum.y() * edgeWidth / 2.0 / angleFactor,
                                                 normalSum.x() * edgeWidth / 2.0 / angleFactor);

        path.lineTo(outPoint);
        deferred.append([&path, inPoint]() { path.lineTo(inPoint); });

        nx = nx1;
        ny = ny1;
    }

    orthx = edgeWidth * ny1;
    orthy = -edgeWidth * nx1;

    if (markerEnd && !openEnded)
    {
        appendMarker(pe, QPointF(-nx, -ny), endSize, endWidth, false);
    }
    else
    {
        path.lineTo(pe.x() - spacing * nx1 + orthx / 2.0,
                    pe.y() - spacing * ny1 + orthy / 2.0);
        const QPointF inStart(pe.x() - spacing * nx1 - orthx / 2.0,
                              pe.y() - spacing * ny1 - orthy / 2.0);

        if (!openEnded)
        {
            path.lineTo(inStart);
        }
        else
        {
            path.moveTo(inStart);
            deferred.prepend([&path, inStart]() { path.moveTo(inStart); });
        }
    }

    for (int i = deferred.size() - 1; i >= 0; --i)
    {
        deferred.at(i)();
    }

    if (!openEnded)
    {
        path.closeSubpath();
    }

    return path;
}

/// draw.io edge labels are relative vertices: the anchor is the point on the
/// polyline at `0.5 + x/2`, offset along the normal by `y` plus the geometry
/// offset (mxGraphView.getPoint).
QPointF anchorOnRoute(const RoutedEdge &route, const Cell &child)
{
    if (route.points.isEmpty())
    {
        return QPointF();
    }

    const double fraction = qBound(0.0, 0.5 + child.geometry.x() / 2.0, 1.0);
    const double length = route.length();
    const double target = fraction * length;
    double walked = 0.0;
    QPointF point = route.points.first();
    QPointF direction(1, 0);

    for (int i = 1; i < route.points.size(); ++i)
    {
        const QPointF a = route.points.at(i - 1);
        const QPointF b = route.points.at(i);
        const double segment = QLineF(a, b).length();

        if (segment <= 0.0)
        {
            continue;
        }

        if (walked + segment >= target || i == route.points.size() - 1)
        {
            const double t = qBound(0.0, (target - walked) / segment, 1.0);
            point = a + (b - a) * t;
            direction = (b - a) / segment;
            break;
        }

        walked += segment;
    }

    // getPoint() uses (dy / length, dx / length) as the normal and flips the
    // y component of the perpendicular offset.
    const QPointF normal(direction.y(), -direction.x());
    const QPointF offset = normal * child.geometry.y() + child.geometryOffset;

    return point + offset;
}

QRectF labelBox(const Cell &label, const QPointF &anchor)
{
    const StyleMap style = edgeStyle(label);
    const QFont font = LabelPainter::fontFor(label);
    const QFontMetricsF metrics(font);
    const QString text = label.plainLabel();
    double width = qMax(1.0, metrics.horizontalAdvance(text));
    double height = qMax(1.0, font.pixelSize() * 1.2);

    // Rich labels mix weights and colours, so the plain text width is too
    // narrow and the label would wrap; measure the markup instead.
    if (label.isHtmlLabel())
    {
        QTextDocument document;
        document.setDefaultFont(font);
        document.setDocumentMargin(0);
        document.setHtml(label.value);
        document.setTextWidth(-1);
        width = qMax(width, document.idealWidth() + 2.0);
        height = qMax(height, document.size().height());
    }
    const double spacing = Style::number(style, QStringLiteral("spacing"), 2.0);
    const double spacingLeft = Style::number(style, QStringLiteral("spacingLeft"), spacing);
    const double spacingRight = Style::number(style, QStringLiteral("spacingRight"), spacing);
    const double spacingTop = Style::number(style, QStringLiteral("spacingTop"), spacing);
    const double spacingBottom = Style::number(style, QStringLiteral("spacingBottom"), spacing);
    const Qt::Alignment alignment = LabelPainter::alignmentFor(label);

    QRectF rect(0, 0, width, height);

    if (alignment & Qt::AlignLeft)
    {
        rect.moveLeft(anchor.x() + spacingLeft);
    }
    else if (alignment & Qt::AlignRight)
    {
        rect.moveRight(anchor.x() - spacingRight);
    }
    else
    {
        rect.moveCenter(QPointF(anchor.x(), rect.center().y()));
    }

    if (alignment & Qt::AlignTop)
    {
        rect.moveTop(anchor.y() + spacingTop + 5.0);
    }
    else if (alignment & Qt::AlignBottom)
    {
        rect.moveBottom(anchor.y() - spacingBottom - 1.0);
    }
    else
    {
        rect.moveCenter(QPointF(rect.center().x(), anchor.y()));
    }

    return rect;
}

void paintLabelAt(QPainter &painter, const Cell &label, const QPointF &anchor)
{
    if (label.value.isEmpty())
    {
        return;
    }

    const QRectF rect = labelBox(label, anchor);
    const StyleMap style = edgeStyle(label);
    const QFont font = LabelPainter::fontFor(label);
    const Qt::Alignment alignment = LabelPainter::alignmentFor(label);
    const QString background = Style::value(style, QStringLiteral("labelBackgroundColor"));

    painter.save();

    // draw.io cuts the label background out of the edge (mxImageExport
    // addLabelGaps); painting the page colour over the line is equivalent.
    if (background.isEmpty() || Style::isNoneColor(background)
        || background.compare(QLatin1String("default"), Qt::CaseInsensitive) == 0)
    {
        painter.fillRect(rect.adjusted(-1, -1, 1, 1), QColor(Qt::white));
    }
    else
    {
        painter.fillRect(rect.adjusted(-1, -1, 1, 1), QColor(background));
    }

    // Rich labels keep their inline runs (colour, bold, italic, underline).
    if (label.isHtmlLabel())
    {
        // LabelPainter insets its bounds by the label spacing; grow the box by
        // the same amount so the text lands exactly on the computed rect.
        const StyleMap labelStyle = Style::withDefaults(label.style);
        const double spacing = Style::number(labelStyle, QStringLiteral("spacing"), 2.0);
        const QRectF bounds = rect.adjusted(-(spacing + Style::number(labelStyle, QStringLiteral("spacingLeft"), 0.0)),
                                            -(spacing + Style::number(labelStyle, QStringLiteral("spacingTop"), 0.0)),
                                            spacing + Style::number(labelStyle, QStringLiteral("spacingRight"), 0.0),
                                            spacing + Style::number(labelStyle, QStringLiteral("spacingBottom"), 0.0));
        LabelPainter::paint(painter, label, bounds, false);
    }
    else
    {
        painter.setFont(font);
        painter.setPen(LabelPainter::textColor(label));
        painter.drawText(rect, alignment, label.plainLabel());
    }

    painter.restore();
}

void paintChild(QPainter &painter, const Cell &edge, const Cell &child)
{
    const RoutedEdge route = EdgeRouter::route(*edge.page, edge);
    const QPointF anchor = anchorOnRoute(route, child);

    if (child.geometry.width() > 0.0 || child.geometry.height() > 0.0)
    {
        ShapeRenderer::paint(painter, child, &anchor);
    }
    else
    {
        paintLabelAt(painter, child, anchor);
    }
}

} // namespace

QPointF EdgeRenderer::childAnchor(const RoutedEdge &route, const Cell &child)
{
    return anchorOnRoute(route, child);
}

QRectF EdgeRenderer::childBounds(const RoutedEdge &route, const Cell &child)
{
    if (route.points.isEmpty())
    {
        return QRectF();
    }

    const QPointF anchor = anchorOnRoute(route, child);

    if (child.geometry.width() > 0.0 || child.geometry.height() > 0.0)
    {
        return QRectF(anchor, child.geometry.size());
    }

    return labelBox(child, anchor);
}

QPainterPath EdgeRenderer::pathFor(const RoutedEdge &route, const Cell &edge)
{
    QPainterPath path;

    if (route.points.isEmpty())
    {
        return path;
    }

    const StyleMap style = edgeStyle(edge);
    const bool curved = Style::flag(style, QStringLiteral("curved"));
    const bool rounded = Style::flag(style, QStringLiteral("rounded"));
    QVector<QPointF> points = route.points;

    if (curved && points.size() > 2)
    {
        // mxPolyline.paintCurvedLine(): quadratic spline through the midpoints
        // of the guide points.
        path.moveTo(points.first());

        for (int i = 1; i < points.size() - 2; ++i)
        {
            const QPointF control = points.at(i);
            const QPointF next = points.at(i + 1);
            path.quadTo(control, (control + next) / 2.0);
        }

        path.quadTo(points.at(points.size() - 2), points.last());
        return path;
    }

    path.moveTo(points.first());

    const double radius = rounded
        ? qBound(0.0, Style::number(style, QStringLiteral("arcSize"), 20.0) / 100.0 * 30.0 + 4.0,
                 30.0)
        : 0.0;

    for (int i = 1; i < points.size(); ++i)
    {
        const QPointF &previous = points.at(i - 1);
        const QPointF &current = points.at(i);

        if (i < points.size() - 1 && radius > 0.0)
        {
            const QPointF &next = points.at(i + 1);
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

QRectF EdgeRenderer::boundsFor(const RoutedEdge &route, const Cell &edge)
{
    QRectF bounds;

    if (route.points.isEmpty())
    {
        return bounds;
    }

    const QPolygonF polygon(route.points);
    bounds = polygon.boundingRect();

    double margin = qMax(1.0, strokeWidthFor(edge))
        + markerSize(edge, QStringLiteral("end"));

    if (Style::value(edgeStyle(edge), QStringLiteral("shape"))
            .compare(QLatin1String("flexArrow"), Qt::CaseInsensitive) == 0)
    {
        margin = qMax(margin, 24.0);
    }

    return bounds.adjusted(-margin, -margin, margin, margin);
}

void EdgeRenderer::paintMarkers(QPainter &painter, const Page &page, const Cell &edge)
{
    Q_UNUSED(page);

    if (Style::value(edgeStyle(edge), QStringLiteral("shape"))
            .compare(QLatin1String("flexArrow"), Qt::CaseInsensitive) == 0)
    {
        return;
    }

    const RoutedEdge route = EdgeRouter::route(page, edge);

    if (route.points.size() < 2)
    {
        return;
    }

    painter.save();
    applyMarker(painter, edge, route.points.last(),
                route.points.last() - route.points.at(route.points.size() - 2), false, true);
    applyMarker(painter, edge, route.points.first(),
                route.points.first() - route.points.at(1), true, true);
    painter.restore();
}

void EdgeRenderer::paint(QPainter &painter, const Page &page, const Cell &edge)
{
    const RoutedEdge route = EdgeRouter::route(page, edge);

    if (route.points.size() < 2)
    {
        return;
    }

    const StyleMap style = edgeStyle(edge);
    const QString shape = Style::value(style, QStringLiteral("shape")).toLower();
    const QColor color = edgeColor(edge);
    const double strokeWidth = strokeWidthFor(edge);
    const double crispOffset = crispOffsetFor(painter, edge);

    painter.save();

    if (crispOffset > 0.0)
    {
        painter.translate(crispOffset, crispOffset);
    }

    if (shape == QLatin1String("flexarrow") || shape == QLatin1String("link"))
    {
        const bool link = shape == QLatin1String("link");
        const QPainterPath path = arrowConnectorPath(edge, route.points, link,
                                                     link ? 4.0 : 10.0, 20.0);
        QPen pen(color, strokeWidth);
        pen.setJoinStyle(Qt::MiterJoin);
        pen.setCapStyle(Qt::FlatCap);
        painter.setPen(pen);

        // flexArrow honours an explicit fillColor; without one it is a hollow
        // outline, exactly like draw.io (mxShape.fill is null for edges).
        QBrush fill(Qt::NoBrush);
        const QString fillValue = Style::value(style, QStringLiteral("fillColor"));

        if (!link && !fillValue.isEmpty()
            && fillValue.compare(QLatin1String("default"), Qt::CaseInsensitive) != 0
            && !Style::isNoneColor(fillValue))
        {
            const QColor parsed = Style::parseColorValue(fillValue);

            if (parsed.isValid())
            {
                fill = QBrush(parsed);
            }
        }

        painter.setBrush(fill);

        if (color.alpha() > 0)
        {
            painter.drawPath(path);
        }

        painter.restore();

        for (const Cell *child : edge.children)
        {
            if (child->visible)
            {
                paintChild(painter, edge, *child);
            }
        }

        if (!edge.value.isEmpty())
        {
            paintLabelAt(painter, edge, route.pointAt(0.5));
        }

        return;
    }

    if (shape == QLatin1String("mermaidsankeylink"))
    {
        // Mermaid flow band: sample the d3-sankey cubic M x0,y0 C mx,y0 mx,y1 x1,y1
        // and offset it perpendicular to the tangent by half the style width.
        const QPointF p0 = route.points.first();
        const QPointF pe = route.points.last();
        const double halfWidth = qMax(1.0,
            Style::number(style, QStringLiteral("width"), 10.0)) / 2.0;
        const double mx = (p0.x() + pe.x()) / 2.0;
        const int steps = 32;
        QVector<QPointF> side1;
        QVector<QPointF> side2;

        for (int i = 0; i <= steps; ++i)
        {
            const double t = static_cast<double>(i) / steps;
            const double u = 1.0 - t;
            const double px = u * u * u * p0.x() + 3.0 * u * u * t * mx
                + 3.0 * u * t * t * mx + t * t * t * pe.x();
            const double py = u * u * u * p0.y() + 3.0 * u * u * t * p0.y()
                + 3.0 * u * t * t * pe.y() + t * t * t * pe.y();
            double dx = 3.0 * u * u * (mx - p0.x()) + 3.0 * t * t * (pe.x() - mx);
            double dy = 6.0 * u * t * (pe.y() - p0.y());
            double length = std::hypot(dx, dy);

            if (length == 0.0)
            {
                dx = 1.0;
                dy = 0.0;
                length = 1.0;
            }

            const double nx = -dy / length * halfWidth;
            const double ny = dx / length * halfWidth;
            side1.append(QPointF(px + nx, py + ny));
            side2.append(QPointF(px - nx, py - ny));
        }

        QPainterPath band;
        band.moveTo(side1.first());

        for (int i = 1; i < side1.size(); ++i)
        {
            band.lineTo(side1.at(i));
        }

        for (int i = side2.size() - 1; i >= 0; --i)
        {
            band.lineTo(side2.at(i));
        }

        band.closeSubpath();

        QColor fill = Style::color(style, QStringLiteral("fillColor"),
                                   QColor(0xaf, 0x7a, 0xa1));
        const QString gradientColor = Style::value(style, QStringLiteral("gradientColor"));
        QBrush brush(fill);

        if (!gradientColor.isEmpty() && !Style::isNoneColor(gradientColor))
        {
            const QColor second = Style::parseColorValue(gradientColor);
            const double left = qMin(p0.x(), pe.x());
            const double right = qMax(p0.x(), pe.x());
            const double middle = (p0.y() + pe.y()) / 2.0;
            QLinearGradient gradient(QPointF(left, middle), QPointF(right, middle));
            gradient.setColorAt(0.0, fill);
            gradient.setColorAt(1.0, second);
            brush = gradient;
        }

        const double opacity = qBound(0.0,
            Style::number(style, QStringLiteral("opacity"), 100.0) / 100.0, 1.0);

        painter.save();
        painter.setOpacity(opacity);
        painter.setPen(Qt::NoPen);
        painter.setBrush(brush);
        painter.drawPath(band);
        painter.restore();
        painter.restore();

        for (const Cell *child : edge.children)
        {
            if (child->visible)
            {
                paintChild(painter, edge, *child);
            }
        }

        return;
    }

    if (shape == QLatin1String("mxgraph.arrows2.wedgearrow")
        || shape == QLatin1String("mxgraph.arrows2.wedgearrowdashed2")
        || shape == QLatin1String("mxgraph.arrows2.wedgearrowdashed"))
    {
        // mxShapeArrowsWedgeArrow*: the wedge is built from the edge endpoints.
        const QPointF p0 = route.points.first();
        const QPointF pe = route.points.last();
        const double dx = pe.x() - p0.x();
        const double dy = pe.y() - p0.y();
        const double dist = std::hypot(dx, dy);
        const double startWidth = qMax(0.0, Style::number(style, QStringLiteral("startWidth"), 20.0));

        painter.save();
        painter.setPen(edgePen(edge));
        painter.setBrush(Qt::NoBrush);

        if (dist > 0.0)
        {
            const double nx = dx * startWidth / dist;
            const double ny = dy * startWidth / dist;

            if (shape == QLatin1String("mxgraph.arrows2.wedgearrow"))
            {
                QPolygonF wedge;
                wedge << QPointF(p0.x() + ny, p0.y() - nx)
                      << QPointF(p0.x() - ny, p0.y() + nx)
                      << pe;
                QColor fill = color;
                const QString fillValue = Style::value(style, QStringLiteral("fillColor"));

                if (fillValue.compare(QLatin1String("strokeColor"), Qt::CaseInsensitive) == 0)
                {
                    fill = color;
                }
                else if (!fillValue.isEmpty() && !Style::isNoneColor(fillValue))
                {
                    fill = Style::parseColorValue(fillValue);
                }

                painter.setBrush(fill);
                painter.drawPolygon(wedge);
            }
            else
            {
                const double stepSize = qMax(0.0, Style::number(style, QStringLiteral("stepSize"),
                                                                   shape.endsWith(QLatin1Char('2'))
                                                                       ? 10.0 : 20.0));
                int steps = 8;

                if (shape.endsWith(QLatin1Char('2')))
                {
                    steps = stepSize > 0.0 ? qMin(int(dist / stepSize), 10000) : 0;
                }

                if (steps > 0)
                {
                    double pcx = p0.x();
                    double pcy = p0.y();

                    for (int i = 0; i <= steps; ++i)
                    {
                        double cnx = nx * (steps - i) / steps;
                        double cny = ny * (steps - i) / steps;

                        if (i == steps)
                        {
                            cnx = nx * (steps - i * 0.98) / steps;
                            cny = ny * (steps - i * 0.98) / steps;
                        }

                        painter.drawLine(QPointF(pcx + cny, pcy - cnx),
                                         QPointF(pcx - cny, pcy + cnx));
                        pcx += dx / steps;
                        pcy += dy / steps;
                    }
                }
                else
                {
                    painter.drawLine(p0, pe);
                }
            }
        }

        painter.restore();
        painter.restore();

        for (const Cell *child : edge.children)
        {
            if (child->visible)
            {
                paintChild(painter, edge, *child);
            }
        }

        if (!edge.value.isEmpty())
        {
            paintLabelAt(painter, edge, route.pointAt(0.5));
        }

        return;
    }

    // Plain connectors: shorten the polyline for the markers, paint the line
    // and then the markers on top. The filled edge and the tapered arrow
    // paint their own band instead of a plain stroke.
    QVector<QPointF> points = route.points;
    const QPointF endDirection = points.last() - points.at(points.size() - 2);
    const QPointF startDirection = points.first() - points.at(1);

    if (shape == QLatin1String("fillededge"))
    {
        QPen pen = edgePen(edge);
        pen.setColor(color);
        pen.setJoinStyle(Qt::MiterJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        if (color.alpha() > 0)
        {
            painter.drawPath(pathFor(route, edge));
        }

        // FilledEdge.paintEdgeShape: strokeWidth >= 3 adds an inner line in
        // fillColor, two pixels thinner, so the edge reads as a hollow pipe.
        const QString fillValue = Style::value(style, QStringLiteral("fillColor"));

        if (strokeWidth >= 3.0 && !fillValue.isEmpty()
            && fillValue.compare(QLatin1String("default"), Qt::CaseInsensitive) != 0
            && !Style::isNoneColor(fillValue))
        {
            const QColor inner = Style::parseColorValue(fillValue);

            if (inner.isValid())
            {
                QPen innerPen(inner, qMax(1.0, strokeWidth - 2.0));
                innerPen.setJoinStyle(Qt::MiterJoin);
                innerPen.setCapStyle(Qt::FlatCap);
                painter.setPen(innerPen);
                painter.drawPath(pathFor(route, edge));
            }
        }
    }
    else if (shape == QLatin1String("taperedarrow"))
    {
        const QPolygonF band = taperedArrowPath(edge, route.points);

        if (!band.isEmpty() && color.alpha() > 0)
        {
            QPen pen(color, strokeWidth);
            pen.setJoinStyle(Qt::MiterJoin);
            pen.setCapStyle(Qt::FlatCap);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPolygon(band);
        }
    }
    else
    {
        const QPointF endShortened = applyMarker(painter, edge, points.last(),
                                                 endDirection, false, false);
        const QPointF startShortened = applyMarker(painter, edge, points.first(),
                                                   startDirection, true, false);
        points.last() = endShortened;
        points.first() = startShortened;

        RoutedEdge shortenedRoute = route;
        shortenedRoute.points = points;

        QPen pen = edgePen(edge);
        pen.setColor(color);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        if (color.alpha() > 0)
        {
            painter.drawPath(pathFor(shortenedRoute, edge));
        }

        applyMarker(painter, edge, route.points.last(), endDirection, false, true);
        applyMarker(painter, edge, route.points.first(), startDirection, true, true);
    }

    painter.restore();

    for (const Cell *child : edge.children)
    {
        if (child->visible)
        {
            paintChild(painter, edge, *child);
        }
    }

    if (!edge.value.isEmpty())
    {
        paintLabelAt(painter, edge, route.pointAt(0.5));
    }
}

} // namespace dq
