#include "render/ShapeRegistry.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QPainterPath>
#include <QRegularExpression>
#include <QtMath>

namespace dq {
namespace ShapeRegistry {
namespace {

/// Maps draw.io stencil names (mxgraph.basic.cloud_callout) to the canonical
/// keys used by the renderer (cloudCallout).
QString normalizeStencilKey(const QString &shape)
{
    // draw.io arrows2 shapes are registered under mxgraph.arrows2.*; the
    // renderer keeps them as arrows2.<stencil> keys.
    if (shape.startsWith(QLatin1String("mxgraph.arrows2."), Qt::CaseInsensitive))
    {
        // The complete set registered by shapes/mxArrows.js (Sidebar-Arrows2).
        static const QHash<QString, QString> arrows2 = {
            {QStringLiteral("arrow"), QStringLiteral("arrows2.arrow")},
            {QStringLiteral("bendarrow"), QStringLiteral("arrows2.bendArrow")},
            {QStringLiteral("benddoublearrow"), QStringLiteral("arrows2.bendDoubleArrow")},
            {QStringLiteral("calloutarrow"), QStringLiteral("arrows2.calloutArrow")},
            {QStringLiteral("calloutdoublearrow"), QStringLiteral("arrows2.calloutDoubleArrow")},
            {QStringLiteral("calloutdouble90arrow"), QStringLiteral("arrows2.calloutDouble90Arrow")},
            {QStringLiteral("calloutquadarrow"), QStringLiteral("arrows2.calloutQuadArrow")},
            {QStringLiteral("jumpinarrow"), QStringLiteral("arrows2.jumpInArrow")},
            {QStringLiteral("quadarrow"), QStringLiteral("arrows2.quadArrow")},
            {QStringLiteral("sharparrow"), QStringLiteral("arrows2.sharpArrow")},
            {QStringLiteral("sharparrow2"), QStringLiteral("arrows2.sharpArrow2")},
            {QStringLiteral("stripedarrow"), QStringLiteral("arrows2.stripedArrow")},
            {QStringLiteral("stylisedarrow"), QStringLiteral("arrows2.stylisedArrow")},
            {QStringLiteral("tailedarrow"), QStringLiteral("arrows2.tailedArrow")},
            {QStringLiteral("tailednotchedarrow"), QStringLiteral("arrows2.tailedNotchedArrow")},
            {QStringLiteral("triadarrow"), QStringLiteral("arrows2.triadArrow")},
            {QStringLiteral("twowayarrow"), QStringLiteral("arrows2.twoWayArrow")},
            {QStringLiteral("uturnarrow"), QStringLiteral("arrows2.uTurnArrow")},
            {QStringLiteral("wedgearrow"), QStringLiteral("arrows2.wedgeArrow")},
            {QStringLiteral("wedgearrowdashed2"), QStringLiteral("arrows2.wedgeArrowDashed2")},
        };

        const QString suffix = shape.mid(QStringLiteral("mxgraph.arrows2.").size()).toLower();
        const auto it = arrows2.constFind(suffix);

        return it != arrows2.constEnd() ? it.value() : shape.toLower();
    }

    // flowchart.xml registers every stencil as mxgraph.flowchart.<name> with
    // blanks replaced by underscores and everything lower cased (Graph.js,
    // mxStencilRegistry.loadStencilSet). Keep the stencil name verbatim so
    // that only the names draw.io really registers resolve to a shape.
    const QString flowchartPrefix = QStringLiteral("mxgraph.flowchart.");

    if (shape.startsWith(flowchartPrefix, Qt::CaseInsensitive))
    {
        QString name = shape.mid(flowchartPrefix.size()).toLower();
        name.replace(QLatin1Char(' '), QLatin1Char('_'));
        return QStringLiteral("flowchart.") + name;
    }

    const QString prefix = QStringLiteral("mxgraph.basic.");

    if (shape.startsWith(prefix, Qt::CaseInsensitive))
    {
        // Every name registered under mxgraph.basic keeps a stable key. Names
        // draw.io does not register fall through to the rectangle fallback.
        QString name = shape.mid(prefix.size()).toLower();
        name.replace(QLatin1Char(' '), QLatin1Char('_'));
        return QStringLiteral("basic.") + name;
    }

    // Bare names used by older revisions of the bundled shape library. Only
    // aliases of registered Basic shapes are listed: core shapes such as
    // cloud, note, document or cross keep their own implementations.
    static const QHash<QString, QString> aliases = {
        {QStringLiteral("cloudCallout"), QStringLiteral("basic.cloud_callout")},
        {QStringLiteral("cloudRect"), QStringLiteral("basic.cloud_rect")},
        {QStringLiteral("cone"), QStringLiteral("basic.cone")},
        {QStringLiteral("cone2"), QStringLiteral("basic.cone2")},
        {QStringLiteral("donut"), QStringLiteral("basic.donut")},
        {QStringLiteral("drop"), QStringLiteral("basic.drop")},
        {QStringLiteral("flash"), QStringLiteral("basic.flash")},
        {QStringLiteral("halfCircle"), QStringLiteral("basic.half_circle")},
        {QStringLiteral("heart"), QStringLiteral("basic.heart")},
        {QStringLiteral("loudCallout"), QStringLiteral("basic.loud_callout")},
        {QStringLiteral("moon"), QStringLiteral("basic.moon")},
        {QStringLiteral("noSymbol"), QStringLiteral("basic.no_symbol")},
        {QStringLiteral("ovalCallout"), QStringLiteral("basic.oval_callout")},
        {QStringLiteral("pointedOval"), QStringLiteral("basic.pointed_oval")},
        {QStringLiteral("pyramid"), QStringLiteral("basic.pyramid")},
        {QStringLiteral("smiley"), QStringLiteral("basic.smiley")},
        {QStringLiteral("star6"), QStringLiteral("basic.6_point_star")},
        {QStringLiteral("star8"), QStringLiteral("basic.8_point_star")},
        {QStringLiteral("sun"), QStringLiteral("basic.sun")},
        {QStringLiteral("tick"), QStringLiteral("basic.tick")},
        {QStringLiteral("wave"), QStringLiteral("basic.wave2")},
        {QStringLiteral("x"), QStringLiteral("basic.x")},
        {QStringLiteral("button"), QStringLiteral("basic.button")},
        {QStringLiteral("shadedButton"), QStringLiteral("basic.shaded_button")},
        {QStringLiteral("octagon"), QStringLiteral("basic.octagon2")},
        {QStringLiteral("pentagon"), QStringLiteral("basic.pentagon")},
        {QStringLiteral("polygon"), QStringLiteral("basic.polygon")},
        {QStringLiteral("acuteTriangle"), QStringLiteral("basic.acute_triangle")},
        {QStringLiteral("obtuseTriangle"), QStringLiteral("basic.obtuse_triangle")},
        {QStringLiteral("orthogonalTriangle"), QStringLiteral("basic.orthogonal_triangle")},
        {QStringLiteral("diagSnipRect"), QStringLiteral("basic.diag_snip_rect")},
        {QStringLiteral("diagRoundRect"), QStringLiteral("basic.diag_round_rect")},
        {QStringLiteral("cornerRoundRect"), QStringLiteral("basic.corner_round_rect")},
        {QStringLiteral("threeCornerRoundRect"), QStringLiteral("basic.three_corner_round_rect")},
        {QStringLiteral("plaque"), QStringLiteral("basic.plaque")},
        {QStringLiteral("frame"), QStringLiteral("basic.frame")},
        {QStringLiteral("roundedFrame"), QStringLiteral("basic.rounded_frame")},
        {QStringLiteral("plaqueFrame"), QStringLiteral("basic.plaque_frame")},
        {QStringLiteral("frameCorner"), QStringLiteral("basic.frame_corner")},
        {QStringLiteral("diagStripe"), QStringLiteral("basic.diag_stripe")},
        {QStringLiteral("rectCallout"), QStringLiteral("basic.rectcallout")},
        {QStringLiteral("roundRectCallout"), QStringLiteral("basic.roundrectcallout")},
        {QStringLiteral("layeredRect"), QStringLiteral("basic.layered_rect")},
        {QStringLiteral("patternFillRect"), QStringLiteral("basic.patternfillrect")},
    };

    const auto alias = aliases.constFind(shape);

    if (alias != aliases.constEnd())
    {
        return alias.value();
    }

    // Other stencil sets are matched by their lower case draw.io names.
    return shape.startsWith(QLatin1String("mxgraph."), Qt::CaseInsensitive)
        ? shape.toLower() : shape;
}

QString shapeKey(const Cell &cell)
{
    // Named styles such as `label` / `icon` expand to shape=label in
    // draw.io's stylesheet, so resolve the defaults before reading the key.
    const QString shape = normalizeStencilKey(
        Style::value(Style::withDefaults(cell.style), QStringLiteral("shape")).trimmed());

    if (!shape.isEmpty())
    {
        return shape;
    }

    if (cell.styleFlag(QStringLiteral("ellipse")))
    {
        return QStringLiteral("ellipse");
    }

    if (cell.styleFlag(QStringLiteral("rhombus")))
    {
        return QStringLiteral("rhombus");
    }

    if (cell.styleFlag(QStringLiteral("triangle")))
    {
        return QStringLiteral("triangle");
    }

    if (cell.styleFlag(QStringLiteral("text")))
    {
        return QStringLiteral("text");
    }

    if (cell.styleFlag(QStringLiteral("line")))
    {
        return QStringLiteral("line");
    }

    if (cell.styleFlag(QStringLiteral("group")))
    {
        return QStringLiteral("group");
    }

    if (cell.styleFlag(QStringLiteral("swimlane")))
    {
        return QStringLiteral("swimlane");
    }

    if (cell.styleFlag(QStringLiteral("image")))
    {
        return QStringLiteral("image");
    }

    if (cell.styleFlag(QStringLiteral("rounded")))
    {
        return QStringLiteral("rounded");
    }

    return QStringLiteral("rect");
}

double arcSizeFor(const StyleMap &style)
{
    const double arcSize = Style::number(style, QStringLiteral("arcSize"), 15.0);
    return qBound(0.0, arcSize, 50.0) / 100.0;
}

/// Corner radius of mxRectangleShape.paintBackground(): either a percentage
/// of the smaller side or, with absoluteArcSize=1, half of arcSize in pixels.
double cornerRadiusFor(const StyleMap &style, double w, double h)
{
    if (Style::flag(style, QStringLiteral("absoluteArcSize")))
    {
        return qMin(qMin(w, h) / 2.0,
                    Style::number(style, QStringLiteral("arcSize"), 15.0) / 2.0);
    }

    return qMin(w, h) * arcSizeFor(style);
}

/// Corner spacing used by mxShape.addPoints(): draw.io divides the style's
/// arcSize by two (mxConstants.LINE_ARCSIZE is 20).
double pointArcSizeFor(const StyleMap &style)
{
    return qMax(0.0, Style::number(style, QStringLiteral("arcSize"), 20.0) / 2.0);
}

/// Port of mxShape.addPoints(): draws the polygon \a pts, optionally with
/// rounded corners (a virtual waypoint is inserted before the first point,
/// exactly like draw.io does for closed shapes).
void addPoints(QPainterPath &path, QVector<QPointF> pts, bool rounded,
               double arcSize, bool close)
{
    if (pts.isEmpty())
    {
        return;
    }

    const QPointF last = pts.last();

    if (close && rounded)
    {
        const QPointF first = pts.first();
        pts.prepend(QPointF(last.x() + (first.x() - last.x()) / 2.0,
                            last.y() + (first.y() - last.y()) / 2.0));
    }

    const int count = pts.size();
    QPointF current = pts.first();
    int index = 1;

    path.moveTo(current);

    while (index < (close ? count : count - 1))
    {
        QPointF tmp = pts.at(index % count);
        double dx = current.x() - tmp.x();
        double dy = current.y() - tmp.y();

        if (rounded && (dx != 0.0 || dy != 0.0))
        {
            double distance = std::sqrt(dx * dx + dy * dy);
            const double offset = qMin(arcSize, distance / 2.0);
            path.lineTo(tmp.x() + dx * offset / distance,
                        tmp.y() + dy * offset / distance);

            QPointF next = pts.at((index + 1) % count);

            while (index < count - 2 && qRound(next.x() - tmp.x()) == 0
                   && qRound(next.y() - tmp.y()) == 0)
            {
                next = pts.at((index + 2) % count);
                ++index;
            }

            dx = next.x() - tmp.x();
            dy = next.y() - tmp.y();
            distance = qMax(1.0, std::sqrt(dx * dx + dy * dy));
            const double nextOffset = qMin(arcSize, distance / 2.0);
            const QPointF end(tmp.x() + dx * nextOffset / distance,
                              tmp.y() + dy * nextOffset / distance);
            path.quadTo(tmp, end);
            tmp = end;
        }
        else
        {
            path.lineTo(tmp);
        }

        current = tmp;
        ++index;
    }

    if (close)
    {
        path.closeSubpath();
    }
    else
    {
        path.lineTo(last);
    }
}

/// mxStencil.computeAspect() for an aspect="variable" stencil: the stencil's
/// own coordinate system (<shape w= h=>) is scaled onto the cell rect.
struct StencilFrame
{
    double x = 0.0;
    double y = 0.0;
    double sx = 1.0;
    double sy = 1.0;

    QPointF p(double px, double py) const
    {
        return QPointF(x + px * sx, y + py * sy);
    }
};

StencilFrame stencilFrame(double x, double y, double w, double h,
                          double stencilWidth, double stencilHeight)
{
    StencilFrame frame;
    frame.x = x;
    frame.y = y;
    frame.sx = w / stencilWidth;
    frame.sy = h / stencilHeight;
    return frame;
}

} // namespace


/// Appends an SVG-style elliptical arc (same semantics as mxStencil's <arc>).
void addSvgArc(QPainterPath &path, const QPointF &from, const QPointF &to,
               double rx, double ry, bool largeArc, bool sweep)
{
    if (qFuzzyCompare(from.x(), to.x()) && qFuzzyCompare(from.y(), to.y()))
    {
        return;
    }

    rx = qAbs(rx);
    ry = qAbs(ry);

    const double dx2 = (from.x() - to.x()) / 2.0;
    const double dy2 = (from.y() - to.y()) / 2.0;
    const double lambda = (dx2 * dx2) / (rx * rx) + (dy2 * dy2) / (ry * ry);

    if (lambda > 1.0)
    {
        const double scale = std::sqrt(lambda);
        rx *= scale;
        ry *= scale;
    }

    const double num = rx * rx * ry * ry - rx * rx * dy2 * dy2 - ry * ry * dx2 * dx2;
    const double den = rx * rx * dy2 * dy2 + ry * ry * dx2 * dx2;
    const double factor = std::sqrt(qMax(0.0, num / den));

    const QPointF mid((from.x() + to.x()) / 2.0, (from.y() + to.y()) / 2.0);
    const QPointF offset(factor * (rx * dy2) / ry, factor * -(ry * dx2) / rx);

    // Both candidate centres are valid for the radii; the flags pick one.
    bool found = false;
    QPointF center;
    double startAngle = 0.0;
    double delta = 0.0;

    for (double sign : {1.0, -1.0})
    {
        const QPointF candidate = mid + offset * sign;
        const double a0 = std::atan2((from.y() - candidate.y()) / ry,
                                     (from.x() - candidate.x()) / rx);
        const double a1 = std::atan2((to.y() - candidate.y()) / ry,
                                     (to.x() - candidate.x()) / rx);
        double d = a1 - a0;

        if (sweep && d < 0.0)
        {
            d += 2.0 * M_PI;
        }
        else if (!sweep && d > 0.0)
        {
            d -= 2.0 * M_PI;
        }

        // A half ellipse (|d| == pi) satisfies both flags, which is what
        // draw.io's mxUtils.arcToCurves() produces for shapes such as
        // "Direct Data" whose end points are diametrically opposed.
        const double magnitude = qAbs(d);
        const bool halfArc = qAbs(magnitude - M_PI) < 1e-9;

        if ((magnitude > M_PI) == largeArc || halfArc)
        {
            center = candidate;
            startAngle = a0;
            delta = d;
            found = true;
            break;
        }
    }

    if (!found)
    {
        path.lineTo(to);
        return;
    }

    const int steps = qMax(8, static_cast<int>(qAbs(delta) * 24.0));

    for (int i = 1; i <= steps; ++i)
    {
        const double angle = startAngle + delta * (static_cast<double>(i) / steps);
        path.lineTo(center.x() + rx * std::cos(angle), center.y() + ry * std::sin(angle));
    }
}

namespace {

// ---------------------------------------------------------------------------
// draw.io "Basic" palette geometry.
//
// The stencil set (stencils/basic.xml) is stored as a command stream: the
// coordinates are the stencil's own units and are scaled onto the cell rect,
// exactly like mxStencil.computeAspect() does. The registered shapes are
// direct ports of the paintVertexShape() bodies in shapes/mxBasic.js and
// grapheditor/Shapes.js.
// ---------------------------------------------------------------------------

enum StencilOp
{
    OpMove = 0,
    OpLine,
    OpCurve,
    OpQuad,
    OpArc,
    OpEllipse,
    OpRect,
    OpRoundRect,
    OpClose,
    OpFillStroke,
    OpStroke,
    OpFill,
    OpSave,
    OpRestore,
    OpStrokeWidth,
    OpMiterLimit,
    OpLineJoin
};

struct StencilCommand
{
    int op;
    double v[7];
};

struct StencilDef
{
    const char *key;
    double w;
    double h;
    const StencilCommand *commands;
    int count;
};

// Generated from draw.io stencils/basic.xml (mxgraph.basic stencil set).
// op: 0 move, 1 line, 2 curve, 3 quad, 4 svg arc, 5 ellipse, 6 rect,
// 7 roundrect, 8 close, 9 fillstroke, 10 stroke, 11 fill, 12 save,
// 13 restore, 14 strokewidth, 15 miterlimit, 16 linejoin.
const StencilCommand k4PointStar[] = {
    {0,46,0,0,0,0,0,0},{1,56,36,0,0,0,0,0},{1,92,46,0,0,0,0,0},{1,56,56,0,0,0,0,0},
    {1,46,92,0,0,0,0,0},{1,36,56,0,0,0,0,0},{1,0,46,0,0,0,0,0},{1,36,36,0,0,0,0,0},
    {8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand k6PointStar[] = {
    {0,23,28.9,0,0,0,0,0},{1,23,0,0,0,0,0,0},{1,48,14.4,0,0,0,0,0},{1,73,0,0,0,0,0,0},
    {1,73,28.9,0,0,0,0,0},{1,96,42.2,0,0,0,0,0},{1,73,55.6,0,0,0,0,0},{1,73,84.5,0,0,0,0,0},
    {1,48,70,0,0,0,0,0},{1,23,84.5,0,0,0,0,0},{1,23,55.6,0,0,0,0,0},{1,0,42.2,0,0,0,0,0},
    {8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand k8PointStar[] = {
    {0,28,28,0,0,0,0,0},{1,28,0,0,0,0,0,0},{1,48,20,0,0,0,0,0},{1,68,0,0,0,0,0,0},
    {1,68,28,0,0,0,0,0},{1,96,28,0,0,0,0,0},{1,76,48,0,0,0,0,0},{1,96,68,0,0,0,0,0},
    {1,68,68,0,0,0,0,0},{1,68,96,0,0,0,0,0},{1,48,76,0,0,0,0,0},{1,28,96,0,0,0,0,0},
    {1,28,68,0,0,0,0,0},{1,0,68,0,0,0,0,0},{1,20,48,0,0,0,0,0},{1,0,28,0,0,0,0,0},
    {8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kBanner[] = {
    {0,0,50,0,0,0,0,0},{1,38,50,0,0,0,0,0},{4,2.5,2.5,0,0,0,40.5,47.5},{1,40.5,40,0,0,0,0,0},
    {1,55.5,40,0,0,0,0,0},{1,55.5,47.5,0,0,0,0,0},{4,2.5,2.5,0,0,0,58,50},{1,96,50,0,0,0,0,0},
    {1,83,30,0,0,0,0,0},{1,96,10,0,0,0,0,0},{1,70.5,10,0,0,0,0,0},{1,70.5,2.5,0,0,0,0,0},
    {4,2.5,2.5,0,0,0,68,0},{1,28,0,0,0,0,0,0},{4,2.5,2.5,0,0,0,25.5,2.5},{1,25.5,10,0,0,0,0,0},
    {1,0,10,0,0,0,0,0},{1,13,30,0,0,0,0,0},{8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
    {0,40.5,47.5,0,0,0,0,0},{4,2.5,2.5,0,0,0,38,45},{1,28,45,0,0,0,0,0},{4,2.5,2.5,0,1,1,28,40},
    {1,68,40,0,0,0,0,0},{4,2.5,2.5,0,0,1,68,45},{1,58,45,0,0,0,0,0},{4,2.5,2.5,0,0,0,55.5,47.5},
    {0,25.5,42.5,0,0,0,0,0},{1,25.5,10,0,0,0,0,0},{0,70.5,42.5,0,0,0,0,0},{1,70.5,10,0,0,0,0,0},
    {10,0,0,0,0,0,0,0},
};

const StencilCommand kCloudCallout[] = {
    {12,0,0,0,0,0,0,0},{0,12.1,31.8,0,0,0,0,0},{4,8,8,0,1,1,12.1,16.8},{4,12,12,0,0,1,33.1,8.8},
    {4,14,14,0,0,1,59.1,8.8},{4,12,12,0,0,1,79.1,16.8},{4,8,8,0,1,1,79.1,31.8},{4,12,12,0,0,1,58.1,38.8},
    {4,14,14,0,0,1,34.1,38.8},{4,10,8,0,0,1,12.1,31.8},{8,0,0,0,0,0,0,0},{16,1,0,0,0,0,0,0},
    {9,0,0,0,0,0,0,0},{13,0,0,0,0,0,0,0},{6,0,0,0,0,0,0,0},{10,0,0,0,0,0,0,0},
    {5,9.1,46.1,12,5.4,0,0,0},{9,0,0,0,0,0,0,0},{5,4.3,53.5,7.6,3.6,0,0,0},{9,0,0,0,0,0,0,0},
    {5,0,58.8,4.8,2.6,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kCloudRect[] = {
    {0,8.46,0,0,0,0,0,0},{2,3.79,0,0,3.79,0,8.46,0},{2,0,11.02,1.17,13.44,3.16,15.05,0},{2,1.16,16.65,0,19.08,0,21.64,0},
    {2,0,24.2,1.17,26.62,3.16,28.22,0},{2,1.16,29.83,0,32.25,0,34.81,0},{2,0,37.38,1.17,39.8,3.16,41.4,0},{2,1.16,43.01,0,45.43,0,47.99,0},
    {2,0,50.55,1.17,52.98,3.16,54.58,0},{2,1.16,56.18,0,58.61,0,61.17,0},{2,0,63.73,1.17,66.15,3.16,67.76,0},{2,1.16,69.36,0,71.79,0,74.35,0},
    {2,0,76.91,1.17,79.33,3.16,80.93,0},{2,1.16,82.54,0,84.96,0,87.53,0},{2,0,90.09,1.17,92.51,3.16,94.11,0},{2,1.16,95.71,0,98.14,0,100.7,0},
    {2,0,103.26,1.17,105.68,3.16,107.29,0},{2,1.16,108.89,0,111.32,0,113.88,0},{2,0,116.44,1.17,118.86,3.16,120.47,0},{2,1.16,122.07,0,124.49,0,127.06,0},
    {2,0,129.62,1.17,132.04,3.16,133.64,0},{2,1.16,135.25,0,137.67,0,140.23,0},{2,0,142.79,1.17,145.22,3.16,146.82,0},{2,1.16,148.42,0,150.85,0,153.41,0},
    {2,0,155.97,1.17,158.39,3.16,160,0},{2,1.16,161.6,0,164.03,0,166.59,0},{2,0,171.26,3.79,175.05,8.46,175.05,0},{2,11.29,175.05,13.94,173.63,15.5,171.27,0},
    {2,17.07,173.63,19.71,175.05,22.55,175.05,0},{2,25.34,175.05,27.95,173.67,29.53,171.37,0},{2,31.1,173.67,33.71,175.05,36.5,175.05,0},{2,39.29,175.05,41.9,173.67,43.48,171.37,0},
    {2,45.05,173.67,47.66,175.05,50.45,175.05,0},{2,53.24,175.05,55.85,173.67,57.43,171.37,0},{2,59,173.67,61.61,175.05,64.4,175.05,0},{2,67.19,175.05,69.8,173.67,71.38,171.37,0},
    {2,72.95,173.67,75.56,175.05,78.35,175.05,0},{2,81.14,175.05,83.75,173.67,85.33,171.37,0},{2,86.91,173.67,89.51,175.05,92.3,175.05,0},{2,95.09,175.05,97.7,173.67,99.28,171.37,0},
    {2,100.86,173.67,103.46,175.05,106.25,175.05,0},{2,109.04,175.05,111.65,173.67,113.23,171.37,0},{2,114.81,173.67,117.41,175.05,120.2,175.05,0},{2,122.99,175.05,125.61,173.67,127.18,171.37,0},
    {2,128.76,173.67,131.36,175.05,134.15,175.05,0},{2,136.94,175.05,139.56,173.67,141.13,171.37,0},{2,142.71,173.67,145.32,175.05,148.1,175.05,0},{2,150.9,175.05,153.51,173.67,155.08,171.37,0},
    {2,156.66,173.67,159.27,175.05,162.06,175.05,0},{2,164.85,175.05,167.46,173.67,169.03,171.37,0},{2,170.61,173.67,173.22,175.05,176,175.05,0},{2,178.8,175.05,181.41,173.67,182.98,171.37,0},
    {2,184.56,173.67,187.17,175.05,189.96,175.05,0},{2,192.8,175.05,195.45,173.61,197.02,171.24,0},{2,198.58,173.62,201.23,175.05,204.08,175.05,0},{2,208.75,175.05,212.54,171.26,212.54,166.59,0},
    {2,212.54,164.03,211.38,161.61,209.38,160,0},{2,211.38,158.4,212.54,155.97,212.54,153.41,0},{2,212.54,150.85,211.38,148.43,209.38,146.83,0},{2,211.38,145.22,212.54,142.8,212.54,140.23,0},
    {2,212.54,137.67,211.38,135.25,209.38,133.65,0},{2,211.38,132.04,212.54,129.62,212.54,127.06,0},{2,212.54,124.5,211.38,122.08,209.38,120.47,0},{2,211.38,118.87,212.54,116.44,212.54,113.88,0},
    {2,212.54,111.32,211.38,108.9,209.38,107.29,0},{2,211.38,105.69,212.54,103.26,212.54,100.7,0},{2,212.54,98.14,211.38,95.72,209.38,94.12,0},{2,211.38,92.51,212.54,90.09,212.54,87.53,0},
    {2,212.54,84.97,211.38,82.54,209.38,80.94,0},{2,211.38,79.33,212.54,76.91,212.54,74.35,0},{2,212.54,71.79,211.38,69.37,209.38,67.76,0},{2,211.38,66.16,212.54,63.73,212.54,61.17,0},
    {2,212.54,58.61,211.38,56.19,209.38,54.58,0},{2,211.38,52.98,212.54,50.56,212.54,47.99,0},{2,212.54,45.43,211.38,43.01,209.38,41.41,0},{2,211.38,39.8,212.54,37.38,212.54,34.81,0},
    {2,212.54,32.25,211.38,29.83,209.38,28.23,0},{2,211.38,26.62,212.54,24.2,212.54,21.64,0},{2,212.54,19.08,211.38,16.66,209.38,15.05,0},{2,211.38,13.45,212.54,11.02,212.54,8.46,0},
    {2,212.54,3.79,208.75,0,204.08,0,0},{2,201.24,0,198.58,1.44,197.02,3.81,0},{2,195.46,1.43,192.8,0,189.96,0,0},{2,187.16,0,184.55,1.38,182.98,3.68,0},
    {2,181.4,1.38,178.79,0,176,0,0},{2,173.21,0,170.6,1.38,169.02,3.68,0},{2,167.45,1.38,164.84,0,162.06,0,0},{2,159.26,0,156.65,1.38,155.07,3.68,0},
    {2,153.5,1.38,150.89,0,148.1,0,0},{2,145.31,0,142.7,1.38,141.12,3.68,0},{2,139.55,1.38,136.94,0,134.15,0,0},{2,131.36,0,128.75,1.38,127.17,3.68,0},
    {2,125.6,1.38,122.99,0,120.2,0,0},{2,117.41,0,114.8,1.38,113.22,3.68,0},{2,111.65,1.38,109.04,0,106.25,0,0},{2,103.46,0,100.85,1.38,99.27,3.68,0},
    {2,97.7,1.38,95.09,0,92.3,0,0},{2,89.51,0,86.9,1.38,85.32,3.68,0},{2,83.75,1.38,81.14,0,78.35,0,0},{2,75.56,0,72.95,1.38,71.37,3.68,0},
    {2,69.79,1.38,67.19,0,64.4,0,0},{2,61.61,0,59,1.38,57.42,3.68,0},{2,55.84,1.38,53.24,0,50.45,0,0},{2,47.66,0,45.05,1.38,43.47,3.68,0},
    {2,41.89,1.38,39.29,0,36.5,0,0},{2,33.71,0,31.09,1.38,29.52,3.68,0},{2,27.94,1.38,25.34,0,22.55,0,0},{2,19.71,0,17.07,1.42,15.5,3.78,0},
    {2,13.94,1.42,11.29,0,8.46,0,0},{8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kCone[] = {
    {0,49.5,0,0,0,0,0,0},{1,99,88,0,0,0,0,0},{4,25,4.5,0,1,1,0,88},{8,0,0,0,0,0,0,0},
    {9,0,0,0,0,0,0,0},{0,0,88,0,0,0,0,0},{4,25,4.5,0,1,1,99,88},{9,0,0,0,0,0,0,0},
};

const StencilCommand kCross[] = {
    {0,0,34,0,0,0,0,0},{1,34,34,0,0,0,0,0},{1,34,0,0,0,0,0,0},{1,64,0,0,0,0,0,0},
    {1,64,34,0,0,0,0,0},{1,98,34,0,0,0,0,0},{1,98,64,0,0,0,0,0},{1,64,64,0,0,0,0,0},
    {1,64,98,0,0,0,0,0},{1,34,98,0,0,0,0,0},{1,34,64,0,0,0,0,0},{1,0,64,0,0,0,0,0},
    {8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kDocument[] = {
    {0,98,14,0,0,0,0,0},{1,98,98,0,0,0,0,0},{1,0,98,0,0,0,0,0},{1,0,0,0,0,0,0,0},
    {1,84,0,0,0,0,0,0},{8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},{0,84,0,0,0,0,0,0},
    {4,18,10,0,0,1,79,9},{1,98,14.3,0,0,0,0,0},{10,0,0,0,0,0,0,0},
};

const StencilCommand kFlash[] = {
    {0,0,95.5,0,0,0,0,0},{1,20,75.5,0,0,0,0,0},{1,3,61.5,0,0,0,0,0},{1,20,49.5,0,0,0,0,0},
    {1,3,31.5,0,0,0,0,0},{1,34,0,0,0,0,0,0},{1,60,25.5,0,0,0,0,0},{1,36,39.5,0,0,0,0,0},
    {1,50,53.5,0,0,0,0,0},{1,29,65.5,0,0,0,0,0},{1,42,76.5,0,0,0,0,0},{8,0,0,0,0,0,0,0},
    {15,6,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kHalfCircle[] = {
    {0,0,0,0,0,0,0,0},{4,44.5,44.5,0,1,0,98,0},{8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kHeart[] = {
    {0,51.94,94.74,0,0,0,0,0},{2,55.79,90.78,77.8,68.16,91.56,54.03,0},{2,103.89,41.37,103.62,22.91,92.42,11.46,0},{2,81.21,0,63.09,0.05,51.94,11.56,0},
    {2,40.79,0.05,22.67,0,11.47,11.45,0},{2,0.26,22.9,0,41.36,12.32,54.03,0},{2,26.08,68.16,48.09,90.78,51.94,94.74,0},{8,0,0,0,0,0,0,0},
    {9,0,0,0,0,0,0,0},
};

const StencilCommand kLoudCallout[] = {
    {0,14.9,43.9,0,0,0,0,0},{1,9.3,46.7,0,0,0,0,0},{1,11.1,40.9,0,0,0,0,0},{1,6.6,43.9,0,0,0,0,0},
    {1,8.3,39.2,0,0,0,0,0},{1,2.8,40.8,0,0,0,0,0},{1,6.6,36.4,0,0,0,0,0},{1,0.9,36.2,0,0,0,0,0},
    {1,5.8,32.7,0,0,0,0,0},{1,0,30.8,0,0,0,0,0},{1,5.3,28.2,0,0,0,0,0},{1,0.3,25.6,0,0,0,0,0},
    {1,5.9,24.19,0,0,0,0,0},{1,0.8,19.9,0,0,0,0,0},{1,6.5,19.8,0,0,0,0,0},{1,2.8,15.1,0,0,0,0,0},
    {1,8.2,16.1,0,0,0,0,0},{1,5.9,11.3,0,0,0,0,0},{1,11.5,13.2,0,0,0,0,0},{1,10.2,8.7,0,0,0,0,0},
    {1,15.7,10.6,0,0,0,0,0},{1,14.9,6.15,0,0,0,0,0},{1,19.2,9.3,0,0,0,0,0},{1,19.8,4.3,0,0,0,0,0},
    {1,23.4,8,0,0,0,0,0},{1,23.8,3.4,0,0,0,0,0},{1,28.5,6.9,0,0,0,0,0},{1,30.3,1.3,0,0,0,0,0},
    {1,33.3,6.2,0,0,0,0,0},{1,34.7,0.6,0,0,0,0,0},{1,38.2,6,0,0,0,0,0},{1,40.6,0,0,0,0,0,0},
    {1,42.8,5.8,0,0,0,0,0},{1,45.6,0,0,0,0,0,0},{1,47.1,6,0,0,0,0,0},{1,51.3,1,0,0,0,0,0},
    {1,50.8,6.3,0,0,0,0,0},{1,55.4,0.6,0,0,0,0,0},{1,55.1,6.6,0,0,0,0,0},{1,60.5,1.4,0,0,0,0,0},
    {1,61.1,7.1,0,0,0,0,0},{1,66.1,2.7,0,0,0,0,0},{1,66.2,8.7,0,0,0,0,0},{1,71.9,4.4,0,0,0,0,0},
    {1,70.5,10,0,0,0,0,0},{1,77.6,6.2,0,0,0,0,0},{1,74.9,11.8,0,0,0,0,0},{1,83.9,7.8,0,0,0,0,0},
    {1,80.1,13.6,0,0,0,0,0},{1,88.1,11.9,0,0,0,0,0},{1,85.2,17,0,0,0,0,0},{1,91.2,16.9,0,0,0,0,0},
    {1,87,20.1,0,0,0,0,0},{1,93.3,21.2,0,0,0,0,0},{1,87.9,24,0,0,0,0,0},{1,93.2,25.8,0,0,0,0,0},
    {1,86.8,26.8,0,0,0,0,0},{1,92.4,30.3,0,0,0,0,0},{1,86.6,30.8,0,0,0,0,0},{1,90.9,34.8,0,0,0,0,0},
    {1,84.2,33.5,0,0,0,0,0},{1,87.8,38.8,0,0,0,0,0},{1,82,36.6,0,0,0,0,0},{1,84.7,41.7,0,0,0,0,0},
    {1,79.2,40.7,0,0,0,0,0},{1,79.8,46,0,0,0,0,0},{1,76.3,42.9,0,0,0,0,0},{1,75.6,48.6,0,0,0,0,0},
    {1,72,44.7,0,0,0,0,0},{1,71.7,51.2,0,0,0,0,0},{1,68,46,0,0,0,0,0},{1,66.2,52.1,0,0,0,0,0},
    {1,63.7,46.6,0,0,0,0,0},{1,61.2,53.7,0,0,0,0,0},{1,59.7,47.6,0,0,0,0,0},{1,56.9,53.8,0,0,0,0,0},
    {1,55,48.1,0,0,0,0,0},{1,52.8,53.9,0,0,0,0,0},{1,50.9,48.1,0,0,0,0,0},{1,48.4,54.5,0,0,0,0,0},
    {1,47,48.1,0,0,0,0,0},{1,44.4,53.7,0,0,0,0,0},{1,43.2,47.4,0,0,0,0,0},{1,40.1,54.2,0,0,0,0,0},
    {1,38.8,47.4,0,0,0,0,0},{1,36.3,54.7,0,0,0,0,0},{1,35.6,47.8,0,0,0,0,0},{1,32.4,55.1,0,0,0,0,0},
    {1,30.9,46.6,0,0,0,0,0},{1,28.6,53.3,0,0,0,0,0},{1,26.8,47.8,0,0,0,0,0},{1,3.8,59.9,0,0,0,0,0},
    {8,0,0,0,0,0,0,0},{15,10,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kMoon[] = {
    {0,37.05,0,0,0,0,0,0},{4,48,48,0,1,0,77.05,92},{4,60,60,0,0,1,37.05,0},{8,0,0,0,0,0,0,0},
    {9,0,0,0,0,0,0,0},
};

const StencilCommand kNoSymbol[] = {
    {0,0,50,0,0,0,0,0},{4,35,35,0,0,1,100,50},{4,35,35,0,0,1,0,50},{8,0,0,0,0,0,0,0},
    {0,78.95,69.7,0,0,0,0,0},{4,35,35,0,0,0,30.3,21.05},{8,0,0,0,0,0,0,0},{0,21.15,30.3,0,0,0,0,0},
    {4,35,35,0,0,0,69.7,79},{8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kOctagon[] = {
    {0,0,29,0,0,0,0,0},{1,29,0,0,0,0,0,0},{1,69,0,0,0,0,0,0},{1,98,29,0,0,0,0,0},
    {1,98,69,0,0,0,0,0},{1,69,98,0,0,0,0,0},{1,29,98,0,0,0,0,0},{1,0,69,0,0,0,0,0},
    {8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kOrthogonalTriangle[] = {
    {0,0,97,0,0,0,0,0},{1,0,0,0,0,0,0,0},{1,97,97,0,0,0,0,0},{8,0,0,0,0,0,0,0},
    {9,0,0,0,0,0,0,0},
};

const StencilCommand kOvalCallout[] = {
    {0,20.53,46.15,0,0,0,0,0},{4,49,25,0,1,1,31.53,50.15},{4,30,30,0,0,1,9.03,63.15},{4,30,30,0,0,0,20.53,46.15},
    {8,0,0,0,0,0,0,0},{15,15,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kParallelepiped[] = {
    {0,0,60,0,0,0,0,0},{1,23.5,0,0,0,0,0,0},{1,97,0,0,0,0,0,0},{1,73.5,60,0,0,0,0,0},
    {8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kPentagon[] = {
    {0,18.5,90,0,0,0,0,0},{1,0,33,0,0,0,0,0},{1,48.5,0,0,0,0,0,0},{1,97,33,0,0,0,0,0},
    {1,78.5,90,0,0,0,0,0},{8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kPointedOval[] = {
    {0,25.46,0,0,0,0,0,0},{4,75,75,0,0,0,25.46,100},{4,75,75,0,0,0,25.46,0},{8,0,0,0,0,0,0,0},
    {9,0,0,0,0,0,0,0},
};

const StencilCommand kRectangularCallout[] = {
    {0,15,43,0,0,0,0,0},{1,0,43,0,0,0,0,0},{1,0,0,0,0,0,0,0},{1,98,0,0,0,0,0,0},
    {1,98,43,0,0,0,0,0},{1,29,43,0,0,0,0,0},{1,4,60,0,0,0,0,0},{8,0,0,0,0,0,0,0},
    {15,10,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kRoundedRectangularCallout[] = {
    {0,15.5,43,0,0,0,0,0},{1,5,43,0,0,0,0,0},{4,5,5,0,0,1,0,38},{1,0,5,0,0,0,0,0},
    {4,5,5,0,0,1,5,0},{1,93,0,0,0,0,0,0},{4,5,5,0,0,1,98,5},{1,98,38,0,0,0,0,0},
    {4,5,5,0,0,1,93,43},{1,29,43,0,0,0,0,0},{4,30,30,0,0,1,4,60},{4,30,30,0,0,0,15.5,43},
    {8,0,0,0,0,0,0,0},{15,15,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kSmiley[] = {
    {12,0,0,0,0,0,0,0},{5,0,0,98,98,0,0,0},{9,0,0,0,0,0,0,0},{14,2,0,0,0,0,0,0},
    {0,11,54,0,0,0,0,0},{4,38,27,0,1,0,87,54},{10,0,0,0,0,0,0,0},{13,0,0,0,0,0,0,0},
    {6,0,0,0,0,0,0,0},{10,0,0,0,0,0,0,0},{0,16,51,0,0,0,0,0},{1,6,57,0,0,0,0,0},
    {9,0,0,0,0,0,0,0},{0,82,51,0,0,0,0,0},{1,92,57,0,0,0,0,0},{9,0,0,0,0,0,0,0},
    {14,6,0,0,0,0,0,0},{5,24,27,6,16,0,0,0},{9,0,0,0,0,0,0,0},{5,68,27,6,16,0,0,0},
    {9,0,0,0,0,0,0,0},
};

const StencilCommand kStar[] = {
    {0,0,33,0,0,0,0,0},{1,36.4,33,0,0,0,0,0},{1,47.5,0,0,0,0,0,0},{1,58.6,33,0,0,0,0,0},
    {1,95,33,0,0,0,0,0},{1,66,55.1,0,0,0,0,0},{1,77.5,90,0,0,0,0,0},{1,47.5,68.4,0,0,0,0,0},
    {1,17.5,90,0,0,0,0,0},{1,29,55.1,0,0,0,0,0},{8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kSun[] = {
    {5,17.5,17.5,60,60,0,0,0},{9,0,0,0,0,0,0,0},{0,42.5,14.5,0,0,0,0,0},{1,47.5,0,0,0,0,0,0},
    {1,52.5,14.5,0,0,0,0,0},{8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},{0,42.5,80.5,0,0,0,0,0},
    {1,47.5,95,0,0,0,0,0},{1,52.5,80.5,0,0,0,0,0},{8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
    {0,14.5,42.5,0,0,0,0,0},{1,0,47.5,0,0,0,0,0},{1,14.5,52.5,0,0,0,0,0},{8,0,0,0,0,0,0,0},
    {9,0,0,0,0,0,0,0},{0,80.5,42.5,0,0,0,0,0},{1,95,47.5,0,0,0,0,0},{1,80.5,52.5,0,0,0,0,0},
    {8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},{0,67.5,20.5,0,0,0,0,0},{1,81.2,13.9,0,0,0,0,0},
    {1,74.5,27.5,0,0,0,0,0},{8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},{0,67.5,74.5,0,0,0,0,0},
    {1,81.2,81.1,0,0,0,0,0},{1,74.5,67.5,0,0,0,0,0},{8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
    {0,27.5,20.5,0,0,0,0,0},{1,13.8,13.9,0,0,0,0,0},{1,20.5,27.5,0,0,0,0,0},{8,0,0,0,0,0,0,0},
    {9,0,0,0,0,0,0,0},{0,27.5,74.5,0,0,0,0,0},{1,13.8,81.1,0,0,0,0,0},{1,20.5,67.5,0,0,0,0,0},
    {8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kTick[] = {
    {0,0.36,66.69,0,0,0,0,0},{4,12,12,0,0,1,16.36,58.69},{4,20,20,0,0,1,26.36,69.69},{4,200,200,0,0,1,63.36,5.69},
    {4,18,18,0,0,1,80.36,1.69},{4,4.5,4.5,0,0,1,83.36,8.69},{4,230,230,0,0,0,35.36,94.69},{4,20,20,0,0,1,17.36,94.69},
    {4,100,100,0,0,0,0.36,68.69},{4,2,2,0,0,1,0.36,66.69},{8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kTrapezoid[] = {
    {0,0,98,0,0,0,0,0},{1,23.5,0,0,0,0,0,0},{1,73.5,0,0,0,0,0,0},{1,97,98,0,0,0,0,0},
    {8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kWave[] = {
    {0,0,8.7,0,0,0,0,0},{4,20,20,0,0,1,33,8.7},{4,20,20,0,0,0,65,8.7},{4,20,20,0,0,1,98,8.7},
    {1,98,48.7,0,0,0,0,0},{4,20,20,0,0,0,65,48.7},{4,20,20,0,0,1,33,48.7},{4,20,20,0,0,0,0,48.7},
    {8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilCommand kX[] = {
    {0,0,0,0,0,0,0,0},{1,28,0,0,0,0,0,0},{1,48,29,0,0,0,0,0},{1,68,0,0,0,0,0,0},
    {1,96,0,0,0,0,0,0},{1,62,49,0,0,0,0,0},{1,96,98,0,0,0,0,0},{1,68,98,0,0,0,0,0},
    {1,48,69,0,0,0,0,0},{1,28,98,0,0,0,0,0},{1,0,98,0,0,0,0,0},{1,32,49,0,0,0,0,0},
    {8,0,0,0,0,0,0,0},{9,0,0,0,0,0,0,0},
};

const StencilDef kBasicStencils[] = {
    {"basic.4_point_star", 92, 92, k4PointStar, 10},
    {"basic.6_point_star", 96, 84.5, k6PointStar, 14},
    {"basic.8_point_star", 96, 96, k8PointStar, 18},
    {"basic.banner", 96, 50, kBanner, 33},
    {"basic.cloud_callout", 90.41, 61.4, kCloudCallout, 22},
    {"basic.cloud_rect", 212.54, 175.05, kCloudRect, 111},
    {"basic.cone", 99, 96.91, kCone, 8},
    {"basic.cross", 98, 98, kCross, 14},
    {"basic.document", 99, 98, kDocument, 11},
    {"basic.flash", 60, 95.5, kFlash, 14},
    {"basic.half_circle", 98, 49, kHalfCircle, 4},
    {"basic.heart", 103.89, 94.74, kHeart, 9},
    {"basic.loud_callout", 93.3, 59.9, kLoudCallout, 95},
    {"basic.moon", 77.05, 103.05, kMoon, 5},
    {"basic.no_symbol", 100, 100, kNoSymbol, 11},
    {"basic.octagon", 98, 98, kOctagon, 10},
    {"basic.orthogonal_triangle", 97, 97, kOrthogonalTriangle, 5},
    {"basic.oval_callout", 109.43, 63.15, kOvalCallout, 7},
    {"basic.parallelepiped", 97, 60, kParallelepiped, 6},
    {"basic.pentagon", 97, 90, kPentagon, 7},
    {"basic.pointed_oval", 50.93, 100, kPointedOval, 5},
    {"basic.rectangular_callout", 98, 60, kRectangularCallout, 10},
    {"basic.rounded_rectangular_callout", 98, 60, kRoundedRectangularCallout, 15},
    {"basic.smiley", 98, 98, kSmiley, 21},
    {"basic.star", 95, 90, kStar, 12},
    {"basic.sun", 95, 95, kSun, 42},
    {"basic.tick", 84.4, 97.54, kTick, 12},
    {"basic.trapezoid", 97, 98, kTrapezoid, 6},
    {"basic.wave", 98, 56.7, kWave, 10},
    {"basic.x", 96, 98, kX, 14},
};

struct BasicGeometry
{
    QPainterPath path;
    QVector<ShapeDecoration> decorations;
};

Qt::PenJoinStyle joinStyle(double value)
{
    if (value == 1.0)
    {
        return Qt::RoundJoin;
    }

    if (value == 2.0)
    {
        return Qt::BevelJoin;
    }

    return Qt::MiterJoin;
}

void addArcCommand(QPainterPath &path, const QPointF &to, double rx, double ry,
                   double largeArc, double sweep)
{
    // mxStencil <arc> uses the current point as start; x-axis-rotation is
    // always 0 in the basic stencil set.
    addSvgArc(path, path.currentPosition(), to, rx, ry,
              largeArc != 0.0, sweep != 0.0);
}

const StencilDef *findStencil(const QString &key)
{
    for (const StencilDef &def : kBasicStencils)
    {
        if (key == QLatin1String(def.key))
        {
            return &def;
        }
    }

    return nullptr;
}

/// Runs one stencil description: fill operations become the filled path,
/// stroke operations become decorations (they must not punch holes).
void runStencil(const StencilDef &def, const QRectF &rect, double baseWidth,
                BasicGeometry &out)
{
    const double sx = rect.width() / def.w;
    const double sy = rect.height() / def.h;
    const double minScale = qMin(sx, sy);
    const auto pt = [&](double px, double py)
    {
        return QPointF(rect.x() + px * sx, rect.y() + py * sy);
    };

    QPainterPath buffer;
    bool hasBuffer = false;

    struct State
    {
        double width;
        double miter;
        Qt::PenJoinStyle join;
    };

    State state = { baseWidth, 4.0, Qt::MiterJoin };
    QVector<State> stack;

    const auto addDecoration = [&](const QPainterPath &path, const State &s)
    {
        if (path.isEmpty())
        {
            return;
        }

        ShapeDecoration decoration;
        decoration.path = path;
        decoration.strokeWidth = s.width;
        decoration.join = s.join;
        decoration.miterLimit = s.miter;
        out.decorations.append(decoration);
    };

    const auto flush = [&](bool fill, bool stroke)
    {
        if (buffer.isEmpty())
        {
            return;
        }

        if (fill)
        {
            out.path.addPath(buffer);
        }

        if (stroke && !qFuzzyCompare(state.width, baseWidth))
        {
            // At the default width the outline of the filled path already
            // provides the stroke; adding it again would darken the edge.
            addDecoration(buffer, state);
        }

        buffer = QPainterPath();
        hasBuffer = false;
    };

    for (int i = 0; i < def.count; ++i)
    {
        const StencilCommand &command = def.commands[i];
        const double *v = command.v;

        switch (command.op)
        {
        case OpMove:
            buffer.moveTo(pt(v[0], v[1]));
            hasBuffer = true;
            break;
        case OpLine:
            if (!hasBuffer)
            {
                buffer.moveTo(pt(v[0], v[1]));
                hasBuffer = true;
            }
            else
            {
                buffer.lineTo(pt(v[0], v[1]));
            }
            break;
        case OpCurve:
            buffer.cubicTo(pt(v[0], v[1]), pt(v[2], v[3]), pt(v[4], v[5]));
            break;
        case OpQuad:
            buffer.quadTo(pt(v[0], v[1]), pt(v[2], v[3]));
            break;
        case OpArc:
            addArcCommand(buffer, pt(v[5], v[6]), v[0] * sx, v[1] * sy, v[3], v[4]);
            break;
        case OpEllipse:
            buffer.addEllipse(QRectF(pt(v[0], v[1]),
                                     QSizeF(v[2] * sx, v[3] * sy)));
            hasBuffer = true;
            break;
        case OpRect:
            if (v[2] != 0.0 || v[3] != 0.0)
            {
                buffer.addRect(QRectF(pt(v[0], v[1]),
                                      QSizeF(v[2] * sx, v[3] * sy)));
                hasBuffer = true;
            }
            break;
        case OpRoundRect:
        {
            double arcsize = v[4];

            if (arcsize == 0.0)
            {
                arcsize = 15.0;
            }

            const double rw = v[2] * sx;
            const double rh = v[3] * sy;
            const double r = qMin(rw * arcsize / 100.0, rh * arcsize / 100.0);
            buffer.addRoundedRect(QRectF(pt(v[0], v[1]), QSizeF(rw, rh)), r, r);
            hasBuffer = true;
            break;
        }
        case OpClose:
            if (!buffer.isEmpty())
            {
                buffer.closeSubpath();
            }
            break;
        case OpFillStroke:
            flush(true, true);
            break;
        case OpStroke:
            addDecoration(buffer, state);
            buffer = QPainterPath();
            hasBuffer = false;
            break;
        case OpFill:
            flush(true, false);
            break;
        case OpSave:
            stack.append(state);
            break;
        case OpRestore:
            if (!stack.isEmpty())
            {
                state = stack.takeLast();
            }
            break;
        case OpStrokeWidth:
            state.width = v[0] * (v[1] != 0.0 ? 1.0 : minScale);
            break;
        case OpMiterLimit:
            state.miter = v[0];
            break;
        case OpLineJoin:
            state.join = joinStyle(v[0]);
            break;
        default:
            break;
        }
    }

    flush(true, true);
    out.path.setFillRule(Qt::WindingFill);
}

void addPolygon(const StyleMap &style, double w, double h, BasicGeometry &out)
{
    const QString coordsText = Style::value(style, QStringLiteral("polyCoords"));
    const QJsonDocument document = QJsonDocument::fromJson(coordsText.toUtf8());
    const QJsonArray coords = document.isArray() ? document.array() : QJsonArray();

    if (coords.isEmpty())
    {
        return;
    }

    const bool polyline = Style::flag(style, QStringLiteral("polyline"));
    const auto pointAt = [&](const QJsonValue &value)
    {
        const QJsonArray pair = value.toArray();
        const double px = pair.size() > 0 ? pair.at(0).toDouble() : 0.0;
        const double py = pair.size() > 1 ? pair.at(1).toDouble() : 0.0;
        return QPointF(px * w, py * h);
    };

    out.path.moveTo(pointAt(coords.at(0)));

    for (int i = 1; i < coords.size(); ++i)
    {
        out.path.lineTo(pointAt(coords.at(i)));
    }

    if (!polyline)
    {
        out.path.closeSubpath();
    }
}

void buildBasicShape(const QString &key, const StyleMap &style, const QRectF &rect,
                     BasicGeometry &out)
{
    const double w = rect.width();
    const double h = rect.height();
    const double x = rect.x();
    const double y = rect.y();
    const auto pt = [&](double px, double py) { return QPointF(x + px, y + py); };
    const auto clamp = [](double value, double low, double high)
    {
        return qBound(low, value, high);
    };
    const auto number = [&](const char *name, double fallback)
    {
        return Style::number(style, QString::fromLatin1(name), fallback);
    };

    if (const StencilDef *stencil = findStencil(key))
    {
        runStencil(*stencil, rect,
                   Style::number(style, QStringLiteral("strokeWidth"), 1.0), out);
        return;
    }

    if (key == QLatin1String("basic.rect"))
    {
        out.path.addRect(rect);
        return;
    }

    if (key == QLatin1String("basic.patternfillrect"))
    {
        out.path.addRect(rect);

        const QString fillStyle = Style::value(style, QStringLiteral("fillStyle"),
                                               QStringLiteral("none"));
        const double fillWidth = number("fillStrokeWidth", 1.0);
        double step = number("step", 5.0);

        if (step <= 0.0)
        {
            return;
        }

        QPainterPath lines;

        if (fillStyle == QLatin1String("diag") || fillStyle == QLatin1String("diagGrid"))
        {
            step *= 1.41;

            for (double i = 0.0; i < h + w && lines.elementCount() < 10000; i += step)
            {
                if (i <= h)
                {
                    lines.moveTo(0, i);
                    lines.lineTo(i <= w ? QPointF(i, 0) : QPointF(w, i - w));
                }
                else
                {
                    lines.moveTo(i - h, h);
                    lines.lineTo(i <= w ? QPointF(i, 0) : QPointF(w, i - w));
                }
            }
        }

        if (fillStyle == QLatin1String("vert") || fillStyle == QLatin1String("grid"))
        {
            for (double i = 0.0; i <= w && lines.elementCount() < 10000; i += step)
            {
                lines.moveTo(i, 0);
                lines.lineTo(i, h);
            }
        }

        if (fillStyle == QLatin1String("diagRev") || fillStyle == QLatin1String("diagGrid"))
        {
            if (fillStyle == QLatin1String("diagRev"))
            {
                step *= 1.41;
            }

            for (double i = 0.0; i < h + w && lines.elementCount() < 10000; i += step)
            {
                if (i <= h)
                {
                    lines.moveTo(w, i);
                    lines.lineTo(i <= w ? QPointF(w - i, 0) : QPointF(0, i - w));
                }
                else
                {
                    lines.moveTo(w - i + h, h);
                    lines.lineTo(i <= w ? QPointF(w - i, 0) : QPointF(0, i - w));
                }
            }
        }

        if (fillStyle == QLatin1String("hor") || fillStyle == QLatin1String("grid"))
        {
            for (double i = 0.0; i <= h && lines.elementCount() < 10000; i += step)
            {
                lines.moveTo(0, i);
                lines.lineTo(w, i);
            }
        }

        if (!lines.isEmpty())
        {
            ShapeDecoration decoration;
            decoration.path = lines;
            decoration.strokeWidth = fillWidth;
            decoration.strokeColor = Style::parseColorValue(
                Style::value(style, QStringLiteral("fillStrokeColor"),
                             QStringLiteral("#cccccc")));
            out.decorations.append(decoration);
        }

        return;
    }

    if (key == QLatin1String("basic.polygon"))
    {
        addPolygon(style, w, h, out);
        return;
    }

    if (key == QLatin1String("basic.4_point_star_2"))
    {
        const double dx = 0.5 * clamp(number("dx", 0.8), 0.0, w);

        out.path.moveTo(pt(0, h * 0.5));
        out.path.lineTo(pt(dx * w, dx * h));
        out.path.lineTo(pt(w * 0.5, 0));
        out.path.lineTo(pt(w - dx * w, dx * h));
        out.path.lineTo(pt(w, h * 0.5));
        out.path.lineTo(pt(w - dx * w, h - dx * h));
        out.path.lineTo(pt(w * 0.5, h));
        out.path.lineTo(pt(dx * w, h - dx * h));
        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.isocube"))
    {
        const double angle = clamp(number("isoAngle", 15.0), 0.01, 94.0) * M_PI / 200.0;
        const double isoH = qMin(w * std::tan(angle), h * 0.5);

        out.path.moveTo(pt(w * 0.5, 0));
        out.path.lineTo(pt(w, isoH));
        out.path.lineTo(pt(w, h - isoH));
        out.path.lineTo(pt(w * 0.5, h));
        out.path.lineTo(pt(0, h - isoH));
        out.path.lineTo(pt(0, isoH));
        out.path.closeSubpath();

        QPainterPath lines;
        lines.moveTo(0, isoH);
        lines.lineTo(w * 0.5, 2 * isoH);
        lines.lineTo(w, isoH);
        lines.moveTo(w * 0.5, 2 * isoH);
        lines.lineTo(w * 0.5, h);

        ShapeDecoration decoration;
        decoration.path = lines.translated(x, y);
        out.decorations.append(decoration);
        return;
    }

    if (key == QLatin1String("basic.acute_triangle")
        || key == QLatin1String("basic.obtuse_triangle"))
    {
        const double dx = w * clamp(number("dx", 0.5), 0.0, w);

        if (key == QLatin1String("basic.acute_triangle"))
        {
            out.path.moveTo(pt(0, h));
            out.path.lineTo(pt(dx, 0));
            out.path.lineTo(pt(w, h));
        }
        else
        {
            out.path.moveTo(pt(dx, h));
            out.path.lineTo(pt(0, 0));
            out.path.lineTo(pt(w, h));
        }

        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.drop"))
    {
        const double r = qMin(h, w) * 0.5;
        const double d = h - r;
        const double a = std::sqrt(qMax(0.0, d * d - r * r));
        const double angle = std::atan2(a, r);
        const double x1 = r * std::sin(angle);
        const double y1 = r * std::cos(angle);

        out.path.moveTo(pt(w * 0.5, 0));
        out.path.lineTo(pt(w * 0.5 + x1, h - r - y1));
        addArcCommand(out.path, pt(w * 0.5 + r, h - r), r, r, 0, 1);
        addArcCommand(out.path, pt(w * 0.5, h), r, r, 0, 1);
        addArcCommand(out.path, pt(w * 0.5 - r, h - r), r, r, 0, 1);
        addArcCommand(out.path, pt(w * 0.5 - x1, h - r - y1), r, r, 0, 1);
        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.cone2"))
    {
        const double dx = w * clamp(number("dx", 0.5), 0.0, w);
        const double dy = h * clamp(number("dy", 0.9), 0.0, h);
        const double ry = h - dy;

        out.path.moveTo(pt(dx, 0));

        if (ry > 0.0)
        {
            out.path.lineTo(pt(w, h - ry));
            addArcCommand(out.path, pt(w * 0.5, h), w * 0.5, ry, 0, 1);
            addArcCommand(out.path, pt(0, h - ry), w * 0.5, ry, 0, 1);
        }
        else
        {
            out.path.lineTo(pt(w, h));
            out.path.lineTo(pt(0, h));
        }

        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.pyramid"))
    {
        const double dx1 = w * clamp(number("dx1", 0.5), 0.0, w);
        const double dx2 = w * clamp(number("dx2", 0.6), 0.0, w);
        const double dy1 = h * clamp(number("dy1", 0.9), 0.0, h);
        const double dy2 = h * clamp(number("dy2", 0.8), 0.0, h);

        out.path.moveTo(pt(dx1, 0));
        out.path.lineTo(pt(w, dy2));
        out.path.lineTo(pt(dx2, h));
        out.path.lineTo(pt(0, dy1));
        out.path.closeSubpath();

        ShapeDecoration decoration;
        decoration.path.moveTo(pt(dx1, 0));
        decoration.path.lineTo(pt(dx2, h));
        out.decorations.append(decoration);
        return;
    }

    if (key == QLatin1String("basic.diag_snip_rect")
        || key == QLatin1String("basic.corner_round_rect")
        || key == QLatin1String("basic.three_corner_round_rect")
        || key == QLatin1String("basic.diag_round_rect"))
    {
        double dx = clamp(number("dx", 0.5), 0.0, w) * 2.0;
        dx = qMin(qMin(w * 0.5, h * 0.5), dx);

        if (key == QLatin1String("basic.diag_snip_rect"))
        {
            out.path.moveTo(pt(dx, 0));
            out.path.lineTo(pt(w, 0));
            out.path.lineTo(pt(w, h - dx));
            out.path.lineTo(pt(w - dx, h));
            out.path.lineTo(pt(0, h));
            out.path.lineTo(pt(0, dx));
        }
        else if (key == QLatin1String("basic.corner_round_rect"))
        {
            out.path.moveTo(pt(dx, 0));
            out.path.lineTo(pt(w, 0));
            out.path.lineTo(pt(w, h));
            out.path.lineTo(pt(0, h));
            out.path.lineTo(pt(0, dx));
            addArcCommand(out.path, pt(dx, 0), dx, dx, 0, 1);
        }
        else if (key == QLatin1String("basic.three_corner_round_rect"))
        {
            out.path.moveTo(pt(dx, 0));
            out.path.lineTo(pt(w - dx, 0));
            addArcCommand(out.path, pt(w, dx), dx, dx, 0, 1);
            out.path.lineTo(pt(w, h - dx));
            addArcCommand(out.path, pt(w - dx, h), dx, dx, 0, 1);
            out.path.lineTo(pt(0, h));
            out.path.lineTo(pt(0, dx));
            addArcCommand(out.path, pt(dx, 0), dx, dx, 0, 1);
        }
        else
        {
            out.path.moveTo(pt(dx, 0));
            out.path.lineTo(pt(w, 0));
            out.path.lineTo(pt(w, h - dx));
            addArcCommand(out.path, pt(w - dx, h), dx, dx, 0, 1);
            out.path.lineTo(pt(0, h));
            out.path.lineTo(pt(0, dx));
            addArcCommand(out.path, pt(dx, 0), dx, dx, 0, 1);
        }

        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.plaque"))
    {
        double dx = clamp(number("dx", 0.5), 0.0, w) * 2.0;
        dx = qMin(qMin(w * 0.5, h * 0.5), dx);

        out.path.moveTo(pt(w - dx, 0));
        addArcCommand(out.path, pt(w, dx), dx, dx, 0, 0);
        out.path.lineTo(pt(w, h - dx));
        addArcCommand(out.path, pt(w - dx, h), dx, dx, 0, 0);
        out.path.lineTo(pt(dx, h));
        addArcCommand(out.path, pt(0, h - dx), dx, dx, 0, 0);
        out.path.lineTo(pt(0, dx));
        addArcCommand(out.path, pt(dx, 0), dx, dx, 0, 0);
        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.frame"))
    {
        double dx = clamp(number("dx", 0.5), 0.0, w);
        dx = qMin(qMin(w * 0.5, h * 0.5), dx);

        out.path.moveTo(pt(w, 0));
        out.path.lineTo(pt(w, h));
        out.path.lineTo(pt(0, h));
        out.path.lineTo(pt(0, 0));
        out.path.closeSubpath();
        out.path.moveTo(pt(dx, dx));
        out.path.lineTo(pt(dx, h - dx));
        out.path.lineTo(pt(w - dx, h - dx));
        out.path.lineTo(pt(w - dx, dx));
        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.plaque_frame"))
    {
        double dx = clamp(number("dx", 0.5), 0.0, w);
        dx = qMin(qMin(w * 0.25, h * 0.25), dx);

        out.path.moveTo(pt(w - dx, 0));
        addArcCommand(out.path, pt(w, dx), dx, dx, 0, 0);
        out.path.lineTo(pt(w, h - dx));
        addArcCommand(out.path, pt(w - dx, h), dx, dx, 0, 0);
        out.path.lineTo(pt(dx, h));
        addArcCommand(out.path, pt(0, h - dx), dx, dx, 0, 0);
        out.path.lineTo(pt(0, dx));
        addArcCommand(out.path, pt(dx, 0), dx, dx, 0, 0);
        out.path.closeSubpath();

        out.path.moveTo(pt(dx * 2, dx));
        addArcCommand(out.path, pt(dx, dx * 2), dx * 2, dx * 2, 0, 1);
        out.path.lineTo(pt(dx, h - 2 * dx));
        addArcCommand(out.path, pt(dx * 2, h - dx), dx * 2, dx * 2, 0, 1);
        out.path.lineTo(pt(w - 2 * dx, h - dx));
        addArcCommand(out.path, pt(w - dx, h - 2 * dx), dx * 2, dx * 2, 0, 1);
        out.path.lineTo(pt(w - dx, dx * 2));
        addArcCommand(out.path, pt(w - 2 * dx, dx), dx * 2, dx * 2, 0, 1);
        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.rounded_frame"))
    {
        double dx = clamp(number("dx", 0.5), 0.0, w);
        dx = qMin(qMin(w * 0.25, h * 0.25), dx);

        out.path.moveTo(pt(w - 2 * dx, 0));
        addArcCommand(out.path, pt(w, 2 * dx), dx * 2, dx * 2, 0, 1);
        out.path.lineTo(pt(w, h - 2 * dx));
        addArcCommand(out.path, pt(w - 2 * dx, h), dx * 2, dx * 2, 0, 1);
        out.path.lineTo(pt(dx * 2, h));
        addArcCommand(out.path, pt(0, h - 2 * dx), dx * 2, dx * 2, 0, 1);
        out.path.lineTo(pt(0, 2 * dx));
        addArcCommand(out.path, pt(2 * dx, 0), dx * 2, dx * 2, 0, 1);
        out.path.closeSubpath();

        out.path.moveTo(pt(dx * 2, dx));
        addArcCommand(out.path, pt(dx, dx * 2), dx, dx, 0, 0);
        out.path.lineTo(pt(dx, h - 2 * dx));
        addArcCommand(out.path, pt(dx * 2, h - dx), dx, dx, 0, 0);
        out.path.lineTo(pt(w - 2 * dx, h - dx));
        addArcCommand(out.path, pt(w - dx, h - 2 * dx), dx, dx, 0, 0);
        out.path.lineTo(pt(w - dx, dx * 2));
        addArcCommand(out.path, pt(w - 2 * dx, dx), dx, dx, 0, 0);
        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.frame_corner"))
    {
        double dx = clamp(number("dx", 0.5), 0.0, w);
        dx = qMin(qMin(w * 0.5, h * 0.5), dx);

        out.path.moveTo(pt(0, 0));
        out.path.lineTo(pt(w, 0));
        out.path.lineTo(pt(w - dx, dx));
        out.path.lineTo(pt(dx, dx));
        out.path.lineTo(pt(dx, h - dx));
        out.path.lineTo(pt(0, h));
        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.diag_stripe"))
    {
        const double dx = qMin(qMin(w, h), number("dx", 0.5));

        out.path.moveTo(pt(0, h));
        out.path.lineTo(pt(w, 0));
        out.path.lineTo(pt(w, qMin(dx * 100.0 / w, h)));
        out.path.lineTo(pt(qMin(dx * 100.0 / h, w), h));
        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.donut"))
    {
        const double dx = qMin(qMin(w * 0.5, h * 0.5), number("dx", 0.5));
        const double rx = w * 0.5;
        const double ry = h * 0.5;

        out.path.moveTo(pt(0, h * 0.5));
        addArcCommand(out.path, pt(w * 0.5, 0), rx, ry, 0, 1);
        addArcCommand(out.path, pt(w, h * 0.5), rx, ry, 0, 1);
        addArcCommand(out.path, pt(w * 0.5, h), rx, ry, 0, 1);
        addArcCommand(out.path, pt(0, h * 0.5), rx, ry, 0, 1);
        out.path.closeSubpath();

        out.path.moveTo(pt(w * 0.5, dx));
        addArcCommand(out.path, pt(dx, h * 0.5), rx - dx, ry - dx, 0, 0);
        addArcCommand(out.path, pt(w * 0.5, h - dx), rx - dx, ry - dx, 0, 0);
        addArcCommand(out.path, pt(w - dx, h * 0.5), rx - dx, ry - dx, 0, 0);
        addArcCommand(out.path, pt(w * 0.5, dx), rx - dx, ry - dx, 0, 0);
        out.path.closeSubpath();
        out.path.setFillRule(Qt::WindingFill);
        return;
    }

    if (key == QLatin1String("basic.layered_rect"))
    {
        // mxShapeBasicLayeredRect paints three filled cards back to front.
        // Only the front card belongs to the fill path; the back cards are
        // filled decorations so the front card covers their inner edges.
        double dx = clamp(number("dx", 0.5), 0.0, w);
        dx = qMin(qMin(w * 0.5, h * 0.5), dx);

        QColor cardFill = Style::color(style, QStringLiteral("fillColor"),
                                       QColor(255, 255, 255));

        if (Style::isNoneColor(Style::value(style, QStringLiteral("fillColor"))))
        {
            cardFill = QColor();
        }

        const auto card = [&](double left, double top, double right, double bottom)
        {
            ShapeDecoration decoration;
            decoration.fillColor = cardFill;
            decoration.behind = true;
            decoration.path.moveTo(pt(left, top));
            decoration.path.lineTo(pt(right, top));
            decoration.path.lineTo(pt(right, bottom));
            decoration.path.lineTo(pt(left, bottom));
            decoration.path.closeSubpath();
            out.decorations.append(decoration);
        };

        card(dx, dx, w, h);
        card(dx * 0.5, dx * 0.5, w - dx * 0.5, h - dx * 0.5);

        out.path.moveTo(pt(0, 0));
        out.path.lineTo(pt(w - dx, 0));
        out.path.lineTo(pt(w - dx, h - dx));
        out.path.lineTo(pt(0, h - dx));
        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.button"))
    {
        double dx = clamp(number("dx", 0.5), 0.0, w);
        dx = qMin(qMin(w * 0.5, h * 0.5), dx);

        out.path.moveTo(pt(0, 0));
        out.path.lineTo(pt(w, 0));
        out.path.lineTo(pt(w, h));
        out.path.lineTo(pt(0, h));
        out.path.closeSubpath();

        const auto bevel = [&](double ax, double ay, double bx, double by,
                               double cx, double cy, double ex, double ey)
        {
            out.path.moveTo(pt(ax, ay));
            out.path.lineTo(pt(bx, by));
            out.path.lineTo(pt(cx, cy));
            out.path.lineTo(pt(ex, ey));
            out.path.closeSubpath();
        };

        bevel(0, h, 0, 0, dx, dx, dx, h - dx);
        bevel(0, 0, w, 0, w - dx, dx, dx, dx);
        bevel(w, 0, w, h, w - dx, h - dx, w - dx, dx);
        bevel(0, h, dx, h - dx, w - dx, h - dx, w, h);
        bevel(0, h, 0, 0, dx, dx, dx, h - dx);
        return;
    }

    if (key == QLatin1String("basic.shaded_button"))
    {
        double dx = clamp(number("dx", 0.5), 0.0, w);
        dx = qMin(qMin(w * 0.5, h * 0.5), dx);

        out.path.addRect(QRectF(x, y, w, h));

        const auto bevel = [&](const QColor &color, double ax, double ay, double bx,
                               double by, double cx, double cy, double ex, double ey)
        {
            ShapeDecoration decoration;
            decoration.fillColor = color;
            decoration.strokeWidth = 0.0;
            decoration.path.moveTo(pt(ax, ay));
            decoration.path.lineTo(pt(bx, by));
            decoration.path.lineTo(pt(cx, cy));
            decoration.path.lineTo(pt(ex, ey));
            decoration.path.closeSubpath();
            out.decorations.append(decoration);
        };

        bevel(QColor(255, 255, 255, 64), 0, h, 0, 0, dx, dx, dx, h - dx);
        bevel(QColor(255, 255, 255, 128), 0, 0, w, 0, w - dx, dx, dx, dx);
        bevel(QColor(0, 0, 0, 64), w, 0, w, h, w - dx, h - dx, w - dx, dx);
        bevel(QColor(0, 0, 0, 128), 0, h, dx, h - dx, w - dx, h - dx, w, h);
        return;
    }

    if (key == QLatin1String("basic.pie"))
    {
        const double startSource = clamp(number("startAngle", 0.25), 0.0, 1.0);
        const double endSource = clamp(number("endAngle", 0.75), 0.0, 1.0);
        const double startAngle = 2.0 * M_PI * startSource;
        const double endAngle = 2.0 * M_PI * endSource;
        const double rx = w * 0.5;
        const double ry = h * 0.5;

        const double startX = rx + std::sin(startAngle) * rx;
        const double startY = ry - std::cos(startAngle) * ry;
        const double endX = rx + std::sin(endAngle) * rx;
        const double endY = ry - std::cos(endAngle) * ry;

        double diff = endAngle - startAngle;

        if (diff < 0.0)
        {
            diff += 2.0 * M_PI;
        }

        const bool bigArc = diff >= M_PI;

        out.path.moveTo(pt(rx, ry));
        out.path.lineTo(pt(startX, startY));
        addArcCommand(out.path, pt(endX, endY), rx, ry, bigArc ? 1 : 0, 1);
        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.arc"))
    {
        const double startSource = clamp(number("startAngle", 0.25), 0.0, 1.0);
        const double endSource = clamp(number("endAngle", 0.75), 0.0, 1.0);
        const double startAngle = 2.0 * M_PI * startSource;
        const double endAngle = 2.0 * M_PI * endSource;
        const double cx = w * 0.5;
        const double cy = h * 0.5;
        const double rx = w * 0.5;
        const double ry = h * 0.5;

        const double startX = cx + std::sin(startAngle) * rx;
        const double startY = cy - std::cos(startAngle) * ry;
        const double endX = cx + std::sin(endAngle) * rx;
        const double endY = cy - std::cos(endAngle) * ry;

        double diff = endAngle - startAngle;

        if (diff < 0.0)
        {
            diff += 2.0 * M_PI;
        }

        const bool bigArc = diff > M_PI;

        out.path.moveTo(pt(startX, startY));
        addArcCommand(out.path, pt(endX, endY), rx, ry, bigArc ? 1 : 0, 1);
        return;
    }

    if (key == QLatin1String("basic.partconcellipse"))
    {
        double startAngle = 2.0 * M_PI * clamp(number("startAngle", 0.25), 0.0, 1.0);
        const double endSource = clamp(number("endAngle", 0.75), 0.0, 1.0);
        double endAngle = 2.0 * M_PI * endSource;
        const double arcWidth = 1.0 - clamp(number("arcWidth", 0.5), 0.0, 1.0);
        const double rx = w * 0.5;
        const double ry = h * 0.5;
        const double rx2 = rx * arcWidth;
        const double ry2 = ry * arcWidth;

        double diff = endAngle - startAngle;

        if (diff < 0.0)
        {
            diff += 2.0 * M_PI;
        }
        else if (qFuzzyCompare(diff, M_PI))
        {
            endAngle += 0.00001;
        }

        const double startX = rx + std::sin(startAngle) * rx;
        const double startY = ry - std::cos(startAngle) * ry;
        const double innerStartX = rx + std::sin(startAngle) * rx2;
        const double innerStartY = ry - std::cos(startAngle) * ry2;
        const double endX = rx + std::sin(endAngle) * rx;
        const double endY = ry - std::cos(endAngle) * ry;
        const double innerEndX = rx + std::sin(endAngle) * rx2;
        const double innerEndY = ry - std::cos(endAngle) * ry2;
        const bool bigArc = diff >= M_PI;

        out.path.moveTo(pt(startX, startY));
        addArcCommand(out.path, pt(endX, endY), rx, ry, bigArc ? 1 : 0, 1);
        out.path.lineTo(pt(innerEndX, innerEndY));
        addArcCommand(out.path, pt(innerStartX, innerStartY), rx2, ry2,
                      bigArc ? 1 : 0, 0);
        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.octagon2"))
    {
        double dx = clamp(number("dx", 0.5), 0.0, w) * 2.0;
        dx = qMin(qMin(w * 0.5, h * 0.5), dx);

        out.path.moveTo(pt(dx, 0));
        out.path.lineTo(pt(w - dx, 0));
        out.path.lineTo(pt(w, dx));
        out.path.lineTo(pt(w, h - dx));
        out.path.lineTo(pt(w - dx, h));
        out.path.lineTo(pt(dx, h));
        out.path.lineTo(pt(0, h - dx));
        out.path.lineTo(pt(0, dx));
        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.wave2"))
    {
        const double dy = h * clamp(number("dy", 0.5), 0.0, h);
        const double fy = 1.4;

        out.path.moveTo(pt(0, dy / 2.0));
        out.path.quadTo(pt(w / 6.0, dy * (1.0 - fy)), pt(w / 3.0, dy / 2.0));
        out.path.quadTo(pt(w / 2.0, dy * fy), pt(w * 2.0 / 3.0, dy / 2.0));
        out.path.quadTo(pt(w * 5.0 / 6.0, dy * (1.0 - fy)), pt(w, dy / 2.0));
        out.path.lineTo(pt(w, h - dy / 2.0));
        out.path.quadTo(pt(w * 5.0 / 6.0, h - dy * fy), pt(w * 2.0 / 3.0, h - dy / 2.0));
        out.path.quadTo(pt(w / 2.0, h - dy * (1.0 - fy)), pt(w / 3.0, h - dy / 2.0));
        out.path.quadTo(pt(w / 6.0, h - dy * fy), pt(0, h - dy / 2.0));
        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.rectcallout"))
    {
        const double dx = clamp(number("dx", 0.5), 0.0, w);
        const double dy = clamp(number("dy", 0.5), 0.0, h);

        out.path.moveTo(pt(dx - dy * 0.5, h - dy));
        out.path.lineTo(pt(0, h - dy));
        out.path.lineTo(pt(0, 0));
        out.path.lineTo(pt(w, 0));
        out.path.lineTo(pt(w, h - dy));
        out.path.lineTo(pt(dx + dy * 0.5, h - dy));
        out.path.lineTo(pt(dx - dy, h));
        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.roundrectcallout"))
    {
        double dx = clamp(number("dx", 0.5), 0.0, w);
        double dy = clamp(number("dy", 0.5), 0.0, h);
        double r = clamp(number("size", 10.0), 0.0, h);

        r = qMin(qMin((h - dy) / 2.0, w / 2.0), r);
        dx = qMax(r + dy * 0.5, dx);
        dx = qMin(w - r - dy * 0.5, dx);

        out.path.moveTo(pt(dx - dy * 0.5, h - dy));
        out.path.lineTo(pt(r, h - dy));
        addArcCommand(out.path, pt(0, h - dy - r), r, r, 0, 1);
        out.path.lineTo(pt(0, r));
        addArcCommand(out.path, pt(r, 0), r, r, 0, 1);
        out.path.lineTo(pt(w - r, 0));
        addArcCommand(out.path, pt(w, r), r, r, 0, 1);
        out.path.lineTo(pt(w, h - dy - r));
        addArcCommand(out.path, pt(w - r, h - dy), r, r, 0, 1);
        out.path.lineTo(pt(dx + dy * 0.5, h - dy));
        addArcCommand(out.path, pt(dx - dy, h), 1.9 * dy, 1.4 * dy, 0, 1);
        addArcCommand(out.path, pt(dx - dy * 0.5, h - dy), 0.9 * dy, 1.4 * dy, 0, 0);
        out.path.closeSubpath();
        return;
    }

    if (key == QLatin1String("basic.cross2"))
    {
        const double dx = clamp(number("dx", 0.5), 0.0, w);

        out.path.moveTo(pt(w * 0.5 + dx, 0));
        out.path.lineTo(pt(w * 0.5 + dx, h * 0.5 - dx));
        out.path.lineTo(pt(w, h * 0.5 - dx));
        out.path.lineTo(pt(w, h * 0.5 + dx));
        out.path.lineTo(pt(w * 0.5 + dx, h * 0.5 + dx));
        out.path.lineTo(pt(w * 0.5 + dx, h));
        out.path.lineTo(pt(w * 0.5 - dx, h));
        out.path.lineTo(pt(w * 0.5 - dx, h * 0.5 + dx));
        out.path.lineTo(pt(0, h * 0.5 + dx));
        out.path.lineTo(pt(0, h * 0.5 - dx));
        out.path.lineTo(pt(w * 0.5 - dx, h * 0.5 - dx));
        out.path.lineTo(pt(w * 0.5 - dx, 0));
        out.path.closeSubpath();
        return;
    }
}

} // namespace

QString defaultVertexStyle()
{
    return QStringLiteral("rounded=0;whiteSpace=wrap;html=1;");
}

QString defaultEdgeStyle()
{
    return QStringLiteral("edgeStyle=orthogonalEdgeStyle;rounded=0;orthogonalLoop=1;"
                          "jettySize=auto;html=1;endArrow=classic;endFill=1;");
}

QVector<ShapeDefinition> toolboxShapes()
{
    QVector<ShapeDefinition> shapes;

    auto add = [&shapes](const QString &key, const QString &name, const QString &style,
                         const QSizeF &size, const QString &category,
                         const QString &value = QString())
    {
        ShapeDefinition definition;
        definition.key = key;
        definition.name = name;
        definition.style = style;
        definition.value = value;
        definition.size = size;
        definition.category = category;
        shapes.append(definition);
    };

    // Palette entries that create child cells (tables, the shape group) use
    // these helpers; child coordinates are relative to the new parent.
    auto child = [](const QString &style, const QString &value, double x, double y,
                    double w, double h)
    {
        ShapeChildDefinition definition;
        definition.style = style;
        definition.value = value;
        definition.geometry = QRectF(x, y, w, h);
        return definition;
    };

    auto addMisc = [&shapes](const QString &key, const QString &name,
                             const QString &style, const QString &value,
                             const QSizeF &size, bool edge,
                             const QVector<ShapeChildDefinition> &children,
                             const QString &link = QString(),
                             bool placeholders = false,
                             const QString &placeholderName = QString())
    {
        ShapeDefinition definition;
        definition.key = key;
        definition.name = name;
        definition.style = style;
        definition.value = value;
        definition.size = size;
        definition.category = QStringLiteral("misc");
        definition.edge = edge;
        definition.children = children;
        definition.link = link;
        definition.placeholders = placeholders;
        definition.placeholderName = placeholderName;
        shapes.append(definition);
    };

    // --- general ---------------------------------------------------------
    // Exactly the entries of Sidebar.prototype.addGeneralPalette in draw.io's
    // grapheditor/Sidebar.js: same order, same style strings, same default
    // sizes and template values. Vertex entries get the editor's white fill,
    // black outline defaults; edge templates stay exactly as draw.io defines
    // them and are inserted as floating edges.
    const QString vertexColors = QStringLiteral("fillColor=#ffffff;strokeColor=#000000;");
    // Shared colour fragments of the remaining palettes.
    const QString blue = vertexColors;
    const QString green = vertexColors;
    const QString orange = vertexColors;
    const QString purple = vertexColors;
    const QString yellow = vertexColors;
    const QString red = vertexColors;
    const QString white = vertexColors;
    const QString plain = QStringLiteral("whiteSpace=wrap;html=1;");
    const QString edgeLabelStyle =
        QStringLiteral("edgeLabel;resizable=0;html=1;inheritText=1;");
    const QString fieldStyle =
        QStringLiteral("text;strokeColor=none;fillColor=none;align=left;"
                       "verticalAlign=middle;spacingLeft=4;spacingRight=4;overflow=hidden;"
                       "points=[[0,0.5],[1,0.5]];portConstraint=eastwest;rotatable=0;"
                       "whiteSpace=wrap;html=1;");

    add(QStringLiteral("rectangle"), QStringLiteral("Rectangle"),
        QStringLiteral("rounded=0;whiteSpace=wrap;html=1;") + vertexColors,
        QSizeF(120, 60), QStringLiteral("general"));
    add(QStringLiteral("rounded"), QStringLiteral("Rounded Rectangle"),
        QStringLiteral("rounded=1;whiteSpace=wrap;html=1;roundedPerimeter=1;") + vertexColors,
        QSizeF(120, 60), QStringLiteral("general"));
    add(QStringLiteral("text"), QStringLiteral("Text"),
        QStringLiteral("text;html=1;whiteSpace=wrap;strokeColor=none;fillColor=none;"
                       "align=center;verticalAlign=middle;rounded=0;"),
        QSizeF(60, 30), QStringLiteral("general"), QStringLiteral("Text"));
    add(QStringLiteral("textbox"), QStringLiteral("Textbox"),
        QStringLiteral("text;html=1;whiteSpace=wrap;overflow=hidden;rounded=0;"
                       "fillColor=none;strokeColor=none;"),
        QSizeF(180, 120), QStringLiteral("general"),
        QStringLiteral("<h1 style=\"margin-top: 0px;\">Heading</h1>"
                       "<p>Lorem ipsum dolor sit amet, consectetur adipisicing elit, "
                       "sed do eiusmod tempor incididunt ut labore et dolore magna "
                       "aliqua.</p>"));
    add(QStringLiteral("ellipse"), QStringLiteral("Ellipse"),
        QStringLiteral("ellipse;whiteSpace=wrap;html=1;shapeInside=1;") + vertexColors,
        QSizeF(120, 80), QStringLiteral("general"));
    add(QStringLiteral("square"), QStringLiteral("Square"),
        QStringLiteral("whiteSpace=wrap;html=1;aspect=fixed;") + vertexColors,
        QSizeF(80, 80), QStringLiteral("general"));
    add(QStringLiteral("circle"), QStringLiteral("Circle"),
        QStringLiteral("ellipse;whiteSpace=wrap;html=1;shapeInside=1;aspect=fixed;")
            + vertexColors,
        QSizeF(80, 80), QStringLiteral("general"));
    add(QStringLiteral("process"), QStringLiteral("Process"),
        QStringLiteral("shape=process;whiteSpace=wrap;html=1;backgroundOutline=1;")
            + vertexColors,
        QSizeF(120, 60), QStringLiteral("general"));
    add(QStringLiteral("diamond"), QStringLiteral("Diamond"),
        QStringLiteral("rhombus;whiteSpace=wrap;html=1;shapeInside=1;") + vertexColors,
        QSizeF(80, 80), QStringLiteral("general"));
    add(QStringLiteral("parallelogram"), QStringLiteral("Parallelogram"),
        QStringLiteral("shape=parallelogram;perimeter=parallelogramPerimeter;"
                       "whiteSpace=wrap;html=1;shapeInside=1;fixedSize=1;") + vertexColors,
        QSizeF(120, 60), QStringLiteral("general"));
    add(QStringLiteral("hexagon"), QStringLiteral("Hexagon"),
        QStringLiteral("shape=hexagon;perimeter=hexagonPerimeter2;whiteSpace=wrap;"
                       "html=1;shapeInside=1;fixedSize=1;") + vertexColors,
        QSizeF(120, 80), QStringLiteral("general"));
    add(QStringLiteral("triangle"), QStringLiteral("Triangle"),
        QStringLiteral("triangle;whiteSpace=wrap;html=1;shapeInside=1;") + vertexColors,
        QSizeF(60, 80), QStringLiteral("general"));
    add(QStringLiteral("cylinder"), QStringLiteral("Cylinder"),
        QStringLiteral("shape=cylinder3;whiteSpace=wrap;html=1;boundedLbl=1;"
                       "backgroundOutline=1;size=15;") + vertexColors,
        QSizeF(60, 80), QStringLiteral("general"));
    add(QStringLiteral("cloud"), QStringLiteral("Cloud"),
        QStringLiteral("ellipse;shape=cloud;whiteSpace=wrap;html=1;"
                       "perimeter=cloudPerimeter;") + vertexColors,
        QSizeF(120, 80), QStringLiteral("general"));
    add(QStringLiteral("document"), QStringLiteral("Document"),
        QStringLiteral("shape=document;whiteSpace=wrap;html=1;boundedLbl=1;") + vertexColors,
        QSizeF(120, 80), QStringLiteral("general"));
    add(QStringLiteral("internalStorage"), QStringLiteral("Internal Storage"),
        QStringLiteral("shape=internalStorage;whiteSpace=wrap;html=1;backgroundOutline=1;")
            + vertexColors,
        QSizeF(80, 80), QStringLiteral("general"));
    add(QStringLiteral("cube"), QStringLiteral("Cube"),
        QStringLiteral("shape=cube;whiteSpace=wrap;html=1;boundedLbl=1;backgroundOutline=1;"
                       "darkOpacity=0.05;darkOpacity2=0.1;") + vertexColors,
        QSizeF(120, 80), QStringLiteral("general"));
    add(QStringLiteral("step"), QStringLiteral("Step"),
        QStringLiteral("shape=step;perimeter=stepPerimeter;whiteSpace=wrap;html=1;"
                       "shapeInside=1;fixedSize=1;") + vertexColors,
        QSizeF(120, 80), QStringLiteral("general"));
    add(QStringLiteral("trapezoid"), QStringLiteral("Trapezoid"),
        QStringLiteral("shape=trapezoid;perimeter=trapezoidPerimeter;whiteSpace=wrap;"
                       "html=1;shapeInside=1;fixedSize=1;") + vertexColors,
        QSizeF(120, 60), QStringLiteral("general"));
    add(QStringLiteral("tape"), QStringLiteral("Tape"),
        QStringLiteral("shape=tape;whiteSpace=wrap;html=1;") + vertexColors,
        QSizeF(120, 100), QStringLiteral("general"));
    add(QStringLiteral("note"), QStringLiteral("Note"),
        QStringLiteral("shape=note;whiteSpace=wrap;html=1;backgroundOutline=1;"
                       "darkOpacity=0.05;") + vertexColors,
        QSizeF(80, 100), QStringLiteral("general"));
    add(QStringLiteral("card"), QStringLiteral("Card"),
        QStringLiteral("shape=card;whiteSpace=wrap;html=1;") + vertexColors,
        QSizeF(80, 100), QStringLiteral("general"));
    add(QStringLiteral("callout"), QStringLiteral("Callout"),
        QStringLiteral("shape=callout;whiteSpace=wrap;html=1;perimeter=calloutPerimeter;")
            + vertexColors,
        QSizeF(120, 80), QStringLiteral("general"));
    add(QStringLiteral("wedgeCallout"), QStringLiteral("Wedge Callout"),
        QStringLiteral("shape=wedgeCallout;whiteSpace=wrap;html=1;") + vertexColors,
        QSizeF(120, 80), QStringLiteral("general"));
    add(QStringLiteral("actor"), QStringLiteral("Actor"),
        QStringLiteral("shape=umlActor;verticalLabelPosition=bottom;verticalAlign=top;"
                       "html=1;outlineConnect=0;") + vertexColors,
        QSizeF(30, 60), QStringLiteral("general"), QStringLiteral("Actor"));
    // draw.io labels the XOR gate "Or" and the D shaped gate "And".
    add(QStringLiteral("or"), QStringLiteral("Or"),
        QStringLiteral("shape=xor;whiteSpace=wrap;html=1;shapeInside=1;") + vertexColors,
        QSizeF(60, 80), QStringLiteral("general"));
    add(QStringLiteral("and"), QStringLiteral("And"),
        QStringLiteral("shape=or;whiteSpace=wrap;html=1;shapeInside=1;") + vertexColors,
        QSizeF(60, 80), QStringLiteral("general"));
    add(QStringLiteral("dataStorage"), QStringLiteral("Data Storage"),
        QStringLiteral("shape=dataStorage;whiteSpace=wrap;html=1;shapeInside=1;fixedSize=1;")
            + vertexColors,
        QSizeF(100, 80), QStringLiteral("general"));
    add(QStringLiteral("container"), QStringLiteral("Container"),
        QStringLiteral("swimlane;startSize=0;") + vertexColors,
        QSizeF(200, 200), QStringLiteral("general"));
    add(QStringLiteral("containerVertical"), QStringLiteral("Container"),
        QStringLiteral("swimlane;whiteSpace=wrap;html=1;") + vertexColors,
        QSizeF(200, 200), QStringLiteral("general"), QStringLiteral("Vertical Container"));
    add(QStringLiteral("containerHorizontal"), QStringLiteral("Horizontal Container"),
        QStringLiteral("swimlane;horizontal=0;whiteSpace=wrap;html=1;") + vertexColors,
        QSizeF(200, 200), QStringLiteral("general"), QStringLiteral("Horizontal Container"));

    // List container with its three item rows (Sidebar's reusable field cell).
    {
        ShapeDefinition list;
        list.key = QStringLiteral("list");
        list.name = QStringLiteral("List");
        list.value = QStringLiteral("List");
        list.style = QStringLiteral("swimlane;fontStyle=0;childLayout=stackLayout;"
                                    "horizontal=1;startSize=30;horizontalStack=0;"
                                    "resizeParent=1;resizeParentMax=0;resizeLast=0;"
                                    "collapsible=1;marginBottom=0;whiteSpace=wrap;html=1;")
            + vertexColors;
        list.size = QSizeF(140, 120);
        list.category = QStringLiteral("general");

        for (int i = 1; i <= 3; ++i)
        {
            ShapeChildDefinition child;
            child.style = fieldStyle;
            child.value = QStringLiteral("Item %1").arg(i);
            child.geometry = QRectF(0, 30.0 * i, 140, 30);
            list.children.append(child);
        }

        shapes.append(list);
    }

    {
        ShapeDefinition field;
        field.key = QStringLiteral("listItem");
        field.name = QStringLiteral("List Item");
        field.value = QStringLiteral("List Item");
        field.style = fieldStyle;
        field.size = QSizeF(80, 30);
        field.category = QStringLiteral("general");
        shapes.append(field);
    }

    // --- edge templates of the General palette ---------------------------
    {
        ShapeDefinition curve;
        curve.key = QStringLiteral("curve");
        curve.name = QStringLiteral("Curve");
        curve.style = QStringLiteral("curved=1;curveGeometry=1;endArrow=classic;html=1;");
        curve.size = QSizeF(50, 50);
        curve.category = QStringLiteral("general");
        curve.edge = true;
        curve.waypoints = {QPointF(50, 50), QPointF(0, 0)};
        shapes.append(curve);
    }

    {
        ShapeDefinition arrow;
        arrow.key = QStringLiteral("flexArrow");
        arrow.name = QStringLiteral("Bidirectional Arrow");
        arrow.style = QStringLiteral("shape=flexArrow;endArrow=classic;startArrow=classic;"
                                     "html=1;");
        arrow.size = QSizeF(100, 100);
        arrow.category = QStringLiteral("general");
        arrow.edge = true;
        shapes.append(arrow);
    }

    {
        ShapeDefinition arrow;
        arrow.key = QStringLiteral("flexArrowOneWay");
        arrow.name = QStringLiteral("Arrow");
        arrow.style = QStringLiteral("shape=flexArrow;endArrow=classic;html=1;");
        arrow.size = QSizeF(50, 50);
        arrow.category = QStringLiteral("general");
        arrow.edge = true;
        shapes.append(arrow);
    }

    {
        ShapeDefinition dashed;
        dashed.key = QStringLiteral("dashedLine");
        dashed.name = QStringLiteral("Dashed Line");
        dashed.style = QStringLiteral("endArrow=none;dashed=1;html=1;");
        dashed.size = QSizeF(50, 50);
        dashed.category = QStringLiteral("general");
        dashed.edge = true;
        shapes.append(dashed);
    }

    {
        ShapeDefinition dotted;
        dotted.key = QStringLiteral("dottedLine");
        dotted.name = QStringLiteral("Dotted Line");
        dotted.style = QStringLiteral("endArrow=none;dashed=1;html=1;dashPattern=1 3;"
                                      "strokeWidth=2;");
        dotted.size = QSizeF(50, 50);
        dotted.category = QStringLiteral("general");
        dotted.edge = true;
        shapes.append(dotted);
    }

    {
        ShapeDefinition line;
        line.key = QStringLiteral("plainLine");
        line.name = QStringLiteral("Line");
        line.style = QStringLiteral("endArrow=none;html=1;");
        line.size = QSizeF(50, 50);
        line.category = QStringLiteral("general");
        line.edge = true;
        shapes.append(line);
    }

    {
        ShapeDefinition connector;
        connector.key = QStringLiteral("bidirectionalConnector");
        connector.name = QStringLiteral("Bidirectional Connector");
        connector.style = QStringLiteral("endArrow=classic;startArrow=classic;html=1;");
        connector.size = QSizeF(50, 50);
        connector.category = QStringLiteral("general");
        connector.edge = true;
        shapes.append(connector);
    }

    {
        ShapeDefinition connector;
        connector.key = QStringLiteral("directionalConnector");
        connector.name = QStringLiteral("Directional Connector");
        connector.style = QStringLiteral("endArrow=classic;html=1;");
        connector.size = QSizeF(50, 50);
        connector.category = QStringLiteral("general");
        connector.edge = true;
        shapes.append(connector);
    }

    {
        ShapeDefinition link;
        link.key = QStringLiteral("link");
        link.name = QStringLiteral("Link");
        link.style = QStringLiteral("shape=link;html=1;");
        link.size = QSizeF(100, 0);
        link.category = QStringLiteral("general");
        link.edge = true;
        shapes.append(link);
    }

    {
        ShapeDefinition connector;
        connector.key = QStringLiteral("connectorLabel");
        connector.name = QStringLiteral("Connector with Label");
        connector.style = QStringLiteral("endArrow=classic;html=1;");
        connector.size = QSizeF(100, 0);
        connector.category = QStringLiteral("general");
        connector.edge = true;

        ShapeChildDefinition label;
        label.style = edgeLabelStyle + QStringLiteral("align=center;verticalAlign=middle;");
        label.value = QStringLiteral("Label");
        label.relative = true;
        connector.children.append(label);
        shapes.append(connector);
    }

    {
        ShapeDefinition connector;
        connector.key = QStringLiteral("connectorLabels");
        connector.name = QStringLiteral("Connector with Labels");
        connector.style = QStringLiteral("endArrow=classic;html=1;");
        connector.size = QSizeF(160, 0);
        connector.category = QStringLiteral("general");
        connector.edge = true;

        ShapeChildDefinition label;
        label.style = edgeLabelStyle + QStringLiteral("align=center;verticalAlign=middle;");
        label.value = QStringLiteral("Label");
        label.relative = true;
        connector.children.append(label);

        ShapeChildDefinition source;
        source.style = edgeLabelStyle + QStringLiteral("align=left;verticalAlign=bottom;");
        source.value = QStringLiteral("Source");
        source.geometry = QRectF(-1, 0, 0, 0);
        source.relative = true;
        connector.children.append(source);

        ShapeChildDefinition target;
        target.style = edgeLabelStyle + QStringLiteral("align=right;verticalAlign=bottom;");
        target.value = QStringLiteral("Target");
        target.geometry = QRectF(1, 0, 0, 0);
        target.relative = true;
        connector.children.append(target);
        shapes.append(connector);
    }

    {
        ShapeDefinition connector;
        connector.key = QStringLiteral("connectorSymbol");
        connector.name = QStringLiteral("Connector with Symbol");
        connector.style = QStringLiteral("endArrow=classic;html=1;");
        connector.size = QSizeF(100, 0);
        connector.category = QStringLiteral("general");
        connector.edge = true;

        ShapeChildDefinition symbol;
        symbol.style = QStringLiteral("shape=message;html=1;outlineConnect=0;");
        symbol.geometry = QRectF(0, 0, 20, 14);
        symbol.offset = QPointF(-10, -7);
        symbol.relative = true;
        connector.children.append(symbol);
        shapes.append(connector);
    }

    // --- basic -----------------------------------------------------------
    // Exactly the entries of Sidebar.prototype.addBasicPalette in draw.io's
    // diagramly/sidebar/Sidebar-Basic.js, in order and with its style strings
    // verbatim. s/s2 are the two prefixes that file builds its styles from.
    const QString s = QStringLiteral("whiteSpace=wrap;html=1;shape=mxgraph.basic.");
    const QString s2 = QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;"
                                      "html=1;shape=mxgraph.basic.");

    add(QStringLiteral("basic.rect"), QStringLiteral("Partial Rectangle"),
        s2 + QStringLiteral("rect;fillColor2=none;strokeWidth=1;size=20;indent=5;"),
        QSizeF(120, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.partialTopBottom"), QStringLiteral("Partial Rectangle"),
        QStringLiteral("shape=partialRectangle;whiteSpace=wrap;html=1;top=0;bottom=0;"
                       "fillColor=none;"),
        QSizeF(120, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.partialRightTopBottom"), QStringLiteral("Partial Rectangle"),
        QStringLiteral("shape=partialRectangle;whiteSpace=wrap;html=1;right=0;top=0;"
                       "bottom=0;fillColor=none;routingCenterX=-0.5;"),
        QSizeF(120, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.partialBottomRight"), QStringLiteral("Partial Rectangle"),
        QStringLiteral("shape=partialRectangle;whiteSpace=wrap;html=1;bottom=0;right=0;"
                       "fillColor=none;"),
        QSizeF(120, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.partialTopLeft"), QStringLiteral("Partial Rectangle"),
        QStringLiteral("shape=partialRectangle;whiteSpace=wrap;html=1;top=0;left=0;"
                       "fillColor=none;"),
        QSizeF(120, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.polygon"), QStringLiteral("Polygon"),
        s2 + QStringLiteral("polygon;polyCoords=[[0.25,0],[0.75,0],[1,0.25],[1,0.75],"
                            "[0.75,1],[0.25,1],[0,0.75],[0,0.25]];polyline=0;"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.polyline"), QStringLiteral("Polyline"),
        s2 + QStringLiteral("polygon;polyCoords=[[0.25,0],[0.75,0],[1,0.25],[1,0.75],"
                            "[0.75,1],[0.25,1],[0,0.75],[0,0.25]];polyline=1;"
                            "fillColor=none;"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.patternDiag"), QStringLiteral("Rectangle with diagonal fill"),
        s2 + QStringLiteral("patternFillRect;fillStyle=diag;step=5;fillStrokeWidth=0.2;"
                            "fillStrokeColor=#dddddd;"),
        QSizeF(120, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.patternDiagRev"),
        QStringLiteral("Rectangle with reverse diagonal fill"),
        s2 + QStringLiteral("patternFillRect;fillStyle=diagRev;step=5;fillStrokeWidth=0.2;"
                            "fillStrokeColor=#dddddd;"),
        QSizeF(120, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.patternVertical"), QStringLiteral("Rectangle with vertical fill"),
        s2 + QStringLiteral("patternFillRect;fillStyle=vert;step=5;fillStrokeWidth=0.2;"
                            "fillStrokeColor=#dddddd;"),
        QSizeF(120, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.patternHorizontal"),
        QStringLiteral("Rectangle with horizontal fill"),
        s2 + QStringLiteral("patternFillRect;fillStyle=hor;step=5;fillStrokeWidth=0.2;"
                            "fillStrokeColor=#dddddd;"),
        QSizeF(120, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.patternGrid"), QStringLiteral("Rectangle with grid fill"),
        s2 + QStringLiteral("patternFillRect;fillStyle=grid;step=5;fillStrokeWidth=0.2;"
                            "fillStrokeColor=#dddddd;"),
        QSizeF(120, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.patternDiagGrid"),
        QStringLiteral("Rectangle with diagonal grid fill"),
        s2 + QStringLiteral("patternFillRect;fillStyle=diagGrid;step=5;fillStrokeWidth=0.2;"
                            "fillStrokeColor=#dddddd;"),
        QSizeF(120, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.fourPointStar2"), QStringLiteral("4 Point Star"),
        s2 + QStringLiteral("4_point_star_2;dx=0.8;"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.star6"), QStringLiteral("6 Point Star"),
        s2 + QStringLiteral("6_point_star"),
        QSizeF(100, 90), QStringLiteral("basic"));
    add(QStringLiteral("basic.star8"), QStringLiteral("8 Point Star"),
        s2 + QStringLiteral("8_point_star"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.banner"), QStringLiteral("Banner"),
        s2 + QStringLiteral("banner"),
        QSizeF(100, 50), QStringLiteral("basic"));
    add(QStringLiteral("basic.cloudCallout"), QStringLiteral("Cloud Callout"),
        s + QStringLiteral("cloud_callout"),
        QSizeF(90, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.cloudRect"), QStringLiteral("Cloud Rectangle"),
        s + QStringLiteral("cloud_rect"),
        QSizeF(120, 90), QStringLiteral("basic"));
    add(QStringLiteral("basic.cone"), QStringLiteral("Cone"),
        s2 + QStringLiteral("cone"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.cone2"), QStringLiteral("Cone (adjustable)"),
        s2 + QStringLiteral("cone2;dx=0.5;dy=0.9;"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.document"), QStringLiteral("Document"),
        s + QStringLiteral("document"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.donut"), QStringLiteral("Donut"),
        s2 + QStringLiteral("donut;dx=25;"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.drop"), QStringLiteral("Drop"),
        s2 + QStringLiteral("drop"),
        QSizeF(70, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.flash"), QStringLiteral("Flash"),
        s2 + QStringLiteral("flash"),
        QSizeF(60, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.halfCircle"), QStringLiteral("Half Circle"),
        s2 + QStringLiteral("half_circle"),
        QSizeF(100, 50), QStringLiteral("basic"));
    add(QStringLiteral("basic.heart"), QStringLiteral("Heart"),
        s2 + QStringLiteral("heart"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.hexagon"), QStringLiteral("Regular Hexagon"),
        QStringLiteral("shape=hexagon;perimeter=hexagonPerimeter2;whiteSpace=wrap;html=1;"
                       "shapeInside=1;aspect=fixed;"),
        QSizeF(100, 86.6), QStringLiteral("basic"));
    add(QStringLiteral("basic.isocube"), QStringLiteral("Isometric Cube"),
        QStringLiteral("html=1;shape=mxgraph.basic.isocube;isoAngle=15;"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.loudCallout"), QStringLiteral("Loud Callout"),
        s + QStringLiteral("loud_callout"),
        QSizeF(100, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.moon"), QStringLiteral("Moon"),
        s2 + QStringLiteral("moon"),
        QSizeF(75, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.noSymbol"), QStringLiteral("No Symbol"),
        s2 + QStringLiteral("no_symbol"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.octagon2"), QStringLiteral("Octagon"),
        s + QStringLiteral("octagon2;align=center;verticalAlign=middle;dx=15;"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.orthogonalTriangle"), QStringLiteral("Orthogonal Triangle"),
        s2 + QStringLiteral("orthogonal_triangle"),
        QSizeF(100, 70), QStringLiteral("basic"));
    add(QStringLiteral("basic.acuteTriangle"), QStringLiteral("Acute Triangle"),
        s2 + QStringLiteral("acute_triangle;dx=0.5;"),
        QSizeF(100, 70), QStringLiteral("basic"));
    add(QStringLiteral("basic.obtuseTriangle"), QStringLiteral("Obtuse Triangle"),
        s2 + QStringLiteral("obtuse_triangle;dx=0.25;"),
        QSizeF(100, 70), QStringLiteral("basic"));
    add(QStringLiteral("basic.ovalCallout"), QStringLiteral("Oval Callout"),
        s + QStringLiteral("oval_callout"),
        QSizeF(100, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.pentagon"), QStringLiteral("Pentagon"),
        s + QStringLiteral("pentagon"),
        QSizeF(100, 90), QStringLiteral("basic"));
    add(QStringLiteral("basic.pointedOval"), QStringLiteral("Pointed Oval"),
        s + QStringLiteral("pointed_oval"),
        QSizeF(50, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.pyramid"), QStringLiteral("Pyramid"),
        s2 + QStringLiteral("pyramid;dx1=0.4;dx2=0.6;dy1=0.9;dy2=0.8;"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.diagSnipRect"), QStringLiteral("Diagonal Snip Rectangle"),
        s2 + QStringLiteral("diag_snip_rect;dx=6;whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.diagRoundRect"), QStringLiteral("Diagonal Rounded Rectangle"),
        s2 + QStringLiteral("diag_round_rect;dx=6;whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.cornerRoundRect"), QStringLiteral("Corner Rounded Rectangle"),
        s2 + QStringLiteral("corner_round_rect;dx=6;whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.threeCornerRoundRect"),
        QStringLiteral("Rounded Rectangle (three corners)"),
        s2 + QStringLiteral("three_corner_round_rect;dx=6;whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.plaque"), QStringLiteral("Plaque"),
        s2 + QStringLiteral("plaque;dx=6;whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.frame"), QStringLiteral("Frame"),
        s2 + QStringLiteral("frame;dx=10;whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.roundedFrame"), QStringLiteral("Rounded Frame"),
        s2 + QStringLiteral("rounded_frame;dx=10;whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.plaqueFrame"), QStringLiteral("Plaque Frame"),
        s2 + QStringLiteral("plaque_frame;dx=10;whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.frameCorner"), QStringLiteral("Frame Corner"),
        s2 + QStringLiteral("frame_corner;dx=10;whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.diagStripe"), QStringLiteral("Diagonal Stripe"),
        s2 + QStringLiteral("diag_stripe;dx=10;"),
        QSizeF(100, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.rectCallout"), QStringLiteral("Rectangular Callout"),
        s + QStringLiteral("rectCallout;dx=30;dy=15;boundedLbl=1;"),
        QSizeF(100, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.roundRectCallout"), QStringLiteral("Rounded Rectangular Callout"),
        s + QStringLiteral("roundRectCallout;dx=30;dy=15;size=5;boundedLbl=1;"),
        QSizeF(100, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.layeredRect"), QStringLiteral("Layered Rectangle"),
        s + QStringLiteral("layered_rect;dx=10;outlineConnect=0;boundedLbl=1;"),
        QSizeF(100, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.smiley"), QStringLiteral("Smiley"),
        s2 + QStringLiteral("smiley"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.star"), QStringLiteral("Star"),
        s2 + QStringLiteral("star"),
        QSizeF(100, 95), QStringLiteral("basic"));
    add(QStringLiteral("basic.sun"), QStringLiteral("Sun"),
        s2 + QStringLiteral("sun"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.tick"), QStringLiteral("Tick"),
        s2 + QStringLiteral("tick"),
        QSizeF(85, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.wave"), QStringLiteral("Wave"),
        s2 + QStringLiteral("wave2;dy=0.3;"),
        QSizeF(100, 60), QStringLiteral("basic"));
    add(QStringLiteral("basic.button"), QStringLiteral("Button"),
        QStringLiteral("labelPosition=center;verticalLabelPosition=middle;align=center;"
                       "html=1;shape=mxgraph.basic.button;dx=10;whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("basic"), QStringLiteral("Button"));
    add(QStringLiteral("basic.shadedButton"), QStringLiteral("Button (shaded)"),
        QStringLiteral("labelPosition=center;verticalLabelPosition=middle;align=center;"
                       "html=1;shape=mxgraph.basic.shaded_button;dx=10;"
                       "fillColor=#E6E6E6;strokeColor=none;whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("basic"), QStringLiteral("Button"));
    add(QStringLiteral("basic.x"), QStringLiteral("X"),
        s2 + QStringLiteral("x"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.pie"), QStringLiteral("Pie"),
        s2 + QStringLiteral("pie;startAngle=0.2;endAngle=0.9;"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.arc"), QStringLiteral("Arc"),
        s2 + QStringLiteral("arc;startAngle=0.3;endAngle=0.1;"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.partConcEllipse"), QStringLiteral("Partial Concentric Ellipse"),
        s2 + QStringLiteral("partConcEllipse;startAngle=0.25;endAngle=0.1;arcWidth=0.5;"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.message"), QStringLiteral("Message"),
        QStringLiteral("shape=message;html=1;html=1;outlineConnect=0;labelPosition=center;"
                       "verticalLabelPosition=bottom;align=center;verticalAlign=top;"),
        QSizeF(60, 40), QStringLiteral("basic"));
    add(QStringLiteral("basic.cylinderStack"), QStringLiteral("Cylinder Stack"),
        QStringLiteral("shape=cylinder3;whiteSpace=wrap;html=1;boundedLbl=1;"
                       "backgroundOutline=1;size=15;lid=0;"),
        QSizeF(60, 80), QStringLiteral("basic"));
    add(QStringLiteral("basic.mindmapBang"), QStringLiteral("Bang"),
        QStringLiteral("shape=mindmapBang;whiteSpace=wrap;html=1;"
                       "perimeter=ellipsePerimeter;"),
        QSizeF(140, 80), QStringLiteral("basic"));
    add(QStringLiteral("basic.gitTag"), QStringLiteral("Tag"),
        QStringLiteral("shape=gitTag;tabSize=8;tabInset=4;holeSize=1;"
                       "whiteSpace=wrap;html=1;"),
        QSizeF(80, 20), QStringLiteral("basic"));

    {
        // createEdgeTemplateEntry: inserted as a floating edge, not a vertex.
        ShapeDefinition sankey;
        sankey.key = QStringLiteral("basic.sankeyLink");
        sankey.name = QStringLiteral("Sankey Link");
        sankey.style = QStringLiteral(
            "shape=mermaidSankeyLink;width=20;fillColor=#AF7AA1;"
            "gradientColor=#76B7B2;gradientDirection=east;opacity=50;"
            "strokeColor=none;bendable=0;html=1;");
        sankey.size = QSizeF(100, 60);
        sankey.category = QStringLiteral("basic");
        sankey.edge = true;
        shapes.append(sankey);
    }

    add(QStringLiteral("basic.mermaidPerson"), QStringLiteral("Person"),
        QStringLiteral("shape=mermaidPerson;perimeter=mermaidPersonPerimeter;"
                       "whiteSpace=wrap;html=1;"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.actor"), QStringLiteral("Use Case Actor"),
        QStringLiteral("shape=mermaidUsecaseActor;actorType=normal;"
                       "verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                       "outlineConnect=0;"),
        QSizeF(56, 72), QStringLiteral("basic"));
    add(QStringLiteral("basic.actorHollow"), QStringLiteral("Hollow Use Case Actor"),
        QStringLiteral("shape=mermaidUsecaseActor;actorType=hollow;"
                       "verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                       "outlineConnect=0;"),
        QSizeF(56, 72), QStringLiteral("basic"));
    add(QStringLiteral("basic.actorAwesome"), QStringLiteral("Awesome Use Case Actor"),
        QStringLiteral("shape=mermaidUsecaseActor;actorType=awesome;"
                       "verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                       "outlineConnect=0;"),
        QSizeF(56, 72), QStringLiteral("basic"));
    add(QStringLiteral("basic.smileyHappy"), QStringLiteral("Smiley"),
        QStringLiteral("shape=smileyFace;aspect=fixed;smileyType=happy;"
                       "perimeter=ellipsePerimeter;fillColor=#FFF8DC;"
                       "strokeColor=#999999;strokeWidth=2;whiteSpace=wrap;html=1;"),
        QSizeF(40, 40), QStringLiteral("basic"));
    add(QStringLiteral("basic.smileyNeutral"), QStringLiteral("Neutral Smiley"),
        QStringLiteral("shape=smileyFace;aspect=fixed;smileyType=neutral;"
                       "perimeter=ellipsePerimeter;fillColor=#FFF8DC;"
                       "strokeColor=#999999;strokeWidth=2;whiteSpace=wrap;html=1;"),
        QSizeF(40, 40), QStringLiteral("basic"));
    add(QStringLiteral("basic.smileySad"), QStringLiteral("Sad Smiley"),
        QStringLiteral("shape=smileyFace;aspect=fixed;smileyType=sad;"
                       "perimeter=ellipsePerimeter;fillColor=#FFF8DC;"
                       "strokeColor=#999999;strokeWidth=2;whiteSpace=wrap;html=1;"),
        QSizeF(40, 40), QStringLiteral("basic"));

    // --- cloud -----------------------------------------------------------
    add(QStringLiteral("cloud.generic"), QStringLiteral("Generic"),
        QStringLiteral("shape=cloud;whiteSpace=wrap;html=1;") + blue,
        QSizeF(150, 100), QStringLiteral("cloud"));
    add(QStringLiteral("cloud.callout"), QStringLiteral("Callout"),
        QStringLiteral("shape=cloudCallout;whiteSpace=wrap;html=1;") + blue,
        QSizeF(160, 100), QStringLiteral("cloud"));
    add(QStringLiteral("cloud.rect"), QStringLiteral("Rect"),
        QStringLiteral("shape=cloudRect;whiteSpace=wrap;html=1;") + blue,
        QSizeF(160, 90), QStringLiteral("cloud"));
    add(QStringLiteral("cloud.server"), QStringLiteral("Server"),
        QStringLiteral("shape=cloudServer;whiteSpace=wrap;html=1;") + blue,
        QSizeF(100, 130), QStringLiteral("cloud"));
    add(QStringLiteral("cloud.datacenter"), QStringLiteral("Datacenter"),
        QStringLiteral("shape=cloudDatacenter;whiteSpace=wrap;html=1;") + blue,
        QSizeF(120, 110), QStringLiteral("cloud"));
    add(QStringLiteral("cloud.firewall"), QStringLiteral("Firewall"),
        QStringLiteral("shape=cloudFirewall;whiteSpace=wrap;html=1;") + red,
        QSizeF(120, 100), QStringLiteral("cloud"));
    add(QStringLiteral("cloud.globe"), QStringLiteral("Globe"),
        QStringLiteral("shape=cloudGlobe;whiteSpace=wrap;html=1;") + green,
        QSizeF(110, 110), QStringLiteral("cloud"));
    add(QStringLiteral("cloud.cdn"), QStringLiteral("CDN"),
        QStringLiteral("shape=cloudCdn;whiteSpace=wrap;html=1;") + green,
        QSizeF(110, 110), QStringLiteral("cloud"));
    add(QStringLiteral("cloud.laptop"), QStringLiteral("Laptop"),
        QStringLiteral("shape=cloudLaptop;whiteSpace=wrap;html=1;") + blue,
        QSizeF(130, 90), QStringLiteral("cloud"));
    add(QStringLiteral("cloud.mobile"), QStringLiteral("Mobile"),
        QStringLiteral("shape=cloudMobile;whiteSpace=wrap;html=1;") + blue,
        QSizeF(70, 120), QStringLiteral("cloud"));
    add(QStringLiteral("cloud.loadbalancer"), QStringLiteral("Loadbalancer"),
        QStringLiteral("shape=cloudLoadBalancer;whiteSpace=wrap;html=1;") + green,
        QSizeF(110, 100), QStringLiteral("cloud"));
    add(QStringLiteral("cloud.router"), QStringLiteral("Router"),
        QStringLiteral("shape=cloudRouter;whiteSpace=wrap;html=1;") + blue,
        QSizeF(100, 100), QStringLiteral("cloud"));
    add(QStringLiteral("cloud.switch"), QStringLiteral("Switch"),
        QStringLiteral("shape=cloudSwitch;whiteSpace=wrap;html=1;") + blue,
        QSizeF(110, 80), QStringLiteral("cloud"));
    add(QStringLiteral("cloud.vpc"), QStringLiteral("VPC"),
        QStringLiteral("shape=cloudVpc;whiteSpace=wrap;html=1;fillColor=none;strokeColor=#000000;"),
        QSizeF(160, 110), QStringLiteral("cloud"));
    add(QStringLiteral("cloud.database"), QStringLiteral("Database"),
        QStringLiteral("shape=cylinder3;whiteSpace=wrap;html=1;boundedLbl=1;backgroundOutline=1;size=15;") + blue,
        QSizeF(90, 110), QStringLiteral("cloud"));
    add(QStringLiteral("cloud.storage"), QStringLiteral("Storage"),
        QStringLiteral("shape=cube;whiteSpace=wrap;html=1;boundedLbl=1;backgroundOutline=1;") + green,
        QSizeF(110, 100), QStringLiteral("cloud"));
    add(QStringLiteral("cloud.user"), QStringLiteral("User"),
        QStringLiteral("shape=umlActor;verticalLabelPosition=bottom;verticalAlign=top;html=1;")
            + blue, QSizeF(60, 110), QStringLiteral("cloud"));

    // --- misc ------------------------------------------------------------
    // Exactly the entries of Sidebar.prototype.addMiscPalette (draw.io's
    // grapheditor/Sidebar.js): same order, same style strings, same sizes and
    // template values. Table entries carry their row/cell children, the shape
    // group its triangle.
    addMisc(QStringLiteral("misc.autosizeText"), QStringLiteral("Autosize Text"),
        QStringLiteral("text;html=1;whiteSpace=wrap;strokeColor=none;fillColor=none;align=center;verticalAlign=middle;rounded=0;autosize=1;resizable=0;"),
        QStringLiteral("Text"), QSizeF(50, 30), false, {});
    addMisc(QStringLiteral("misc.autosizeTitle"), QStringLiteral("Autosize Title"),
        QStringLiteral("text;strokeColor=none;fillColor=none;html=1;fontSize=25;fontStyle=1;verticalAlign=middle;align=center;autosizeText=1;"),
        QStringLiteral("Autosize Title"), QSizeF(160, 40), false, {});
    addMisc(QStringLiteral("misc.link"), QStringLiteral("Link"),
        QStringLiteral("text;html=1;strokeColor=none;fillColor=none;whiteSpace=wrap;align=center;verticalAlign=middle;fontColor=#0000EE;fontStyle=4;"),
        QStringLiteral("Link"), QSizeF(60, 40), false, {},
        QStringLiteral("https://www.draw.io"));
    addMisc(QStringLiteral("misc.curvedText"), QStringLiteral("Curved Text"),
        QStringLiteral("shape=curvedText;noLabel=1;align=center;verticalAlign=middle;strokeColor=none;fillColor=none;"),
        QStringLiteral("Curved Text"), QSizeF(70, 50), false, {});
    addMisc(QStringLiteral("misc.slantedTextLeft"), QStringLiteral("Slanted Text Left"),
        QStringLiteral("text;html=1;strokeColor=none;fillColor=none;align=center;verticalAlign=middle;whiteSpace=wrap;"),
        QStringLiteral("<div style=\"transform:skewY(30deg);\">Text</div>"), QSizeF(100, 60), false, {});
    addMisc(QStringLiteral("misc.slantedTextRight"), QStringLiteral("Slanted Text Right"),
        QStringLiteral("text;html=1;strokeColor=none;fillColor=none;align=center;verticalAlign=middle;whiteSpace=wrap;"),
        QStringLiteral("<div style=\"transform:skewY(-30deg);\">Text</div>"), QSizeF(100, 60), false, {});
    addMisc(QStringLiteral("misc.slantedTextTop"), QStringLiteral("Slanted Text Top"),
        QStringLiteral("text;html=1;strokeColor=none;fillColor=none;align=center;verticalAlign=middle;whiteSpace=wrap;"),
        QStringLiteral("<div style=\"transform:matrix(0.866,-0.5,0.866,0.5,0,0);\">Text</div>"), QSizeF(100, 60), false, {});
    addMisc(QStringLiteral("misc.variable"), QStringLiteral("Variable"),
        QStringLiteral("text;html=1;strokeColor=none;fillColor=none;align=center;verticalAlign=middle;whiteSpace=wrap;overflow=hidden;"),
        QStringLiteral("%name% Text"), QSizeF(80, 20), false, {}, QString(),
        true, QStringLiteral("Variable"));
    addMisc(QStringLiteral("misc.timestamp"), QStringLiteral("Timestamp"),
        QStringLiteral("text;html=1;strokeColor=none;fillColor=none;align=center;verticalAlign=middle;whiteSpace=wrap;overflow=hidden;"),
        QStringLiteral("%date{ddd mmm dd yyyy HH:MM:ss}%"), QSizeF(160, 20), false, {},
        QString(), true);
    addMisc(QStringLiteral("misc.unorderedList"), QStringLiteral("Unordered List"),
        QStringLiteral("text;strokeColor=none;fillColor=none;html=1;whiteSpace=wrap;verticalAlign=middle;overflow=hidden;"),
        QStringLiteral("<ul><li>Value 1</li><li>Value 2</li><li>Value 3</li></ul>"), QSizeF(100, 80), false, {});
    addMisc(QStringLiteral("misc.orderedList"), QStringLiteral("Ordered List"),
        QStringLiteral("text;strokeColor=none;fillColor=none;html=1;whiteSpace=wrap;verticalAlign=middle;overflow=hidden;"),
        QStringLiteral("<ol><li>Value 1</li><li>Value 2</li><li>Value 3</li></ol>"), QSizeF(100, 80), false, {});
    addMisc(QStringLiteral("misc.verticalList"), QStringLiteral("Vertical List"),
        QStringLiteral("shape=table;tableRender=collapsed;startSize=0;container=1;collapsible=0;childLayout=tableLayout;fontSize=11;fillColor=none;strokeColor=none;"),
        QStringLiteral(""), QSizeF(60, 60), false, {
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;strokeColor=inherit;collapsible=0;dropTarget=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;fontSize=16;"), QStringLiteral(""), 0, 0, 60, 15),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=12;align=left;spacingLeft=5;"), QStringLiteral("Item 1"), 0, 0, 60, 15),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;strokeColor=inherit;collapsible=0;dropTarget=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;fontSize=16;"), QStringLiteral(""), 0, 15, 60, 15),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=12;align=left;spacingLeft=5;"), QStringLiteral("Item 2"), 0, 15, 60, 15),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;strokeColor=inherit;collapsible=0;dropTarget=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;fontSize=16;"), QStringLiteral(""), 0, 30, 60, 15),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=12;align=left;spacingLeft=5;"), QStringLiteral("Item 3"), 0, 30, 60, 15),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;strokeColor=inherit;collapsible=0;dropTarget=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;fontSize=16;"), QStringLiteral(""), 0, 45, 60, 15),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=12;align=left;spacingLeft=5;"), QStringLiteral("Item 4"), 0, 45, 60, 15)});
    addMisc(QStringLiteral("misc.note"), QStringLiteral("Note"),
        QStringLiteral("shape=note;whiteSpace=wrap;html=1;backgroundOutline=1;autosizeText=1;fontColor=#000000;darkOpacity=0.05;fillColor=#FFF9B2;strokeColor=none;fillStyle=solid;fontSize=20;direction=west;gradientDirection=north;gradientColor=#FFF2A1;shadow=1;size=20;pointerEvents=1;"),
        QStringLiteral("The size of the font in this note will change so that it fits within the note shape"), QSizeF(150, 150), false, {});
    addMisc(QStringLiteral("misc.table1"), QStringLiteral("Table 1"),
        QStringLiteral("shape=table;tableRender=collapsed;startSize=0;container=1;collapsible=0;childLayout=tableLayout;fontSize=16;"),
        QStringLiteral(""), QSizeF(180, 120), false, {
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;strokeColor=inherit;collapsible=0;dropTarget=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;fontSize=16;"), QStringLiteral(""), 0, 0, 180, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 0, 0, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 60, 0, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 120, 0, 60, 40),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;strokeColor=inherit;collapsible=0;dropTarget=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;fontSize=16;"), QStringLiteral(""), 0, 40, 180, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 0, 40, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 60, 40, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 120, 40, 60, 40),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;strokeColor=inherit;collapsible=0;dropTarget=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;fontSize=16;"), QStringLiteral(""), 0, 80, 180, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 0, 80, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 60, 80, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 120, 80, 60, 40)});
    addMisc(QStringLiteral("misc.table2"), QStringLiteral("Table 2"),
        QStringLiteral("shape=table;tableRender=collapsed;html=1;whiteSpace=wrap;startSize=0;container=1;collapsible=0;childLayout=tableLayout;columnLines=0;rowLines=0;fontSize=16;strokeColor=default;"),
        QStringLiteral(""), QSizeF(180, 120), false, {
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;collapsible=0;dropTarget=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;fontSize=16;strokeColor=inherit;"), QStringLiteral(""), 0, 0, 180, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;fillColor=none;overflow=hidden;fontSize=12;strokeColor=inherit;"), QStringLiteral("Value 1"), 0, 0, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;fillColor=none;overflow=hidden;fontSize=12;strokeColor=inherit;"), QStringLiteral("Value 2"), 60, 0, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;fillColor=none;overflow=hidden;fontSize=12;strokeColor=inherit;"), QStringLiteral("Value 3"), 120, 0, 60, 40),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;collapsible=0;dropTarget=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;fontSize=16;strokeColor=inherit;"), QStringLiteral(""), 0, 40, 180, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;fillColor=none;overflow=hidden;fontSize=12;strokeColor=inherit;"), QStringLiteral("Value 4"), 0, 40, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;fillColor=none;overflow=hidden;fontSize=12;strokeColor=inherit;"), QStringLiteral("Value 5"), 60, 40, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;fillColor=none;overflow=hidden;fontSize=12;strokeColor=inherit;"), QStringLiteral("Value 6"), 120, 40, 60, 40),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;collapsible=0;dropTarget=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;fontSize=16;strokeColor=inherit;"), QStringLiteral(""), 0, 80, 180, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;fillColor=none;overflow=hidden;fontSize=12;strokeColor=inherit;"), QStringLiteral("Value 7"), 0, 80, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;fillColor=none;overflow=hidden;fontSize=12;strokeColor=inherit;"), QStringLiteral("Value 8"), 60, 80, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;fillColor=none;overflow=hidden;fontSize=12;strokeColor=inherit;"), QStringLiteral("Value 9"), 120, 80, 60, 40)});
    addMisc(QStringLiteral("misc.tableWithTitle1"), QStringLiteral("Table with Title 1"),
        QStringLiteral("shape=table;tableRender=collapsed;startSize=30;container=1;collapsible=0;childLayout=tableLayout;strokeColor=default;fontSize=16;"),
        QStringLiteral("Table"), QSizeF(180, 150), false, {
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;strokeColor=inherit;collapsible=0;dropTarget=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;fontSize=16;"), QStringLiteral(""), 0, 30, 180, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 0, 30, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 60, 30, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 120, 30, 60, 40),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;strokeColor=inherit;collapsible=0;dropTarget=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;fontSize=16;"), QStringLiteral(""), 0, 70, 180, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 0, 70, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 60, 70, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 120, 70, 60, 40),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;strokeColor=inherit;collapsible=0;dropTarget=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;fontSize=16;"), QStringLiteral(""), 0, 110, 180, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 0, 110, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 60, 110, 60, 40),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;strokeColor=inherit;overflow=hidden;fillColor=none;pointerEvents=1;fontSize=16;"), QStringLiteral(""), 120, 110, 60, 40)});
    addMisc(QStringLiteral("misc.tableWithTitle2"), QStringLiteral("Table with Title 2"),
        QStringLiteral("shape=table;tableRender=collapsed;startSize=30;container=1;collapsible=0;childLayout=tableLayout;fixedRows=1;rowLines=0;fontStyle=0;strokeColor=default;fontSize=16;"),
        QStringLiteral("Table"), QSizeF(180, 120), false, {
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;collapsible=0;dropTarget=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;strokeColor=inherit;fontSize=16;"), QStringLiteral(""), 0, 30, 180, 30),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;fillColor=none;overflow=hidden;pointerEvents=1;strokeColor=inherit;fontSize=16;"), QStringLiteral("1"), 0, 30, 40, 30),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;fillColor=none;align=left;spacingLeft=6;overflow=hidden;strokeColor=inherit;fontSize=16;"), QStringLiteral("Value 1"), 40, 30, 140, 30),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;collapsible=0;dropTarget=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;strokeColor=inherit;fontSize=16;"), QStringLiteral(""), 0, 60, 180, 30),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;fillColor=none;overflow=hidden;strokeColor=inherit;fontSize=16;"), QStringLiteral("2"), 0, 60, 40, 30),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;fillColor=none;align=left;spacingLeft=6;overflow=hidden;strokeColor=inherit;fontSize=16;"), QStringLiteral("Value 2"), 40, 60, 140, 30),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;collapsible=0;dropTarget=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;strokeColor=inherit;fontSize=16;"), QStringLiteral(""), 0, 90, 180, 30),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;fillColor=none;overflow=hidden;strokeColor=inherit;fontSize=16;"), QStringLiteral("3"), 0, 90, 40, 30),
            child(QStringLiteral("shape=partialRectangle;html=1;whiteSpace=wrap;connectable=0;fillColor=none;align=left;spacingLeft=6;overflow=hidden;strokeColor=inherit;fontSize=16;"), QStringLiteral("Value 3"), 40, 90, 140, 30)});
    addMisc(QStringLiteral("misc.crossFunctional"), QStringLiteral("Cross-Functional Flowchart"),
        QStringLiteral("shape=table;tableRender=collapsed;childLayout=tableLayout;startSize=40;collapsible=0;recursiveResize=0;expand=0;fontSize=16;"),
        QStringLiteral("Cross-Functional Flowchart"), QSizeF(400, 400), false, {
            child(QStringLiteral("shape=tableRow;horizontal=0;swimlaneHead=0;swimlaneBody=0;strokeColor=inherit;dropTarget=0;fontStyle=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;startSize=40;collapsible=0;recursiveResize=0;expand=0;fontSize=16;"), QStringLiteral("Actor 1"), 0, 40, 400, 120),
            child(QStringLiteral("swimlane;swimlaneHead=0;swimlaneBody=0;fontStyle=0;strokeColor=inherit;connectable=0;fillColor=none;startSize=40;collapsible=0;recursiveResize=0;expand=0;fontSize=16;"), QStringLiteral("Phase 1"), 40, 40, 120, 120),
            child(QStringLiteral("swimlane;swimlaneHead=0;swimlaneBody=0;fontStyle=0;strokeColor=inherit;connectable=0;fillColor=none;startSize=40;collapsible=0;recursiveResize=0;expand=0;fontSize=16;"), QStringLiteral("Phase 2"), 160, 40, 120, 120),
            child(QStringLiteral("swimlane;swimlaneHead=0;swimlaneBody=0;fontStyle=0;strokeColor=inherit;connectable=0;fillColor=none;startSize=40;collapsible=0;recursiveResize=0;expand=0;fontSize=16;"), QStringLiteral("Phase 3"), 280, 40, 120, 120),
            child(QStringLiteral("shape=tableRow;horizontal=0;swimlaneHead=0;swimlaneBody=0;strokeColor=inherit;dropTarget=0;fontStyle=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;startSize=40;collapsible=0;recursiveResize=0;expand=0;fontSize=16;"), QStringLiteral("Actor 2"), 0, 160, 400, 120),
            child(QStringLiteral("swimlane;swimlaneHead=0;swimlaneBody=0;fontStyle=0;connectable=0;strokeColor=inherit;fillColor=none;startSize=0;collapsible=0;recursiveResize=0;expand=0;fontSize=16;"), QStringLiteral(""), 40, 160, 120, 120),
            child(QStringLiteral("swimlane;swimlaneHead=0;swimlaneBody=0;fontStyle=0;connectable=0;strokeColor=inherit;fillColor=none;startSize=0;collapsible=0;recursiveResize=0;expand=0;fontSize=16;"), QStringLiteral(""), 160, 160, 120, 120),
            child(QStringLiteral("swimlane;swimlaneHead=0;swimlaneBody=0;fontStyle=0;connectable=0;strokeColor=inherit;fillColor=none;startSize=0;collapsible=0;recursiveResize=0;expand=0;fontSize=16;"), QStringLiteral(""), 280, 160, 120, 120),
            child(QStringLiteral("shape=tableRow;horizontal=0;swimlaneHead=0;swimlaneBody=0;strokeColor=inherit;dropTarget=0;fontStyle=0;fillColor=none;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;startSize=40;collapsible=0;recursiveResize=0;expand=0;fontSize=16;"), QStringLiteral("Actor 3"), 0, 280, 400, 120),
            child(QStringLiteral("swimlane;swimlaneHead=0;swimlaneBody=0;fontStyle=0;connectable=0;strokeColor=inherit;fillColor=none;startSize=0;collapsible=0;recursiveResize=0;expand=0;fontSize=16;"), QStringLiteral(""), 40, 280, 120, 120),
            child(QStringLiteral("swimlane;swimlaneHead=0;swimlaneBody=0;fontStyle=0;connectable=0;strokeColor=inherit;fillColor=none;startSize=0;collapsible=0;recursiveResize=0;expand=0;fontSize=16;"), QStringLiteral(""), 160, 280, 120, 120),
            child(QStringLiteral("swimlane;swimlaneHead=0;swimlaneBody=0;fontStyle=0;connectable=0;strokeColor=inherit;fillColor=none;startSize=0;collapsible=0;recursiveResize=0;expand=0;fontSize=16;"), QStringLiteral(""), 280, 280, 120, 120)});
    addMisc(QStringLiteral("misc.tableAssets"), QStringLiteral("Table"),
        QStringLiteral("childLayout=tableLayout;recursiveResize=0;strokeColor=#98bf21;fillColor=#A7C942;shadow=1;"),
        QStringLiteral("Assets"), QSizeF(280, 160), false, {
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;top=0;left=0;bottom=0;right=0;dropTarget=0;collapsible=0;recursiveResize=0;expand=0;fontStyle=0;strokeColor=inherit;fillColor=#ffffff;"), QStringLiteral(""), 0, 0, 280, 33),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=#A7C942;align=center;fontStyle=1;fontColor=#FFFFFF;html=1;"), QStringLiteral("Title 1"), 0, 0, 93, 33),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=#A7C942;align=center;fontStyle=1;fontColor=#FFFFFF;html=1;"), QStringLiteral("Title 2"), 93, 0, 94, 33),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=#A7C942;align=center;fontStyle=1;fontColor=#FFFFFF;html=1;"), QStringLiteral("Title 3"), 187, 0, 93, 33),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;top=0;left=0;bottom=0;right=0;dropTarget=0;collapsible=0;recursiveResize=0;expand=0;fontStyle=0;strokeColor=inherit;fillColor=#ffffff;"), QStringLiteral(""), 0, 33, 280, 32),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=inherit;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 1"), 0, 33, 93, 32),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=inherit;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 2"), 93, 33, 94, 32),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=inherit;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 3"), 187, 33, 93, 32),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;top=0;left=0;bottom=0;right=0;dropTarget=0;collapsible=0;recursiveResize=0;expand=0;fontStyle=1;strokeColor=inherit;fillColor=#EAF2D3;"), QStringLiteral(""), 0, 65, 280, 31),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=inherit;whiteSpace=wrap;html=1;"), QStringLiteral("Value 4"), 0, 65, 93, 31),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=inherit;whiteSpace=wrap;html=1;"), QStringLiteral("Value 5"), 93, 65, 94, 31),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=inherit;whiteSpace=wrap;html=1;"), QStringLiteral("Value 6"), 187, 65, 93, 31),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;top=0;left=0;bottom=0;right=0;dropTarget=0;collapsible=0;recursiveResize=0;expand=0;fontStyle=0;strokeColor=inherit;fillColor=#ffffff;"), QStringLiteral(""), 0, 96, 280, 33),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=inherit;fontStyle=0;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 7"), 0, 96, 93, 33),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=inherit;fontStyle=0;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 8"), 93, 96, 94, 33),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=inherit;fontStyle=0;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 9"), 187, 96, 93, 33),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;top=0;left=0;bottom=0;right=0;dropTarget=0;collapsible=0;recursiveResize=0;expand=0;fontStyle=1;strokeColor=inherit;fillColor=#EAF2D3;"), QStringLiteral(""), 0, 129, 280, 31),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=inherit;whiteSpace=wrap;html=1;"), QStringLiteral("Value 10"), 0, 129, 93, 31),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=inherit;whiteSpace=wrap;html=1;"), QStringLiteral("Value 11"), 93, 129, 94, 31),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=inherit;whiteSpace=wrap;html=1;"), QStringLiteral("Value 12"), 187, 129, 93, 31)});
    addMisc(QStringLiteral("misc.tableBordered"), QStringLiteral("Table"),
        QStringLiteral("childLayout=tableLayout;recursiveResize=0;shadow=0;fillColor=none;strokeColor=#C0C0C0;"),
        QStringLiteral(""), QSizeF(180, 140), false, {
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;top=0;left=0;bottom=0;right=0;dropTarget=0;collapsible=0;recursiveResize=0;expand=0;fontStyle=0;fillColor=none;"), QStringLiteral(""), 0, 0, 180, 47),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=none;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 1"), 0, 0, 60, 47),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=none;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 2"), 60, 0, 60, 47),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=none;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 3"), 120, 0, 60, 47),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;top=0;left=0;bottom=0;right=0;dropTarget=0;collapsible=0;recursiveResize=0;expand=0;fontStyle=0;fillColor=none;"), QStringLiteral(""), 0, 47, 180, 46),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=none;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 4"), 0, 47, 60, 46),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=none;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 5"), 60, 47, 60, 46),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=none;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 6"), 120, 47, 60, 46),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;top=0;left=0;bottom=0;right=0;dropTarget=0;collapsible=0;recursiveResize=0;expand=0;fontStyle=0;fillColor=none;"), QStringLiteral(""), 0, 93, 180, 47),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=none;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 7"), 0, 93, 60, 47),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=none;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 8"), 60, 93, 60, 47),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=none;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 9"), 120, 93, 60, 47)});
    addMisc(QStringLiteral("misc.tablePlain"), QStringLiteral("Table"),
        QStringLiteral("childLayout=tableLayout;recursiveResize=0;shadow=0;fillColor=none;"),
        QStringLiteral(""), QSizeF(180, 140), false, {
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;top=0;left=0;bottom=0;right=0;dropTarget=0;collapsible=0;recursiveResize=0;expand=0;fontStyle=0;fillColor=none;strokeColor=inherit;"), QStringLiteral(""), 0, 0, 180, 47),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 1"), 0, 0, 60, 47),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 2"), 60, 0, 60, 47),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 3"), 120, 0, 60, 47),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;top=0;left=0;bottom=0;right=0;dropTarget=0;collapsible=0;recursiveResize=0;expand=0;fontStyle=0;fillColor=none;strokeColor=inherit;"), QStringLiteral(""), 0, 47, 180, 46),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 4"), 0, 47, 60, 46),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 5"), 60, 47, 60, 46),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 6"), 120, 47, 60, 46),
            child(QStringLiteral("shape=tableRow;horizontal=0;startSize=0;swimlaneHead=0;swimlaneBody=0;top=0;left=0;bottom=0;right=0;dropTarget=0;collapsible=0;recursiveResize=0;expand=0;fontStyle=0;fillColor=none;strokeColor=inherit;"), QStringLiteral(""), 0, 93, 180, 47),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 7"), 0, 93, 60, 47),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 8"), 60, 93, 60, 47),
            child(QStringLiteral("connectable=0;recursiveResize=0;strokeColor=inherit;fillColor=none;align=center;whiteSpace=wrap;html=1;"), QStringLiteral("Value 9"), 120, 93, 60, 47)});
    addMisc(QStringLiteral("misc.htmlTable"), QStringLiteral("HTML Table 4"),
        QStringLiteral("text;html=1;whiteSpace=wrap;strokeColor=none;fillColor=none;overflow=fill;"),
        QStringLiteral("<table border=\"1\" width=\"100%\" height=\"100%\" cellpadding=\"4\" style=\"width:100%;height:100%;border-collapse:collapse;\"><tr><th align=\"center\"><b>Title</b></th></tr><tr><td align=\"center\">Section 1.1\nSection 1.2\nSection 1.3</td></tr><tr><td align=\"center\">Section 2.1\nSection 2.2\nSection 2.3</td></tr></table>"), QSizeF(180, 180), false, {});
    addMisc(QStringLiteral("misc.rectangleSketch"), QStringLiteral("Rectangle Sketch"),
        QStringLiteral("rounded=1;whiteSpace=wrap;html=1;strokeWidth=2;fillWeight=4;hachureGap=8;hachureAngle=45;fillColor=#1ba1e2;sketch=1;"),
        QStringLiteral(""), QSizeF(120, 60), false, {});
    addMisc(QStringLiteral("misc.ellipseSketch"), QStringLiteral("Ellipse Sketch"),
        QStringLiteral("ellipse;whiteSpace=wrap;html=1;strokeWidth=2;fillWeight=2;hachureGap=8;fillColor=#990000;fillStyle=dots;sketch=1;"),
        QStringLiteral(""), QSizeF(120, 60), false, {});
    addMisc(QStringLiteral("misc.diamondSketch"), QStringLiteral("Diamond Sketch"),
        QStringLiteral("rhombus;whiteSpace=wrap;html=1;strokeWidth=2;fillWeight=-1;hachureGap=8;fillStyle=cross-hatch;fillColor=#006600;sketch=1;"),
        QStringLiteral(""), QSizeF(120, 60), false, {});
    addMisc(QStringLiteral("misc.isometricCube"), QStringLiteral("Isometric Cube"),
        QStringLiteral("html=1;whiteSpace=wrap;shape=isoCube2;backgroundOutline=1;isoAngle=15;"),
        QStringLiteral(""), QSizeF(90, 100), false, {});
    addMisc(QStringLiteral("misc.isometricSquare"), QStringLiteral("Isometric Square"),
        QStringLiteral("html=1;whiteSpace=wrap;aspect=fixed;shape=isoRectangle;"),
        QStringLiteral(""), QSizeF(150, 90), false, {});
    addMisc(QStringLiteral("misc.isometricEdge1"), QStringLiteral("Isometric Edge 1"),
        QStringLiteral("edgeStyle=isometricEdgeStyle;endArrow=none;html=1;rounded=0;defaultLabelBackgroundColor=page;"),
        QStringLiteral(""), QSizeF(50, 100), true, {});
    addMisc(QStringLiteral("misc.isometricEdge2"), QStringLiteral("Isometric Edge 2"),
        QStringLiteral("edgeStyle=isometricEdgeStyle;endArrow=none;html=1;elbow=vertical;rounded=0;defaultLabelBackgroundColor=page;"),
        QStringLiteral(""), QSizeF(50, 100), true, {});
    addMisc(QStringLiteral("misc.leftCurlyBracket"), QStringLiteral("Left Curly Bracket"),
        QStringLiteral("shape=curlyBracket;whiteSpace=wrap;html=1;rounded=1;labelPosition=left;verticalLabelPosition=middle;align=right;verticalAlign=middle;labelPerimeter=1;"),
        QStringLiteral(""), QSizeF(20, 120), false, {});
    addMisc(QStringLiteral("misc.rightCurlyBracket"), QStringLiteral("Right Curly Bracket"),
        QStringLiteral("shape=curlyBracket;whiteSpace=wrap;html=1;rounded=1;flipH=1;labelPosition=right;verticalLabelPosition=middle;align=left;verticalAlign=middle;labelPerimeter=1;legacyAnchorPoints=0;"),
        QStringLiteral(""), QSizeF(20, 120), false, {});
    addMisc(QStringLiteral("misc.horizontalLine"), QStringLiteral("Horizontal Line"),
        QStringLiteral("line;strokeWidth=2;html=1;"),
        QStringLiteral(""), QSizeF(160, 10), false, {});
    addMisc(QStringLiteral("misc.verticalLine"), QStringLiteral("Vertical Line"),
        QStringLiteral("line;strokeWidth=2;direction=south;html=1;"),
        QStringLiteral(""), QSizeF(10, 160), false, {});
    addMisc(QStringLiteral("misc.horizontalBackbone"), QStringLiteral("Horizontal Backbone"),
        QStringLiteral("line;strokeWidth=4;html=1;perimeter=backbonePerimeter;points=[];outlineConnect=0;"),
        QStringLiteral(""), QSizeF(160, 10), false, {});
    addMisc(QStringLiteral("misc.verticalBackbone"), QStringLiteral("Vertical Backbone"),
        QStringLiteral("line;strokeWidth=4;direction=south;html=1;perimeter=backbonePerimeter;points=[];outlineConnect=0;"),
        QStringLiteral(""), QSizeF(10, 160), false, {});
    addMisc(QStringLiteral("misc.horizontalCrossbar"), QStringLiteral("Horizontal Crossbar"),
        QStringLiteral("shape=crossbar;whiteSpace=wrap;html=1;rounded=1;"),
        QStringLiteral(""), QSizeF(120, 20), false, {});
    addMisc(QStringLiteral("misc.verticalCrossbar"), QStringLiteral("Vertical Crossbar"),
        QStringLiteral("shape=crossbar;whiteSpace=wrap;html=1;rounded=1;direction=south;"),
        QStringLiteral(""), QSizeF(20, 120), false, {});
    addMisc(QStringLiteral("misc.imageFixedAspect"), QStringLiteral("Image (Fixed Aspect)"),
        QStringLiteral("shape=image;html=1;verticalLabelPosition=bottom;verticalAlign=top;imageAspect=1;aspect=fixed;image=img/clipart/Gear_128x128.png;labelPerimeter=1;"),
        QStringLiteral(""), QSizeF(52, 61), false, {});
    addMisc(QStringLiteral("misc.imageVariableAspect"), QStringLiteral("Image (Variable Aspect)"),
        QStringLiteral("shape=image;html=1;verticalLabelPosition=bottom;verticalAlign=top;imageAspect=0;image=img/clipart/Gear_128x128.png;labelPerimeter=1;"),
        QStringLiteral(""), QSizeF(50, 60), false, {});
    addMisc(QStringLiteral("misc.icon"), QStringLiteral("Icon"),
        QStringLiteral("icon;html=1;image=img/clipart/Gear_128x128.png;labelPerimeter=1;"),
        QStringLiteral("Icon"), QSizeF(60, 60), false, {});
    addMisc(QStringLiteral("misc.label1"), QStringLiteral("Label 1"),
        QStringLiteral("label;whiteSpace=wrap;html=1;image=img/clipart/Gear_128x128.png"),
        QStringLiteral("Label"), QSizeF(140, 60), false, {});
    addMisc(QStringLiteral("misc.label2"), QStringLiteral("Label 2"),
        QStringLiteral("label;whiteSpace=wrap;html=1;align=center;verticalAlign=bottom;spacingLeft=0;spacingBottom=4;imageAlign=center;imageVerticalAlign=top;image=img/clipart/Gear_128x128.png"),
        QStringLiteral("Label"), QSizeF(120, 80), false, {});
    addMisc(QStringLiteral("misc.shapeGroup"), QStringLiteral("Shape Group"),
        QStringLiteral("html=1;whiteSpace=wrap;container=1;recursiveResize=0;collapsible=0;"),
        QStringLiteral("Label"), QSizeF(160, 70), false, {
            child(QStringLiteral("triangle;html=1;whiteSpace=wrap;"), QStringLiteral(""), 20, 20, 20, 30)});
    addMisc(QStringLiteral("misc.partialRectangle1"), QStringLiteral("Partial Rectangle"),
        QStringLiteral("shape=partialRectangle;whiteSpace=wrap;html=1;left=0;right=0;fillColor=none;"),
        QStringLiteral(""), QSizeF(120, 60), false, {});
    addMisc(QStringLiteral("misc.partialRectangle2"), QStringLiteral("Partial Rectangle"),
        QStringLiteral("shape=partialRectangle;whiteSpace=wrap;html=1;bottom=0;top=0;fillColor=none;"),
        QStringLiteral(""), QSizeF(120, 60), false, {});
    addMisc(QStringLiteral("misc.partialRectangle3"), QStringLiteral("Partial Rectangle"),
        QStringLiteral("shape=partialRectangle;whiteSpace=wrap;html=1;bottom=1;right=1;left=1;top=0;fillColor=none;routingCenterX=-0.5;"),
        QStringLiteral(""), QSizeF(120, 60), false, {});
    addMisc(QStringLiteral("misc.waypoint"), QStringLiteral("Waypoint"),
        QStringLiteral("shape=waypoint;sketch=0;fillStyle=solid;size=6;pointerEvents=1;points=[];fillColor=none;resizable=0;rotatable=0;perimeter=centerPerimeter;snapToPoint=1;"),
        QStringLiteral(""), QSizeF(20, 20), false, {});
    addMisc(QStringLiteral("misc.manualLine"), QStringLiteral("Manual Line"),
        QStringLiteral("edgeStyle=segmentEdgeStyle;endArrow=classic;html=1;curved=0;rounded=0;endSize=8;startSize=8;defaultLabelBackgroundColor=page;"),
        QStringLiteral(""), QSizeF(50, 50), true, {});
    addMisc(QStringLiteral("misc.filledEdge"), QStringLiteral("Filled Edge"),
        QStringLiteral("shape=filledEdge;curved=0;rounded=0;fixDash=1;endArrow=none;strokeWidth=10;fillColor=#ffffff;edgeStyle=orthogonalEdgeStyle;html=1;defaultLabelBackgroundColor=page;"),
        QStringLiteral(""), QSizeF(60, 40), true, {});
    addMisc(QStringLiteral("misc.taperedArrow"), QStringLiteral("Tapered Arrow"),
        QStringLiteral("shape=taperedArrow;startWidth=12;endWidth=2;endArrow=block;endFill=1;html=1;rounded=0;defaultLabelBackgroundColor=page;"),
        QStringLiteral(""), QSizeF(60, 40), true, {});
    addMisc(QStringLiteral("misc.horizontalElbow"), QStringLiteral("Horizontal Elbow"),
        QStringLiteral("edgeStyle=elbowEdgeStyle;elbow=horizontal;endArrow=classic;html=1;curved=0;rounded=0;endSize=8;startSize=8;defaultLabelBackgroundColor=page;"),
        QStringLiteral(""), QSizeF(50, 50), true, {});
    addMisc(QStringLiteral("misc.verticalElbow"), QStringLiteral("Vertical Elbow"),
        QStringLiteral("edgeStyle=elbowEdgeStyle;elbow=vertical;endArrow=classic;html=1;curved=0;rounded=0;endSize=8;startSize=8;defaultLabelBackgroundColor=page;"),
        QStringLiteral(""), QSizeF(50, 50), true, {});
    addMisc(QStringLiteral("misc.arc"), QStringLiteral("Arc"),
        QStringLiteral("shape=mxgraph.basic.arc;html=1;startAngle=0.3;endAngle=0.1;endArrow=classic;endFill=1;"),
        QStringLiteral(""), QSizeF(50, 50), false, {});
    addMisc(QStringLiteral("misc.zigzag"), QStringLiteral("Zigzag"),
        QStringLiteral("shape=zigzag;html=1;fillColor=none;"),
        QStringLiteral(""), QSizeF(160, 20), false, {});

    // --- advanced --------------------------------------------------------
    // Exactly the palette of Sidebar.prototype.createAdvancedShapes() plus the
    // entries Sidebar-Advanced.js appends, in sidebar order and with its style
    // strings verbatim (sizes and child cells included). The layout
    // containers use the styles of Menus.layoutContainers (see the
    // swimlane()/entry() helpers there).
    {
        auto addAdvanced = [&shapes](const QString &key, const QString &name,
                                     const QString &style, const QSizeF &size,
                                     const QString &value = QString())
        {
            ShapeDefinition definition;
            definition.key = key;
            definition.name = name;
            definition.style = style;
            definition.value = value;
            definition.size = size;
            definition.category = QStringLiteral("advanced");
            shapes.append(definition);
        };

        // Child cells of a palette entry: a vertex at (x, y, w, h) or an edge
        // between two of the entry's child vertices.
        auto childVertex = [](const QString &style, const QString &value,
                              double x, double y, double w, double h)
        {
            ShapeChildDefinition definition;
            definition.style = style;
            definition.value = value;
            definition.geometry = QRectF(x, y, w, h);
            return definition;
        };

        auto childEdge = [](const QString &style, int source, int target)
        {
            ShapeChildDefinition definition;
            definition.style = style;
            definition.edge = true;
            definition.source = source;
            definition.target = target;
            return definition;
        };

        // Sidebar helper styles.
        const QString wrap = QStringLiteral("whiteSpace=wrap;html=1;");
        const QString fieldStyle = QStringLiteral(
            "text;strokeColor=none;fillColor=none;align=left;verticalAlign=top;"
            "spacingLeft=4;spacingRight=4;overflow=hidden;rotatable=0;"
            "points=[[0,0.5],[1,0.5]];portConstraint=eastwest;whiteSpace=wrap;html=1;");
        const QString stepStyle = QStringLiteral(
            "shape=step;perimeter=stepPerimeter;fixedSize=1;points=[];"
            "whiteSpace=wrap;html=1;");
        const QString laneStyle = QStringLiteral("swimlane;startSize=20;horizontal=0;html=1;");
        const QString layoutEdgeStyle = QStringLiteral(
            "html=1;rounded=1;curved=0;sourcePerimeterSpacing=0;targetPerimeterSpacing=0;"
            "startSize=6;endSize=6;edgeStyle=orthogonalEdgeStyle;");
        const QString balloonEdgeStyle = QStringLiteral(
            "html=1;rounded=1;curved=0;sourcePerimeterSpacing=0;targetPerimeterSpacing=0;"
            "startSize=6;endSize=6;startArrow=none;endArrow=none;");
        const QString topPad = QStringLiteral("[top=40,left=20,bottom=20,right=20]");
        const QString leftPad = QStringLiteral("[top=20,left=40,bottom=20,right=20]");

        addAdvanced(QStringLiteral("advanced.doubleRectangle"),
            QStringLiteral("Double Rectangle"),
            QStringLiteral("shape=ext;double=1;rounded=0;whiteSpace=wrap;html=1;"),
            QSizeF(120, 80));
        addAdvanced(QStringLiteral("advanced.doubleRoundedRectangle"),
            QStringLiteral("Double Rounded Rectangle"),
            QStringLiteral("shape=ext;double=1;rounded=1;whiteSpace=wrap;html=1;"),
            QSizeF(120, 80));
        addAdvanced(QStringLiteral("advanced.doubleEllipse"), QStringLiteral("Double Ellipse"),
            QStringLiteral("ellipse;shape=doubleEllipse;whiteSpace=wrap;html=1;"),
            QSizeF(100, 60));
        addAdvanced(QStringLiteral("advanced.doubleSquare"), QStringLiteral("Double Square"),
            QStringLiteral("shape=ext;double=1;whiteSpace=wrap;html=1;aspect=fixed;"),
            QSizeF(80, 80));
        addAdvanced(QStringLiteral("advanced.doubleCircle"), QStringLiteral("Double Circle"),
            QStringLiteral("ellipse;shape=doubleEllipse;whiteSpace=wrap;html=1;aspect=fixed;"),
            QSizeF(80, 80));
        addAdvanced(QStringLiteral("advanced.tapeData"), QStringLiteral("Tape Data"),
            QStringLiteral("shape=tapeData;whiteSpace=wrap;html=1;perimeter=ellipsePerimeter;"),
            QSizeF(80, 80));
        addAdvanced(QStringLiteral("advanced.manualInput"), QStringLiteral("Manual Input"),
            QStringLiteral("shape=manualInput;boundedLbl=1;whiteSpace=wrap;html=1;"),
            QSizeF(80, 80));
        addAdvanced(QStringLiteral("advanced.loopLimit"), QStringLiteral("Loop Limit"),
            QStringLiteral("shape=loopLimit;whiteSpace=wrap;html=1;"), QSizeF(100, 80));
        addAdvanced(QStringLiteral("advanced.offPageConnector"),
            QStringLiteral("Off Page Connector"),
            QStringLiteral("shape=offPageConnector;whiteSpace=wrap;html=1;"), QSizeF(80, 80));
        addAdvanced(QStringLiteral("advanced.delay"), QStringLiteral("Delay"),
            QStringLiteral("shape=delay;whiteSpace=wrap;html=1;"), QSizeF(80, 40));
        addAdvanced(QStringLiteral("advanced.display"), QStringLiteral("Display"),
            QStringLiteral("shape=display;whiteSpace=wrap;html=1;"), QSizeF(80, 40));
        addAdvanced(QStringLiteral("advanced.arrowLeft"), QStringLiteral("Arrow Left"),
            QStringLiteral("shape=singleArrow;direction=west;whiteSpace=wrap;html=1;"),
            QSizeF(100, 60));
        addAdvanced(QStringLiteral("advanced.arrowRight"), QStringLiteral("Arrow Right"),
            QStringLiteral("shape=singleArrow;whiteSpace=wrap;html=1;"), QSizeF(100, 60));
        addAdvanced(QStringLiteral("advanced.arrowUp"), QStringLiteral("Arrow Up"),
            QStringLiteral("shape=singleArrow;direction=north;whiteSpace=wrap;html=1;"),
            QSizeF(60, 100));
        addAdvanced(QStringLiteral("advanced.arrowDown"), QStringLiteral("Arrow Down"),
            QStringLiteral("shape=singleArrow;direction=south;whiteSpace=wrap;html=1;"),
            QSizeF(60, 100));
        addAdvanced(QStringLiteral("advanced.doubleArrow"), QStringLiteral("Double Arrow"),
            QStringLiteral("shape=doubleArrow;whiteSpace=wrap;html=1;"), QSizeF(100, 60));
        addAdvanced(QStringLiteral("advanced.doubleArrowVertical"),
            QStringLiteral("Double Arrow Vertical"),
            QStringLiteral("shape=doubleArrow;direction=south;whiteSpace=wrap;html=1;"),
            QSizeF(60, 100));
        addAdvanced(QStringLiteral("advanced.user"), QStringLiteral("User"),
            QStringLiteral("shape=actor;whiteSpace=wrap;html=1;"), QSizeF(40, 60));
        addAdvanced(QStringLiteral("advanced.cross"), QStringLiteral("Cross"),
            QStringLiteral("shape=cross;whiteSpace=wrap;html=1;"), QSizeF(80, 80));
        addAdvanced(QStringLiteral("advanced.corner"), QStringLiteral("Corner"),
            QStringLiteral("shape=corner;whiteSpace=wrap;html=1;"), QSizeF(80, 80));
        addAdvanced(QStringLiteral("advanced.tee"), QStringLiteral("Tee"),
            QStringLiteral("shape=tee;whiteSpace=wrap;html=1;"), QSizeF(80, 80));
        addAdvanced(QStringLiteral("advanced.dataStore"), QStringLiteral("Data Store"),
            QStringLiteral("shape=datastore;whiteSpace=wrap;html=1;"), QSizeF(60, 60));
        addAdvanced(QStringLiteral("advanced.or"), QStringLiteral("Or"),
            QStringLiteral("shape=orEllipse;perimeter=ellipsePerimeter;whiteSpace=wrap;"
                           "html=1;backgroundOutline=1;"),
            QSizeF(80, 80));
        addAdvanced(QStringLiteral("advanced.sum"), QStringLiteral("Sum"),
            QStringLiteral("shape=sumEllipse;perimeter=ellipsePerimeter;whiteSpace=wrap;"
                           "html=1;backgroundOutline=1;"),
            QSizeF(80, 80));
        addAdvanced(QStringLiteral("advanced.ellipseHorizontalDivider"),
            QStringLiteral("Ellipse with horizontal divider"),
            QStringLiteral("shape=lineEllipse;perimeter=ellipsePerimeter;whiteSpace=wrap;"
                           "html=1;backgroundOutline=1;"),
            QSizeF(80, 80));
        addAdvanced(QStringLiteral("advanced.ellipseVerticalDivider"),
            QStringLiteral("Ellipse with vertical divider"),
            QStringLiteral("shape=lineEllipse;line=vertical;perimeter=ellipsePerimeter;"
                           "whiteSpace=wrap;html=1;backgroundOutline=1;"),
            QSizeF(80, 80));
        addAdvanced(QStringLiteral("advanced.sort"), QStringLiteral("Sort"),
            QStringLiteral("shape=sortShape;perimeter=rhombusPerimeter;whiteSpace=wrap;html=1;"),
            QSizeF(80, 80));
        addAdvanced(QStringLiteral("advanced.collate"), QStringLiteral("Collate"),
            QStringLiteral("shape=collate;whiteSpace=wrap;html=1;"), QSizeF(80, 80));
        addAdvanced(QStringLiteral("advanced.switch"), QStringLiteral("Switch"),
            QStringLiteral("shape=switch;whiteSpace=wrap;html=1;"), QSizeF(60, 60));

        // Process Bar: the swimlane of the sidebar's data entry with its three
        // steps (the values and styles are the decoded template).
        {
            ShapeDefinition definition;
            definition.key = QStringLiteral("advanced.processBar");
            definition.name = QStringLiteral("Process Bar");
            definition.style = QStringLiteral(
                "swimlane;childLayout=stackLayout;horizontal=1;fillColor=none;"
                "horizontalStack=1;resizeParent=1;resizeParentMax=0;resizeLast=0;"
                "collapsible=0;strokeColor=none;stackBorder=10;stackSpacing=-12;"
                "resizable=1;align=center;points=[];whiteSpace=wrap;html=1;");
            definition.value = QStringLiteral("Process Bar");
            definition.size = QSizeF(296, 100);
            definition.category = QStringLiteral("advanced");
            definition.children.append(childVertex(stepStyle, QStringLiteral("Step 1"),
                                                   10, 33, 100, 57));
            definition.children.append(childVertex(stepStyle, QStringLiteral("Step 2"),
                                                   98, 33, 100, 57));
            definition.children.append(childVertex(stepStyle, QStringLiteral("Step 3"),
                                                   186, 33, 100, 57));
            shapes.append(definition);
        }

        addAdvanced(QStringLiteral("advanced.container"), QStringLiteral("Container"),
            QStringLiteral("swimlane;"), QSizeF(200, 200), QStringLiteral("Container"));

        // List: the sidebar's List container with three field rows.
        {
            ShapeDefinition definition;
            definition.key = QStringLiteral("advanced.list");
            definition.name = QStringLiteral("List");
            definition.style = QStringLiteral(
                "swimlane;fontStyle=0;childLayout=stackLayout;horizontal=1;startSize=26;"
                "fillColor=none;horizontalStack=0;resizeParent=1;resizeParentMax=0;"
                "resizeLast=0;collapsible=1;marginBottom=0;html=1;");
            definition.value = QStringLiteral("List");
            definition.size = QSizeF(140, 110);
            definition.category = QStringLiteral("advanced");
            definition.children.append(childVertex(fieldStyle, QStringLiteral("Item 1"),
                                                   0, 26, 140, 26));
            definition.children.append(childVertex(fieldStyle, QStringLiteral("Item 2"),
                                                   0, 52, 140, 26));
            definition.children.append(childVertex(fieldStyle, QStringLiteral("Item 3"),
                                                   0, 78, 140, 26));
            shapes.append(definition);
        }

        addAdvanced(QStringLiteral("advanced.listItem"), QStringLiteral("List Item"),
            fieldStyle, QSizeF(60, 26), QStringLiteral("List Item"));

        // Horizontal Pool 1 / 2, Vertical Pool 1 / 2: the four addDataEntry()
        // templates of Sidebar-Advanced.js, each a pool with three lanes.
        {
            struct PoolTemplate
            {
                const char *key;
                const char *name;
                const char *style;
                double width;
                double height;
                bool poolHorizontal;
                bool lanesHorizontal;
            };

            static const PoolTemplate pools[] = {
                {"advanced.horizontalPool1", "Horizontal Pool 1",
                 "swimlane;childLayout=stackLayout;resizeParent=1;resizeParentMax=0;"
                 "horizontal=1;startSize=20;horizontalStack=0;html=1;", 480, 380, true, false},
                {"advanced.horizontalPool2", "Horizontal Pool 2",
                 "swimlane;childLayout=stackLayout;resizeParent=1;resizeParentMax=0;"
                 "horizontal=0;startSize=20;horizontalStack=0;html=1;", 480, 360, false, false},
                {"advanced.verticalPool1", "Vertical Pool 1",
                 "swimlane;childLayout=stackLayout;resizeParent=1;resizeParentMax=0;"
                 "startSize=20;html=1;", 360, 480, true, true},
                {"advanced.verticalPool2", "Vertical Pool 2",
                 "swimlane;childLayout=stackLayout;resizeParent=1;resizeParentMax=0;"
                 "startSize=20;horizontal=0;horizontalStack=1;", 380, 480, false, true},
            };

            for (const PoolTemplate &pool : pools)
            {
                ShapeDefinition definition;
                definition.key = QString::fromLatin1(pool.key);
                definition.name = QString::fromLatin1(pool.name);
                definition.style = QString::fromLatin1(pool.style);
                definition.value = QStringLiteral("Pool");
                definition.size = QSizeF(pool.width, pool.height);
                definition.category = QStringLiteral("advanced");

                // Lane 1..3 stacked below the pool title (horizontal=1) or
                // beside it (horizontal=0), exactly as the data entries store
                // them.
                for (int lane = 0; lane < 3; ++lane)
                {
                    QRectF geometry;

                    if (pool.lanesHorizontal)
                    {
                        geometry = QRectF((pool.poolHorizontal ? 0.0 : 20.0) + lane * 120.0,
                                          pool.poolHorizontal ? 20.0 : 0.0, 120,
                                          pool.poolHorizontal ? 460.0 : 480.0);
                    }
                    else
                    {
                        geometry = QRectF(pool.poolHorizontal ? 0.0 : 20.0,
                                          (pool.poolHorizontal ? 20.0 : 0.0) + lane * 120.0,
                                          pool.poolHorizontal ? 480.0 : 460.0, 120);
                    }

                    definition.children.append(childVertex(laneStyle,
                        QStringLiteral("Lane %1").arg(lane + 1),
                        geometry.x(), geometry.y(), geometry.width(), geometry.height()));
                }

                shapes.append(definition);
            }
        }

        // Layout containers (Sidebar-Advanced.js addLayoutContainerEntry): the
        // container is a transparentBounds swimlane whose box is derived from
        // its children, seeded with the nodes and edges of the ELK layout.
        {
            struct NodeSeed
            {
                const char *value;
                double x;
                double y;
            };

            struct LayoutTemplate
            {
                const char *key;
                const char *name;
                QString style;
                double width;
                double height;
                QVector<NodeSeed> nodes;
                QVector<QPair<int, int>> edges;
                QString edgeStyle;
            };

            const QString treeChildLayout = QStringLiteral("containerType=tree;childLayout=");

            QVector<LayoutTemplate> layouts;
            layouts.append({ "advanced.verticalTreeLayout", "Vertical Tree Layout",
                QStringLiteral("swimlane;html=1;startSize=20;horizontal=1;resizable=0;"
                               "fontSize=12;transparentBounds=1;groupPadding=20;") + treeChildLayout +
                QStringLiteral("%5B%7B%22layout%22%3A%22elkTree%22%2C%22config%22%3A%7B%22elk.direction%22"
                               "%3A%22DOWN%22%2C%22elk.spacing.nodeNode%22%3A%2240%22%2C%22elk.spacing."
                               "edgeNode%22%3A%2220%22%2C%22elk.padding%22%3A%22%5Btop%3D40%2Cleft%3D20"
                               "%2Cbottom%3D20%2Cright%3D20%5D%22%2C%22corners%22%3A%22rounded%22%2C"
                               "%22resizeLayoutRoot%22%3Atrue%2C%22resizeNodes%22%3Afalse%7D%7D%5D;"),
                280, 180,
                {{"Root", 90, 40}, {"Child 1", 20, 120}, {"Child 2", 160, 120}},
                {{0, 1}, {0, 2}}, layoutEdgeStyle });

            layouts.append({ "advanced.horizontalTreeLayout", "Horizontal Tree Layout",
                QStringLiteral("swimlane;html=1;startSize=20;horizontal=0;resizable=0;"
                               "fontSize=12;transparentBounds=1;groupPadding=20;") + treeChildLayout +
                QStringLiteral("%5B%7B%22layout%22%3A%22elkTree%22%2C%22config%22%3A%7B%22elk.direction%22"
                               "%3A%22RIGHT%22%2C%22elk.spacing.nodeNode%22%3A%2240%22%2C%22elk.spacing."
                               "edgeNode%22%3A%2220%22%2C%22elk.padding%22%3A%22%5Btop%3D20%2Cleft%3D40"
                               "%2Cbottom%3D20%2Cright%3D20%5D%22%2C%22corners%22%3A%22rounded%22%2C"
                               "%22resizeLayoutRoot%22%3Atrue%2C%22resizeNodes%22%3Afalse%7D%7D%5D;"),
                300, 160,
                {{"Root", 40, 60}, {"Child 1", 180, 20}, {"Child 2", 180, 100}},
                {{0, 1}, {0, 2}}, layoutEdgeStyle });

            layouts.append({ "advanced.balloonTreeLayout", "Balloon Tree Layout",
                QStringLiteral("swimlane;html=1;startSize=20;horizontal=1;resizable=0;"
                               "fontSize=12;transparentBounds=1;groupPadding=20;") + treeChildLayout +
                QStringLiteral("%5B%7B%22layout%22%3A%22BalloonTreeLayout%22%2C%22config%22%3A"
                               "%7B%22levelDistance%22%3A30%2C%22nodeDistance%22%3A10%2C"
                               "%22arc%22%3A300%2C%22childArc%22%3A90%7D%7D%5D;"),
                645, 537,
                {{"Root", 272, 242}, {"Child 1", 392, 173}, {"Child 2", 272, 380},
                 {"Child 3", 153, 173}, {"Child 1.1", 427, 40}, {"Child 1.2", 525, 209},
                 {"Child 2.1", 370, 477}, {"Child 2.2", 175, 477}, {"Child 3.1", 20, 209},
                 {"Child 3.2", 117, 40}},
                {{0, 1}, {0, 2}, {0, 3}, {1, 4}, {1, 5}, {2, 6}, {2, 7}, {3, 8}, {3, 9}},
                balloonEdgeStyle });

            layouts.append({ "advanced.verticalFlowLayout", "Vertical Flow Layout",
                QStringLiteral("swimlane;html=1;startSize=20;horizontal=1;resizable=0;"
                               "fontSize=12;transparentBounds=1;groupPadding=20;") + treeChildLayout +
                QStringLiteral("%5B%7B%22layout%22%3A%22elkLayered%22%2C%22config%22%3A%7B"
                               "%22elk.direction%22%3A%22DOWN%22%2C%22elk.layered.spacing.nodeNode"
                               "BetweenLayers%22%3A%2250%22%2C%22elk.layered.considerModelOrder."
                               "strategy%22%3A%22NODES_AND_EDGES%22%2C%22elk.padding%22%3A%22%5Btop"
                               "%3D40%2Cleft%3D20%2Cbottom%3D20%2Cright%3D20%5D%22%2C%22edgeStyle%22"
                               "%3A%22orthogonalEdgeStyle%22%2C%22corners%22%3A%22rounded%22%2C"
                               "%22extractIsolated%22%3Afalse%2C%22resizeLayoutRoot%22%3Atrue%2C"
                               "%22resizeNodes%22%3Afalse%7D%7D%5D;"),
                270, 280,
                {{"Start", 36.67, 40}, {"Task", 20, 130}, {"Task", 150, 130},
                 {"End", 36.67, 220}},
                {{0, 1}, {0, 2}, {1, 3}, {2, 3}}, layoutEdgeStyle });

            layouts.append({ "advanced.horizontalFlowLayout", "Horizontal Flow Layout",
                QStringLiteral("swimlane;html=1;startSize=20;horizontal=0;resizable=0;"
                               "fontSize=12;transparentBounds=1;groupPadding=20;") + treeChildLayout +
                QStringLiteral("%5B%7B%22layout%22%3A%22elkLayered%22%2C%22config%22%3A%7B"
                               "%22elk.direction%22%3A%22RIGHT%22%2C%22elk.layered.spacing.nodeNode"
                               "BetweenLayers%22%3A%2250%22%2C%22elk.layered.considerModelOrder."
                               "strategy%22%3A%22NODES_AND_EDGES%22%2C%22elk.padding%22%3A%22%5Btop"
                               "%3D20%2Cleft%3D40%2Cbottom%3D20%2Cright%3D20%5D%22%2C%22edgeStyle%22"
                               "%3A%22orthogonalEdgeStyle%22%2C%22corners%22%3A%22rounded%22%2C"
                               "%22extractIsolated%22%3Afalse%2C%22resizeLayoutRoot%22%3Atrue%2C"
                               "%22resizeNodes%22%3Afalse%7D%7D%5D;"),
                460, 150,
                {{"Start", 40, 26.67}, {"Task", 190, 20}, {"Task", 190, 90},
                 {"End", 340, 26.67}},
                {{0, 1}, {0, 2}, {1, 3}, {2, 3}}, layoutEdgeStyle });

            for (const LayoutTemplate &layout : layouts)
            {
                ShapeDefinition definition;
                definition.key = QString::fromLatin1(layout.key);
                definition.name = QString::fromLatin1(layout.name);
                definition.style = layout.style;
                definition.value = QStringLiteral("Layout Container");
                definition.size = QSizeF(layout.width, layout.height);
                definition.category = QStringLiteral("advanced");

                for (const NodeSeed &node : layout.nodes)
                {
                    definition.children.append(childVertex(wrap,
                        QString::fromLatin1(node.value), node.x, node.y, 100, 40));
                }

                for (const QPair<int, int> &edge : layout.edges)
                {
                    definition.children.append(childEdge(layout.edgeStyle,
                                                         edge.first, edge.second));
                }

                shapes.append(definition);
            }
        }
    }

    // --- flowchart -------------------------------------------------------
    // Exactly the palette of Sidebar-Flowchart.js, using its style strings
    // verbatim: the stencil entries of flowchart.xml (mxgraph.flowchart.*)
    // plus the registered shapes card, hexagon, internalStorage, manualInput,
    // offPageConnector, parallelogram, process, tape and trapezoid, and the
    // Transfer arrow.
    add(QStringLiteral("flowchart.annotation1"), QStringLiteral("Annotation"),
        QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.annotation_1;"
                       "align=left;pointerEvents=1;"),
        QSizeF(50, 100), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.annotation2"), QStringLiteral("Annotation"),
        QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.annotation_2;"
                       "align=left;labelPosition=right;pointerEvents=1;"),
        QSizeF(50, 100), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.card"), QStringLiteral("Card"),
        QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;shape=card;"
                       "whiteSpace=wrap;size=20;arcSize=12;"),
        QSizeF(100, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.collate"), QStringLiteral("Collate"),
        QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                       "shape=mxgraph.flowchart.collate;"),
        QSizeF(100, 100), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.data"), QStringLiteral("Data"),
        QStringLiteral("shape=parallelogram;html=1;strokeWidth=2;"
                       "perimeter=parallelogramPerimeter;whiteSpace=wrap;rounded=1;"
                       "arcSize=12;size=0.23;"),
        QSizeF(100, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.database"), QStringLiteral("Database"),
        QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.database;"
                       "whiteSpace=wrap;"),
        QSizeF(60, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.decision"), QStringLiteral("Decision"),
        QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.decision;"
                       "whiteSpace=wrap;"),
        QSizeF(100, 100), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.delay"), QStringLiteral("Delay"),
        QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.delay;"
                       "whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.directData"), QStringLiteral("Direct Data"),
        QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.direct_data;"
                       "whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.display"), QStringLiteral("Display"),
        QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.display;"
                       "whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.document"), QStringLiteral("Document"),
        QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.document2;"
                       "whiteSpace=wrap;size=0.25;"),
        QSizeF(100, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.extract"), QStringLiteral("Extract or Measurement"),
        QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.extract_or_measurement;"
                       "whiteSpace=wrap;"),
        QSizeF(95, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.internalStorage"), QStringLiteral("Internal Storage"),
        QStringLiteral("shape=internalStorage;whiteSpace=wrap;html=1;dx=15;dy=15;"
                       "rounded=1;arcSize=8;strokeWidth=2;"),
        QSizeF(70, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.loopLimit"), QStringLiteral("Loop Limit"),
        QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.loop_limit;"
                       "whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.manualInput"), QStringLiteral("Manual Input"),
        QStringLiteral("html=1;strokeWidth=2;shape=manualInput;boundedLbl=1;"
                       "whiteSpace=wrap;rounded=1;size=26;arcSize=11;"),
        QSizeF(100, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.manualOperation"), QStringLiteral("Manual Operation"),
        QStringLiteral("verticalLabelPosition=middle;verticalAlign=middle;html=1;"
                       "shape=trapezoid;perimeter=trapezoidPerimeter;whiteSpace=wrap;"
                       "size=0.23;arcSize=10;flipV=1;labelPosition=center;align=center;"),
        QSizeF(100, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.merge"), QStringLiteral("Merge or Storage"),
        QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.merge_or_storage;"
                       "whiteSpace=wrap;"),
        QSizeF(95, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.multiDocument"), QStringLiteral("Multi-Document"),
        QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.multi-document;"
                       "whiteSpace=wrap;boundedLbl=1;"),
        QSizeF(88, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.offPage"), QStringLiteral("Off-Page Reference"),
        QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                       "shape=offPageConnector;rounded=0;size=0.5;"),
        QSizeF(60, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.onPage"), QStringLiteral("On-Page Reference"),
        QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                       "shape=mxgraph.flowchart.on-page_reference;"),
        QSizeF(60, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.or"), QStringLiteral("Or"),
        QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                       "shape=mxgraph.flowchart.or_2;"),
        QSizeF(70, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.tape"), QStringLiteral("Tape"),
        QStringLiteral("shape=tape;whiteSpace=wrap;html=1;strokeWidth=2;size=0.19"),
        QSizeF(100, 65), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.parallelMode"), QStringLiteral("Parallel Mode"),
        QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                       "shape=mxgraph.flowchart.parallel_mode;accentColor=#ffff00;"
                       "pointerEvents=1"),
        QSizeF(95, 40), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.predefinedProcess"), QStringLiteral("Predefined Process"),
        QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                       "shape=process;whiteSpace=wrap;rounded=1;size=0.14;arcSize=6;"),
        QSizeF(100, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.preparation"), QStringLiteral("Preparation"),
        QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                       "shape=hexagon;perimeter=hexagonPerimeter2;arcSize=6;size=0.27;"),
        QSizeF(100, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.process"), QStringLiteral("Process"),
        QStringLiteral("rounded=1;whiteSpace=wrap;html=1;absoluteArcSize=1;arcSize=14;"
                       "strokeWidth=2;"),
        QSizeF(100, 100), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.sequentialData"), QStringLiteral("Sequential Data"),
        QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.sequential_data;"
                       "whiteSpace=wrap;"),
        QSizeF(100, 100), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.sort"), QStringLiteral("Sort"),
        QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                       "shape=mxgraph.flowchart.sort;"),
        QSizeF(100, 100), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.start1"), QStringLiteral("Start"),
        QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.start_1;"
                       "whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.start2"), QStringLiteral("Start"),
        QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.start_2;"
                       "whiteSpace=wrap;"),
        QSizeF(100, 100), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.storedData"), QStringLiteral("Stored Data"),
        QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.stored_data;"
                       "whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.summingJunction"), QStringLiteral("Summing Junction"),
        QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                       "shape=mxgraph.flowchart.summing_junction;"),
        QSizeF(70, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.terminator"), QStringLiteral("Terminator"),
        QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.terminator;"
                       "whiteSpace=wrap;"),
        QSizeF(100, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("flowchart.transfer"), QStringLiteral("Transfer"),
        QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                       "strokeWidth=2;shape=mxgraph.arrows2.arrow;dy=0.6;dx=40;notch=0;"),
        QSizeF(100, 70), QStringLiteral("flowchart"));

    // --- arrows ----------------------------------------------------------
    // Exactly the arrows2 palette of draw.io (shapes/mxArrows.js), using the
    // style strings of Sidebar-Arrows2.js.
    add(QStringLiteral("arrow.arrow"), QStringLiteral("Arrow"),
        QStringLiteral("shape=mxgraph.arrows2.arrow;dy=0.6;dx=40;notch=0;")
            + plain + green,
        QSizeF(100, 70), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.twoWay"), QStringLiteral("Two Way Arrow"),
        QStringLiteral("shape=mxgraph.arrows2.twoWayArrow;dy=0.6;dx=35;")
            + plain + green,
        QSizeF(100, 60), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.stylised"), QStringLiteral("Stylised Arrow"),
        QStringLiteral("shape=mxgraph.arrows2.stylisedArrow;dy=0.6;dx=40;notch=15;feather=0.4;")
            + plain + blue,
        QSizeF(100, 60), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.sharp"), QStringLiteral("Sharp Arrow"),
        QStringLiteral("shape=mxgraph.arrows2.sharpArrow;dy1=0.67;dx1=18;dx2=18;notch=0;")
            + plain + green,
        QSizeF(100, 60), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.sharp2"), QStringLiteral("Sharp Arrow 2"),
        QStringLiteral("shape=mxgraph.arrows2.sharpArrow2;dy1=0.67;dx1=18;dx2=18;dy3=0.15;"
                       "dx3=27;notch=0;") + plain + green,
        QSizeF(100, 60), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.callout"), QStringLiteral("Callout with Arrow"),
        QStringLiteral("shape=mxgraph.arrows2.calloutArrow;dy=10;dx=20;notch=60;arrowHead=10;")
            + plain + blue,
        QSizeF(100, 60), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.bend"), QStringLiteral("Bend Arrow"),
        QStringLiteral("shape=mxgraph.arrows2.bendArrow;dy=15;dx=38;notch=0;arrowHead=55;"
                       "rounded=0;") + plain + green,
        QSizeF(100, 100), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.bendDouble"), QStringLiteral("Bend Double Arrow"),
        QStringLiteral("shape=mxgraph.arrows2.bendDoubleArrow;dy=15;dx=38;arrowHead=55;"
                       "rounded=0;") + plain + green,
        QSizeF(100, 100), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.calloutDouble"), QStringLiteral("Callout with Double Arrow"),
        QStringLiteral("shape=mxgraph.arrows2.calloutDoubleArrow;dy=10;dx=20;notch=24;"
                       "arrowHead=10;") + plain + blue,
        QSizeF(100, 50), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.calloutQuad"), QStringLiteral("Callout with Quad Arrow"),
        QStringLiteral("shape=mxgraph.arrows2.calloutQuadArrow;dy=10;dx=20;notch=24;"
                       "arrowHead=10;") + plain + blue,
        QSizeF(100, 100), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.calloutDouble90"),
        QStringLiteral("Callout with Double Arrow 90\u00b0"),
        QStringLiteral("shape=mxgraph.arrows2.calloutDouble90Arrow;dy1=10;dx1=20;dx2=70;"
                       "dy2=70;arrowHead=10;") + plain + blue,
        QSizeF(100, 100), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.quad"), QStringLiteral("Quad Arrow"),
        QStringLiteral("shape=mxgraph.arrows2.quadArrow;dy=10;dx=20;notch=24;arrowHead=10;")
            + plain + purple,
        QSizeF(100, 100), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.triad"), QStringLiteral("Triad Arrow"),
        QStringLiteral("shape=mxgraph.arrows2.triadArrow;dy=10;dx=20;arrowHead=40;")
            + plain + purple,
        QSizeF(100, 70), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.tailed"), QStringLiteral("Tailed Arrow"),
        QStringLiteral("shape=mxgraph.arrows2.tailedArrow;dy1=10;dx1=20;notch=0;arrowHead=20;"
                       "dx2=25;dy2=30;") + plain + green,
        QSizeF(100, 60), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.tailedNotch"), QStringLiteral("Tailed Arrow with Notch"),
        QStringLiteral("shape=mxgraph.arrows2.tailedNotchedArrow;dy1=10;dx1=20;notch=20;"
                       "arrowHead=20;dx2=25;dy2=30;") + plain + green,
        QSizeF(100, 60), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.striped"), QStringLiteral("Striped Arrow"),
        QStringLiteral("shape=mxgraph.arrows2.stripedArrow;dy=0.6;dx=40;notch=25;")
            + plain + yellow,
        QSizeF(100, 70), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.jumpIn"), QStringLiteral("Jump-In Arrow"),
        QStringLiteral("shape=mxgraph.arrows2.jumpInArrow;dy=15;dx=38;arrowHead=55;")
            + plain + blue,
        QSizeF(100, 100), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.uturn"), QStringLiteral("U Turn Arrow"),
        QStringLiteral("shape=mxgraph.arrows2.uTurnArrow;dy=11;arrowHead=43;dx2=25;")
            + plain + green,
        QSizeF(100, 100), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.wedge"), QStringLiteral("Wedge Arrow"),
        QStringLiteral("shape=mxgraph.arrows2.wedgeArrow;startWidth=50;") + plain + orange,
        QSizeF(100, 100), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.wedgeDashed"), QStringLiteral("Wedge Arrow Dashed"),
        QStringLiteral("shape=mxgraph.arrows2.wedgeArrowDashed2;startWidth=50;stepSize=15;")
            + plain + orange,
        QSizeF(100, 100), QStringLiteral("arrows"));

    // --- UML -------------------------------------------------------------
    add(QStringLiteral("uml.class"), QStringLiteral("Class"),
        QStringLiteral("swimlane;fontStyle=1;childLayout=stackLayout;horizontal=1;"
                       "startSize=26;") + blue + QStringLiteral("html=1;"),
        QSizeF(220, 140), QStringLiteral("uml"));
    add(QStringLiteral("uml.interface"), QStringLiteral("Interface"),
        QStringLiteral("ellipse;") + plain + green, QSizeF(100, 100), QStringLiteral("uml"));
    add(QStringLiteral("uml.package"), QStringLiteral("Package"),
        QStringLiteral("shape=mxgraph.uml.package;") + plain + yellow,
        QSizeF(150, 110), QStringLiteral("uml"));
    add(QStringLiteral("uml.component"), QStringLiteral("Component"),
        QStringLiteral("shape=mxgraph.uml.component;") + plain + blue,
        QSizeF(160, 100), QStringLiteral("uml"));
    add(QStringLiteral("uml.note"), QStringLiteral("Note"),
        QStringLiteral("shape=note;backgroundOutline=1;") + plain + yellow,
        QSizeF(130, 100), QStringLiteral("uml"));
    add(QStringLiteral("uml.usecase"), QStringLiteral("Usecase"),
        QStringLiteral("ellipse;") + plain + green, QSizeF(150, 80), QStringLiteral("uml"));
    add(QStringLiteral("uml.state"), QStringLiteral("State"),
        QStringLiteral("rounded=1;arcSize=40;") + plain + green,
        QSizeF(140, 70), QStringLiteral("uml"));
    add(QStringLiteral("uml.initial"), QStringLiteral("Initial"),
        QStringLiteral("ellipse;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(36, 36), QStringLiteral("uml"));
    add(QStringLiteral("uml.final"), QStringLiteral("Final"),
        QStringLiteral("shape=doubleEllipse;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(44, 44), QStringLiteral("uml"));
    add(QStringLiteral("uml.decision"), QStringLiteral("Decision"),
        QStringLiteral("rhombus;") + plain + yellow, QSizeF(90, 90), QStringLiteral("uml"));
    add(QStringLiteral("uml.fork"), QStringLiteral("Fork"),
        QStringLiteral("rounded=0;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(140, 12), QStringLiteral("uml"));
    add(QStringLiteral("uml.object"), QStringLiteral("Object"),
        QStringLiteral("swimlane;fontStyle=4;startSize=26;") + blue + QStringLiteral("html=1;"),
        QSizeF(200, 100), QStringLiteral("uml"));
    add(QStringLiteral("uml.activity"), QStringLiteral("Activity"),
        QStringLiteral("rounded=1;arcSize=45;") + plain + green,
        QSizeF(150, 60), QStringLiteral("uml"));
    add(QStringLiteral("uml.node"), QStringLiteral("Node"),
        QStringLiteral("shape=cube;boundedLbl=1;backgroundOutline=1;darkOpacity=0.05;")
            + plain + blue, QSizeF(140, 100), QStringLiteral("uml"));
    add(QStringLiteral("uml.artifact"), QStringLiteral("Artifact"),
        QStringLiteral("shape=note;backgroundOutline=1;") + plain + blue,
        QSizeF(120, 90), QStringLiteral("uml"));
    add(QStringLiteral("uml.database"), QStringLiteral("Database"),
        QStringLiteral("shape=cylinder3;boundedLbl=1;backgroundOutline=1;size=15;")
            + plain + blue, QSizeF(110, 100), QStringLiteral("uml"));

    // --- ER --------------------------------------------------------------
    add(QStringLiteral("er.entity"), QStringLiteral("Entity"),
        QStringLiteral("swimlane;fontStyle=1;startSize=28;") + green + QStringLiteral("html=1;"),
        QSizeF(180, 90), QStringLiteral("er"));
    add(QStringLiteral("er.weakentity"), QStringLiteral("Weakentity"),
        QStringLiteral("shape=mxgraph.er.weak_entity;fontStyle=1;") + green,
        QSizeF(180, 90), QStringLiteral("er"));
    add(QStringLiteral("er.relationship"), QStringLiteral("Relationship"),
        QStringLiteral("rhombus;") + plain + orange, QSizeF(140, 90), QStringLiteral("er"));
    add(QStringLiteral("er.attribute"), QStringLiteral("Attribute"),
        QStringLiteral("ellipse;") + plain + yellow, QSizeF(130, 70), QStringLiteral("er"));
    add(QStringLiteral("er.keyattribute"), QStringLiteral("Keyattribute"),
        QStringLiteral("ellipse;fontStyle=4;") + plain + yellow,
        QSizeF(140, 70), QStringLiteral("er"));
    add(QStringLiteral("er.multivalued"), QStringLiteral("Multivalued"),
        QStringLiteral("shape=doubleEllipse;") + plain + yellow,
        QSizeF(140, 80), QStringLiteral("er"));
    add(QStringLiteral("er.derived"), QStringLiteral("Derived"),
        QStringLiteral("ellipse;dashed=1;") + plain + yellow,
        QSizeF(140, 70), QStringLiteral("er"));
    add(QStringLiteral("er.associative"), QStringLiteral("Associative"),
        QStringLiteral("shape=rhombus;") + plain + purple,
        QSizeF(150, 100), QStringLiteral("er"));

    // --- BPMN ------------------------------------------------------------
    add(QStringLiteral("bpmn.start"), QStringLiteral("Start"),
        QStringLiteral("ellipse;strokeWidth=1.5;") + white,
        QSizeF(56, 56), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.intermediate"), QStringLiteral("Intermediate"),
        QStringLiteral("shape=doubleEllipse;strokeWidth=1.5;") + white,
        QSizeF(64, 64), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.end"), QStringLiteral("End"),
        QStringLiteral("ellipse;strokeWidth=4;") + white,
        QSizeF(56, 56), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.task"), QStringLiteral("Task"),
        QStringLiteral("rounded=1;arcSize=12;") + white, QSizeF(150, 80), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.usertask"), QStringLiteral("Usertask"),
        QStringLiteral("rounded=1;arcSize=12;bpmnType=user;") + white, QSizeF(150, 80), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.servicetask"), QStringLiteral("Servicetask"),
        QStringLiteral("rounded=1;arcSize=12;bpmnType=service;") + white, QSizeF(150, 80), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.scripttask"), QStringLiteral("Scripttask"),
        QStringLiteral("rounded=1;arcSize=12;bpmnType=script;") + white, QSizeF(150, 80), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.subprocess"), QStringLiteral("Subprocess"),
        QStringLiteral("rounded=1;arcSize=12;bpmnType=subprocess;") + white, QSizeF(170, 90), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.exclusive"), QStringLiteral("Exclusive"),
        QStringLiteral("rhombus;bpmnType=exclusive;") + white, QSizeF(80, 80), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.parallel"), QStringLiteral("Parallel"),
        QStringLiteral("rhombus;bpmnType=parallel;") + white, QSizeF(80, 80), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.inclusive"), QStringLiteral("Inclusive"),
        QStringLiteral("rhombus;bpmnType=inclusive;") + white, QSizeF(80, 80), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.pool"), QStringLiteral("Pool"),
        QStringLiteral("swimlane;horizontal=0;startSize=30;") + white + QStringLiteral("html=1;"),
        QSizeF(420, 180), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.lane"), QStringLiteral("Lane"),
        QStringLiteral("swimlane;horizontal=0;startSize=26;") + white + QStringLiteral("html=1;"),
        QSizeF(420, 120), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.dataobject"), QStringLiteral("Dataobject"),
        QStringLiteral("shape=note;backgroundOutline=1;") + white,
        QSizeF(70, 90), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.datastore"), QStringLiteral("Datastore"),
        QStringLiteral("shape=mxgraph.bpmn.data_store;") + white,
        QSizeF(100, 80), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.message"), QStringLiteral("Message"),
        QStringLiteral("shape=mxgraph.bpmn.message;") + white,
        QSizeF(90, 60), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.group"), QStringLiteral("Group"),
        QStringLiteral("rounded=1;dashed=1;fillColor=none;strokeColor=#000000;"),
        QSizeF(200, 140), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.annotation"), QStringLiteral("Annotation"),
        QStringLiteral("shape=mxgraph.flowchart.annotation_1;align=left;verticalAlign=top;")
            + plain, QSizeF(50, 100), QStringLiteral("bpmn"));

    // --- network ---------------------------------------------------------
    add(QStringLiteral("net.server"), QStringLiteral("Server"),
        QStringLiteral("rounded=0;netType=server;") + blue, QSizeF(100, 120), QStringLiteral("network"));
    add(QStringLiteral("net.router"), QStringLiteral("Router"),
        QStringLiteral("shape=cylinder3;boundedLbl=1;backgroundOutline=1;size=18;") + blue,
        QSizeF(120, 70), QStringLiteral("network"));
    add(QStringLiteral("net.switch"), QStringLiteral("Switch"),
        QStringLiteral("rounded=0;netType=switch;") + blue, QSizeF(130, 60), QStringLiteral("network"));
    add(QStringLiteral("net.firewall"), QStringLiteral("Firewall"),
        QStringLiteral("shape=mxgraph.networks.firewall;") + red,
        QSizeF(120, 80), QStringLiteral("network"));
    add(QStringLiteral("net.cloud"), QStringLiteral("Cloud"),
        QStringLiteral("ellipse;shape=cloud;") + plain + purple,
        QSizeF(160, 100), QStringLiteral("network"));
    add(QStringLiteral("net.database"), QStringLiteral("Database"),
        QStringLiteral("shape=cylinder3;boundedLbl=1;backgroundOutline=1;size=15;") + blue,
        QSizeF(100, 110), QStringLiteral("network"));
    add(QStringLiteral("net.storage"), QStringLiteral("Storage"),
        QStringLiteral("shape=mxgraph.networks.storage;") + blue,
        QSizeF(100, 110), QStringLiteral("network"));
    add(QStringLiteral("net.workstation"), QStringLiteral("Workstation"),
        QStringLiteral("rounded=0;netType=workstation;") + blue, QSizeF(120, 90), QStringLiteral("network"));
    add(QStringLiteral("net.laptop"), QStringLiteral("Laptop"),
        QStringLiteral("shape=mxgraph.networks.laptop;") + blue,
        QSizeF(130, 80), QStringLiteral("network"));
    add(QStringLiteral("net.phone"), QStringLiteral("Phone"),
        QStringLiteral("rounded=1;arcSize=20;") + blue, QSizeF(60, 100), QStringLiteral("network"));
    add(QStringLiteral("net.printer"), QStringLiteral("Printer"),
        QStringLiteral("shape=mxgraph.networks.printer;") + blue,
        QSizeF(110, 80), QStringLiteral("network"));
    add(QStringLiteral("net.wireless"), QStringLiteral("Wireless"),
        QStringLiteral("rounded=1;arcSize=15;netType=wireless;") + blue, QSizeF(90, 90), QStringLiteral("network"));
    add(QStringLiteral("net.vpn"), QStringLiteral("Vpn"),
        QStringLiteral("shape=mxgraph.networks.vpn;") + green,
        QSizeF(110, 110), QStringLiteral("network"));
    add(QStringLiteral("net.loadbalancer"), QStringLiteral("Loadbalancer"),
        QStringLiteral("ellipse;netType=loadbalancer;") + blue, QSizeF(100, 100), QStringLiteral("network"));
    add(QStringLiteral("net.users"), QStringLiteral("Users"),
        QStringLiteral("shape=mxgraph.networks.users;") + blue,
        QSizeF(120, 90), QStringLiteral("network"));

    // --- data flow / C4 / UI kits ---------------------------------------
    add(QStringLiteral("dfd.process"), QStringLiteral("Process"),
        QStringLiteral("ellipse;") + green, QSizeF(120, 80), QStringLiteral("dfd"));
    add(QStringLiteral("dfd.datastore"), QStringLiteral("Datastore"),
        QStringLiteral("shape=mxgraph.dfd.data_store;") + blue,
        QSizeF(160, 50), QStringLiteral("dfd"));
    add(QStringLiteral("dfd.external"), QStringLiteral("External"),
        QStringLiteral("rounded=0;") + yellow, QSizeF(140, 70), QStringLiteral("dfd"));
    add(QStringLiteral("dfd.processgane"), QStringLiteral("Processgane"),
        QStringLiteral("rounded=1;arcSize=10;") + green, QSizeF(140, 80), QStringLiteral("dfd"));

    add(QStringLiteral("c4.person"), QStringLiteral("Person"),
        QStringLiteral("shape=umlActor;verticalLabelPosition=bottom;verticalAlign=top;")
            + blue, QSizeF(70, 120), QStringLiteral("c4"));
    add(QStringLiteral("c4.system"), QStringLiteral("System"),
        QStringLiteral("rounded=1;arcSize=6;") + blue, QSizeF(200, 120), QStringLiteral("c4"));
    add(QStringLiteral("c4.container"), QStringLiteral("Container"),
        QStringLiteral("rounded=1;arcSize=6;") + green, QSizeF(200, 110), QStringLiteral("c4"));
    add(QStringLiteral("c4.component"), QStringLiteral("Component"),
        QStringLiteral("rounded=1;arcSize=6;") + yellow, QSizeF(180, 100), QStringLiteral("c4"));

    add(QStringLiteral("ui.button"), QStringLiteral("Button"),
        QStringLiteral("rounded=1;arcSize=20;") + blue, QSizeF(110, 40), QStringLiteral("mockup"));
    add(QStringLiteral("ui.input"), QStringLiteral("Input"),
        QStringLiteral("rounded=0;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(180, 40), QStringLiteral("mockup"));
    add(QStringLiteral("ui.window"), QStringLiteral("Window"),
        QStringLiteral("swimlane;startSize=26;") + blue, QSizeF(280, 180),
        QStringLiteral("mockup"));
    add(QStringLiteral("ui.navbar"), QStringLiteral("Navbar"),
        QStringLiteral("rounded=0;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(320, 50), QStringLiteral("mockup"));
    add(QStringLiteral("ui.browser"), QStringLiteral("Browser"),
        QStringLiteral("swimlane;startSize=30;") + blue, QSizeF(320, 220),
        QStringLiteral("mockup"));

    add(QStringLiteral("android.phone"), QStringLiteral("Phone"),
        QStringLiteral("rounded=1;arcSize=8;androidType=phone;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(100, 190), QStringLiteral("software"));
    add(QStringLiteral("android.tablet"), QStringLiteral("Tablet"),
        QStringLiteral("rounded=1;arcSize=6;androidType=tablet;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(160, 220), QStringLiteral("software"));
    add(QStringLiteral("android.watch"), QStringLiteral("Watch"),
        QStringLiteral("rounded=1;arcSize=30;androidType=watch;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(100, 100), QStringLiteral("software"));

    // --- electrical ------------------------------------------------------
    const QString lineArt = QStringLiteral("fillColor=none;strokeColor=#000000;strokeWidth=1.5;"
                                           "html=1;verticalLabelPosition=bottom;"
                                           "verticalAlign=top;spacingTop=2;");
    const QString solidArt = QStringLiteral("fillColor=#ffffff;strokeColor=#000000;strokeWidth=1.5;"
                                            "html=1;verticalLabelPosition=bottom;"
                                            "verticalAlign=top;spacingTop=2;");

    struct ElecDef { const char *key; const char *name; const char *shape; bool filled; QSizeF size; };
    const QVector<ElecDef> electrical = {
        {"elec.resistor", "Resistor", "elec.resistor", false, QSizeF(120, 50)},
        {"elec.resistorus", "Resistor (Zigzag)", "elec.resistorus", false, QSizeF(120, 60)},
        {"elec.capacitor", "Capacitor", "elec.capacitor", false, QSizeF(80, 70)},
        {"elec.capacitorpolar", "Polarized Capacitor", "elec.capacitorpolar", false, QSizeF(80, 70)},
        {"elec.inductor", "Inductor", "elec.inductor", false, QSizeF(120, 50)},
        {"elec.diode", "Diode", "elec.diode", true, QSizeF(110, 60)},
        {"elec.zener", "Zener Diode", "elec.zener", true, QSizeF(110, 70)},
        {"elec.led", "LED", "elec.led", true, QSizeF(110, 70)},
        {"elec.transistornpn", "NPN Transistor", "elec.transistornpn", false, QSizeF(100, 100)},
        {"elec.transistorpnp", "PNP Transistor", "elec.transistorpnp", false, QSizeF(100, 100)},
        {"elec.batterystack", "Battery", "mxgraph.electrical.miscellaneous.batteryStack", false, QSizeF(120, 60)},
        {"elec.ground", "Ground", "elec.ground", false, QSizeF(80, 70)},
        {"elec.groundchassis", "Chassis Ground", "elec.groundchassis", false, QSizeF(80, 70)},
        {"elec.lamp", "Lamp", "elec.lamp", false, QSizeF(80, 80)},
        {"elec.motor", "Motor", "elec.motor", false, QSizeF(90, 90)},
        {"elec.switchspst", "Switch (SPST)", "elec.switchspst", false, QSizeF(120, 60)},
        {"elec.pushbutton", "Push Button", "elec.pushbutton", false, QSizeF(120, 70)},
        {"elec.transformer", "Transformer", "elec.transformer", false, QSizeF(130, 90)},
        {"elec.fuse", "Fuse", "elec.fuse", false, QSizeF(120, 50)},
        {"elec.acsource", "AC Source", "elec.acsource", false, QSizeF(90, 90)},
        {"elec.voltagesource", "Voltage Source", "elec.voltagesource", false, QSizeF(90, 90)},
        {"elec.ammeter", "Ammeter", "elec.ammeter", false, QSizeF(80, 80)},
        {"elec.voltmeter", "Voltmeter", "elec.voltmeter", false, QSizeF(80, 80)},
        {"elec.antenna", "Antenna", "elec.antenna", false, QSizeF(100, 100)},
        {"elec.speaker", "Speaker", "elec.speaker", false, QSizeF(100, 80)},
        {"elec.microphone", "Microphone", "elec.microphone", false, QSizeF(80, 100)},
        {"elec.opamp", "Op-Amp", "elec.opamp", true, QSizeF(110, 100)},
        {"elec.gate.and", "AND Gate", "elec.gate.and", true, QSizeF(120, 80)},
        {"elec.gate.or", "OR Gate", "elec.gate.or", true, QSizeF(120, 80)},
        {"elec.gate.not", "NOT Gate", "elec.gate.not", true, QSizeF(110, 80)},
        {"elec.gate.nand", "NAND Gate", "elec.gate.nand", true, QSizeF(130, 80)},
        {"elec.gate.nor", "NOR Gate", "elec.gate.nor", true, QSizeF(130, 80)},
        {"elec.gate.xor", "XOR Gate", "elec.gate.xor", true, QSizeF(130, 80)},
    };

    for (const ElecDef &entry : electrical)
    {
        add(QString::fromLatin1(entry.key), QString::fromUtf8(entry.name),
            QStringLiteral("shape=%1;%2").arg(QString::fromLatin1(entry.shape),
                                             entry.filled ? solidArt : lineArt),
            entry.size, QStringLiteral("electrical"));
    }

    add(QStringLiteral("elec.junction"), QStringLiteral("Junction"),
        QStringLiteral("shape=elec.junction;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(24, 24), QStringLiteral("electrical"));

    // --- floor plan -------------------------------------------------------
    add(QStringLiteral("floor.wall"), QStringLiteral("Wall"),
        QStringLiteral("rounded=0;fillColor=#ffffff;strokeColor=#000000;strokeWidth=2;"),
        QSizeF(240, 16), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.door"), QStringLiteral("Door"),
        QStringLiteral("shape=floor.door;fillColor=none;strokeColor=#000000;strokeWidth=2;"),
        QSizeF(90, 90), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.window"), QStringLiteral("Window"),
        QStringLiteral("shape=floor.window;fillColor=#ffffff;strokeColor=#000000;strokeWidth=2;"),
        QSizeF(140, 16), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.stairs"), QStringLiteral("Stairs"),
        QStringLiteral("shape=floor.stairs;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(120, 160), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.table"), QStringLiteral("Table"),
        QStringLiteral("rounded=1;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(160, 90), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.chair"), QStringLiteral("Chair"),
        QStringLiteral("rounded=1;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(60, 60), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.bed"), QStringLiteral("Bed"),
        QStringLiteral("rounded=1;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(180, 120), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.sofa"), QStringLiteral("Sofa"),
        QStringLiteral("rounded=1;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(180, 80), QStringLiteral("floorplan"));

    // --- infographic ------------------------------------------------------
    add(QStringLiteral("info.chevron"), QStringLiteral("Chevron"),
        QStringLiteral("shape=step;perimeter=stepPerimeter;fixedSize=1;fillColor=#ffffff;"
                       "strokeColor=none;fontColor=#ffffff;html=1;"),
        QSizeF(180, 70), QStringLiteral("infographic"));
    add(QStringLiteral("info.ribbon"), QStringLiteral("Ribbon"),
        QStringLiteral("shape=info.ribbon;fillColor=#ffffff;strokeColor=none;"
                       "fontColor=#ffffff;html=1;"),
        QSizeF(180, 70), QStringLiteral("infographic"));
    add(QStringLiteral("info.badge"), QStringLiteral("Badge"),
        QStringLiteral("shape=star;perimeter=starPerimeter;fillColor=#ffffff;strokeColor=none;"
                       "fontColor=#ffffff;html=1;"),
        QSizeF(110, 110), QStringLiteral("infographic"));
    add(QStringLiteral("info.bubble"), QStringLiteral("Bubble"),
        QStringLiteral("shape=callout;perimeter=calloutPerimeter;fillColor=#ffffff;"
                       "strokeColor=#000000;html=1;"),
        QSizeF(180, 100), QStringLiteral("infographic"));
    add(QStringLiteral("info.progress"), QStringLiteral("Progress"),
        QStringLiteral("rounded=1;arcSize=50;fillColor=#ffffff;strokeColor=none;"),
        QSizeF(240, 24), QStringLiteral("infographic"));

    // --- SysML ------------------------------------------------------------
    add(QStringLiteral("sysml.block"), QStringLiteral("Block"),
        QStringLiteral("swimlane;fontStyle=1;startSize=28;fillColor=#ffffff;"
                       "strokeColor=#000000;html=1;"),
        QSizeF(200, 120), QStringLiteral("sysml"));
    add(QStringLiteral("sysml.port"), QStringLiteral("Port"),
        QStringLiteral("fillColor=#ffffff;strokeColor=#000000;"), QSizeF(24, 24), QStringLiteral("sysml"));
    add(QStringLiteral("sysml.requirement"), QStringLiteral("Requirement"),
        QStringLiteral("shape=note;backgroundOutline=1;fillColor=#ffffff;strokeColor=#000000;"
                       "html=1;"), QSizeF(180, 100), QStringLiteral("sysml"));
    add(QStringLiteral("sysml.constraint"), QStringLiteral("Constraint"),
        QStringLiteral("rounded=0;fillColor=#ffffff;strokeColor=#000000;html=1;"),
        QSizeF(170, 90), QStringLiteral("sysml"));
    add(QStringLiteral("sysml.item"), QStringLiteral("Item"),
        QStringLiteral("rounded=0;fillColor=#ffffff;strokeColor=#000000;html=1;"),
        QSizeF(140, 70), QStringLiteral("sysml"));

    // --- ArchiMate --------------------------------------------------------
    add(QStringLiteral("archimate.actor"), QStringLiteral("Actor"),
        QStringLiteral("rounded=1;arcSize=30;fillColor=#ffffff;strokeColor=#000000;html=1;"),
        QSizeF(140, 70), QStringLiteral("archimate"));
    add(QStringLiteral("archimate.role"), QStringLiteral("Role"),
        QStringLiteral("rounded=1;arcSize=30;fillColor=#ffffff;strokeColor=#000000;html=1;"),
        QSizeF(140, 70), QStringLiteral("archimate"));
    add(QStringLiteral("archimate.process"), QStringLiteral("Process"),
        QStringLiteral("rounded=1;arcSize=30;fillColor=#ffffff;strokeColor=#000000;html=1;"),
        QSizeF(160, 70), QStringLiteral("archimate"));
    add(QStringLiteral("archimate.appcomponent"), QStringLiteral("Appcomponent"),
        QStringLiteral("rounded=1;arcSize=30;fillColor=#ffffff;strokeColor=#000000;html=1;"),
        QSizeF(160, 70), QStringLiteral("archimate"));
    add(QStringLiteral("archimate.node"), QStringLiteral("Node"),
        QStringLiteral("shape=cube;fillColor=#ffffff;strokeColor=#000000;boundedLbl=1;"
                       "backgroundOutline=1;html=1;"),
        QSizeF(160, 100), QStringLiteral("archimate"));
    add(QStringLiteral("archimate.device"), QStringLiteral("Device"),
        QStringLiteral("rounded=0;fillColor=#ffffff;strokeColor=#000000;html=1;"),
        QSizeF(150, 80), QStringLiteral("archimate"));

    // --- Enterprise integration patterns ---------------------------------
    add(QStringLiteral("eip.channel"), QStringLiteral("Channel"),
        QStringLiteral("rounded=0;fillColor=#ffffff;strokeColor=#000000;html=1;"),
        QSizeF(200, 24), QStringLiteral("eip"));
    add(QStringLiteral("eip.router"), QStringLiteral("Router"),
        QStringLiteral("shape=eip.router;fillColor=#ffffff;strokeColor=#000000;html=1;"),
        QSizeF(180, 90), QStringLiteral("eip"));
    add(QStringLiteral("eip.splitter"), QStringLiteral("Splitter"),
        QStringLiteral("shape=eip.splitter;fillColor=#ffffff;strokeColor=#000000;html=1;"),
        QSizeF(160, 90), QStringLiteral("eip"));
    add(QStringLiteral("eip.aggregator"), QStringLiteral("Aggregator"),
        QStringLiteral("shape=eip.splitter;direction=west;fillColor=#ffffff;"
                       "strokeColor=#000000;html=1;"),
        QSizeF(160, 90), QStringLiteral("eip"));

    // --- Bootstrap --------------------------------------------------------
    add(QStringLiteral("bs.button"), QStringLiteral("Button"),
        QStringLiteral("rounded=1;arcSize=25;fillColor=#ffffff;strokeColor=#000000;"
                       "fontColor=#ffffff;html=1;"),
        QSizeF(120, 42), QStringLiteral("bootstrap"));
    add(QStringLiteral("bs.panel"), QStringLiteral("Panel"),
        QStringLiteral("swimlane;startSize=34;fillColor=#ffffff;strokeColor=#000000;html=1;"),
        QSizeF(240, 160), QStringLiteral("bootstrap"));
    add(QStringLiteral("bs.navbar"), QStringLiteral("Navbar"),
        QStringLiteral("rounded=0;fillColor=#ffffff;strokeColor=#000000;fontColor=#ffffff;html=1;"),
        QSizeF(320, 48), QStringLiteral("bootstrap"));
    add(QStringLiteral("bs.badge"), QStringLiteral("Badge"),
        QStringLiteral("rounded=1;arcSize=50;fillColor=#ffffff;strokeColor=#000000;"
                       "fontColor=#ffffff;html=1;"),
        QSizeF(70, 28), QStringLiteral("bootstrap"));
    add(QStringLiteral("bs.alert"), QStringLiteral("Alert"),
        QStringLiteral("rounded=1;arcSize=8;fillColor=#ffffff;strokeColor=#000000;html=1;"),
        QSizeF(260, 56), QStringLiteral("bootstrap"));

    // --- Kubernetes -------------------------------------------------------
    add(QStringLiteral("k8s.pod"), QStringLiteral("Pod"),
        QStringLiteral("shape=hexagon;perimeter=hexagonPerimeter2;fixedSize=1;"
                       "fillColor=#ffffff;strokeColor=#000000;fontColor=#ffffff;html=1;"),
        QSizeF(110, 100), QStringLiteral("kubernetes"));
    add(QStringLiteral("k8s.service"), QStringLiteral("Service"),
        QStringLiteral("ellipse;fillColor=#ffffff;strokeColor=#000000;fontColor=#ffffff;html=1;"),
        QSizeF(100, 100), QStringLiteral("kubernetes"));
    add(QStringLiteral("k8s.deployment"), QStringLiteral("Deployment"),
        QStringLiteral("rounded=1;arcSize=10;fillColor=#ffffff;strokeColor=#000000;"
                       "fontColor=#ffffff;html=1;"),
        QSizeF(150, 90), QStringLiteral("kubernetes"));
    add(QStringLiteral("k8s.ingress"), QStringLiteral("Ingress"),
        QStringLiteral("shape=pentagon;perimeter=pentagonPerimeter;fillColor=#ffffff;"
                       "strokeColor=#000000;fontColor=#ffffff;html=1;"),
        QSizeF(130, 100), QStringLiteral("kubernetes"));
    add(QStringLiteral("k8s.configmap"), QStringLiteral("ConfigMap"),
        QStringLiteral("shape=note;backgroundOutline=1;fillColor=#ffffff;strokeColor=#000000;"
                       "html=1;"),
        QSizeF(120, 100), QStringLiteral("kubernetes"));
    add(QStringLiteral("k8s.node"), QStringLiteral("Node"),
        QStringLiteral("shape=cube;fillColor=#ffffff;strokeColor=#000000;boundedLbl=1;"
                       "backgroundOutline=1;html=1;"),
        QSizeF(130, 110), QStringLiteral("kubernetes"));

    // --- iOS ---------------------------------------------------------------
    add(QStringLiteral("ios.phone"), QStringLiteral("iPhone"),
        QStringLiteral("shape=ios.phone;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(100, 200), QStringLiteral("software"));
    add(QStringLiteral("ios.tablet"), QStringLiteral("iPad"),
        QStringLiteral("shape=ios.phone;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(160, 220), QStringLiteral("software"));
    add(QStringLiteral("ios.watch"), QStringLiteral("Apple Watch"),
        QStringLiteral("shape=ios.phone;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(90, 110), QStringLiteral("software"));

    return shapes;
}

ShapeDefinition definition(const QString &key)
{
    for (const ShapeDefinition &entry : toolboxShapes())
    {
        if (entry.key == key)
        {
            return entry;
        }
    }

    return toolboxShapes().first();
}

bool isTextShape(const Cell &cell)
{
    // draw.io's mxLabel (shape=label) is a rectangle with an image, not a
    // label-only text cell; only `text;` cells paint text without a shape.
    return shapeKey(cell) == QLatin1String("text");
}

bool isImageShape(const Cell &cell)
{
    return shapeKey(cell) == QLatin1String("image")
        || cell.styleValue(QStringLiteral("image")).startsWith(QLatin1String("data:"));
}

bool isLineArt(const Cell &cell)
{
    const QString key = shapeKey(cell);
    return key == QLatin1String("umlActor")
        || key == QLatin1String("line") || key == QLatin1String("connector")
        || key == QLatin1String("basic.arc")
        || key.startsWith(QLatin1String("mxgraph.electrical."));
}

bool isContainerShape(const Cell &cell)
{
    const QString key = shapeKey(cell);
    return cell.styleFlag(QStringLiteral("swimlane"))
        || key == QLatin1String("swimlane") || key == QLatin1String("group");
}

namespace {

/// mxGraph.getSwimlaneDirection(): the side the title bar sits on.
QString swimlaneDirection(const StyleMap &style)
{
    const QString direction = Style::value(style, QStringLiteral("direction"),
                                           QStringLiteral("east")).toLower();
    const bool flipH = Style::flag(style, QStringLiteral("flipH"));
    const bool flipV = Style::flag(style, QStringLiteral("flipV"));
    const bool horizontal = Style::flag(style, QStringLiteral("horizontal"), true);
    int n = horizontal ? 0 : 3;

    if (direction == QLatin1String("north"))
    {
        --n;
    }
    else if (direction == QLatin1String("west"))
    {
        n += 2;
    }
    else if (direction == QLatin1String("south"))
    {
        ++n;
    }

    const int mod = ((n % 2) + 2) % 2;

    if (flipH && mod == 1)
    {
        n += 2;
    }

    if (flipV && mod == 0)
    {
        n += 2;
    }

    static const QStringList directions = {QStringLiteral("north"), QStringLiteral("east"),
                                           QStringLiteral("south"), QStringLiteral("west")};
    return directions.at(((n % 4) + 4) % 4);
}

/// mxGraph.getActualStartSize(): the title bar as a rectangle offset.
QRectF actualStartSize(const Cell &cell)
{
    QRectF result;

    if (!cell.styleFlag(QStringLiteral("swimlane")))
    {
        return result;
    }

    const StyleMap style = Style::withDefaults(cell.style);
    const double startSize = Style::number(style, QStringLiteral("startSize"), 23.0);
    const QString direction = swimlaneDirection(style);

    // mxRectangle keeps the components independent, so build the offset
    // directly: QRectF::setY()/setHeight() move the opposite edge.
    if (direction == QLatin1String("north"))
    {
        result = QRectF(0, startSize, 0, 0);
    }
    else if (direction == QLatin1String("west"))
    {
        result = QRectF(startSize, 0, 0, 0);
    }
    else if (direction == QLatin1String("south"))
    {
        result = QRectF(0, 0, 0, startSize);
    }
    else
    {
        result = QRectF(0, 0, startSize, 0);
    }

    return result;
}

/// mxUtils.parsePadding(): CSS style 1-4 value shorthand (top, right, bottom,
/// left). The result is stored as n, e, s, w in a QRectF's x/y/width/height.
QRectF parsePadding(const QString &value)
{
    QString text = value;

    // Style values written by the properties panel are URI encoded.
    text.replace(QLatin1String("%20"), QLatin1String(" "));
    const QStringList parts = text.trimmed().split(QRegularExpression(QStringLiteral("\\s+")),
                                                   Qt::SkipEmptyParts);
    QVector<double> numbers;

    for (const QString &part : parts)
    {
        bool ok = false;
        const double number = part.toDouble(&ok);

        if (ok)
        {
            numbers.append(number);
        }
    }

    if (numbers.isEmpty())
    {
        return QRectF();
    }

    const double top = numbers.at(0);
    const double right = numbers.size() > 1 ? numbers.at(1) : top;
    const double bottom = numbers.size() > 2 ? numbers.at(2) : top;
    const double left = numbers.size() > 3 ? numbers.at(3) : right;

    return QRectF(left, top, right, bottom);
}

} // namespace

bool hasTransparentBounds(const Cell &cell)
{
    return cell.styleValue(QStringLiteral("transparentBounds")).trimmed()
        == QLatin1String("1");
}

QRectF transparentBounds(const Cell &cell)
{
    QRectF bounds;

    for (const Cell *child : cell.children)
    {
        QRectF rect;

        if (child->isVertex())
        {
            if (child->relativeGeometry)
            {
                continue;
            }

            if (hasTransparentBounds(*child))
            {
                const QRectF local = transparentBounds(*child);

                if (!local.isNull())
                {
                    rect = local.translated(child->geometry.topLeft());
                }
            }
            else
            {
                rect = child->geometry;
            }
        }
        else if (child->isEdge())
        {
            // Edges contribute their waypoints and free terminal points; the
            // routed path of a connected edge stays within the terminals.
            QVector<QPointF> points = child->waypoints;

            if (child->sourceId.isEmpty() && !child->sourcePoint.isNull())
            {
                points.append(child->sourcePoint);
            }

            if (child->targetId.isEmpty() && !child->targetPoint.isNull())
            {
                points.append(child->targetPoint);
            }

            for (const QPointF &point : points)
            {
                const QRectF dot(point, QSizeF(0, 0));
                rect = rect.isNull() ? dot : rect.united(dot);
            }
        }

        if (!rect.isNull())
        {
            bounds = bounds.isNull() ? rect : bounds.united(rect);
        }
    }

    if (bounds.isNull())
    {
        return QRectF();
    }

    const StyleMap style = Style::withDefaults(cell.style);
    const QRectF padding = parsePadding(Style::value(style, QStringLiteral("groupPadding")));
    const QRectF start = actualStartSize(cell);
    return QRectF(bounds.x() - padding.x() - start.x(),
                  bounds.y() - padding.y() - start.y(),
                  bounds.width() + padding.x() + padding.width() + start.x() + start.width(),
                  bounds.height() + padding.y() + padding.height() + start.y() + start.height());
}

QRectF visibleRect(const Cell &cell)
{
    if (hasTransparentBounds(cell))
    {
        const QRectF bounds = transparentBounds(cell);

        if (!bounds.isNull())
        {
            // The derived box lives in the same coordinate space as the
            // children, i.e. relative to the cell's own geometry origin.
            return bounds.translated(cell.absolutePosition());
        }
    }

    return cell.absoluteRect();
}

QPainterPath pathFor(const Cell &cell)
{
    return pathForStyle(cell.style, QRectF(QPointF(0, 0), cell.geometry.size()));
}

void appendSvgArc(QPainterPath &path, const QPointF &from, const QPointF &to,
                  double rx, double ry, bool largeArc, bool sweep)
{
    addSvgArc(path, from, to, rx, ry, largeArc, sweep);
}

QString canonicalKey(const StyleMap &style)
{
    Cell probe(Cell::Kind::Vertex, QStringLiteral("probe"));
    probe.style = style;
    return shapeKey(probe);
}

QPainterPath pathForStyle(const StyleMap &style, const QRectF &rect)
{
    QPainterPath path;
    Cell probe(Cell::Kind::Vertex, QStringLiteral("probe"));
    probe.style = style;

    const QString key = shapeKey(probe);
    const double w = rect.width();
    const double h = rect.height();
    const double x = rect.x();
    const double y = rect.y();

    auto point = [&](double px, double py)
    {
        return QPointF(x + px, y + py);
    };

    if (key.startsWith(QLatin1String("basic.")))
    {
        BasicGeometry geometry;
        buildBasicShape(key, probe.style, rect, geometry);

        // A name draw.io does not register has no geometry: fall through to
        // the rectangle fallback like mxGraph does for unknown shapes.
        if (!geometry.path.isEmpty())
        {
            return geometry.path;
        }
    }

    if (key == QLatin1String("table") || key == QLatin1String("tableRow"))
    {
        // TableShape extends mxSwimlane: its geometry is the cell rectangle.
        // The grid lines are painted by the renderer from the child geometry
        // (see paintTableLines) so the path only needs the outer frame.
        path.addRect(rect);
        return path;
    }

    if (key == QLatin1String("isoRectangle"))
    {
        // IsoRectangleShape.redrawPath: an isometric diamond. tan30Dx is
        // (0.5 - tan(30deg))/2 and m is the side that fits the cell.
        const double tan30 = std::tan(qDegreesToRadians(30.0));
        const double tan30Dx = (0.5 - tan30) / 2.0;
        const double m = qMin(w, h / tan30);
        const double ox = (w - m) / 2.0;
        const double oy = (h - m) / 2.0 + m / 4.0;

        path.moveTo(point(ox, oy + 0.25 * m));
        path.lineTo(point(ox + 0.5 * m, oy + m * tan30Dx));
        path.lineTo(point(ox + m, oy + 0.25 * m));
        path.lineTo(point(ox + 0.5 * m, oy + (0.5 - tan30Dx) * m));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("isoCube2"))
    {
        // IsoCubeShape2.paintVertexShape: isoAngle is halved (15 -> 7.5deg).
        const double isoAngle = qBound(0.01, Style::number(probe.style,
            QStringLiteral("isoAngle"), 15.0), 94.0) * M_PI / 200.0;
        const double isoH = qMin(w * std::tan(isoAngle), h * 0.5);

        path.moveTo(point(w * 0.5, 0));
        path.lineTo(point(w, isoH));
        path.lineTo(point(w, h - isoH));
        path.lineTo(point(w * 0.5, h));
        path.lineTo(point(0, h - isoH));
        path.lineTo(point(0, isoH));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("curlyBracket"))
    {
        // CurlyBracketShape.redrawPath: an open path, never filled.
        const double s = w * qBound(0.0, Style::number(probe.style,
            QStringLiteral("size"), 0.5), 1.0);
        const double arcSize = qMax(0.0,
            Style::number(probe.style, QStringLiteral("arcSize"), 20.0)) / 2.0;
        const bool rounded = probe.styleFlag(QStringLiteral("rounded"));

        addPoints(path,
                  {point(w, 0), point(s, 0), point(s, h / 2), point(0, h / 2),
                   point(s, h / 2), point(s, h), point(w, h)},
                  rounded, arcSize, false);
        return path;
    }

    if (key == QLatin1String("crossbar"))
    {
        // CrossbarShape.redrawPath: two vertical end ticks and a middle bar.
        path.moveTo(point(0, 0));
        path.lineTo(point(0, h));
        path.moveTo(point(w, 0));
        path.lineTo(point(w, h));
        path.moveTo(point(0, h / 2));
        path.lineTo(point(w, h / 2));
        return path;
    }

    if (key == QLatin1String("zigzag"))
    {
        // ZigzagShape.paintVertexShape: peaks inset by the stroke width.
        const double size = qMax(5.0, Style::number(probe.style,
            QStringLiteral("size"), 10.0));
        const double strokeWidth = qMax(0.5, Style::number(probe.style,
            QStringLiteral("strokeWidth"), 1.0));
        const bool rounded = probe.styleFlag(QStringLiteral("rounded"));
        const double inset = rounded ? strokeWidth / 2.0 : strokeWidth;
        const double centerY = h / 2.0;
        const double topY = inset;
        const double bottomY = h - inset;
        const int numFull = qBound(1, qRound(w / size) - 1, 10000);
        const double halfWave = w / (numFull + 1.0);
        const double halfEnd = halfWave / 2.0;

        path.moveTo(point(0, centerY));

        if (rounded)
        {
            const double k = 0.4;
            path.quadTo(point(k * halfEnd, centerY - (centerY - topY) * k),
                        point((1 - k) * halfEnd, topY));

            for (int j = 0; j < numFull; ++j)
            {
                const double sx = halfEnd + j * halfWave;
                const double ex = sx + halfWave;
                const double sy = (j % 2 == 0) ? topY : bottomY;
                const double ey = (j % 2 == 0) ? bottomY : topY;
                path.quadTo(point(sx + k * halfWave, sy),
                            point(ex - k * halfWave, ey));
                path.lineTo(point(ex, ey));
            }

            const double lastX = halfEnd + numFull * halfWave;
            const double lastY = (numFull % 2 == 0) ? topY : bottomY;
            const double dirSign = (lastY == topY) ? 1.0 : -1.0;
            path.quadTo(point(lastX + k * halfEnd, lastY),
                        point(w - k * halfEnd,
                              centerY - dirSign * (centerY - topY) * k));
        }
        else
        {
            path.lineTo(point(halfEnd, topY));

            for (int j = 0; j < numFull; ++j)
            {
                const double ex = halfEnd + (j + 1) * halfWave;
                const double ey = (j % 2 == 0) ? bottomY : topY;
                path.lineTo(point(ex, ey));
            }
        }
        return path;
    }

    if (key == QLatin1String("curvedText"))
    {
        // CurvedTextShape is an invisible rectangle; the text is painted on
        // the curve by ShapeRenderer.
        path.addRect(rect);
        return path;
    }

    if (key == QLatin1String("ellipse") || key == QLatin1String("circle")
        || key == QLatin1String("doubleEllipse"))
    {
        // mxDoubleEllipse paints the outer ellipse as background and the
        // inner one as a stroke-only foreground (see ShapeRenderer).
        path.addEllipse(rect);
        return path;
    }

    if (key == QLatin1String("rhombus"))
    {
        path.moveTo(point(w / 2, 0));
        path.lineTo(point(w, h / 2));
        path.lineTo(point(w / 2, h));
        path.lineTo(point(0, h / 2));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("triangle"))
    {
        // draw.io's mxTriangle points right: (0,0) -> (w,h/2) -> (0,h).
        path.moveTo(point(0, 0));
        path.lineTo(point(w, h * 0.5));
        path.lineTo(point(0, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("orthogonalTriangle"))
    {
        path.moveTo(point(0, 0));
        path.lineTo(point(w, h));
        path.lineTo(point(0, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("acuteTriangle") || key == QLatin1String("obtuseTriangle"))
    {
        // Up-pointing triangle stencils from draw.io's basic library.
        const double fallback = key == QLatin1String("obtuseTriangle") ? 0.25 : 0.5;
        const double dx = w * qBound(0.0, Style::number(probe.style, QStringLiteral("dx"), fallback), 1.0);

        path.moveTo(point(0, h));
        path.lineTo(point(dx, 0));
        path.lineTo(point(w, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("hexagon"))
    {
        // HexagonShape.redrawPath: size is a percentage unless fixedSize=1.
        const bool fixed = probe.styleFlag(QStringLiteral("fixedSize"));
        const double size = Style::number(probe.style, QStringLiteral("size"),
                                          fixed ? 20.0 : 0.25);
        const double s = fixed ? qBound(0.0, size, w * 0.5)
                               : w * qBound(0.0, size, 1.0);

        addPoints(path, {point(s, 0), point(w - s, 0), point(w, h * 0.5),
                         point(w - s, h), point(s, h), point(0, h * 0.5)},
                  probe.styleFlag(QStringLiteral("rounded")), pointArcSizeFor(probe.style),
                  true);
        return path;
    }

    if (key == QLatin1String("parallelogram"))
    {
        // ParallelogramShape.redrawPath
        const bool fixed = probe.styleFlag(QStringLiteral("fixedSize"));
        const double size = Style::number(probe.style, QStringLiteral("size"),
                                          fixed ? 20.0 : 0.2);
        const double dx = fixed ? qBound(0.0, size, w)
                                : w * qBound(0.0, size, 1.0);

        addPoints(path, {point(0, h), point(dx, 0), point(w, 0), point(w - dx, h)},
                  probe.styleFlag(QStringLiteral("rounded")), pointArcSizeFor(probe.style),
                  true);
        return path;
    }

    if (key == QLatin1String("trapezoid"))
    {
        // TrapezoidShape.redrawPath: the offset is capped at half the width.
        const bool fixed = probe.styleFlag(QStringLiteral("fixedSize"));
        const double size = Style::number(probe.style, QStringLiteral("size"),
                                          fixed ? 20.0 : 0.2);
        const double dx = fixed ? qBound(0.0, size, w * 0.5)
                                : w * qBound(0.0, size, 0.5);

        addPoints(path, {point(0, h), point(dx, 0), point(w - dx, 0), point(w, h)},
                  probe.styleFlag(QStringLiteral("rounded")), pointArcSizeFor(probe.style),
                  true);
        return path;
    }

    if (key == QLatin1String("step"))
    {
        // draw.io StepShape: pointed right edge, notched left edge.
        const bool fixed = Style::flag(probe.style, QStringLiteral("fixedSize"));
        const double size = Style::number(probe.style, QStringLiteral("size"), fixed ? 20.0 : 0.2);
        const double s = fixed ? qBound(0.0, size, w) : w * qBound(0.0, size, 1.0);

        path.moveTo(point(0, 0));
        path.lineTo(point(w - s, 0));
        path.lineTo(point(w, h / 2));
        path.lineTo(point(w - s, h));
        path.lineTo(point(0, h));
        path.lineTo(point(s, h / 2));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("cylinder3") || key == QLatin1String("cylinder")
        || key == QLatin1String("database"))
    {
        // draw.io's CylinderShape3: size is a pixel cap height (default 15).
        const double size = qBound(0.0, Style::number(probe.style, QStringLiteral("size"), 15.0),
                                   h * 0.5);

        if (size <= 0.0)
        {
            path.addRect(rect);
            return path;
        }

        const QRectF topEllipse(0, 0, w, 2 * size);
        const QRectF bottomEllipse(0, h - 2 * size, w, 2 * size);
        const bool lid = Style::flag(probe.style, QStringLiteral("lid"), true);

        if (lid)
        {
            path.moveTo(point(0, size));
            path.arcTo(topEllipse, 180, -180);
        }
        else
        {
            // Without a lid the top edge dips inward, like a stacked tube.
            path.moveTo(point(0, 0));
            path.arcTo(QRectF(0, -size, w, 2 * size), 180, 180);
        }

        path.lineTo(point(w, h - size));
        path.arcTo(bottomEllipse, 0, -180);
        path.closeSubpath();

        return path;
    }

    if (key == QLatin1String("cloud"))
    {
        // mxCloud.redrawPath: the six bumps of draw.io's cloud outline.
        path.moveTo(point(0.25 * w, 0.25 * h));
        path.cubicTo(point(0.05 * w, 0.25 * h), point(0.0, 0.5 * h),
                     point(0.16 * w, 0.55 * h));
        path.cubicTo(point(0.0, 0.66 * h), point(0.18 * w, 0.9 * h),
                     point(0.31 * w, 0.8 * h));
        path.cubicTo(point(0.4 * w, h), point(0.7 * w, h), point(0.8 * w, 0.8 * h));
        path.cubicTo(point(w, 0.8 * h), point(w, 0.6 * h), point(0.875 * w, 0.5 * h));
        path.cubicTo(point(w, 0.3 * h), point(0.8 * w, 0.1 * h), point(0.625 * w, 0.2 * h));
        path.cubicTo(point(0.5 * w, 0.05 * h), point(0.3 * w, 0.05 * h),
                     point(0.25 * w, 0.25 * h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("cloudCallout"))
    {
        // mxgraph.basic.cloud_callout / cloud (90.41 x 61.4) bump outline.
        const auto px = [&](double v) { return x + w * (v / 90.41); };
        const auto py = [&](double v) { return y + h * (v / 61.4); };

        QPointF current(px(12.1), py(31.8));
        path.moveTo(current);

        const double arcs[][4] = {
            {8, 8, 12.1, 16.8}, {12, 12, 33.1, 8.8}, {14, 14, 59.1, 8.8},
            {12, 12, 79.1, 16.8}, {8, 8, 79.1, 31.8}, {12, 12, 58.1, 38.8},
            {14, 14, 34.1, 38.8}, {10, 8, 12.1, 31.8}
        };

        for (const auto &arc : arcs)
        {
            const QPointF next(px(arc[2]), py(arc[3]));
            addSvgArc(path, current, next, w * arc[0] / 90.41, h * arc[1] / 61.4,
                      true, true);
            current = next;
        }

        path.closeSubpath();

        // Three bubbles in the foreground path.
        path.addEllipse(QPointF(px(15.1), py(48.8)), w * 6.0 / 90.41, h * 2.7 / 61.4);
        path.addEllipse(QPointF(px(8.1), py(55.3)), w * 3.8 / 90.41, h * 1.8 / 61.4);
        path.addEllipse(QPointF(px(2.4), py(60.1)), w * 2.4 / 90.41, h * 1.3 / 61.4);
        return path;
    }

    if (key == QLatin1String("document"))
    {
        // DocumentShape.redrawPath: the bottom edge is a wave made of two
        // quadratic curves whose height comes from size (0.3 by default).
        const double dy = h * qBound(0.0,
            Style::number(probe.style, QStringLiteral("size"), 0.3), 1.0);
        const double fy = 1.4;

        path.moveTo(point(0, 0));
        path.lineTo(point(w, 0));
        path.lineTo(point(w, h - dy / 2.0));
        path.quadTo(point(w * 3.0 / 4.0, h - dy * fy), point(w / 2.0, h - dy / 2.0));
        path.quadTo(point(w / 4.0, h - dy * (1 - fy)), point(0, h - dy / 2.0));
        path.lineTo(point(0, dy / 2.0));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("tape"))
    {
        // draw.io TapeShape: wavy top and bottom edges made of quadratic arcs.
        const double dy = h * qBound(0.0, Style::number(probe.style, QStringLiteral("size"), 0.4), 1.0);
        const double fy = 1.4;

        path.moveTo(point(0, dy / 2));
        path.quadTo(point(w / 4, dy * fy), point(w / 2, dy / 2));
        path.quadTo(point(w * 3 / 4, dy * (1 - fy)), point(w, dy / 2));
        path.lineTo(point(w, h - dy / 2));
        path.quadTo(point(w * 3 / 4, h - dy * fy), point(w / 2, h - dy / 2));
        path.quadTo(point(w / 4, h - dy * (1 - fy)), point(0, h - dy / 2));
        path.lineTo(point(0, dy / 2));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("note"))
    {
        // NoteShape: the folded corner is `size` pixels (default 30).
        const double size = qBound(0.0,
            Style::number(probe.style, QStringLiteral("size"), 30.0), qMin(w, h));

        path.moveTo(point(0, 0));
        path.lineTo(point(w - size, 0));
        path.lineTo(point(w, size));
        path.lineTo(point(w, h));
        path.lineTo(point(0, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("callout"))
    {
        // draw.io CalloutShape: rectangle with a triangular tail from the base.
        const double s = qBound(0.0, Style::number(probe.style, QStringLiteral("size"), 30.0), h);
        const double position = qBound(0.0, Style::number(probe.style, QStringLiteral("position"), 0.5), 1.0);
        const double position2 = qBound(0.0, Style::number(probe.style, QStringLiteral("position2"), 0.5), 1.0);
        const double base = qBound(0.0, Style::number(probe.style, QStringLiteral("base"), 20.0), w);
        const double dx = w * position;
        const double dx2 = w * position2;

        path.moveTo(point(0, 0));
        path.lineTo(point(w, 0));
        path.lineTo(point(w, h - s));
        path.lineTo(point(qMin(w, dx + base), h - s));
        path.lineTo(point(dx2, h));
        path.lineTo(point(qMax(0.0, dx), h - s));
        path.lineTo(point(0, h - s));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("wedgeCallout"))
    {
        // draw.io WedgeCalloutShape with the default tip below the bubble.
        const double base = qMax(0.0, Style::number(probe.style, QStringLiteral("base"), 20.0));
        const double hb = qMax(0.0, qMin(base, w)) / 2;
        const double tipX = 0.25 * w;
        const double tipY = 1.5 * h;
        // The tail base is where the line from the tip crosses the bottom edge.
        double ex = w / 2 + (tipX - w / 2) * (h / 2) / qMax(1e-6, tipY - h / 2);
        ex = qBound(hb, w - hb, ex);

        path.moveTo(point(0, 0));
        path.lineTo(point(w, 0));
        path.lineTo(point(w, h));
        path.lineTo(point(ex + hb, h));
        path.lineTo(point(tipX, tipY));
        path.lineTo(point(ex - hb, h));
        path.lineTo(point(0, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("actor"))
    {
        // mxActor.redrawPath (draw.io's default "actor"): a filled person
        // silhouette, not the UML stick figure.
        const double width = w / 3.0;

        path.moveTo(point(0, h));
        path.cubicTo(point(0, 3.0 * h / 5.0), point(0, 2.0 * h / 5.0), point(w / 2.0, 2.0 * h / 5.0));
        path.cubicTo(point(w / 2.0 - width, 2.0 * h / 5.0), point(w / 2.0 - width, 0),
                     point(w / 2.0, 0));
        path.cubicTo(point(w / 2.0 + width, 0), point(w / 2.0 + width, 2.0 * h / 5.0),
                     point(w / 2.0, 2.0 * h / 5.0));
        path.cubicTo(point(w, 2.0 * h / 5.0), point(w, 3.0 * h / 5.0), point(w, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("umlActor"))
    {
        // UmlActorShape.paintBackground: head ellipse in the top quarter, the
        // body, arms at one third and legs from two thirds to the bottom.
        path.addEllipse(QRectF(x + w / 4.0, y, w / 2.0, h / 4.0));
        path.moveTo(point(w / 2.0, h / 4.0));
        path.lineTo(point(w / 2.0, 2.0 * h / 3.0));
        path.moveTo(point(w / 2.0, h / 3.0));
        path.lineTo(point(0, h / 3.0));
        path.moveTo(point(w / 2.0, h / 3.0));
        path.lineTo(point(w, h / 3.0));
        path.moveTo(point(w / 2.0, 2.0 * h / 3.0));
        path.lineTo(point(0, h));
        path.moveTo(point(w / 2.0, 2.0 * h / 3.0));
        path.lineTo(point(w, h));
        return path;
    }

    if (key == QLatin1String("process"))
    {
        // ProcessShape extends mxRectangleShape, so the body is a plain or
        // rounded rectangle; the two inset lines are decorations that
        // ShapeRenderer.cpp strokes on top.
        if (probe.styleFlag(QStringLiteral("rounded")))
        {
            const double r = cornerRadiusFor(probe.style, w, h);
            path.addRoundedRect(rect, r, r);
        }
        else
        {
            path.addRect(rect);
        }

        return path;
    }

    if (key == QLatin1String("swimlane") || key == QLatin1String("group"))
    {
        // mxSwimlane.paintSwimlane() paints the title bar and the content
        // area as two separate paths. The renderer draws the title bar
        // (which takes the cell fill) and the divider itself, so the shape
        // path here is only the content area.
        const bool horizontal = probe.styleFlag(QStringLiteral("horizontal"), true);
        const double startSize = qMax(0.0, Style::number(
            Style::withDefaults(probe.style), QStringLiteral("startSize"), 23.0));
        const double start = horizontal ? qMin(startSize, h) : qMin(startSize, w);

        if (horizontal)
        {
            path.moveTo(point(0, start));
            path.lineTo(point(0, h));
            path.lineTo(point(w, h));
            path.lineTo(point(w, start));
        }
        else
        {
            path.moveTo(point(start, 0));
            path.lineTo(point(w, 0));
            path.lineTo(point(w, h));
            path.lineTo(point(start, h));
        }

        return path;
    }

    if (key == QLatin1String("line") || key == QLatin1String("connector"))
    {
        path.moveTo(point(0, h / 2));
        path.lineTo(point(w, h / 2));
        return path;
    }

    if (key == QLatin1String("cube"))
    {
        // draw.io CubeShape: top-right and bottom-left corners are cut by size.
        const double depth = qBound(0.0, Style::number(probe.style, QStringLiteral("size"), 20.0),
                                    qMin(w, h));

        path.moveTo(point(0, 0));
        path.lineTo(point(w - depth, 0));
        path.lineTo(point(w, depth));
        path.lineTo(point(w, h));
        path.lineTo(point(depth, h));
        path.lineTo(point(0, h - depth));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("or") || key == QLatin1String("xor"))
    {
        // draw.io OrShape / XorShape: flat left edge, curved right edge.
        path.moveTo(point(0, 0));
        path.quadTo(point(w, 0), point(w, h / 2));
        path.quadTo(point(w, h), point(0, h));

        if (key == QLatin1String("xor"))
        {
            path.quadTo(point(w / 2, h / 2), point(0, 0));
        }

        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("dataStorage"))
    {
        // draw.io DataStorageShape: curved left and right edges.
        const bool fixed = Style::flag(probe.style, QStringLiteral("fixedSize"));
        const double size = Style::number(probe.style, QStringLiteral("size"), fixed ? 20.0 : 0.1);
        const double s = fixed ? qBound(0.0, size, w) : w * qBound(0.0, size, 1.0);

        path.moveTo(point(s, 0));
        path.lineTo(point(w, 0));
        path.quadTo(point(w - 2 * s, h / 2), point(w, h));
        path.lineTo(point(s, h));
        path.quadTo(point(s - 2 * s, h / 2), point(s, 0));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("internalStorage"))
    {
        // InternalStorageShape extends mxRectangleShape; the two inset lines
        // are decorations in ShapeRenderer.cpp so the fill stays solid.
        if (probe.styleFlag(QStringLiteral("rounded")))
        {
            const double r = cornerRadiusFor(probe.style, w, h);
            path.addRoundedRect(rect, r, r);
        }
        else
        {
            path.addRect(rect);
        }

        return path;
    }

    // --- additional basic polygons --------------------------------------
    if (key == QLatin1String("pentagon"))
    {
        path.moveTo(point(w * 0.5, 0));
        path.lineTo(point(w, h * 0.38));
        path.lineTo(point(w * 0.82, h));
        path.lineTo(point(w * 0.18, h));
        path.lineTo(point(0, h * 0.38));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("octagon"))
    {
        const double cut = qMin(w, h) * 0.29;
        path.moveTo(point(cut, 0));
        path.lineTo(point(w - cut, 0));
        path.lineTo(point(w, cut));
        path.lineTo(point(w, h - cut));
        path.lineTo(point(w - cut, h));
        path.lineTo(point(cut, h));
        path.lineTo(point(0, h - cut));
        path.lineTo(point(0, cut));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("partialRectangle"))
    {
        const bool top = Style::number(style, QStringLiteral("top"), 1.0) != 0.0;
        const bool bottom = Style::number(style, QStringLiteral("bottom"), 1.0) != 0.0;
        const bool left = Style::number(style, QStringLiteral("left"), 1.0) != 0.0;
        const bool right = Style::number(style, QStringLiteral("right"), 1.0) != 0.0;

        if (top) { path.moveTo(point(0, 0)); path.lineTo(point(w, 0)); }
        if (right) { path.moveTo(point(w, 0)); path.lineTo(point(w, h)); }
        if (bottom) { path.moveTo(point(w, h)); path.lineTo(point(0, h)); }
        if (left) { path.moveTo(point(0, h)); path.lineTo(point(0, 0)); }
        return path;
    }

    if (key == QLatin1String("polygon"))
    {
        const QPointF center(w / 2.0, h / 2.0);

        for (int i = 0; i < 8; ++i)
        {
            const double angle = -M_PI / 2.0 + i * M_PI / 4.0;
            const QPointF p(center.x() + std::cos(angle) * w / 2.0,
                            center.y() + std::sin(angle) * h / 2.0);
            i == 0 ? path.moveTo(p) : path.lineTo(p);
        }

        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("star6") || key == QLatin1String("star8"))
    {
        // mxgraph.basic.6_point_star / 8_point_star, in stencil units.
        static const double star6[][2] = {
            {23, 28.9}, {23, 0}, {48, 14.4}, {73, 0}, {73, 28.9}, {96, 42.2},
            {73, 55.6}, {73, 84.5}, {48, 70}, {23, 84.5}, {23, 55.6}, {0, 42.2}
        };
        static const double star8[][2] = {
            {28, 28}, {28, 0}, {48, 20}, {68, 0}, {68, 28}, {96, 28}, {76, 48},
            {96, 68}, {68, 68}, {68, 96}, {48, 76}, {28, 96}, {28, 68}, {0, 68},
            {20, 48}, {0, 28}
        };

        const bool six = key == QLatin1String("star6");
        const double baseW = 96.0;
        const double baseH = six ? 84.5 : 96.0;
        const int count = six ? 12 : 16;

        for (int i = 0; i < count; ++i)
        {
            const double px = six ? star6[i][0] : star8[i][0];
            const double py = six ? star6[i][1] : star8[i][1];
            const QPointF p = point(w * px / baseW, h * py / baseH);
            i == 0 ? path.moveTo(p) : path.lineTo(p);
        }

        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("star4"))
    {
        // mxgraph.basic.4_point_star_2: slim four point sparkle.
        const QPointF center(w / 2.0, h / 2.0);
        const double outer = qMin(w, h) / 2.0;
        const double inner = outer * 0.28;

        for (int i = 0; i < 8; ++i)
        {
            const double radius = i % 2 == 0 ? outer : inner;
            const double angle = -M_PI / 2.0 + i * M_PI / 4.0;
            const QPointF p(center.x() + std::cos(angle) * radius * (w / qMin(w, h)),
                            center.y() + std::sin(angle) * radius);
            i == 0 ? path.moveTo(p) : path.lineTo(p);
        }

        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("banner"))
    {
        // mxgraph.basic.banner (96 x 50). The panel sits on top of two
        // swallow-tailed ribbons; the notch at the bottom centre separates the
        // two fold tabs. Fold lines are stroked separately.
        const auto px = [&](double v) { return x + w * (v / 96.0); };
        const auto py = [&](double v) { return y + h * (v / 50.0); };

        path.moveTo(px(0), py(50));
        path.lineTo(px(38), py(50));
        addSvgArc(path, QPointF(px(38), py(50)), QPointF(px(40.5), py(47.5)),
                  w * 2.5 / 96.0, h * 2.5 / 50.0, false, false);
        path.lineTo(px(40.5), py(40));
        path.lineTo(px(55.5), py(40));
        path.lineTo(px(55.5), py(47.5));
        addSvgArc(path, QPointF(px(55.5), py(47.5)), QPointF(px(58), py(50)),
                  w * 2.5 / 96.0, h * 2.5 / 50.0, false, false);
        path.lineTo(px(96), py(50));
        path.lineTo(px(83), py(30));
        path.lineTo(px(96), py(10));
        path.lineTo(px(70.5), py(10));
        path.lineTo(px(70.5), py(2.5));
        addSvgArc(path, QPointF(px(70.5), py(2.5)), QPointF(px(68), py(0)),
                  w * 2.5 / 96.0, h * 2.5 / 50.0, false, false);
        path.lineTo(px(28), py(0));
        addSvgArc(path, QPointF(px(28), py(0)), QPointF(px(25.5), py(2.5)),
                  w * 2.5 / 96.0, h * 2.5 / 50.0, false, false);
        path.lineTo(px(25.5), py(10));
        path.lineTo(px(0), py(10));
        path.lineTo(px(13), py(30));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("patternFillRect"))
    {
        path.addRect(rect);
        const QString fillStyle = Style::value(style, QStringLiteral("fillStyle"),
                                               QStringLiteral("diag"));
        const double step = qMax(5.0, Style::number(style, QStringLiteral("step"), 8.0));
        const double left = x, top = y, right = x + w, bottom = y + h;

        const auto clipLine = [&](QPointF a, QPointF b)
        {
            double t0 = 0.0, t1 = 1.0;
            const double dx = b.x() - a.x();
            const double dy = b.y() - a.y();
            const auto clip = [&](double p, double q)
            {
                if (qFuzzyIsNull(p)) return q >= 0.0;
                const double r = q / p;
                if (p < 0.0) { if (r > t1) return false; if (r > t0) t0 = r; }
                else { if (r < t0) return false; if (r < t1) t1 = r; }
                return true;
            };

            if (!clip(-dx, a.x() - left) || !clip(dx, right - a.x())
                || !clip(-dy, a.y() - top) || !clip(dy, bottom - a.y()))
            {
                return;
            }

            const QPointF p1(a.x() + t0 * dx, a.y() + t0 * dy);
            const QPointF p2(a.x() + t1 * dx, a.y() + t1 * dy);
            path.moveTo(p1);
            path.lineTo(p2);
        };

        const bool vertical = fillStyle == QLatin1String("vert")
            || fillStyle == QLatin1String("grid");
        const bool horizontal = fillStyle == QLatin1String("hor")
            || fillStyle == QLatin1String("grid");
        const bool diagonal = fillStyle == QLatin1String("diag")
            || fillStyle == QLatin1String("diagGrid");
        const bool reverse = fillStyle == QLatin1String("diagRev")
            || fillStyle == QLatin1String("diagGrid");

        if (vertical)
            for (double px = left; px <= right; px += step) clipLine(QPointF(px, top), QPointF(px, bottom));
        if (horizontal)
            for (double py = top; py <= bottom; py += step) clipLine(QPointF(left, py), QPointF(right, py));
        if (diagonal)
            for (double px = left - h; px <= right; px += step) clipLine(QPointF(px, top), QPointF(px + h, bottom));
        if (reverse)
            for (double px = left; px <= right + h; px += step) clipLine(QPointF(px, top), QPointF(px - h, bottom));

        return path;
    }

    if (key == QLatin1String("miscTable"))
    {
        path.addRect(QRectF(x, y, w, h));
        path.moveTo(point(0, h / 4.0));
        path.lineTo(point(w, h / 4.0));
        path.moveTo(point(0, h / 2.0));
        path.lineTo(point(w, h / 2.0));
        path.moveTo(point(0, h * 3.0 / 4.0));
        path.lineTo(point(w, h * 3.0 / 4.0));
        path.moveTo(point(w / 3.0, 0));
        path.lineTo(point(w / 3.0, h));
        path.moveTo(point(w * 2.0 / 3.0, 0));
        path.lineTo(point(w * 2.0 / 3.0, h));
        return path;
    }

    if (key == QLatin1String("miscList"))
    {
        path.addRect(QRectF(x, y, w, h));

        for (int i = 1; i < 4; ++i)
        {
            path.moveTo(point(0, h * i / 4.0));
            path.lineTo(point(w, h * i / 4.0));
        }

        for (int i = 0; i < 4; ++i)
        {
            path.addEllipse(QPointF(x + w * 0.1, y + h * (i + 0.5) / 4.0),
                            w * 0.025, w * 0.025);
        }

        return path;
    }

    // --- draw.io arrows2 shapes (shapes/mxArrows.js) ----------------------
    // Each block is a literal port of the matching
    // mxShapeArrows2*.paintVertexShape: the same clamping, the same order of
    // moveTo/lineTo/arcTo calls, and the shape constructor's values
    // (this.dy = 0.5, this.dx = 0.5, ...) as fallback for missing style keys.
    const auto param = [&](const char *name, double fallback, double maxValue)
    {
        return qBound(0.0,
            Style::number(probe.style, QString::fromLatin1(name), fallback), maxValue);
    };
    const auto ratio = [&](const char *name, double fallback)
    {
        return qBound(0.0,
            Style::number(probe.style, QString::fromLatin1(name), fallback), 1.0);
    };
    // c.arcTo(rx, ry, 0, largeArc, sweep, px, py), starting at the current point.
    const auto arcTo = [&](double rx, double ry, bool largeArc, bool sweep,
                           double px, double py)
    {
        if (rx <= 0.0 || ry <= 0.0)
        {
            // SVG treats an arc with a zero radius as a straight line.
            path.lineTo(point(px, py));
            return;
        }

        appendSvgArc(path, path.currentPosition(), point(px, py), rx, ry, largeArc, sweep);
    };
    const bool roundedArrow = Style::flag(probe.style, QStringLiteral("rounded"))
        || Style::value(probe.style, QStringLiteral("rounded")) == QLatin1String("1");

    if (key == QLatin1String("arrows2.arrow"))
    {
        const double dy = h * 0.5 * ratio("dy", 0.5);
        const double dx = param("dx", 0.5, w);
        const double notch = param("notch", 0.0, w);

        path.moveTo(point(0, dy));
        path.lineTo(point(w - dx, dy));
        path.lineTo(point(w - dx, 0));
        path.lineTo(point(w, h * 0.5));
        path.lineTo(point(w - dx, h));
        path.lineTo(point(w - dx, h - dy));
        path.lineTo(point(0, h - dy));
        path.lineTo(point(notch, h * 0.5));
        path.closeSubpath();

        // Head and tail crosslines are stroked on their own in draw.io; as
        // degenerate subpaths they stay invisible when the shape is filled.
        if (Style::flag(probe.style, QStringLiteral("headCrossline")))
        {
            path.moveTo(point(w - dx, dy));
            path.lineTo(point(w - dx, h - dy));
        }

        if (Style::flag(probe.style, QStringLiteral("tailCrossline")))
        {
            path.moveTo(point(notch, dy));
            path.lineTo(point(notch, h - dy));
        }

        return path;
    }

    if (key == QLatin1String("arrows2.twoWayArrow"))
    {
        const double dy = h * 0.5 * ratio("dy", 0.5);
        const double dx = param("dx", 0.5, w);

        path.moveTo(point(dx, dy));
        path.lineTo(point(w - dx, dy));
        path.lineTo(point(w - dx, 0));
        path.lineTo(point(w, h * 0.5));
        path.lineTo(point(w - dx, h));
        path.lineTo(point(w - dx, h - dy));
        path.lineTo(point(dx, h - dy));
        path.lineTo(point(dx, h));
        path.lineTo(point(0, h * 0.5));
        path.lineTo(point(dx, 0));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("arrows2.stylisedArrow"))
    {
        const double dy = h * 0.5 * ratio("dy", 0.5);
        const double dx = param("dx", 0.5, w);
        const double notch = param("notch", 0.0, w);
        const double feather = h * 0.5 * ratio("feather", 0.5);

        path.moveTo(point(0, feather));
        path.lineTo(point(w - dx, dy));
        path.lineTo(point(w - dx - 10, 0));
        path.lineTo(point(w, h * 0.5));
        path.lineTo(point(w - dx - 10, h));
        path.lineTo(point(w - dx, h - dy));
        path.lineTo(point(0, h - feather));
        path.lineTo(point(notch, h * 0.5));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("arrows2.sharpArrow"))
    {
        const double dy1 = h * 0.5 * ratio("dy1", 0.5);
        const double dx1 = param("dx1", 0.5, w);
        const double dx2 = param("dx2", 0.5, w);
        const double notch = param("notch", 0.0, w);
        const double dx1a = param("dx1", 0.5, w);
        const double dy1a = h * 0.5 * qBound(0.0,
            Style::number(probe.style, QStringLiteral("dy1"), 0.5), h);
        const double x2 = (h != 0.0) ? dx1a + dx2 * dy1a * 2.0 / h : 0.0;

        path.moveTo(point(0, dy1));
        path.lineTo(point(w - dx1, dy1));
        path.lineTo(point(w - x2, 0));
        path.lineTo(point(w - dx2, 0));
        path.lineTo(point(w, h * 0.5));
        path.lineTo(point(w - dx2, h));
        path.lineTo(point(w - x2, h));
        path.lineTo(point(w - dx1, h - dy1));
        path.lineTo(point(0, h - dy1));
        path.lineTo(point(notch, h * 0.5));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("arrows2.sharpArrow2"))
    {
        const double dy1 = h * 0.5 * ratio("dy1", 0.5);
        const double dx1 = param("dx1", 0.5, w);
        const double dx2 = param("dx2", 0.5, w);
        const double dy3 = h * 0.5 * ratio("dy3", 0.5);
        const double dx3 = param("dx3", 0.5, w);
        const double notch = param("notch", 0.0, w);

        path.moveTo(point(0, dy1));
        path.lineTo(point(w - dx1, dy1));
        path.lineTo(point(w - dx3, dy3));
        path.lineTo(point(w - dx2, 0));
        path.lineTo(point(w, h * 0.5));
        path.lineTo(point(w - dx2, h));
        path.lineTo(point(w - dx3, h - dy3));
        path.lineTo(point(w - dx1, h - dy1));
        path.lineTo(point(0, h - dy1));
        path.lineTo(point(notch, h * 0.5));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("arrows2.calloutArrow"))
    {
        const double dy = param("dy", 0.5, h);
        const double dx = param("dx", 0.5, w);
        const double notch = param("notch", 0.0, w);
        const double arrowHead = param("arrowHead", 0.0, h);

        path.moveTo(point(0, 0));
        path.lineTo(point(notch, 0));
        path.lineTo(point(notch, h * 0.5 - dy));
        path.lineTo(point(w - dx, h * 0.5 - dy));
        path.lineTo(point(w - dx, h * 0.5 - dy - arrowHead));
        path.lineTo(point(w, h * 0.5));
        path.lineTo(point(w - dx, h * 0.5 + dy + arrowHead));
        path.lineTo(point(w - dx, h * 0.5 + dy));
        path.lineTo(point(notch, h * 0.5 + dy));
        path.lineTo(point(notch, h));
        path.lineTo(point(0, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("arrows2.bendArrow"))
    {
        const double dy = param("dy", 0.5, h);
        const double dx = param("dx", 0.5, w);
        const double notch = param("notch", 0.0, h);
        const double arrowHead = param("arrowHead", 40.0, h);

        path.moveTo(point(w - dx, 0));
        path.lineTo(point(w, arrowHead * 0.5));
        path.lineTo(point(w - dx, arrowHead));
        path.lineTo(point(w - dx, arrowHead / 2.0 + dy));

        if (roundedArrow)
        {
            path.lineTo(point(dy * 2.2, arrowHead / 2.0 + dy));
            arcTo(dy * 0.2, dy * 0.2, false, false, dy * 2.0, arrowHead / 2.0 + dy * 1.2);
        }
        else
        {
            path.lineTo(point(dy * 2.0, arrowHead / 2.0 + dy));
        }

        path.lineTo(point(dy * 2.0, h));
        path.lineTo(point(dy, h - notch));
        path.lineTo(point(0, h));

        if (roundedArrow)
        {
            path.lineTo(point(0, arrowHead / 2.0 + dy));
            arcTo(dy * 2.0, dy * 2.0, false, true, dy * 2.0, arrowHead / 2.0 - dy);
        }
        else
        {
            path.lineTo(point(0, arrowHead / 2.0 - dy));
        }

        path.lineTo(point(w - dx, arrowHead / 2.0 - dy));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("arrows2.bendDoubleArrow"))
    {
        const double dy = param("dy", 0.5, h);
        const double dx = param("dx", 0.5, w);
        const double arrowHead = param("arrowHead", 40.0, w);

        path.moveTo(point(w - dx, 0));
        path.lineTo(point(w, arrowHead * 0.5));
        path.lineTo(point(w - dx, arrowHead));
        path.lineTo(point(w - dx, arrowHead / 2.0 + dy));

        if (roundedArrow)
        {
            path.lineTo(point(arrowHead / 2.0 + dy * 1.2, arrowHead / 2.0 + dy));
            arcTo(dy * 0.2, dy * 0.2, false, false,
                  arrowHead / 2.0 + dy, arrowHead / 2.0 + dy * 1.2);
        }
        else
        {
            path.lineTo(point(arrowHead / 2.0 + dy, arrowHead / 2.0 + dy));
        }

        path.lineTo(point(arrowHead / 2.0 + dy, h - dx));
        path.lineTo(point(arrowHead, h - dx));
        path.lineTo(point(arrowHead / 2.0, h));
        path.lineTo(point(0, h - dx));
        path.lineTo(point(arrowHead / 2.0 - dy, h - dx));

        if (roundedArrow)
        {
            path.lineTo(point(arrowHead / 2.0 - dy, arrowHead / 2.0 + dy));
            arcTo(dy * 2.0, dy * 2.0, false, true,
                  arrowHead / 2.0 + dy, arrowHead / 2.0 - dy);
        }
        else
        {
            path.lineTo(point(arrowHead / 2.0 - dy, arrowHead / 2.0 - dy));
        }

        path.lineTo(point(w - dx, arrowHead / 2.0 - dy));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("arrows2.calloutDoubleArrow"))
    {
        const double dy = param("dy", 0.5, h);
        const double dx = param("dx", 0.5, w);
        const double notch = param("notch", 0.0, w);
        const double arrowHead = param("arrowHead", 0.0, h);

        path.moveTo(point(w / 2.0 - notch, 0));
        path.lineTo(point(w / 2.0 + notch, 0));
        path.lineTo(point(w / 2.0 + notch, h * 0.5 - dy));
        path.lineTo(point(w - dx, h * 0.5 - dy));
        path.lineTo(point(w - dx, h * 0.5 - dy - arrowHead));
        path.lineTo(point(w, h * 0.5));
        path.lineTo(point(w - dx, h * 0.5 + dy + arrowHead));
        path.lineTo(point(w - dx, h * 0.5 + dy));
        path.lineTo(point(w / 2.0 + notch, h * 0.5 + dy));
        path.lineTo(point(w / 2.0 + notch, h));
        path.lineTo(point(w / 2.0 - notch, h));
        path.lineTo(point(w / 2.0 - notch, h * 0.5 + dy));
        path.lineTo(point(dx, h * 0.5 + dy));
        path.lineTo(point(dx, h * 0.5 + dy + arrowHead));
        path.lineTo(point(0, h * 0.5));
        path.lineTo(point(dx, h * 0.5 - dy - arrowHead));
        path.lineTo(point(dx, h * 0.5 - dy));
        path.lineTo(point(w / 2.0 - notch, h * 0.5 - dy));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("arrows2.calloutQuadArrow"))
    {
        const double dy = param("dy", 0.5, h);
        const double dx = param("dx", 0.5, w);
        const double notch = param("notch", 0.0, w);
        const double arrowHead = param("arrowHead", 0.0, h);

        path.moveTo(point(w * 0.5 + dy, h * 0.5 - notch));
        path.lineTo(point(w * 0.5 + notch, h * 0.5 - notch));
        path.lineTo(point(w * 0.5 + notch, h * 0.5 - dy));
        path.lineTo(point(w - dx, h * 0.5 - dy));
        path.lineTo(point(w - dx, h * 0.5 - dy - arrowHead));
        path.lineTo(point(w, h * 0.5));
        path.lineTo(point(w - dx, h * 0.5 + dy + arrowHead));
        path.lineTo(point(w - dx, h * 0.5 + dy));
        path.lineTo(point(w * 0.5 + notch, h * 0.5 + dy));
        path.lineTo(point(w * 0.5 + notch, h * 0.5 + notch));
        path.lineTo(point(w * 0.5 + dy, h * 0.5 + notch));
        path.lineTo(point(w * 0.5 + dy, h - dx));
        path.lineTo(point(w * 0.5 + dy + arrowHead, h - dx));
        path.lineTo(point(w * 0.5, h));
        path.lineTo(point(w * 0.5 - dy - arrowHead, h - dx));
        path.lineTo(point(w * 0.5 - dy, h - dx));
        path.lineTo(point(w * 0.5 - dy, h * 0.5 + notch));
        path.lineTo(point(w * 0.5 - notch, h * 0.5 + notch));
        path.lineTo(point(w * 0.5 - notch, h * 0.5 + dy));
        path.lineTo(point(dx, h * 0.5 + dy));
        path.lineTo(point(dx, h * 0.5 + dy + arrowHead));
        path.lineTo(point(0, h * 0.5));
        path.lineTo(point(dx, h * 0.5 - dy - arrowHead));
        path.lineTo(point(dx, h * 0.5 - dy));
        path.lineTo(point(w * 0.5 - notch, h * 0.5 - dy));
        path.lineTo(point(w * 0.5 - notch, h * 0.5 - notch));
        path.lineTo(point(w * 0.5 - dy, h * 0.5 - notch));
        path.lineTo(point(w * 0.5 - dy, dx));
        path.lineTo(point(w * 0.5 - dy - arrowHead, dx));
        path.lineTo(point(w * 0.5, 0));
        path.lineTo(point(w * 0.5 + dy + arrowHead, dx));
        path.lineTo(point(w * 0.5 + dy, dx));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("arrows2.calloutDouble90Arrow"))
    {
        const double dy1 = param("dy1", 0.5, h);
        const double dx1 = param("dx1", 0.5, w);
        const double dx2 = param("dx2", 0.0, w);
        const double dy2 = param("dy2", 0.0, w);
        const double arrowHead = param("arrowHead", 0.0, h);

        path.moveTo(point(0, 0));
        path.lineTo(point(dx2, 0));
        path.lineTo(point(dx2, dy2 * 0.5 - dy1));
        path.lineTo(point(w - dx1, dy2 * 0.5 - dy1));
        path.lineTo(point(w - dx1, dy2 * 0.5 - dy1 - arrowHead));
        path.lineTo(point(w, dy2 * 0.5));
        path.lineTo(point(w - dx1, dy2 * 0.5 + dy1 + arrowHead));
        path.lineTo(point(w - dx1, dy2 * 0.5 + dy1));
        path.lineTo(point(dx2, dy2 * 0.5 + dy1));
        path.lineTo(point(dx2, dy2));
        path.lineTo(point(dx2 / 2.0 + dy1, dy2));
        path.lineTo(point(dx2 / 2.0 + dy1, h - dx1));
        path.lineTo(point(dx2 / 2.0 + dy1 + arrowHead, h - dx1));
        path.lineTo(point(dx2 / 2.0, h));
        path.lineTo(point(dx2 / 2.0 - dy1 - arrowHead, h - dx1));
        path.lineTo(point(dx2 / 2.0 - dy1, h - dx1));
        path.lineTo(point(dx2 / 2.0 - dy1, dy2));
        path.lineTo(point(0, dy2));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("arrows2.quadArrow"))
    {
        const double dy = param("dy", 0.5, h);
        const double dx = param("dx", 0.5, w);
        const double arrowHead = param("arrowHead", 0.0, h);

        path.moveTo(point(w * 0.5 + dy, h * 0.5 - dy));
        path.lineTo(point(w - dx, h * 0.5 - dy));
        path.lineTo(point(w - dx, h * 0.5 - dy - arrowHead));
        path.lineTo(point(w, h * 0.5));
        path.lineTo(point(w - dx, h * 0.5 + dy + arrowHead));
        path.lineTo(point(w - dx, h * 0.5 + dy));
        path.lineTo(point(w * 0.5 + dy, h * 0.5 + dy));
        path.lineTo(point(w * 0.5 + dy, h - dx));
        path.lineTo(point(w * 0.5 + dy + arrowHead, h - dx));
        path.lineTo(point(w * 0.5, h));
        path.lineTo(point(w * 0.5 - dy - arrowHead, h - dx));
        path.lineTo(point(w * 0.5 - dy, h - dx));
        path.lineTo(point(w * 0.5 - dy, h * 0.5 + dy));
        path.lineTo(point(dx, h * 0.5 + dy));
        path.lineTo(point(dx, h * 0.5 + dy + arrowHead));
        path.lineTo(point(0, h * 0.5));
        path.lineTo(point(dx, h * 0.5 - dy - arrowHead));
        path.lineTo(point(dx, h * 0.5 - dy));
        path.lineTo(point(w * 0.5 - dy, h * 0.5 - dy));
        path.lineTo(point(w * 0.5 - dy, dx));
        path.lineTo(point(w * 0.5 - dy - arrowHead, dx));
        path.lineTo(point(w * 0.5, 0));
        path.lineTo(point(w * 0.5 + dy + arrowHead, dx));
        path.lineTo(point(w * 0.5 + dy, dx));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("arrows2.triadArrow"))
    {
        const double dy = param("dy", 0.5, h);
        const double dx = param("dx", 0.5, w);
        const double arrowHead = param("arrowHead", 0.0, h);

        path.moveTo(point(w * 0.5 + arrowHead * 0.5 - dy, h - arrowHead + dy));
        path.lineTo(point(w - dx, h - arrowHead + dy));
        path.lineTo(point(w - dx, h - arrowHead));
        path.lineTo(point(w, h - arrowHead * 0.5));
        path.lineTo(point(w - dx, h));
        path.lineTo(point(w - dx, h - dy));
        path.lineTo(point(dx, h - dy));
        path.lineTo(point(dx, h));
        path.lineTo(point(0, h - arrowHead * 0.5));
        path.lineTo(point(dx, h - arrowHead));
        path.lineTo(point(dx, h - arrowHead + dy));
        path.lineTo(point(w * 0.5 - arrowHead * 0.5 + dy, h - arrowHead + dy));
        path.lineTo(point(w * 0.5 - arrowHead * 0.5 + dy, dx));
        path.lineTo(point(w * 0.5 - arrowHead * 0.5, dx));
        path.lineTo(point(w * 0.5, 0));
        path.lineTo(point(w * 0.5 + arrowHead * 0.5, dx));
        path.lineTo(point(w * 0.5 + arrowHead * 0.5 - dy, dx));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("arrows2.tailedArrow")
        || key == QLatin1String("arrows2.tailedNotchedArrow"))
    {
        const bool notched = key == QLatin1String("arrows2.tailedNotchedArrow");
        const double dy1 = param("dy1", 10.0, h);
        const double dx1 = param("dx1", 20.0, w);
        const double dy2 = param("dy2", 30.0, h);
        const double dx2 = param("dx2", 25.0, w);
        const double notch = param("notch", 0.0, w);
        const double arrowHead = param("arrowHead", 0.0, h);
        // The tail taper ends at the notch, or at the tail length itself.
        const double x2 = (dy2 != 0.0)
            ? dx2 + (notched ? notch : dy2) * (dy2 - dy1) / dy2 : 0.0;

        path.moveTo(point(0, h * 0.5 - dy2));
        path.lineTo(point(dx2, h * 0.5 - dy2));
        path.lineTo(point(x2, h * 0.5 - dy1));
        path.lineTo(point(w - dx1, h * 0.5 - dy1));
        path.lineTo(point(w - dx1, h * 0.5 - dy1 - arrowHead));
        path.lineTo(point(w, h * 0.5));
        path.lineTo(point(w - dx1, h * 0.5 + dy1 + arrowHead));
        path.lineTo(point(w - dx1, h * 0.5 + dy1));
        path.lineTo(point(x2, h * 0.5 + dy1));
        path.lineTo(point(dx2, h * 0.5 + dy2));
        path.lineTo(point(0, h * 0.5 + dy2));
        path.lineTo(point(notch, h * 0.5));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("arrows2.stripedArrow"))
    {
        const double dy = h * 0.5 * ratio("dy", 0.5);
        const double dx = param("dx", 0.5, w);
        const double notch = param("notch", 0.0, w);

        path.moveTo(point(notch, dy));
        path.lineTo(point(w - dx, dy));
        path.lineTo(point(w - dx, 0));
        path.lineTo(point(w, h * 0.5));
        path.lineTo(point(w - dx, h));
        path.lineTo(point(w - dx, h - dy));
        path.lineTo(point(notch, h - dy));
        path.closeSubpath();
        path.moveTo(point(0, h - dy));
        path.lineTo(point(notch * 0.16, h - dy));
        path.lineTo(point(notch * 0.16, dy));
        path.lineTo(point(0, dy));
        path.closeSubpath();
        path.moveTo(point(notch * 0.32, h - dy));
        path.lineTo(point(notch * 0.8, h - dy));
        path.lineTo(point(notch * 0.8, dy));
        path.lineTo(point(notch * 0.32, dy));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("arrows2.jumpInArrow"))
    {
        const double dy = param("dy", 0.5, h);
        const double dx = param("dx", 0.5, w);
        const double arrowHead = param("arrowHead", 40.0, h);

        path.moveTo(point(w - dx, 0));
        path.lineTo(point(w, arrowHead * 0.5));
        path.lineTo(point(w - dx, arrowHead));
        path.lineTo(point(w - dx, arrowHead / 2.0 + dy));
        arcTo(w - dx, h - arrowHead / 2.0 - dy, false, false, 0, h);
        arcTo(w - dx, h - arrowHead / 2.0 + dy, false, true, w - dx, arrowHead / 2.0 - dy);
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("arrows2.uTurnArrow"))
    {
        const double dy = param("dy", 0.5, h);
        const double arrowHead = param("arrowHead", 40.0, h);
        const double dx = (h - arrowHead / 2.0 + dy) / 2.0;
        // getArrowheadLength(): any length up to 1e7 keeps the tip finite.
        const double dx2 = qMin(qMax(0.0,
            Style::number(probe.style, QStringLiteral("dx2"), 25.0)), 1e7);

        path.moveTo(point(dx, 0));
        path.lineTo(point(dx + dx2, arrowHead * 0.5));
        path.lineTo(point(dx, arrowHead));
        path.lineTo(point(dx, arrowHead / 2.0 + dy));
        arcTo(dx - 2.0 * dy, dx - 2.0 * dy, false, false, dx, h - 2.0 * dy);
        path.lineTo(point(qMax(w, dx), h - 2.0 * dy));
        path.lineTo(point(qMax(w, dx), h));
        path.lineTo(point(dx, h));
        arcTo(dx, dx, false, true, dx, arrowHead / 2.0 - dy);
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("arrows2.wedgeArrow"))
    {
        // mxShapeArrowsWedgeArrow.paintEdgeShape for the palette's edge, which
        // runs from the left middle of the bounds to the right middle.
        const double startWidth = qMax(0.0,
            Style::number(probe.style, QStringLiteral("startWidth"), 20.0));
        const double dist = w; // pe.x - p0.x with dy = 0

        if (dist > 0.0)
        {
            const double nx = w * startWidth / dist;
            path.moveTo(point(0, h * 0.5 - nx));
            path.lineTo(point(0, h * 0.5 + nx));
            path.lineTo(point(w, h * 0.5));
            path.closeSubpath();
        }

        return path;
    }

    if (key == QLatin1String("arrows2.wedgeArrowDashed2"))
    {
        // mxShapeArrowsWedgeArrowDashed2.paintEdgeShape: one rung per step,
        // narrowing towards the tip (the rungs are the dashed pattern).
        const double startWidth = qMax(0.0,
            Style::number(probe.style, QStringLiteral("startWidth"), 20.0));
        const double stepSize = qMax(0.0,
            Style::number(probe.style, QStringLiteral("stepSize"), 10.0));
        const double dist = w;
        const double maxSteps = 10000.0;
        int steps = (stepSize > 0.0)
            ? static_cast<int>(qMin(std::floor(dist / stepSize), maxSteps)) : 0;

        if (steps <= 0)
        {
            // draw.io divides by the step count; a single base rung keeps the
            // shape visible instead of producing NaN coordinates.
            steps = 1;
        }

        for (int i = 0; i <= steps; ++i)
        {
            double half = startWidth * (steps - i) / steps;

            if (i == steps)
            {
                half = startWidth * (steps - i * 0.98) / steps;
            }

            const double px = w * i / steps;
            path.moveTo(point(px, h * 0.5 - half));
            path.lineTo(point(px, h * 0.5 + half));
        }

        return path;
    }

    if (key == QLatin1String("cloudServer"))
    {
        path.addRect(QRectF(x, y, w, h));

        for (int i = 1; i < 4; ++i)
        {
            path.moveTo(point(0, h * i / 4.0));
            path.lineTo(point(w, h * i / 4.0));
        }

        path.addEllipse(QPointF(x + w * 0.86, y + h * 0.12), w * 0.025, w * 0.025);
        return path;
    }

    if (key == QLatin1String("cloudDatacenter"))
    {
        path.addRect(QRectF(x, y, w, h));

        for (int i = 1; i < 4; ++i)
        {
            path.moveTo(point(w * i / 4.0, 0));
            path.lineTo(point(w * i / 4.0, h));
        }

        for (int i = 1; i < 3; ++i)
        {
            path.moveTo(point(0, h * i / 3.0));
            path.lineTo(point(w, h * i / 3.0));
        }

        return path;
    }

    if (key == QLatin1String("cloudFirewall"))
    {
        path.addRect(QRectF(x, y, w, h));

        for (int row = 1; row < 4; ++row)
        {
            path.moveTo(point(0, h * row / 4.0));
            path.lineTo(point(w, h * row / 4.0));
        }

        for (int row = 0; row < 4; ++row)
        {
            const double offset = row % 2 == 0 ? 0.5 : 0.0;

            for (int col = 0; col < 3; ++col)
            {
                path.moveTo(point(w * (col + offset) / 3.0, h * row / 4.0));
                path.lineTo(point(w * (col + offset) / 3.0, h * (row + 1) / 4.0));
            }
        }

        return path;
    }

    if (key == QLatin1String("cloudGlobe") || key == QLatin1String("cloudCdn"))
    {
        const double rx = w / 2.0;
        const double ry = h / 2.0;
        path.addEllipse(QPointF(x + rx, y + ry), rx, ry);
        path.addEllipse(QPointF(x + rx, y + ry), rx * 0.5, ry);
        path.moveTo(point(0, h * 0.5));
        path.lineTo(point(w, h * 0.5));
        path.moveTo(point(w * 0.15, h * 0.25));
        path.lineTo(point(w * 0.85, h * 0.25));
        path.moveTo(point(w * 0.15, h * 0.75));
        path.lineTo(point(w * 0.85, h * 0.75));
        return path;
    }

    if (key == QLatin1String("cloudLaptop"))
    {
        const double screenH = h * 0.72;
        path.addRoundedRect(QRectF(x, y, w, screenH), 4, 4);
        path.moveTo(point(w * 0.1, screenH));
        path.lineTo(point(w * 0.05, h));
        path.lineTo(point(w * 0.95, h));
        path.lineTo(point(w * 0.9, screenH));
        return path;
    }

    if (key == QLatin1String("cloudMobile"))
    {
        path.addRoundedRect(QRectF(x + w * 0.2, y, w * 0.6, h), 8, 8);
        path.moveTo(point(w * 0.4, h * 0.92));
        path.lineTo(point(w * 0.6, h * 0.92));
        return path;
    }

    if (key == QLatin1String("cloudLoadBalancer") || key == QLatin1String("cloudRouter"))
    {
        const QPointF center(w / 2.0, h / 2.0);

        for (int i = 0; i < 6; ++i)
        {
            const double angle = i * M_PI / 3.0;
            const QPointF p(center.x() + std::cos(angle) * w / 2.0,
                            center.y() + std::sin(angle) * h / 2.0);
            i == 0 ? path.moveTo(p) : path.lineTo(p);
        }

        path.closeSubpath();
        path.moveTo(point(w * 0.2, h * 0.5));
        path.lineTo(point(w * 0.8, h * 0.5));
        path.moveTo(point(w * 0.5, h * 0.2));
        path.lineTo(point(w * 0.5, h * 0.8));
        return path;
    }

    if (key == QLatin1String("cloudSwitch"))
    {
        path.addRect(QRectF(x, y + h * 0.2, w, h * 0.6));

        for (int i = 1; i < 4; ++i)
        {
            path.moveTo(point(w * i / 4.0, h * 0.2));
            path.lineTo(point(w * i / 4.0, h * 0.8));
        }

        return path;
    }

    if (key == QLatin1String("cloudVpc"))
    {
        path.addRoundedRect(QRectF(x, y, w, h), 10, 10);
        path.addRoundedRect(QRectF(x + w * 0.1, y + h * 0.1, w * 0.8, h * 0.8), 8, 8);
        return path;
    }

    if (key == QLatin1String("cloudCallout") || key == QLatin1String("cloudRect"))
    {
        const double ch = h * 0.72;
        const double cw = w;
        path.moveTo(point(cw * 0.22, ch * 0.3));
        path.cubicTo(point(cw * 0.02, ch * 0.3), point(0, ch * 0.62), point(cw * 0.2, ch * 0.72));
        path.cubicTo(point(cw * 0.18, ch * 0.95), point(cw * 0.48, ch), point(cw * 0.56, ch * 0.82));
        path.cubicTo(point(cw * 0.68, ch), point(cw * 0.95, ch * 0.9), point(cw * 0.9, ch * 0.68));
        path.cubicTo(point(cw * 1.02, ch * 0.5), point(cw * 0.9, ch * 0.26), point(cw * 0.7, ch * 0.3));
        path.cubicTo(point(cw * 0.62, ch * 0.06), point(cw * 0.3, ch * 0.02), point(cw * 0.22, ch * 0.3));
        path.closeSubpath();

        if (key == QLatin1String("cloudCallout"))
        {
            // mxgraph.basic.cloud_callout (90.41 x 61.4): bubbles are part of
            // the foreground path, outside the cloud body.
            path.addEllipse(QPointF(x + w * 0.1006, y + h * 0.7956),
                            w * 0.0664, h * 0.0440);
            path.addEllipse(QPointF(x + w * 0.0896, y + h * 0.9015),
                            w * 0.0420, h * 0.0293);
            path.addEllipse(QPointF(x + w * 0.0265, y + h * 0.9827),
                            w * 0.0265, h * 0.0212);
        }

        return path;
    }

    if (key == QLatin1String("cone") || key == QLatin1String("cone2"))
    {
        const double apexX = key == QLatin1String("cone2") ? w * 0.4 : w * 0.5;
        const double baseY = h * 0.82;
        path.moveTo(point(apexX, 0));
        path.lineTo(point(0, baseY));
        path.cubicTo(point(0, h), point(w, h), point(w, baseY));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("donut"))
    {
        const double thickness = qMin(w, h) * 0.22;
        path.addEllipse(QRectF(x, y, w, h));
        path.addEllipse(QRectF(x + thickness, y + thickness,
                               w - thickness * 2, h - thickness * 2));
        return path;
    }

    if (key == QLatin1String("drop"))
    {
        // Teardrop: pointed top, round bottom.
        path.moveTo(point(w * 0.5, 0));
        path.cubicTo(point(w * 0.66, h * 0.24), point(w, h * 0.5), point(w, h * 0.68));
        path.cubicTo(point(w, h * 0.88), point(w * 0.78, h), point(w * 0.5, h));
        path.cubicTo(point(w * 0.22, h), point(0, h * 0.88), point(0, h * 0.68));
        path.cubicTo(point(0, h * 0.5), point(w * 0.34, h * 0.24), point(w * 0.5, 0));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("flash"))
    {
        // mxgraph.basic.flash (60 x 95.5)
        static const double bolt[][2] = {
            {0.0, 1.0}, {0.3333, 0.7906}, {0.05, 0.6440}, {0.3333, 0.5183},
            {0.05, 0.3298}, {0.5667, 0.0}, {1.0, 0.2670}, {0.6, 0.4136},
            {0.8333, 0.5602}, {0.4833, 0.6859}, {0.7, 0.8008}
        };

        for (int i = 0; i < int(sizeof(bolt) / sizeof(bolt[0])); ++i)
        {
            const QPointF p = point(w * bolt[i][0], h * bolt[i][1]);
            i == 0 ? path.moveTo(p) : path.lineTo(p);
        }

        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("halfCircle"))
    {
        path.moveTo(point(0, h));
        path.cubicTo(point(0, 0), point(w, 0), point(w, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("heart"))
    {
        // mxgraph.basic.heart (103.89 x 94.74)
        const auto px = [&](double v) { return x + w * (v / 103.89); };
        const auto py = [&](double v) { return y + h * (v / 94.74); };

        path.moveTo(px(51.94), py(94.74));
        path.cubicTo(px(55.79), py(90.78), px(77.8), py(68.16), px(91.56), py(54.03));
        path.cubicTo(px(103.89), py(41.37), px(103.62), py(22.91), px(92.42), py(11.46));
        path.cubicTo(px(81.21), py(0), px(63.09), py(0.05), px(51.94), py(11.56));
        path.cubicTo(px(40.79), py(0.05), px(22.67), py(0), px(11.47), py(11.45));
        path.cubicTo(px(0.26), py(22.9), px(0), py(41.36), px(12.32), py(54.03));
        path.cubicTo(px(26.08), py(68.16), px(48.09), py(90.78), px(51.94), py(94.74));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("isoCube"))
    {
        // mxShapeBasicIsoCube: hexagon whose depth follows isoAngle.
        const double angle = qBound(0.01, Style::number(probe.style,
                                                       QStringLiteral("isoAngle"), 15.0), 94.0);
        const double isoH = qMin(w * std::tan(angle * M_PI / 200.0), h * 0.5);

        path.moveTo(point(w * 0.5, 0));
        path.lineTo(point(w, isoH));
        path.lineTo(point(w, h - isoH));
        path.lineTo(point(w * 0.5, h));
        path.lineTo(point(0, h - isoH));
        path.lineTo(point(0, isoH));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("loudCallout"))
    {
        const QPointF center(w / 2.0, h / 2.0);
        const int spikes = 12;

        for (int i = 0; i < spikes * 2; ++i)
        {
            const double radius = i % 2 == 0 ? 1.0 : 0.72;
            const double angle = -M_PI / 2.0 + i * M_PI / spikes;
            const QPointF p(center.x() + std::cos(angle) * w / 2.0 * radius,
                            center.y() + std::sin(angle) * h / 2.0 * radius);
            i == 0 ? path.moveTo(p) : path.lineTo(p);
        }

        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("moon"))
    {
        path.moveTo(point(0, 0));
        path.cubicTo(point(w * 0.78, 0), point(w * 0.78, h), point(0, h));
        path.cubicTo(point(w * 0.38, h * 0.7), point(w * 0.38, h * 0.3), point(0, 0));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("noSymbol"))
    {
        // mxgraph.basic.no_symbol: circle plus two lens shaped cut-outs that
        // together form the diagonal bar.
        const auto px = [&](double v) { return x + w * (v / 100.0); };
        const auto py = [&](double v) { return y + h * (v / 100.0); };

        path.moveTo(px(0), py(50));
        addSvgArc(path, QPointF(px(0), py(50)), QPointF(px(100), py(50)),
                  w * 0.35, h * 0.35, false, true);
        addSvgArc(path, QPointF(px(100), py(50)), QPointF(px(0), py(50)),
                  w * 0.35, h * 0.35, false, true);
        path.closeSubpath();

        path.moveTo(px(78.95), py(69.7));
        addSvgArc(path, QPointF(px(78.95), py(69.7)), QPointF(px(30.3), py(21.05)),
                  w * 0.35, h * 0.35, false, false);
        path.closeSubpath();

        path.moveTo(px(21.15), py(30.3));
        addSvgArc(path, QPointF(px(21.15), py(30.3)), QPointF(px(69.7), py(79)),
                  w * 0.35, h * 0.35, false, false);
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("orthogonalTriangle"))
    {
        path.moveTo(point(0, h));
        path.lineTo(point(0, 0));
        path.lineTo(point(w, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("acuteTriangle") || key == QLatin1String("obtuseTriangle"))
    {
        const double apex = key == QLatin1String("acuteTriangle") ? w * 0.5 : w * 0.25;
        path.moveTo(point(apex, 0));
        path.lineTo(point(w, h));
        path.lineTo(point(0, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("ovalCallout") || key == QLatin1String("roundRectCallout"))
    {
        const double bodyH = h * 0.78;
        path.addEllipse(QRectF(x, y, w, bodyH));
        path.moveTo(point(w * 0.3, bodyH * 0.9));
        path.lineTo(point(w * 0.2, h));
        path.lineTo(point(w * 0.46, bodyH * 0.82));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("pointedOval"))
    {
        path.moveTo(point(0, h * 0.5));
        path.quadTo(point(w * 0.35, 0), point(w * 0.5, 0));
        path.quadTo(point(w * 0.65, 0), point(w, h * 0.5));
        path.quadTo(point(w * 0.65, h), point(w * 0.5, h));
        path.quadTo(point(w * 0.35, h), point(0, h * 0.5));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("pyramid"))
    {
        // draw.io's mxShapeBasicPyramid: a quadrilateral with a single ridge
        // line running from the top corner to the bottom corner.
        const double dx1 = w * qBound(0.0, Style::number(probe.style, QStringLiteral("dx1"), 0.5), 1.0);
        const double dx2 = w * qBound(0.0, Style::number(probe.style, QStringLiteral("dx2"), 0.6), 1.0);
        const double dy1 = h * qBound(0.0, Style::number(probe.style, QStringLiteral("dy1"), 0.9), 1.0);
        const double dy2 = h * qBound(0.0, Style::number(probe.style, QStringLiteral("dy2"), 0.8), 1.0);

        path.moveTo(point(dx1, 0));
        path.lineTo(point(w, dy2));
        path.lineTo(point(dx2, h));
        path.lineTo(point(0, dy1));
        path.closeSubpath();
        path.moveTo(point(dx1, 0));
        path.lineTo(point(dx2, h));
        return path;
    }

    if (key == QLatin1String("diagSnipRect"))
    {
        const double cut = qMin(qMin(w, h) * 0.2, 24.0);
        path.moveTo(point(0, 0));
        path.lineTo(point(w - cut, 0));
        path.lineTo(point(w, cut));
        path.lineTo(point(w, h));
        path.lineTo(point(0, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("diagRoundRect") || key == QLatin1String("cornerRoundRect")
        || key == QLatin1String("threeCornerRoundRect") || key == QLatin1String("plaque"))
    {
        const double cut = qMin(qMin(w, h) * 0.2, 24.0);

        if (key == QLatin1String("plaque"))
        {
            path.moveTo(point(cut, 0));
            path.lineTo(point(w - cut, 0));
            path.lineTo(point(w, cut));
            path.lineTo(point(w, h - cut));
            path.lineTo(point(w - cut, h));
            path.lineTo(point(cut, h));
            path.lineTo(point(0, h - cut));
            path.lineTo(point(0, cut));
            path.closeSubpath();
        }
        else
        {
            path.addRoundedRect(QRectF(x, y, w, h), cut, cut);
        }

        return path;
    }

    if (key == QLatin1String("frame") || key == QLatin1String("roundedFrame")
        || key == QLatin1String("plaqueFrame"))
    {
        const double inset = qMin(qMin(w, h) * 0.12, 12.0);
        path.addRect(QRectF(x, y, w, h));
        path.addRect(QRectF(x + inset, y + inset, w - inset * 2, h - inset * 2));
        return path;
    }

    if (key == QLatin1String("frameCorner"))
    {
        path.moveTo(point(0, h * 0.25));
        path.lineTo(point(0, 0));
        path.lineTo(point(w * 0.25, 0));
        path.moveTo(point(w * 0.75, h));
        path.lineTo(point(w, h));
        path.lineTo(point(w, h * 0.75));
        return path;
    }

    if (key == QLatin1String("diagStripe"))
    {
        path.moveTo(point(0, h));
        path.lineTo(point(w * 0.55, 0));
        path.lineTo(point(w, 0));
        path.lineTo(point(w * 0.45, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("rectCallout"))
    {
        const double bodyH = h * 0.78;
        path.addRect(QRectF(x, y, w, bodyH));
        path.moveTo(point(w * 0.28, bodyH));
        path.lineTo(point(w * 0.2, h));
        path.lineTo(point(w * 0.45, bodyH));
        return path;
    }

    if (key == QLatin1String("layeredRect"))
    {
        const double d = qMin(qMin(w, h) * 0.12, 10.0);
        path.addRect(QRectF(x, y + d, w - d, h - d));
        path.addRect(QRectF(x + d, y, w - d, h - d));
        return path;
    }

    if (key == QLatin1String("smiley"))
    {
        // mxgraph.basic.smiley (98 x 98)
        const auto px = [&](double v) { return x + w * (v / 98.0); };
        const auto py = [&](double v) { return y + h * (v / 98.0); };

        path.addEllipse(QRectF(px(0), py(0), w, h));

        path.moveTo(px(16), py(51));
        path.lineTo(px(6), py(57));
        path.moveTo(px(82), py(51));
        path.lineTo(px(92), py(57));
        return path;
    }

    if (key == QLatin1String("sun"))
    {
        // mxgraph.basic.sun (95 x 95): disc plus eight triangular rays.
        const auto px = [&](double v) { return x + w * (v / 95.0); };
        const auto py = [&](double v) { return y + h * (v / 95.0); };

        path.addEllipse(QRectF(px(17.5), py(17.5), w * 60.0 / 95.0, h * 60.0 / 95.0));

        static const double rays[][6] = {
            {42.5, 14.5, 47.5, 0, 52.5, 14.5},
            {42.5, 80.5, 47.5, 95, 52.5, 80.5},
            {14.5, 42.5, 0, 47.5, 14.5, 52.5},
            {80.5, 42.5, 95, 47.5, 80.5, 52.5},
            {67.5, 20.5, 81.2, 13.9, 74.5, 27.5},
            {67.5, 74.5, 81.2, 81.1, 74.5, 67.5},
            {27.5, 20.5, 13.8, 13.9, 20.5, 27.5},
            {27.5, 74.5, 13.8, 81.1, 20.5, 67.5}
        };

        for (const auto &ray : rays)
        {
            path.moveTo(px(ray[0]), py(ray[1]));
            path.lineTo(px(ray[2]), py(ray[3]));
            path.lineTo(px(ray[4]), py(ray[5]));
            path.closeSubpath();
        }

        return path;
    }

    if (key == QLatin1String("tick"))
    {
        // mxgraph.basic.tick (84.4 x 97.54): a brush-stroke check made of arcs.
        const auto px = [&](double v) { return x + w * (v / 84.4); };
        const auto py = [&](double v) { return y + h * (v / 97.54); };
        const auto arc = [&](double rx, double ry, double tx, double ty,
                             bool largeArc, bool sweep)
        {
            const QPointF from = path.currentPosition();
            addSvgArc(path, from, QPointF(px(tx), py(ty)),
                      w * rx / 84.4, h * ry / 97.54, largeArc, sweep);
        };

        path.moveTo(px(0.36), py(66.69));
        arc(12, 12, 16.36, 58.69, false, true);
        arc(20, 20, 26.36, 69.69, false, true);
        arc(200, 200, 63.36, 5.69, false, true);
        arc(18, 18, 80.36, 1.69, false, true);
        arc(4.5, 4.5, 83.36, 8.69, false, true);
        arc(230, 230, 35.36, 94.69, false, false);
        arc(20, 20, 17.36, 94.69, false, true);
        arc(100, 100, 0.36, 68.69, false, false);
        arc(2, 2, 0.36, 66.69, false, true);
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("wave") || key == QLatin1String("wave2"))
    {
        // mxShapeBasicWave (mxgraph.basic.wave2)
        const double dy = h * qBound(0.0, Style::number(probe.style, QStringLiteral("dy"), 0.5), 1.0);
        const double fy = 1.4;

        path.moveTo(point(0, dy / 2));
        path.quadTo(point(w / 6, dy * (1 - fy)), point(w / 3, dy / 2));
        path.quadTo(point(w / 2, dy * fy), point(w * 2 / 3, dy / 2));
        path.quadTo(point(w * 5 / 6, dy * (1 - fy)), point(w, dy / 2));
        path.lineTo(point(w, h - dy / 2));
        path.quadTo(point(w * 5 / 6, h - dy * fy), point(w * 2 / 3, h - dy / 2));
        path.quadTo(point(w / 2, h - dy * (1 - fy)), point(w / 3, h - dy / 2));
        path.quadTo(point(w / 6, h - dy * fy), point(0, h - dy / 2));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("pie"))
    {
        // mxShapeBasicPie (mxgraph.basic.pie)
        const double startFraction = qBound(0.0,
            Style::number(probe.style, QStringLiteral("startAngle"), 0.25), 1.0);
        const double endFraction = qBound(0.0,
            Style::number(probe.style, QStringLiteral("endAngle"), 0.75), 1.0);
        const double rx = w / 2.0;
        const double ry = h / 2.0;
        double a0 = 2.0 * M_PI * startFraction;
        double a1 = 2.0 * M_PI * endFraction;

        if (a1 < a0)
        {
            a1 += 2.0 * M_PI;
        }

        const QPointF start(rx + std::sin(a0) * rx, ry - std::cos(a0) * ry);

        path.moveTo(point(rx, ry));
        path.lineTo(point(start.x(), start.y()));
        path.arcTo(QRectF(x, y, w, h),
                   90.0 - a0 * 180.0 / M_PI, -(a1 - a0) * 180.0 / M_PI);
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("x"))
    {
        // mxgraph.basic.x (96 x 98)
        static const double letter[][2] = {
            {0, 0}, {28, 0}, {48, 29}, {68, 0}, {96, 0}, {62, 49},
            {96, 98}, {68, 98}, {48, 69}, {28, 98}, {0, 98}, {32, 49}
        };

        for (int i = 0; i < 12; ++i)
        {
            const QPointF p = point(w * letter[i][0] / 96.0, h * letter[i][1] / 98.0);
            i == 0 ? path.moveTo(p) : path.lineTo(p);
        }

        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("button"))
    {
        path.addRoundedRect(QRectF(x, y, w, h), qMin(h * 0.25, 10.0), qMin(h * 0.25, 10.0));
        return path;
    }

    if (key == QLatin1String("star"))
    {
        const QPointF center(w / 2.0, h / 2.0);
        const double outer = qMin(w, h) / 2.0;
        const double inner = outer * 0.42;

        for (int i = 0; i < 10; ++i)
        {
            const double radius = (i % 2 == 0) ? outer : inner;
            const double angle = -M_PI / 2.0 + i * M_PI / 5.0;
            if (i == 0)
            {
                path.moveTo(QPointF(center.x() + std::cos(angle) * radius * (w / h),
                                    center.y() + std::sin(angle) * radius));
            }
            else
            {
                path.lineTo(center.x() + std::cos(angle) * radius * (w / h),
                            center.y() + std::sin(angle) * radius);
            }
        }

        path.closeSubpath();
        return path;
    }

    // --- flowchart -------------------------------------------------------
    // Literal ports of the stencil set mxGraph.flowchart (flowchart.xml).
    // The <move>/<line>/<arc>/<ellipse>/<roundrect>/<close> commands are
    // replayed in document order and scaled from the stencil units
    // (<shape w= h=>) to the cell rect, like mxStencil.computeAspect().
    // Foreground paths that draw.io strokes on top of the outline are
    // painted as decorations in ShapeRenderer.cpp so they cannot punch
    // holes into the filled path.

    // Annotation 1
    if (key == QLatin1String("flowchart.annotation_1"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 50, 98);
        path.moveTo(f.p(50, 0));
        path.lineTo(f.p(0, 0));
        path.lineTo(f.p(0, 98));
        path.lineTo(f.p(50, 98));
        return path;
    }

    // Annotation 2
    if (key == QLatin1String("flowchart.annotation_2"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 100, 98);
        path.moveTo(f.p(100, 0));
        path.lineTo(f.p(50, 0));
        path.lineTo(f.p(50, 98));
        path.lineTo(f.p(100, 98));
        return path;
    }

    // Card
    if (key == QLatin1String("flowchart.card"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 98, 60);
        path.moveTo(f.p(19, 0));
        path.lineTo(f.p(93, 0));
        addSvgArc(path, f.p(93, 0), f.p(98, 5), 5 * f.sx, 5 * f.sy, false, true);
        path.lineTo(f.p(98, 55));
        addSvgArc(path, f.p(98, 55), f.p(93, 60), 5 * f.sx, 5 * f.sy, false, true);
        path.lineTo(f.p(5, 60));
        addSvgArc(path, f.p(5, 60), f.p(0, 55), 5 * f.sx, 5 * f.sy, false, true);
        path.lineTo(f.p(0, 20));
        path.lineTo(f.p(19, 0));
        path.closeSubpath();
        return path;
    }

    // Collate
    if (key == QLatin1String("flowchart.collate"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 96.82, 98);
        path.moveTo(f.p(92.41, 0));
        addSvgArc(path, f.p(92.41, 0), f.p(95.41, 5), 6 * f.sx, 3.5 * f.sy, false, true);
        path.lineTo(f.p(1.41, 93));
        addSvgArc(path, f.p(1.41, 93), f.p(4.41, 98), 6 * f.sx, 3.5 * f.sy, false, false);
        path.lineTo(f.p(92.41, 98));
        addSvgArc(path, f.p(92.41, 98), f.p(95.41, 93), 6 * f.sx, 3.5 * f.sy, false, false);
        path.lineTo(f.p(1.41, 5));
        addSvgArc(path, f.p(1.41, 5), f.p(4.41, 0), 6 * f.sx, 3.5 * f.sy, false, true);
        path.lineTo(f.p(92.41, 0));
        path.closeSubpath();
        return path;
    }

    // Data
    if (key == QLatin1String("flowchart.data"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 98.77, 60.24);
        path.moveTo(f.p(19.37, 5.12));
        addSvgArc(path, f.p(19.37, 5.12), f.p(24.37, 0.12), 6 * f.sx, 12 * f.sy, false, true);
        path.lineTo(f.p(93.37, 0.12));
        addSvgArc(path, f.p(93.37, 0.12), f.p(98.37, 5.12), 5 * f.sx, 4 * f.sy, false, true);
        path.lineTo(f.p(79.37, 55.12));
        addSvgArc(path, f.p(79.37, 55.12), f.p(74.37, 60.12), 6 * f.sx, 12 * f.sy, false, true);
        path.lineTo(f.p(4.37, 60.12));
        addSvgArc(path, f.p(4.37, 60.12), f.p(0.37, 55.12), 5 * f.sx, 4 * f.sy, false, true);
        path.closeSubpath();
        return path;
    }

    // Database
    if (key == QLatin1String("flowchart.database"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 60, 60);
        path.moveTo(f.p(0, 50));
        path.lineTo(f.p(0, 10));
        addSvgArc(path, f.p(0, 10), f.p(60, 10), 30 * f.sx, 10 * f.sy, false, true);
        path.lineTo(f.p(60, 50));
        addSvgArc(path, f.p(60, 50), f.p(0, 50), 30 * f.sx, 10 * f.sy, false, true);
        path.closeSubpath();
        return path;
    }

    // Decision
    if (key == QLatin1String("flowchart.decision"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 100, 100);
        path.moveTo(f.p(50, 0));
        path.lineTo(f.p(100, 50));
        path.lineTo(f.p(50, 100));
        path.lineTo(f.p(0, 50));
        path.closeSubpath();
        return path;
    }

    // Delay
    if (key == QLatin1String("flowchart.delay"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 98.25, 60);
        path.moveTo(f.p(0, 5));
        addSvgArc(path, f.p(0, 5), f.p(5, 0), 5 * f.sx, 5 * f.sy, false, true);
        path.lineTo(f.p(79, 0));
        addSvgArc(path, f.p(79, 0), f.p(79, 60), 33 * f.sx, 33 * f.sy, false, true);
        path.lineTo(f.p(5, 60));
        addSvgArc(path, f.p(5, 60), f.p(0, 55), 5 * f.sx, 5 * f.sy, false, true);
        path.lineTo(f.p(0, 5));
        path.closeSubpath();
        return path;
    }

    // Direct Data
    if (key == QLatin1String("flowchart.direct_data"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 98, 60);
        path.moveTo(f.p(9, 0));
        path.lineTo(f.p(89, 0));
        addSvgArc(path, f.p(89, 0), f.p(89, 60), 9 * f.sx, 30 * f.sy, true, true);
        path.lineTo(f.p(9, 60));
        addSvgArc(path, f.p(9, 60), f.p(9, 0), 9 * f.sx, 30 * f.sy, true, true);
        path.closeSubpath();
        return path;
    }

    // Display
    if (key == QLatin1String("flowchart.display"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 98.25, 60);
        path.moveTo(f.p(0, 30));
        addSvgArc(path, f.p(0, 30), f.p(39, 0), 60 * f.sx, 60 * f.sy, false, true);
        path.lineTo(f.p(79, 0));
        addSvgArc(path, f.p(79, 0), f.p(79, 60), 33 * f.sx, 33 * f.sy, false, true);
        path.lineTo(f.p(39, 60));
        addSvgArc(path, f.p(39, 60), f.p(0, 30), 60 * f.sx, 60 * f.sy, false, true);
        path.closeSubpath();
        return path;
    }

    // Document
    if (key == QLatin1String("flowchart.document"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 98, 60.9);
        path.moveTo(f.p(0, 5));
        addSvgArc(path, f.p(0, 5), f.p(5, 0), 5 * f.sx, 5 * f.sy, false, true);
        path.lineTo(f.p(93, 0));
        addSvgArc(path, f.p(93, 0), f.p(98, 5), 5 * f.sx, 5 * f.sy, false, true);
        path.lineTo(f.p(98, 55));
        addSvgArc(path, f.p(98, 55), f.p(49, 55), 70 * f.sx, 70 * f.sy, false, false);
        addSvgArc(path, f.p(49, 55), f.p(0, 55), 70 * f.sx, 70 * f.sy, false, true);
        path.closeSubpath();
        return path;
    }

    // Extract or Measurement
    if (key == QLatin1String("flowchart.extract_or_measurement"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 95.01, 60.84);
        path.moveTo(f.p(3.5, 60.84));
        path.lineTo(f.p(91.5, 60.84));
        addSvgArc(path, f.p(91.5, 60.84), f.p(94.5, 55.84), 3.5 * f.sx, 3.5 * f.sy, false, false);
        path.lineTo(f.p(49.5, 0.84));
        addSvgArc(path, f.p(49.5, 0.84), f.p(45.5, 0.84), 3.5 * f.sx, 3.5 * f.sy, false, false);
        path.lineTo(f.p(0.5, 55.84));
        addSvgArc(path, f.p(0.5, 55.84), f.p(3.5, 60.84), 3.5 * f.sx, 3.5 * f.sy, false, false);
        path.closeSubpath();
        return path;
    }

    // Internal Storage
    if (key == QLatin1String("flowchart.internal_storage"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 70, 70);
        const double r = qMin(70 * f.sx, 70 * f.sy) * 7.143 / 100.0;
        path.addRoundedRect(QRectF(f.p(0, 0), QSizeF(70 * f.sx, 70 * f.sy)), r, r);
        return path;
    }

    // Loop Limit
    if (key == QLatin1String("flowchart.loop_limit"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 98, 60);
        path.moveTo(f.p(19, 0));
        path.lineTo(f.p(79, 0));
        path.lineTo(f.p(98, 20));
        path.lineTo(f.p(98, 55));
        addSvgArc(path, f.p(98, 55), f.p(93, 60), 5 * f.sx, 5 * f.sy, false, true);
        path.lineTo(f.p(5, 60));
        addSvgArc(path, f.p(5, 60), f.p(0, 55), 5 * f.sx, 5 * f.sy, false, true);
        path.lineTo(f.p(0, 20));
        path.lineTo(f.p(19, 0));
        path.closeSubpath();
        return path;
    }

    // Manual Input
    if (key == QLatin1String("flowchart.manual_input"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 98.05, 60);
        path.moveTo(f.p(0, 28.73));
        addSvgArc(path, f.p(0, 28.73), f.p(3.7, 24), 5 * f.sx, 5 * f.sy, false, true);
        path.lineTo(f.p(91.7, 0.17));
        addSvgArc(path, f.p(91.7, 0.17), f.p(98, 5), 5 * f.sx, 5 * f.sy, false, true);
        path.lineTo(f.p(98, 55));
        addSvgArc(path, f.p(98, 55), f.p(94, 60), 5 * f.sx, 5 * f.sy, false, true);
        path.lineTo(f.p(5, 60));
        addSvgArc(path, f.p(5, 60), f.p(0, 55), 5 * f.sx, 5 * f.sy, false, true);
        path.lineTo(f.p(0, 28.73));
        path.closeSubpath();
        return path;
    }

    // Manual Operation
    if (key == QLatin1String("flowchart.manual_operation"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 98.79, 60.04);
        path.moveTo(f.p(0.39, 5.04));
        addSvgArc(path, f.p(0.39, 5.04), f.p(5.39, 0.04), 5 * f.sx, 4 * f.sy, false, true);
        path.lineTo(f.p(93.39, 0.04));
        addSvgArc(path, f.p(93.39, 0.04), f.p(98.39, 5.04), 5 * f.sx, 4 * f.sy, false, true);
        path.lineTo(f.p(79.39, 55.04));
        addSvgArc(path, f.p(79.39, 55.04), f.p(74.39, 60.04), 6.5 * f.sx, 6.5 * f.sy, false, true);
        path.lineTo(f.p(24.39, 60.04));
        addSvgArc(path, f.p(24.39, 60.04), f.p(19.39, 55.04), 6.5 * f.sx, 6.5 * f.sy, false, true);
        path.lineTo(f.p(0.39, 5.04));
        path.closeSubpath();
        return path;
    }

    // Merge or Storage
    if (key == QLatin1String("flowchart.merge_or_storage"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 95.01, 60.84);
        path.moveTo(f.p(3.5, 0));
        path.lineTo(f.p(91.5, 0));
        addSvgArc(path, f.p(91.5, 0), f.p(94.5, 5), 3.5 * f.sx, 3.5 * f.sy, false, true);
        path.lineTo(f.p(49.5, 60));
        addSvgArc(path, f.p(49.5, 60), f.p(45.5, 60), 3.5 * f.sx, 3.5 * f.sy, false, true);
        path.lineTo(f.p(0.5, 5));
        addSvgArc(path, f.p(0.5, 5), f.p(3.5, 0), 3.5 * f.sx, 3.5 * f.sy, false, true);
        path.closeSubpath();
        return path;
    }

    // Multi-Document
    if (key == QLatin1String("flowchart.multi-document"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 88, 60.28);
        path.moveTo(f.p(10, 5));
        addSvgArc(path, f.p(10, 5), f.p(15, 0), 5 * f.sx, 5 * f.sy, false, true);
        path.lineTo(f.p(83, 0));
        addSvgArc(path, f.p(83, 0), f.p(88, 5), 5 * f.sx, 5 * f.sy, false, true);
        path.lineTo(f.p(88, 45));
        addSvgArc(path, f.p(88, 45), f.p(49, 45), 50 * f.sx, 50 * f.sy, false, false);
        addSvgArc(path, f.p(49, 45), f.p(10, 45), 50 * f.sx, 50 * f.sy, false, true);
        path.closeSubpath();
        return path;
    }

    // Off-page Reference
    if (key == QLatin1String("flowchart.off-page_reference"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 60, 60);
        path.moveTo(f.p(0, 0));
        path.lineTo(f.p(60, 0));
        path.lineTo(f.p(60, 30));
        path.lineTo(f.p(30, 60));
        path.lineTo(f.p(0, 30));
        path.closeSubpath();
        return path;
    }

    // On-page Reference
    if (key == QLatin1String("flowchart.on-page_reference"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 60, 60);
        path.addEllipse(QRectF(f.p(0, 0), QSizeF(60 * f.sx, 60 * f.sy)));
        return path;
    }

    // Or
    if (key == QLatin1String("flowchart.or"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 70, 70);
        path.addEllipse(QRectF(f.p(0, 0), QSizeF(70 * f.sx, 70 * f.sy)));
        return path;
    }

    // Or 2
    if (key == QLatin1String("flowchart.or_2"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 70, 70);
        path.addEllipse(QRectF(f.p(0, 0), QSizeF(70 * f.sx, 70 * f.sy)));
        return path;
    }

    // Paper Tape
    if (key == QLatin1String("flowchart.paper_tape"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 98, 61.81);
        path.moveTo(f.p(0, 5.9));
        addSvgArc(path, f.p(0, 5.9), f.p(49, 5.9), 70 * f.sx, 70 * f.sy, false, false);
        addSvgArc(path, f.p(49, 5.9), f.p(98, 5.9), 70 * f.sx, 70 * f.sy, false, true);
        path.lineTo(f.p(98, 55.9));
        addSvgArc(path, f.p(98, 55.9), f.p(49, 55.9), 70 * f.sx, 70 * f.sy, false, false);
        addSvgArc(path, f.p(49, 55.9), f.p(0, 55.9), 70 * f.sx, 70 * f.sy, false, true);
        path.lineTo(f.p(0, 5.9));
        path.closeSubpath();
        return path;
    }

    // Parallel Mode
    if (key == QLatin1String("flowchart.parallel_mode"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 94, 40);
        path.moveTo(f.p(47, 15));
        path.lineTo(f.p(52, 20));
        path.lineTo(f.p(47, 25));
        path.lineTo(f.p(42, 20));
        path.lineTo(f.p(47, 15));
        path.closeSubpath();
        path.moveTo(f.p(27, 15));
        path.lineTo(f.p(32, 20));
        path.lineTo(f.p(27, 25));
        path.lineTo(f.p(22, 20));
        path.lineTo(f.p(27, 15));
        path.closeSubpath();
        path.moveTo(f.p(67, 15));
        path.lineTo(f.p(72, 20));
        path.lineTo(f.p(67, 25));
        path.lineTo(f.p(62, 20));
        path.lineTo(f.p(67, 15));
        path.closeSubpath();
        return path;
    }

    // Predefined Process
    if (key == QLatin1String("flowchart.predefined_process"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 98, 60);
        const double r = qMin(98 * f.sx, 60 * f.sy) * 6.718 / 100.0;
        path.addRoundedRect(QRectF(f.p(0, 0), QSizeF(98 * f.sx, 60 * f.sy)), r, r);
        return path;
    }

    // Preparation
    if (key == QLatin1String("flowchart.preparation"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 97.11, 60);
        path.moveTo(f.p(20.56, 5));
        addSvgArc(path, f.p(20.56, 5), f.p(31.56, 0), 15 * f.sx, 15 * f.sy, false, true);
        path.lineTo(f.p(65.56, 0));
        addSvgArc(path, f.p(65.56, 0), f.p(76.56, 5), 15 * f.sx, 15 * f.sy, false, true);
        path.lineTo(f.p(96.56, 28));
        addSvgArc(path, f.p(96.56, 28), f.p(96.56, 32), 5 * f.sx, 5 * f.sy, false, true);
        path.lineTo(f.p(76.56, 55));
        addSvgArc(path, f.p(76.56, 55), f.p(65.56, 60), 15 * f.sx, 15 * f.sy, false, true);
        path.lineTo(f.p(31.56, 60));
        addSvgArc(path, f.p(31.56, 60), f.p(20.56, 55), 15 * f.sx, 15 * f.sy, false, true);
        path.lineTo(f.p(0.56, 32));
        addSvgArc(path, f.p(0.56, 32), f.p(0.56, 28), 5 * f.sx, 5 * f.sy, false, true);
        path.lineTo(f.p(20.56, 5));
        path.closeSubpath();
        return path;
    }

    // Process
    if (key == QLatin1String("flowchart.process"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 100, 100);
        const double r = qMin(100 * f.sx, 100 * f.sy) * 6 / 100.0;
        path.addRoundedRect(QRectF(f.p(0, 0), QSizeF(100 * f.sx, 100 * f.sy)), r, r);
        return path;
    }

    // Sequential Data
    if (key == QLatin1String("flowchart.sequential_data"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 99, 99);
        path.addEllipse(QRectF(f.p(0, 0), QSizeF(99 * f.sx, 99 * f.sy)));
        return path;
    }

    // Sort
    if (key == QLatin1String("flowchart.sort"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 98, 98);
        path.moveTo(f.p(51, 1));
        path.lineTo(f.p(97, 47));
        addSvgArc(path, f.p(97, 47), f.p(97, 51), 2.5 * f.sx, 2.5 * f.sy, false, true);
        path.lineTo(f.p(51, 97));
        addSvgArc(path, f.p(51, 97), f.p(47, 97), 2.5 * f.sx, 2.5 * f.sy, false, true);
        path.lineTo(f.p(1, 51));
        addSvgArc(path, f.p(1, 51), f.p(1, 47), 2.5 * f.sx, 2.5 * f.sy, false, true);
        path.lineTo(f.p(47, 1));
        addSvgArc(path, f.p(47, 1), f.p(51, 1), 2.5 * f.sx, 2.5 * f.sy, false, true);
        path.closeSubpath();
        return path;
    }

    // Start 1
    if (key == QLatin1String("flowchart.start_1"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 99, 60);
        path.addEllipse(QRectF(f.p(0, 0), QSizeF(99 * f.sx, 60 * f.sy)));
        return path;
    }

    // Start 2
    if (key == QLatin1String("flowchart.start_2"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 99, 99);
        path.addEllipse(QRectF(f.p(0, 0), QSizeF(99 * f.sx, 99 * f.sy)));
        return path;
    }

    // Stored Data
    if (key == QLatin1String("flowchart.stored_data"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 96.51, 60);
        path.moveTo(f.p(10, 0));
        path.lineTo(f.p(96, 0));
        addSvgArc(path, f.p(96, 0), f.p(96, 2), 1.5 * f.sx, 1.5 * f.sy, false, true);
        addSvgArc(path, f.p(96, 2), f.p(96, 58), 10 * f.sx, 30 * f.sy, false, false);
        addSvgArc(path, f.p(96, 58), f.p(96, 60), 1.5 * f.sx, 1.5 * f.sy, false, true);
        path.lineTo(f.p(10, 60));
        addSvgArc(path, f.p(10, 60), f.p(10, 0), 10 * f.sx, 30 * f.sy, false, true);
        path.closeSubpath();
        return path;
    }

    // Summing Function
    if (key == QLatin1String("flowchart.summing_function"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 70, 70);
        path.addEllipse(QRectF(f.p(0, 0), QSizeF(70 * f.sx, 70 * f.sy)));
        return path;
    }

    // Summing Junction
    if (key == QLatin1String("flowchart.summing_junction"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 70, 70);
        path.addEllipse(QRectF(f.p(0, 0), QSizeF(70 * f.sx, 70 * f.sy)));
        return path;
    }

    // Terminator
    if (key == QLatin1String("flowchart.terminator"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 98, 60);
        path.moveTo(f.p(30, 0));
        path.lineTo(f.p(68, 0));
        addSvgArc(path, f.p(68, 0), f.p(68, 60), 30 * f.sx, 30 * f.sy, false, true);
        path.lineTo(f.p(30, 60));
        addSvgArc(path, f.p(30, 60), f.p(30, 0), 30 * f.sx, 30 * f.sy, false, true);
        path.closeSubpath();
        return path;
    }

    // Transfer
    if (key == QLatin1String("flowchart.transfer"))
    {
        const StencilFrame f = stencilFrame(x, y, w, h, 97.5, 70);
        path.moveTo(f.p(0, 20));
        path.lineTo(f.p(59, 20));
        path.lineTo(f.p(59, 0));
        path.lineTo(f.p(97.5, 35));
        path.lineTo(f.p(59, 70));
        path.lineTo(f.p(59, 50));
        path.lineTo(f.p(0, 50));
        path.closeSubpath();
        return path;
    }

    // Registered (non-stencil) flowchart shapes: shapes/mxFlowchart.js and
    // js/grapheditor/Shapes.js. These are mxActor/mxRectangleShape ports, so
    // their corners are rounded through mxShape.addPoints() when the style
    // sets rounded=1.
    //
    // draw.io registers the stencil and the plain shape under different
    // names (mxgraph.flowchart.card vs. card), which is why the stencil
    // branches above carry the "flowchart." prefix.

    // mxShapeFlowchartDocument2.paintVertexShape (shapes/mxFlowchart.js)
    if (key == QLatin1String("flowchart.document2"))
    {
        const double dy = h * qBound(0.0,
            Style::number(probe.style, QStringLiteral("size"), 0.25), 1.0);
        const double fy = 1.4;
        const double r = 5;

        path.moveTo(point(w - r, 0));
        addSvgArc(path, point(w - r, 0), point(w, r), r, r, false, true);
        path.lineTo(point(w, h - dy / 2.0));
        path.quadTo(point(w * 3.0 / 4.0, h - dy * fy), point(w / 2.0, h - dy / 2.0));
        path.quadTo(point(w / 4.0, h - dy * (1 - fy)), point(0, h - dy / 2.0));
        path.lineTo(point(0, dy / 2.0));
        path.lineTo(point(0, r));
        addSvgArc(path, point(0, r), point(r, 0), r, r, false, true);
        path.closeSubpath();
        return path;
    }

    // CardShape.redrawPath
    if (key == QLatin1String("message"))
    {
        // MessageShape extends mxCylinder: a plain rectangle body; the
        // envelope fold is stroked by ShapeRenderer.
        path.addRect(rect);
        return path;
    }

    if (key == QLatin1String("card"))
    {
        const double s = qBound(0.0,
            Style::number(probe.style, QStringLiteral("size"), 30.0), qMin(w, h));

        addPoints(path, {point(s, 0), point(w, 0), point(w, h), point(0, h), point(0, s)},
                  probe.styleFlag(QStringLiteral("rounded")), pointArcSizeFor(probe.style),
                  true);
        return path;
    }

    // ManualInputShape.redrawPath
    if (key == QLatin1String("manualInput"))
    {
        const double s = qMin(h, Style::number(probe.style, QStringLiteral("size"), 30.0));

        addPoints(path, {point(0, h), point(0, s), point(w, 0), point(w, h)},
                  probe.styleFlag(QStringLiteral("rounded")), pointArcSizeFor(probe.style),
                  true);
        return path;
    }

    // OffPageConnectorShape.redrawPath
    if (key == QLatin1String("offPageConnector"))
    {
        const double s = h * qBound(0.0,
            Style::number(probe.style, QStringLiteral("size"), 3.0 / 8.0), 1.0);

        addPoints(path, {point(0, 0), point(w, 0), point(w, h - s), point(w / 2.0, h),
                         point(0, h - s)},
                  probe.styleFlag(QStringLiteral("rounded")), pointArcSizeFor(probe.style),
                  true);
        return path;
    }

    // --- Advanced palette (Sidebar-Advanced.js / Shapes.js) ---------------
    // Every shape below is a literal port of the draw.io implementation that
    // createAdvancedShapes() puts in the "advanced" palette. Interior lines,
    // the double outlines and the data store rings are painted as
    // decorations by ShapeRenderer.cpp so they cannot punch holes into the
    // filled outline.

    // ExtendedShape (shape=ext): mxRectangleShape plus a foreground frame
    // when double=1.
    if (key == QLatin1String("ext"))
    {
        if (probe.styleFlag(QStringLiteral("rounded")))
        {
            const double r = cornerRadiusFor(probe.style, w, h);
            path.addRoundedRect(rect, r, r);
        }
        else
        {
            path.addRect(rect);
        }

        return path;
    }

    // LoopLimitShape.redrawPath
    if (key == QLatin1String("loopLimit"))
    {
        const double s = qMin(w / 2.0,
            qMin(h, Style::number(probe.style, QStringLiteral("size"), 20.0)));

        addPoints(path, {point(s, 0), point(w - s, 0), point(w, s * 0.8),
                         point(w, h), point(0, h), point(0, s * 0.8)},
                  probe.styleFlag(QStringLiteral("rounded")), pointArcSizeFor(probe.style),
                  true);
        return path;
    }

    // TapeDataShape, OrEllipseShape, SumEllipseShape, LineEllipseShape all
    // extend mxEllipse; only their foreground lines differ.
    if (key == QLatin1String("tapeData") || key == QLatin1String("orEllipse")
        || key == QLatin1String("sumEllipse") || key == QLatin1String("lineEllipse"))
    {
        path.addEllipse(rect);
        return path;
    }

    // SortShape extends mxRhombus.
    if (key == QLatin1String("sortShape"))
    {
        path.moveTo(point(w / 2.0, 0));
        path.lineTo(point(w, h / 2.0));
        path.lineTo(point(w / 2.0, h));
        path.lineTo(point(0, h / 2.0));
        path.closeSubpath();
        return path;
    }

    // CollateShape: two filled and stroked triangles meeting in the middle.
    if (key == QLatin1String("collate"))
    {
        path.moveTo(point(0, 0));
        path.lineTo(point(w, 0));
        path.lineTo(point(w / 2.0, h / 2.0));
        path.closeSubpath();
        path.moveTo(point(0, h));
        path.lineTo(point(w, h));
        path.lineTo(point(w / 2.0, h / 2.0));
        path.closeSubpath();
        return path;
    }

    // DelayShape.redrawPath
    if (key == QLatin1String("delay"))
    {
        const double dx = qMin(w, h / 2.0);

        path.moveTo(point(0, 0));
        path.lineTo(point(w - dx, 0));
        path.quadTo(point(w, 0), point(w, h / 2.0));
        path.quadTo(point(w, h), point(w - dx, h));
        path.lineTo(point(0, h));
        path.closeSubpath();
        return path;
    }

    // DisplayShape.redrawPath
    if (key == QLatin1String("display"))
    {
        const double dx = qMin(w, h / 2.0);
        const double s = qMin(w - dx,
            qMax(0.0, Style::number(probe.style, QStringLiteral("size"), 0.25)) * w);

        path.moveTo(point(0, h / 2.0));
        path.lineTo(point(s, 0));
        path.lineTo(point(w - dx, 0));
        path.quadTo(point(w, 0), point(w, h / 2.0));
        path.quadTo(point(w, h), point(w - dx, h));
        path.lineTo(point(s, h));
        path.closeSubpath();
        return path;
    }

    // CrossShape.redrawPath
    if (key == QLatin1String("cross"))
    {
        const double m = qMin(h, w);
        const double size = qMax(0.0, qMin(m,
            m * Style::number(probe.style, QStringLiteral("size"), 0.2)));
        const double t = (h - size) / 2.0;
        const double b = t + size;
        const double l = (w - size) / 2.0;
        const double r = l + size;

        path.moveTo(point(0, t));
        path.lineTo(point(l, t));
        path.lineTo(point(l, 0));
        path.lineTo(point(r, 0));
        path.lineTo(point(r, t));
        path.lineTo(point(w, t));
        path.lineTo(point(w, b));
        path.lineTo(point(r, b));
        path.lineTo(point(r, h));
        path.lineTo(point(l, h));
        path.lineTo(point(l, b));
        path.lineTo(point(0, b));
        path.closeSubpath();
        return path;
    }

    // CornerShape.redrawPath
    if (key == QLatin1String("corner"))
    {
        const double dx = qBound(0.0, Style::number(probe.style, QStringLiteral("dx"), 20.0), w);
        const double dy = qBound(0.0, Style::number(probe.style, QStringLiteral("dy"), 20.0), h);

        addPoints(path, {point(0, 0), point(w, 0), point(w, dy), point(dx, dy),
                         point(dx, h), point(0, h)},
                  probe.styleFlag(QStringLiteral("rounded")), pointArcSizeFor(probe.style),
                  true);
        return path;
    }

    // TeeShape.redrawPath
    if (key == QLatin1String("tee"))
    {
        const double dx = qBound(0.0, Style::number(probe.style, QStringLiteral("dx"), 20.0), w);
        const double dy = qBound(0.0, Style::number(probe.style, QStringLiteral("dy"), 20.0), h);

        addPoints(path, {point(0, 0), point(w, 0), point(w, dy), point((w + dx) / 2.0, dy),
                         point((w + dx) / 2.0, h), point((w - dx) / 2.0, h),
                         point((w - dx) / 2.0, dy), point(0, dy)},
                  probe.styleFlag(QStringLiteral("rounded")), pointArcSizeFor(probe.style),
                  true);
        return path;
    }

    // DataStoreShape.redrawPath (the body only; the three rings are stroked
    // on top by ShapeRenderer).
    if (key == QLatin1String("datastore"))
    {
        const double strokeWidth = Style::number(probe.style,
            QStringLiteral("strokeWidth"), 1.0);
        const double dy = qMin(h / 2.0,
            std::round(h / 8.0) + strokeWidth - 1.0);

        path.moveTo(point(0, dy));
        path.cubicTo(point(0, -dy / 3.0), point(w, -dy / 3.0), point(w, dy));
        path.lineTo(point(w, h - dy));
        path.cubicTo(point(w, h + dy / 3.0), point(0, h + dy / 3.0), point(0, h - dy));
        path.closeSubpath();
        return path;
    }

    // SwitchShape.redrawPath
    if (key == QLatin1String("switch"))
    {
        path.moveTo(point(0, 0));
        path.quadTo(point(w / 2.0, h / 2.0), point(w, 0));
        path.quadTo(point(w / 2.0, h / 2.0), point(w, h));
        path.quadTo(point(w / 2.0, h / 2.0), point(0, h));
        path.quadTo(point(w / 2.0, h / 2.0), point(0, 0));
        return path;
    }

    // SingleArrowShape.redrawPath and DoubleArrowShape.redrawPath. draw.io
    // rotates the whole shape for direction=north/west/south (mxShape.
    // getShapeRotation), which ShapeRenderer::paint applies for every shape.
    if (key == QLatin1String("singleArrow") || key == QLatin1String("doubleArrow"))
    {
        const bool doubleHeaded = key == QLatin1String("doubleArrow");
        const double aw = h * qBound(0.0,
            Style::number(probe.style, QStringLiteral("arrowWidth"), 0.3), 1.0);
        const double as = w * qBound(0.0,
            Style::number(probe.style, QStringLiteral("arrowSize"), 0.2), 1.0);
        const double at = (h - aw) / 2.0;
        const double ab = at + aw;
        const bool rounded = probe.styleFlag(QStringLiteral("rounded"));
        const double arcSize = pointArcSizeFor(probe.style);

        if (doubleHeaded)
        {
            addPoints(path, {point(0, h / 2.0), point(as, 0), point(as, at),
                             point(w - as, at), point(w - as, 0), point(w, h / 2.0),
                             point(w - as, h), point(w - as, ab), point(as, ab),
                             point(as, h)},
                      rounded, arcSize, true);
        }
        else
        {
            addPoints(path, {point(0, at), point(w - as, at), point(w - as, 0),
                             point(w, h / 2.0), point(w - as, h), point(w - as, ab),
                             point(0, ab)},
                      rounded, arcSize, true);
        }

        return path;
    }

    // --- UML -------------------------------------------------------------
    if (key == QLatin1String("mxgraph.uml.package"))
    {
        path.moveTo(point(0, h * 0.22));
        path.lineTo(point(0, 0));
        path.lineTo(point(w * 0.4, 0));
        path.lineTo(point(w * 0.46, h * 0.22));
        path.lineTo(point(w, h * 0.22));
        path.lineTo(point(w, h));
        path.lineTo(point(0, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mxgraph.uml.component"))
    {
        path.addRect(QRectF(x + w * 0.12, y, w * 0.88, h));
        path.addRect(QRectF(x, y + h * 0.16, w * 0.22, h * 0.16));
        path.addRect(QRectF(x, y + h * 0.46, w * 0.22, h * 0.16));
        return path;
    }

    // --- ER ---------------------------------------------------------------
    if (key == QLatin1String("mxgraph.er.weak_entity"))
    {
        path.addRect(rect);
        path.addRect(rect.adjusted(4, 4, -4, -4));
        return path;
    }

    // --- BPMN -------------------------------------------------------------
    if (key == QLatin1String("mxgraph.bpmn.data_store"))
    {
        path.moveTo(point(0, h * 0.15));
        path.quadTo(point(w / 2.0, 0), point(w, h * 0.15));
        path.lineTo(point(w, h));
        path.lineTo(point(0, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mxgraph.bpmn.message"))
    {
        path.addRect(rect);
        return path;
    }

    // --- network ----------------------------------------------------------
    if (key == QLatin1String("mxgraph.networks.firewall"))
    {
        path.addRect(rect);
        return path;
    }

    if (key == QLatin1String("mxgraph.networks.storage"))
    {
        const double cap = qMin(h * 0.22, 18.0);
        path.moveTo(point(0, cap / 2.0));
        path.cubicTo(point(0, 0), point(w, 0), point(w, cap / 2.0));
        path.lineTo(point(w, h - cap / 2.0));
        path.cubicTo(point(w, h), point(0, h), point(0, h - cap / 2.0));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mxgraph.networks.laptop"))
    {
        path.moveTo(point(w * 0.08, 0));
        path.lineTo(point(w * 0.92, 0));
        path.lineTo(point(w * 0.92, h * 0.75));
        path.lineTo(point(w * 0.08, h * 0.75));
        path.closeSubpath();
        path.moveTo(point(0, h * 0.75));
        path.lineTo(point(w, h * 0.75));
        path.lineTo(point(w * 0.9, h));
        path.lineTo(point(w * 0.1, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mxgraph.networks.printer"))
    {
        path.addRect(QRectF(x + w * 0.2, y, w * 0.6, h * 0.3));
        path.addRect(QRectF(x, y + h * 0.3, w, h * 0.45));
        path.addRect(QRectF(x + w * 0.2, y + h * 0.75, w * 0.6, h * 0.25));
        return path;
    }

    if (key == QLatin1String("mxgraph.networks.vpn"))
    {
        path.moveTo(point(w / 2.0, 0));
        path.lineTo(point(w, h * 0.15));
        path.lineTo(point(w, h * 0.55));
        path.quadTo(point(w, h * 0.85), point(w / 2.0, h));
        path.quadTo(point(0, h * 0.85), point(0, h * 0.55));
        path.lineTo(point(0, h * 0.15));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mxgraph.networks.users"))
    {
        path.addEllipse(QRectF(x + w * 0.08, y + h * 0.1, w * 0.3, h * 0.3));
        path.addEllipse(QRectF(x + w * 0.55, y + h * 0.1, w * 0.3, h * 0.3));
        path.addRoundedRect(QRectF(x + w * 0.02, y + h * 0.45, w * 0.42, h * 0.45), 10, 10);
        path.addRoundedRect(QRectF(x + w * 0.5, y + h * 0.45, w * 0.42, h * 0.45), 10, 10);
        return path;
    }

    // --- data flow --------------------------------------------------------
    if (key == QLatin1String("mxgraph.dfd.data_store"))
    {
        path.moveTo(point(0, 0));
        path.lineTo(point(w, 0));
        path.moveTo(point(0, h));
        path.lineTo(point(w, h));
        path.moveTo(point(0, 0));
        path.lineTo(point(0, h));
        path.moveTo(point(w, 0));
        path.lineTo(point(w, h));
        return path;
    }

    // --- electrical symbols ----------------------------------------------
    if (key.startsWith(QLatin1String("elec.")))
    {
        const double cy = h / 2.0;
        const double cx = w / 2.0;

        auto lead = [&](double fromX, double toX)
        {
            path.moveTo(point(fromX, cy));
            path.lineTo(point(toX, cy));
        };

        if (key == QLatin1String("elec.resistor"))
        {
            lead(0, w * 0.22);
            path.addRect(QRectF(x + w * 0.22, y + h * 0.28, w * 0.56, h * 0.44));
            path.moveTo(point(w * 0.78, cy));
            path.lineTo(point(w, cy));
        }
        else if (key == QLatin1String("elec.resistorus"))
        {
            lead(0, w * 0.18);
            const QPointF points[] = {
                QPointF(w * 0.18, cy), QPointF(w * 0.26, h * 0.2),
                QPointF(w * 0.34, h * 0.8), QPointF(w * 0.42, h * 0.2),
                QPointF(w * 0.5, h * 0.8), QPointF(w * 0.58, h * 0.2),
                QPointF(w * 0.66, h * 0.8), QPointF(w * 0.74, h * 0.2),
                QPointF(w * 0.82, cy)
            };
            path.moveTo(point(points[0].x(), points[0].y()));

            for (int i = 1; i < 9; ++i)
            {
                path.lineTo(point(points[i].x(), points[i].y()));
            }

            path.lineTo(point(w, cy));
        }
        else if (key == QLatin1String("elec.capacitor")
                 || key == QLatin1String("elec.capacitorpolar"))
        {
            lead(0, w * 0.42);
            path.moveTo(point(w * 0.42, h * 0.18));
            path.lineTo(point(w * 0.42, h * 0.82));
            path.moveTo(point(w * 0.58, h * 0.18));
            path.lineTo(point(w * 0.58, h * 0.82));
            path.moveTo(point(w * 0.58, cy));
            path.lineTo(point(w, cy));
        }
        else if (key == QLatin1String("elec.inductor"))
        {
            lead(0, w * 0.15);
            const double bump = w * 0.14;

            for (int i = 0; i < 4; ++i)
            {
                path.arcTo(QRectF(x + w * 0.15 + i * bump, y + cy - bump / 2.0,
                                  bump, bump),
                           180, -180);
            }

            path.moveTo(point(w * 0.71, cy));
            path.lineTo(point(w, cy));
        }
        else if (key == QLatin1String("elec.diode") || key == QLatin1String("elec.zener")
                 || key == QLatin1String("elec.led"))
        {
            lead(0, w * 0.32);
            path.moveTo(point(w * 0.32, h * 0.18));
            path.lineTo(point(w * 0.32, h * 0.82));
            path.lineTo(point(w * 0.68, cy));
            path.closeSubpath();
            path.moveTo(point(w * 0.68, h * 0.18));
            path.lineTo(point(w * 0.68, h * 0.82));

            if (key == QLatin1String("elec.zener"))
            {
                path.moveTo(point(w * 0.68, h * 0.18));
                path.lineTo(point(w * 0.78, h * 0.18));
                path.moveTo(point(w * 0.68, h * 0.82));
                path.lineTo(point(w * 0.58, h * 0.82));
            }

            path.moveTo(point(w * 0.68, cy));
            path.lineTo(point(w, cy));

            if (key == QLatin1String("elec.led"))
            {
                for (int i = 0; i < 2; ++i)
                {
                    const double offset = i * w * 0.1;
                    path.moveTo(point(w * 0.36 + offset, h * 0.12));
                    path.lineTo(point(w * 0.46 + offset, h * 0.02));
                    path.moveTo(point(w * 0.44 + offset, h * 0.12));
                    path.lineTo(point(w * 0.54 + offset, h * 0.02));
                }
            }
        }
        else if (key == QLatin1String("elec.transistornpn")
                 || key == QLatin1String("elec.transistorpnp"))
        {
            const double radius = qMin(w, h) * 0.42;
            path.addEllipse(QPointF(cx, cy), radius, radius);
            path.moveTo(point(cx - radius * 0.6, cy - radius * 0.35));
            path.lineTo(point(cx - radius * 0.6, cy + radius * 0.35));
            path.moveTo(point(cx - radius * 0.6, cy - radius * 0.2));
            path.lineTo(point(cx + radius * 0.35, cy - radius * 0.65));
            path.moveTo(point(cx - radius * 0.6, cy + radius * 0.2));
            path.lineTo(point(cx + radius * 0.35, cy + radius * 0.65));

            const bool npn = key == QLatin1String("elec.transistornpn");
            const QPointF arrowTip(cx + radius * 0.05, cy + (npn ? radius * 0.42 : radius * 0.02));
            path.moveTo(arrowTip);
            path.lineTo(arrowTip + QPointF(-radius * 0.18, -radius * 0.16));
            path.moveTo(arrowTip);
            path.lineTo(arrowTip + QPointF(radius * 0.18, -radius * 0.16));
        }
        else if (key == QLatin1String("elec.battery"))
        {
            lead(0, w * 0.4);
            path.moveTo(point(w * 0.4, h * 0.2));
            path.lineTo(point(w * 0.4, h * 0.8));
            path.moveTo(point(w * 0.47, h * 0.34));
            path.lineTo(point(w * 0.47, h * 0.66));
            path.moveTo(point(w * 0.54, h * 0.2));
            path.lineTo(point(w * 0.54, h * 0.8));
            path.moveTo(point(w * 0.61, h * 0.34));
            path.lineTo(point(w * 0.61, h * 0.66));
            path.moveTo(point(w * 0.61, cy));
            path.lineTo(point(w, cy));
        }
        else if (key == QLatin1String("elec.ground")
                 || key == QLatin1String("elec.groundchassis"))
        {
            path.moveTo(point(cx, 0));
            path.lineTo(point(cx, h * 0.42));
            path.moveTo(point(w * 0.2, h * 0.42));
            path.lineTo(point(w * 0.8, h * 0.42));
            path.moveTo(point(w * 0.3, h * 0.62));
            path.lineTo(point(w * 0.7, h * 0.62));
            path.moveTo(point(w * 0.4, h * 0.82));
            path.lineTo(point(w * 0.6, h * 0.82));
        }
        else if (key == QLatin1String("elec.lamp"))
        {
            const double radius = qMin(w, h) * 0.4;
            path.addEllipse(QPointF(cx, cy), radius, radius);
            path.moveTo(point(cx - radius * 0.6, cy - radius * 0.6));
            path.lineTo(point(cx + radius * 0.6, cy + radius * 0.6));
            path.moveTo(point(cx + radius * 0.6, cy - radius * 0.6));
            path.lineTo(point(cx - radius * 0.6, cy + radius * 0.6));
        }
        else if (key == QLatin1String("elec.motor"))
        {
            const double radius = qMin(w, h) * 0.42;
            path.addEllipse(QPointF(cx, cy), radius, radius);
        }
        else if (key == QLatin1String("elec.switchspst"))
        {
            lead(0, w * 0.3);
            path.moveTo(point(w * 0.3, cy));
            path.lineTo(point(w * 0.7, h * 0.2));
            path.moveTo(point(w * 0.7, cy));
            path.lineTo(point(w, cy));
            path.addEllipse(QPointF(w * 0.3, cy), 3, 3);
            path.addEllipse(QPointF(w * 0.7, cy), 3, 3);
        }
        else if (key == QLatin1String("elec.pushbutton"))
        {
            lead(0, w * 0.35);
            lead(w * 0.65, w);
            path.moveTo(point(w * 0.35, h * 0.25));
            path.lineTo(point(w * 0.35, h * 0.75));
            path.moveTo(point(w * 0.65, h * 0.25));
            path.lineTo(point(w * 0.65, h * 0.75));
            path.moveTo(point(w * 0.25, h * 0.25));
            path.lineTo(point(w * 0.75, h * 0.25));
            path.moveTo(point(cx, h * 0.25));
            path.lineTo(point(cx, 0));
        }
        else if (key == QLatin1String("elec.transformer"))
        {
            const double coilWidth = w * 0.16;
            path.addEllipse(QPointF(w * 0.3, cy), coilWidth, h * 0.3);
            path.addEllipse(QPointF(w * 0.7, cy), coilWidth, h * 0.3);
            path.moveTo(point(w * 0.45, h * 0.15));
            path.lineTo(point(w * 0.45, h * 0.85));
            path.moveTo(point(w * 0.5, h * 0.15));
            path.lineTo(point(w * 0.5, h * 0.85));
        }
        else if (key == QLatin1String("elec.fuse"))
        {
            lead(0, w * 0.25);
            path.addRect(QRectF(x + w * 0.25, y + h * 0.32, w * 0.5, h * 0.36));
            path.moveTo(point(w * 0.25, cy));
            path.lineTo(point(w * 0.75, cy));
            path.moveTo(point(w * 0.75, cy));
            path.lineTo(point(w, cy));
        }
        else if (key == QLatin1String("elec.acsource"))
        {
            const double radius = qMin(w, h) * 0.4;
            path.addEllipse(QPointF(cx, cy), radius, radius);
            path.moveTo(point(cx - radius * 0.7, cy));

            for (int i = 0; i < 3; ++i)
            {
                const double startX = cx - radius * 0.7 + i * radius * 0.47;
                path.quadTo(QPointF(startX + radius * 0.12, cy - radius * 0.35),
                            QPointF(startX + radius * 0.24, cy));
                path.quadTo(QPointF(startX + radius * 0.35, cy + radius * 0.35),
                            QPointF(startX + radius * 0.47, cy));
            }
        }
        else if (key == QLatin1String("elec.voltagesource"))
        {
            const double radius = qMin(w, h) * 0.4;
            path.addEllipse(QPointF(cx, cy), radius, radius);

            // Plus sign in the upper half, minus sign in the lower half.
            path.moveTo(point(cx - radius * 0.28, cy - radius * 0.35));
            path.lineTo(point(cx + radius * 0.28, cy - radius * 0.35));
            path.moveTo(point(cx, cy - radius * 0.35 - radius * 0.22));
            path.lineTo(point(cx, cy - radius * 0.35 + radius * 0.22));
            path.moveTo(point(cx - radius * 0.28, cy + radius * 0.4));
            path.lineTo(point(cx + radius * 0.28, cy + radius * 0.4));
        }
        else if (key == QLatin1String("elec.ammeter")
                 || key == QLatin1String("elec.voltmeter"))
        {
            const double radius = qMin(w, h) * 0.4;
            path.addEllipse(QPointF(cx, cy), radius, radius);
        }
        else if (key == QLatin1String("elec.antenna"))
        {
            path.moveTo(point(cx, h));
            path.lineTo(point(cx, h * 0.3));
            path.moveTo(point(cx, h * 0.3));
            path.lineTo(point(w * 0.2, 0));
            path.moveTo(point(cx, h * 0.3));
            path.lineTo(point(w * 0.8, 0));
            path.moveTo(point(w * 0.32, h * 0.12));
            path.lineTo(point(w * 0.68, h * 0.12));
        }
        else if (key == QLatin1String("elec.speaker"))
        {
            path.addRect(QRectF(x, y + h * 0.32, w * 0.3, h * 0.36));
            path.moveTo(point(w * 0.3, h * 0.32));
            path.lineTo(point(w * 0.6, h * 0.1));
            path.lineTo(point(w * 0.6, h * 0.9));
            path.lineTo(point(w * 0.3, h * 0.68));
            path.closeSubpath();

            for (int i = 0; i < 2; ++i)
            {
                const double radius = w * (0.14 + i * 0.1);
                path.arcTo(QRectF(x + w * 0.58, cy - radius, radius * 2, radius * 2),
                           -60, 120);
            }
        }
        else if (key == QLatin1String("elec.microphone"))
        {
            path.addRoundedRect(QRectF(x + w * 0.34, y + h * 0.05, w * 0.32, h * 0.4),
                                w * 0.16, w * 0.16);
            path.moveTo(point(w * 0.2, h * 0.45));
            path.arcTo(QRectF(x + w * 0.2, y + h * 0.3, w * 0.6, h * 0.4), 180, -180);
            path.moveTo(point(cx, h * 0.7));
            path.lineTo(point(cx, h * 0.9));
            path.moveTo(point(w * 0.3, h * 0.9));
            path.lineTo(point(w * 0.7, h * 0.9));
        }
        else if (key == QLatin1String("elec.opamp"))
        {
            path.moveTo(point(0, 0));
            path.lineTo(point(w, cy));
            path.lineTo(point(0, h));
            path.closeSubpath();
        }
        else if (key == QLatin1String("elec.gate.and"))
        {
            path.moveTo(point(0, 0));
            path.lineTo(point(w * 0.5, 0));
            path.arcTo(QRectF(x + w * 0.5 - h / 2.0, y, h, h), 90, -180);
            path.lineTo(point(0, h));
            path.closeSubpath();
        }
        else if (key == QLatin1String("elec.gate.or") || key == QLatin1String("elec.gate.nor")
                 || key == QLatin1String("elec.gate.xor"))
        {
            path.moveTo(point(0, 0));
            path.quadTo(point(w * 0.45, h * 0.1), point(w * 0.62, cy));
            path.quadTo(point(w * 0.45, h * 0.9), point(0, h));
            path.quadTo(point(w * 0.18, cy), point(0, 0));
            path.closeSubpath();

            if (key == QLatin1String("elec.gate.xor"))
            {
                path.moveTo(point(w * 0.08, 0));
                path.quadTo(point(w * 0.26, cy), point(w * 0.08, h));
            }

            if (key == QLatin1String("elec.gate.nor"))
            {
                path.addEllipse(QPointF(w * 0.66, cy), h * 0.08, h * 0.08);
            }
        }
        else if (key == QLatin1String("elec.gate.not"))
        {
            path.moveTo(point(0, 0));
            path.lineTo(point(w * 0.72, cy));
            path.lineTo(point(0, h));
            path.closeSubpath();
            path.addEllipse(QPointF(w * 0.8, cy), h * 0.08, h * 0.08);
        }
        else if (key == QLatin1String("elec.gate.nand"))
        {
            path.moveTo(point(0, 0));
            path.lineTo(point(w * 0.45, 0));
            path.arcTo(QRectF(x + w * 0.45 - h / 2.0, y, h, h), 90, -180);
            path.lineTo(point(0, h));
            path.closeSubpath();
            path.addEllipse(QPointF(w * 0.78, cy), h * 0.08, h * 0.08);
        }
        else if (key == QLatin1String("elec.junction"))
        {
            path.addEllipse(rect);
        }

        return path;
    }

    // --- floor plan -------------------------------------------------------
    if (key == QLatin1String("floor.door"))
    {
        path.moveTo(point(0, h));
        path.arcTo(QRectF(x, y, w * 2, h * 2), 90, 90);
        path.moveTo(point(0, h - 2));
        path.lineTo(point(w, h - 2));
        return path;
    }

    if (key == QLatin1String("floor.window"))
    {
        path.addRect(rect);
        path.moveTo(point(0, h * 0.35));
        path.lineTo(point(w, h * 0.35));
        path.moveTo(point(0, h * 0.65));
        path.lineTo(point(w, h * 0.65));
        return path;
    }

    if (key == QLatin1String("floor.stairs"))
    {
        path.addRect(rect);
        const int steps = 8;

        for (int i = 1; i < steps; ++i)
        {
            path.moveTo(point(0, h * i / steps));
            path.lineTo(point(w, h * i / steps));
        }

        return path;
    }

    if (key == QLatin1String("info.ribbon"))
    {
        path.moveTo(point(0, h * 0.2));
        path.lineTo(point(w, h * 0.2));
        path.lineTo(point(w * 0.9, h * 0.5));
        path.lineTo(point(w, h * 0.8));
        path.lineTo(point(0, h * 0.8));
        path.lineTo(point(w * 0.1, h * 0.5));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("eip.router"))
    {
        path.moveTo(point(0, h * 0.35));
        path.lineTo(point(w * 0.6, h * 0.35));
        path.lineTo(point(w * 0.6, h * 0.1));
        path.lineTo(point(w, h * 0.5));
        path.lineTo(point(w * 0.6, h * 0.9));
        path.lineTo(point(w * 0.6, h * 0.65));
        path.lineTo(point(0, h * 0.65));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("eip.splitter"))
    {
        path.moveTo(point(0, h * 0.35));
        path.lineTo(point(w * 0.55, h * 0.35));
        path.lineTo(point(w, 0));
        path.lineTo(point(w, h * 0.3));
        path.lineTo(point(w * 0.75, h * 0.5));
        path.lineTo(point(w, h * 0.7));
        path.lineTo(point(w, h));
        path.lineTo(point(w * 0.55, h * 0.65));
        path.lineTo(point(0, h * 0.65));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("ios.phone"))
    {
        path.addRoundedRect(rect, qMin(w, h) * 0.12, qMin(w, h) * 0.12);
        path.addRoundedRect(QRectF(x + w * 0.08, y + h * 0.1, w * 0.84, h * 0.8),
                            qMin(w, h) * 0.06, qMin(w, h) * 0.06);
        return path;
    }

    // --- draw.io electrical library aliases ------------------------------
    if (key == QLatin1String("waypoint"))
    {
        // draw.io's waypoint is a small filled junction dot whose diameter is
        // the "size" style value, not the full cell bounds.
        const double diameter = qBound(2.0, Style::number(style, QStringLiteral("size"), 6.0), 40.0);
        path.addEllipse(rect.center(), diameter / 2.0, diameter / 2.0);
        return path;
    }

    if (key == QLatin1String("mxgraph.electrical.resistors.resistor_1"))
    {
        // IEC resistor: a rectangle with two leads.
        const double cy = h / 2.0;
        path.moveTo(point(0, cy));
        path.lineTo(point(w * 0.2, cy));
        path.addRect(QRectF(x + w * 0.2, y + h * 0.28, w * 0.6, h * 0.44));
        path.moveTo(point(w * 0.8, cy));
        path.lineTo(point(w, cy));
        return path;
    }

    if (key == QLatin1String("mxgraph.electrical.inductors.inductor"))
    {
        const double cy = h / 2.0;
        const double lead = w * 0.04;
        const double length = w * 0.92;
        const double spacing = length / 4.0;
        const double bulge = spacing * 1.35;

        path.moveTo(point(0, cy));
        path.lineTo(point(lead, cy));

        for (int i = 0; i < 4; ++i)
        {
            const double x0 = x + lead + i * spacing;
            path.moveTo(QPointF(x0, y + cy));
            path.cubicTo(QPointF(x0 + spacing * 0.25, y + cy - bulge),
                         QPointF(x0 + spacing * 0.75, y + cy - bulge),
                         QPointF(x0 + spacing, y + cy));
        }

        path.moveTo(point(w * 0.96, cy));
        path.lineTo(point(w, cy));

        // Inlet/outlet stubs on opposite sides of the coil. After the ±90°
        // rotation they become the short horizontal leads at the top and
        // bottom of the coil, matching draw.io's inductor symbol.
        path.moveTo(point(0, cy));
        path.lineTo(point(0, h));
        path.moveTo(point(w, cy));
        path.lineTo(point(w, h));
        return path;
    }

    if (key == QLatin1String("mxgraph.electrical.miscellaneous.batterystack"))
    {
        const double cy = h / 2.0;
        path.moveTo(point(0, cy));
        path.lineTo(point(w * 0.12, cy));

        for (int i = 0; i < 4; ++i)
        {
            const double plateX = w * (0.14 + i * 0.18);
            path.moveTo(point(plateX, h * 0.22));
            path.lineTo(point(plateX, h * 0.78));
            path.moveTo(point(plateX + w * 0.08, h * 0.35));
            path.lineTo(point(plateX + w * 0.08, h * 0.65));
        }

        path.moveTo(point(w * 0.8, cy));
        path.lineTo(point(w, cy));
        return path;
    }

    if (key == QLatin1String("text") || key == QLatin1String("image"))
    {
        path.addRect(rect);
        return path;
    }

    if (key == QLatin1String("gitTag"))
    {
        // GitTagShape.redrawPath: paper tag with a pointed left tab.
        const double tabSize = qBound(0.0,
            Style::number(probe.style, QStringLiteral("tabSize"), 8.0), w);
        const double tabInset = qBound(0.0,
            Style::number(probe.style, QStringLiteral("tabInset"), 4.0), h);
        const double tabY1 = (h - tabInset) / 2.0;
        const double tabY2 = tabY1 + tabInset;

        path.moveTo(point(0, tabY1));
        path.lineTo(point(0, tabY2));
        path.lineTo(point(tabSize, h));
        path.lineTo(point(w, h));
        path.lineTo(point(w, 0));
        path.lineTo(point(tabSize, 0));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mindmapBang"))
    {
        // MindmapBangShape.redrawPath: mermaid's bang silhouette.
        const double W = w * 0.8;
        const double H = h * 0.8;
        const double r = W * 0.15;
        const double r80 = r * 0.8;
        double px = W * 0.10;
        double py = H * 0.10;

        path.moveTo(point(px, py));
        px += W * 0.25; py += -H * 0.10;
        addSvgArc(path, point(px - W * 0.25, py + H * 0.10), point(px, py), r, r, false, false);
        px += W * 0.25;
        addSvgArc(path, point(px - W * 0.25, py), point(px, py), r, r, false, false);
        px += W * 0.25;
        addSvgArc(path, point(px - W * 0.25, py), point(px, py), r, r, false, false);
        px += W * 0.25; py += H * 0.10;
        addSvgArc(path, point(px - W * 0.25, py - H * 0.10), point(px, py), r, r, false, false);

        px += W * 0.15; py += H * 0.33;
        addSvgArc(path, point(px - W * 0.15, py - H * 0.33), point(px, py), r, r, false, false);
        py += H * 0.34;
        addSvgArc(path, point(px, py - H * 0.34), point(px, py), r80, r80, false, false);
        px += -W * 0.15; py += H * 0.33;
        addSvgArc(path, point(px + W * 0.15, py - H * 0.33), point(px, py), r, r, false, false);

        px += -W * 0.25; py += H * 0.15;
        addSvgArc(path, point(px + W * 0.25, py - H * 0.15), point(px, py), r, r, false, false);
        px += -W * 0.25;
        addSvgArc(path, point(px + W * 0.25, py), point(px, py), r, r, false, false);
        px += -W * 0.25;
        addSvgArc(path, point(px + W * 0.25, py), point(px, py), r, r, false, false);
        px += -W * 0.25; py += -H * 0.15;
        addSvgArc(path, point(px + W * 0.25, py + H * 0.15), point(px, py), r, r, false, false);

        px += -W * 0.10; py += -H * 0.33;
        addSvgArc(path, point(px + W * 0.10, py + H * 0.33), point(px, py), r, r, false, false);
        py += -H * 0.34;
        addSvgArc(path, point(px, py + H * 0.34), point(px, py), r80, r80, false, false);
        px += W * 0.10; py += -H * 0.33;
        addSvgArc(path, point(px - W * 0.10, py + H * 0.33), point(px, py), r, r, false, false);

        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mermaidPerson"))
    {
        // MermaidPersonShape: circular head over a rounded body.
        const double headRadius = qBound(16.0, w * 0.23, 56.0);
        const double bodyTop = qMin(h, 2.0 * headRadius - headRadius * 0.27);
        const double bodyHeight = h - bodyTop;
        const double bodyRadius = qMax(0.0,
            qMin(w * 0.177, bodyHeight * 0.45));

        path.addRoundedRect(QRectF(x, y + bodyTop, w, bodyHeight),
                            bodyRadius, bodyRadius);
        path.addEllipse(QRectF(x + w / 2.0 - headRadius, y,
                               headRadius * 2.0, headRadius * 2.0));
        path.setFillRule(Qt::WindingFill);
        return path;
    }

    if (key == QLatin1String("mermaidUsecaseActor"))
    {
        // MermaidUsecaseActorShape: the 56 x 72 mermaid figure.
        const QString type = Style::value(probe.style, QStringLiteral("actorType"),
                                          QStringLiteral("normal"));
        const bool hollow = type == QLatin1String("hollow");
        const bool awesome = type == QLatin1String("awesome");
        const double sx = w / 56.0;
        const double sy = h / 72.0;
        const double cx = x + w / 2.0;
        const double cy = y + h / 2.0;
        const auto px = [&](double value) { return cx + value * sx; };
        const auto py = [&](double value) { return cy + value * sy; };

        const auto circle = [&](QPainterPath &target, double ccy, double r)
        {
            const double k = r * 0.55228;
            target.moveTo(px(0), py(ccy + r));
            target.cubicTo(px(k), py(ccy + r), px(r), py(ccy + k), px(r), py(ccy));
            target.cubicTo(px(r), py(ccy - k), px(k), py(ccy - r), px(0), py(ccy - r));
            target.cubicTo(px(-k), py(ccy - r), px(-r), py(ccy - k), px(-r), py(ccy));
            target.cubicTo(px(-r), py(ccy + k), px(-k), py(ccy + r), px(0), py(ccy + r));
            target.closeSubpath();
        };

        if (hollow)
        {
            circle(path, -23, 9);
            path.moveTo(px(-22), py(-10));
            path.lineTo(px(22), py(-10));
            path.lineTo(px(22), py(0));
            path.lineTo(px(6), py(0));
            path.lineTo(px(22), py(17));
            path.lineTo(px(13), py(28));
            path.lineTo(px(0), py(13));
            path.lineTo(px(-13), py(28));
            path.lineTo(px(-22), py(17));
            path.lineTo(px(-6), py(0));
            path.lineTo(px(-22), py(0));
            path.closeSubpath();
            path.setFillRule(Qt::WindingFill);
            return path;
        }

        if (awesome)
        {
            circle(path, -21, 13);
            path.moveTo(px(-24), py(25));
            path.cubicTo(px(-24), py(7), px(-14), py(-3), px(0), py(-3));
            path.cubicTo(px(14), py(-3), px(24), py(7), px(24), py(25));
            path.cubicTo(px(24), py(28), px(21), py(30), px(18), py(30));
            path.lineTo(px(-18), py(30));
            path.cubicTo(px(-21), py(30), px(-24), py(28), px(-24), py(25));
            path.closeSubpath();
            path.setFillRule(Qt::WindingFill);
            return path;
        }

        circle(path, -24, 12);
        path.moveTo(px(0), py(-12));
        path.lineTo(px(0), py(8));
        path.moveTo(px(-17), py(-5));
        path.lineTo(px(17), py(-5));
        path.moveTo(px(0), py(8));
        path.lineTo(px(-15), py(28));
        path.moveTo(px(0), py(8));
        path.lineTo(px(15), py(28));
        path.setFillRule(Qt::WindingFill);
        return path;
    }

    if (key == QLatin1String("smileyFace"))
    {
        // SmileyFaceShape: the face circle; features are decorations.
        const double r = qMin(w, h) / 2.0;
        path.addEllipse(QRectF(x + w / 2.0 - r, y + h / 2.0 - r, r * 2.0, r * 2.0));
        return path;
    }

    // Default: rectangle, optionally rounded.
    if (key == QLatin1String("label"))
    {
        // mxLabel extends mxRectangleShape and the `label` named style is
        // rounded by default (styles/default.xml).
        if (Style::flag(Style::withDefaults(style), QStringLiteral("rounded"), true))
        {
            const double radius = cornerRadiusFor(Style::withDefaults(style), w, h);
            path.addRoundedRect(rect, radius, radius);
        }
        else
        {
            path.addRect(rect);
        }
        return path;
    }

    if (probe.styleFlag(QStringLiteral("rounded")) || key == QLatin1String("rounded"))
    {
        const double radius = cornerRadiusFor(style, w, h);
        path.addRoundedRect(rect, radius, radius);
        return path;
    }

    path.addRect(rect);
    return path;
}

QVector<ShapeDecoration> decorationsForStyle(const StyleMap &style, const QRectF &rect)
{
    QVector<ShapeDecoration> decorations;
    Cell probe(Cell::Kind::Vertex, QStringLiteral("probe"));
    probe.style = style;

    const QString key = shapeKey(probe);
    const double w = rect.width();
    const double h = rect.height();
    const double x = rect.x();
    const double y = rect.y();
    const auto point = [&](double px, double py) { return QPointF(x + px, y + py); };

    if (key.startsWith(QLatin1String("basic.")))
    {
        BasicGeometry geometry;
        buildBasicShape(key, style, rect, geometry);
        return geometry.decorations;
    }

    if (key == QLatin1String("gitTag"))
    {
        // GitTagShape.paintVertexShape: the pierce hole is filled with the
        // font (or holeColor) colour.
        const double holeSize = qMax(0.0,
            Style::number(style, QStringLiteral("holeSize"), 1.0));

        if (holeSize > 0.0)
        {
            const double tabSize = qMax(0.0,
                Style::number(style, QStringLiteral("tabSize"), 8.0));
            // draw.io resolves the default font colour (#000000) when the
            // style has no explicit holeColor/fontColor.
            const QColor color = Style::color(style, QStringLiteral("holeColor"),
                Style::color(style, QStringLiteral("fontColor"), QColor(Qt::black)));

            ShapeDecoration hole;
            hole.fillColor = color;
            hole.strokeColor = color;
            hole.strokeWidth = -1.0;
            hole.path.addEllipse(QRectF(point(tabSize / 2.0 - holeSize, h / 2.0 - holeSize),
                                        QSizeF(holeSize * 2.0, holeSize * 2.0)));
            decorations.append(hole);
        }

        return decorations;
    }

    if (key == QLatin1String("smileyFace"))
    {
        // Eyes and mouth of the mermaid journey face.
        const QString type = Style::value(style, QStringLiteral("smileyType"),
                                          QStringLiteral("happy"));
        const QColor feature = Style::color(style, QStringLiteral("smileyFeatureColor"),
                                            QColor(0x66, 0x66, 0x66));
        const double s = qMin(w, h) / 30.0;
        const double cx = x + w / 2.0;
        const double cy = y + h / 2.0;
        const double eyeR = 1.5 * s;
        const double offset = 5.0 * s;

        for (double sign : {-1.0, 1.0})
        {
            ShapeDecoration eye;
            eye.fillColor = feature;
            eye.strokeColor = feature;
            eye.strokeWidth = 2.0 * s;
            eye.path.addEllipse(QRectF(cx + sign * offset - eyeR,
                                       cy - offset - eyeR,
                                       eyeR * 2.0, eyeR * 2.0));
            decorations.append(eye);
        }

        ShapeDecoration mouth;
        mouth.strokeColor = feature;

        if (type == QLatin1String("happy"))
        {
            mouth.fillColor = QColor(0, 0, 0);
            mouth.strokeWidth = 1.0 * s;
            const double mcy = cy + 2.0 * s;
            mouth.path.moveTo(cx + 7.5 * s, mcy);
            addSvgArc(mouth.path, QPointF(cx + 7.5 * s, mcy),
                      QPointF(cx - 7.5 * s, mcy), 7.5 * s, 7.5 * s, true, true);
            mouth.path.lineTo(cx - 6.818 * s, mcy);
            addSvgArc(mouth.path, QPointF(cx - 6.818 * s, mcy),
                      QPointF(cx + 6.818 * s, mcy), 6.818 * s, 6.818 * s, true, false);
            mouth.path.closeSubpath();
        }
        else if (type == QLatin1String("sad"))
        {
            mouth.fillColor = QColor(0, 0, 0);
            mouth.strokeWidth = 1.0 * s;
            const double mcy = cy + 7.0 * s;
            mouth.path.moveTo(cx - 7.5 * s, mcy);
            addSvgArc(mouth.path, QPointF(cx - 7.5 * s, mcy),
                      QPointF(cx + 7.5 * s, mcy), 7.5 * s, 7.5 * s, true, true);
            mouth.path.lineTo(cx + 6.818 * s, mcy);
            addSvgArc(mouth.path, QPointF(cx + 6.818 * s, mcy),
                      QPointF(cx - 6.818 * s, mcy), 6.818 * s, 6.818 * s, true, false);
            mouth.path.closeSubpath();
        }
        else
        {
            mouth.strokeWidth = 1.0 * s;
            mouth.path.moveTo(cx - 5.0 * s, cy + 7.0 * s);
            mouth.path.lineTo(cx + 5.0 * s, cy + 7.0 * s);
        }

        decorations.append(mouth);
        return decorations;
    }

    if (key == QLatin1String("isoCube2"))
    {
        // IsoCubeShape2.paintVertexShape: the two inner edges of the cube.
        const double isoAngle = qBound(0.01, Style::number(style,
            QStringLiteral("isoAngle"), 15.0), 94.0) * M_PI / 200.0;
        const double isoH = qMin(w * std::tan(isoAngle), h * 0.5);

        ShapeDecoration lines;
        lines.strokeColor = QColor(); // cell stroke
        lines.path.moveTo(point(0, isoH));
        lines.path.lineTo(point(w * 0.5, 2.0 * isoH));
        lines.path.lineTo(point(w, isoH));
        lines.path.moveTo(point(w * 0.5, 2.0 * isoH));
        lines.path.lineTo(point(w * 0.5, h));
        decorations.append(lines);
        return decorations;
    }

    return decorations;
}

void applyDefinition(Cell &cell, const ShapeDefinition &definition)
{
    cell.style = Style::parse(definition.style);
    cell.geometry.setSize(definition.size);
}

} // namespace ShapeRegistry
} // namespace dq
