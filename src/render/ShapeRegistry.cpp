#include "render/ShapeRegistry.h"

#include <QPainterPath>
#include <QRegularExpression>
#include <QtMath>

namespace dq {
namespace ShapeRegistry {
namespace {

QString camelize(const QString &name)
{
    const QStringList parts = name.split(QLatin1Char('_'), Qt::SkipEmptyParts);
    QString result;

    for (int i = 0; i < parts.size(); ++i)
    {
        const QString part = parts.at(i);
        result += i == 0 ? part.toLower()
                         : part.left(1).toUpper() + part.mid(1).toLower();
    }

    return result;
}

/// Maps draw.io stencil names (mxgraph.basic.cloud_callout) to the canonical
/// keys used by the renderer (cloudCallout).
QString normalizeStencilKey(const QString &shape)
{
    const QString prefix = QStringLiteral("mxgraph.basic.");

    if (!shape.startsWith(prefix, Qt::CaseInsensitive))
    {
        // Other stencil sets are matched by their lower case draw.io names.
        return shape.startsWith(QLatin1String("mxgraph."), Qt::CaseInsensitive)
            ? shape.toLower() : shape;
    }

    QString name = shape.mid(prefix.size()).toLower();

    if (name == QLatin1String("isocube"))
    {
        return QStringLiteral("isoCube");
    }
    static const QRegularExpression star(QStringLiteral("^(\\d+)_point_star(_2)?$"));
    const QRegularExpressionMatch match = star.match(name);

    if (match.hasMatch())
    {
        return QStringLiteral("star") + match.captured(1);
    }

    return camelize(name);
}

QString shapeKey(const Cell &cell)
{
    const QString shape = normalizeStencilKey(
        cell.styleValue(QStringLiteral("shape")).trimmed());

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

        if ((qAbs(d) > M_PI) == largeArc)
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
                         const QSizeF &size, const QString &category)
    {
        ShapeDefinition definition;
        definition.key = key;
        definition.name = name;
        definition.style = style;
        definition.size = size;
        definition.category = category;
        shapes.append(definition);
    };

    add(QStringLiteral("basic.waypoint"), QStringLiteral("Waypoint"),
        QStringLiteral("shape=waypoint;size=8;fillColor=#000000;strokeColor=#000000;"),
        QSizeF(20, 20), QStringLiteral("basic"));
    add(QStringLiteral("rectangle"), QStringLiteral("Rectangle"),
        QStringLiteral("rounded=0;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(140, 70), QStringLiteral("basic"));
    add(QStringLiteral("rounded"), QStringLiteral("Rounded Rectangle"),
        QStringLiteral("rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;arcSize=20;"),
        QSizeF(140, 70), QStringLiteral("basic"));
    add(QStringLiteral("ellipse"), QStringLiteral("Ellipse"),
        QStringLiteral("ellipse;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;"),
        QSizeF(140, 80), QStringLiteral("basic"));
    add(QStringLiteral("circle"), QStringLiteral("Circle"),
        QStringLiteral("ellipse;aspect=fixed;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;"),
        QSizeF(90, 90), QStringLiteral("basic"));
    add(QStringLiteral("diamond"), QStringLiteral("Diamond"),
        QStringLiteral("rhombus;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;"),
        QSizeF(120, 90), QStringLiteral("basic"));
    add(QStringLiteral("triangle"), QStringLiteral("Triangle"),
        QStringLiteral("shape=mxgraph.basic.acute_triangle;dx=0.5;whiteSpace=wrap;html=1;"
                       "fillColor=#ffe6cc;strokeColor=#d79b00;"),
        QSizeF(110, 90), QStringLiteral("basic"));
    add(QStringLiteral("basic.orthogonalTriangle"), QStringLiteral("Orthogonal Triangle"),
        QStringLiteral("shape=mxgraph.basic.orthogonal_triangle;whiteSpace=wrap;html=1;"
                       "fillColor=#ffe6cc;strokeColor=#d79b00;"),
        QSizeF(110, 90), QStringLiteral("basic"));
    add(QStringLiteral("basic.obtuseTriangle"), QStringLiteral("Obtuse Triangle"),
        QStringLiteral("shape=mxgraph.basic.obtuse_triangle;dx=0.25;whiteSpace=wrap;html=1;"
                       "fillColor=#ffe6cc;strokeColor=#d79b00;"),
        QSizeF(130, 90), QStringLiteral("basic"));
    add(QStringLiteral("hexagon"), QStringLiteral("Hexagon"),
        QStringLiteral("shape=hexagon;perimeter=hexagonPerimeter2;whiteSpace=wrap;html=1;fixedSize=1;"
                       "fillColor=#e1d5e7;strokeColor=#9673a6;"),
        QSizeF(140, 80), QStringLiteral("basic"));
    add(QStringLiteral("parallelogram"), QStringLiteral("Parallelogram"),
        QStringLiteral("shape=parallelogram;perimeter=parallelogramPerimeter;whiteSpace=wrap;html=1;"
                       "fixedSize=1;fillColor=#fff2cc;strokeColor=#d6b656;"),
        QSizeF(140, 70), QStringLiteral("basic"));
    add(QStringLiteral("trapezoid"), QStringLiteral("Trapezoid"),
        QStringLiteral("shape=trapezoid;perimeter=trapezoidPerimeter;whiteSpace=wrap;html=1;"
                       "fixedSize=1;fillColor=#fff2cc;strokeColor=#d6b656;"),
        QSizeF(140, 70), QStringLiteral("basic"));
    add(QStringLiteral("pentagon"), QStringLiteral("Pentagon"),
        QStringLiteral("shape=pentagon;perimeter=pentagonPerimeter;whiteSpace=wrap;html=1;"
                       "fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(110, 100), QStringLiteral("basic"));
    add(QStringLiteral("octagon"), QStringLiteral("Octagon"),
        QStringLiteral("shape=octagon;perimeter=octagonPerimeter;whiteSpace=wrap;html=1;"
                       "fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(110, 100), QStringLiteral("basic"));
    add(QStringLiteral("star"), QStringLiteral("Star"),
        QStringLiteral("shape=star;perimeter=starPerimeter;whiteSpace=wrap;html=1;"
                       "fillColor=#fff2cc;strokeColor=#d6b656;"),
        QSizeF(110, 100), QStringLiteral("basic"));
    add(QStringLiteral("cross"), QStringLiteral("Cross"),
        QStringLiteral("shape=cross;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("cube"), QStringLiteral("Cube"),
        QStringLiteral("shape=cube;whiteSpace=wrap;html=1;boundedLbl=1;backgroundOutline=1;"
                       "darkOpacity=0.05;fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(110, 100), QStringLiteral("basic"));
    add(QStringLiteral("doubleEllipse"), QStringLiteral("Double Ellipse"),
        QStringLiteral("shape=doubleEllipse;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;"),
        QSizeF(130, 80), QStringLiteral("basic"));
    add(QStringLiteral("cylinder"), QStringLiteral("Database"),
        QStringLiteral("shape=cylinder3;whiteSpace=wrap;html=1;boundedLbl=1;backgroundOutline=1;size=15;"
                       "fillColor=#bac8d3;strokeColor=#23445d;"),
        QSizeF(110, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.cylinder"), QStringLiteral("Cylinder"),
        QStringLiteral("shape=cylinder3;whiteSpace=wrap;html=1;boundedLbl=1;backgroundOutline=1;"
                       "size=15;fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(90, 110), QStringLiteral("basic"));
    add(QStringLiteral("basic.cylinderStack"), QStringLiteral("Cylinder Stack"),
        QStringLiteral("shape=cylinder3;whiteSpace=wrap;html=1;boundedLbl=1;backgroundOutline=1;"
                       "size=15;lid=0;fillColor=#bac8d3;strokeColor=#23445d;"),
        QSizeF(80, 100), QStringLiteral("basic"));
    add(QStringLiteral("cloud"), QStringLiteral("Cloud"),
        QStringLiteral("ellipse;shape=cloud;whiteSpace=wrap;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;"),
        QSizeF(150, 100), QStringLiteral("basic"));
    add(QStringLiteral("document"), QStringLiteral("Document"),
        QStringLiteral("shape=document;whiteSpace=wrap;html=1;boundedLbl=1;fillColor=#f8cecc;strokeColor=#b85450;"),
        QSizeF(140, 90), QStringLiteral("basic"));
    add(QStringLiteral("tape"), QStringLiteral("Tape"),
        QStringLiteral("shape=tape;whiteSpace=wrap;html=1;"
                       "fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(120, 100), QStringLiteral("basic"));
    add(QStringLiteral("note"), QStringLiteral("Note"),
        QStringLiteral("shape=note;whiteSpace=wrap;html=1;backgroundOutline=1;darkOpacity=0.05;"
                       "fillColor=#fff2cc;strokeColor=#d6b656;"),
        QSizeF(120, 100), QStringLiteral("basic"));
    add(QStringLiteral("or"), QStringLiteral("Or"),
        QStringLiteral("shape=or;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(60, 80), QStringLiteral("basic"));
    add(QStringLiteral("xor"), QStringLiteral("Xor"),
        QStringLiteral("shape=xor;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(60, 80), QStringLiteral("basic"));
    add(QStringLiteral("dataStorage"), QStringLiteral("Data Storage"),
        QStringLiteral("shape=dataStorage;whiteSpace=wrap;html=1;fixedSize=1;"
                       "fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(100, 80), QStringLiteral("basic"));
    add(QStringLiteral("internalStorage"), QStringLiteral("Internal Storage"),
        QStringLiteral("shape=internalStorage;whiteSpace=wrap;html=1;backgroundOutline=1;"
                       "fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(80, 80), QStringLiteral("basic"));
    add(QStringLiteral("wedgeCallout"), QStringLiteral("Wedge Callout"),
        QStringLiteral("shape=wedgeCallout;whiteSpace=wrap;html=1;"
                       "fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(120, 80), QStringLiteral("basic"));
    add(QStringLiteral("callout"), QStringLiteral("Callout"),
        QStringLiteral("shape=callout;whiteSpace=wrap;html=1;perimeter=calloutPerimeter;"
                       "fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(150, 90), QStringLiteral("basic"));
    add(QStringLiteral("swimlane"), QStringLiteral("Swimlane"),
        QStringLiteral("swimlane;html=1;startSize=23;fillColor=#f5f5f5;strokeColor=#666666;"),
        QSizeF(240, 160), QStringLiteral("basic"));
    add(QStringLiteral("text"), QStringLiteral("Text"),
        QStringLiteral("text;html=1;strokeColor=none;fillColor=none;align=center;"
                       "verticalAlign=middle;whiteSpace=wrap;rounded=0;"),
        QSizeF(120, 40), QStringLiteral("basic"));
    add(QStringLiteral("line"), QStringLiteral("Line"),
        QStringLiteral("line;strokeWidth=2;html=1;fillColor=none;strokeColor=#000000;"),
        QSizeF(120, 60), QStringLiteral("basic"));

    add(QStringLiteral("actor"), QStringLiteral("Actor"),
        QStringLiteral("shape=umlActor;verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                       "outlineConnect=0;fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(60, 110), QStringLiteral("uml"));
    add(QStringLiteral("arrow"), QStringLiteral("Arrow"),
        QStringLiteral("shape=singleArrow;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;"),
        QSizeF(140, 70), QStringLiteral("arrows"));
    add(QStringLiteral("process"), QStringLiteral("Process"),
        QStringLiteral("shape=process;whiteSpace=wrap;html=1;backgroundOutline=1;"
                       "fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(150, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("step"), QStringLiteral("Step"),
        QStringLiteral("shape=step;perimeter=stepPerimeter;whiteSpace=wrap;html=1;fixedSize=1;"
                       "fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(150, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("terminator"), QStringLiteral("Terminator"),
        QStringLiteral("rounded=1;arcSize=50;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;"),
        QSizeF(140, 60), QStringLiteral("flowchart"));
    const QString blue = QStringLiteral("fillColor=#dae8fc;strokeColor=#6c8ebf;");
    const QString green = QStringLiteral("fillColor=#d5e8d4;strokeColor=#82b366;");
    const QString orange = QStringLiteral("fillColor=#ffe6cc;strokeColor=#d79b00;");
    const QString purple = QStringLiteral("fillColor=#e1d5e7;strokeColor=#9673a6;");
    const QString yellow = QStringLiteral("fillColor=#fff2cc;strokeColor=#d6b656;");
    const QString red = QStringLiteral("fillColor=#f8cecc;strokeColor=#b85450;");
    const QString white = QStringLiteral("fillColor=#ffffff;strokeColor=#000000;");
    const QString plain = QStringLiteral("whiteSpace=wrap;html=1;");

    // --- additional draw.io Basic shapes (Sidebar-Basic.js) --------------
    add(QStringLiteral("basic.partialTopBottom"), QStringLiteral("Partial Top Bottom"),
        QStringLiteral("shape=partialRectangle;top=0;bottom=0;whiteSpace=wrap;html=1;fillColor=none;strokeColor=#6c8ebf;"),
        QSizeF(140, 70), QStringLiteral("basic"));
    add(QStringLiteral("basic.partialTop"), QStringLiteral("Partial Top"),
        QStringLiteral("shape=partialRectangle;top=0;whiteSpace=wrap;html=1;fillColor=none;strokeColor=#6c8ebf;"),
        QSizeF(140, 70), QStringLiteral("basic"));
    add(QStringLiteral("basic.partialBottom"), QStringLiteral("Partial Bottom"),
        QStringLiteral("shape=partialRectangle;bottom=0;whiteSpace=wrap;html=1;fillColor=none;strokeColor=#6c8ebf;"),
        QSizeF(140, 70), QStringLiteral("basic"));
    add(QStringLiteral("basic.partialLeft"), QStringLiteral("Partial Left"),
        QStringLiteral("shape=partialRectangle;left=0;whiteSpace=wrap;html=1;fillColor=none;strokeColor=#6c8ebf;"),
        QSizeF(140, 70), QStringLiteral("basic"));
    add(QStringLiteral("basic.polygon"), QStringLiteral("Polygon"),
        QStringLiteral("shape=polygon;whiteSpace=wrap;html=1;") + blue,
        QSizeF(120, 120), QStringLiteral("basic"));
    add(QStringLiteral("basic.star4"), QStringLiteral("Star4"),
        QStringLiteral("shape=star4;whiteSpace=wrap;html=1;") + yellow,
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.star6"), QStringLiteral("Star6"),
        QStringLiteral("shape=star6;whiteSpace=wrap;html=1;") + yellow,
        QSizeF(110, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.star8"), QStringLiteral("Star8"),
        QStringLiteral("shape=star8;whiteSpace=wrap;html=1;") + yellow,
        QSizeF(110, 110), QStringLiteral("basic"));
    add(QStringLiteral("basic.banner"), QStringLiteral("Banner"),
        QStringLiteral("shape=banner;whiteSpace=wrap;html=1;") + orange,
        QSizeF(180, 70), QStringLiteral("basic"));
    add(QStringLiteral("basic.cloudCallout"), QStringLiteral("Cloud Callout"),
        QStringLiteral("shape=cloudCallout;whiteSpace=wrap;html=1;") + purple,
        QSizeF(160, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.cloudRect"), QStringLiteral("Cloud Rect"),
        QStringLiteral("shape=cloudRect;whiteSpace=wrap;html=1;") + purple,
        QSizeF(160, 90), QStringLiteral("basic"));
    add(QStringLiteral("basic.cone"), QStringLiteral("Cone"),
        QStringLiteral("shape=cone;whiteSpace=wrap;html=1;") + yellow,
        QSizeF(100, 120), QStringLiteral("basic"));
    add(QStringLiteral("basic.cone2"), QStringLiteral("Cone2"),
        QStringLiteral("shape=cone2;whiteSpace=wrap;html=1;") + yellow,
        QSizeF(100, 120), QStringLiteral("basic"));
    add(QStringLiteral("basic.donut"), QStringLiteral("Donut"),
        QStringLiteral("shape=donut;whiteSpace=wrap;html=1;") + green,
        QSizeF(110, 110), QStringLiteral("basic"));
    add(QStringLiteral("basic.drop"), QStringLiteral("Drop"),
        QStringLiteral("shape=drop;whiteSpace=wrap;html=1;") + blue,
        QSizeF(80, 110), QStringLiteral("basic"));
    add(QStringLiteral("basic.flash"), QStringLiteral("Flash"),
        QStringLiteral("shape=flash;whiteSpace=wrap;html=1;") + yellow,
        QSizeF(80, 120), QStringLiteral("basic"));
    add(QStringLiteral("basic.halfCircle"), QStringLiteral("Half Circle"),
        QStringLiteral("shape=halfCircle;whiteSpace=wrap;html=1;") + green,
        QSizeF(120, 70), QStringLiteral("basic"));
    add(QStringLiteral("basic.heart"), QStringLiteral("Heart"),
        QStringLiteral("shape=heart;whiteSpace=wrap;html=1;") + red,
        QSizeF(110, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.isocube"), QStringLiteral("Isometric Cube"),
        QStringLiteral("shape=mxgraph.basic.isocube;isoAngle=15;whiteSpace=wrap;html=1;") + blue,
        QSizeF(110, 110), QStringLiteral("basic"));
    add(QStringLiteral("basic.loudCallout"), QStringLiteral("Loud Callout"),
        QStringLiteral("shape=loudCallout;whiteSpace=wrap;html=1;") + orange,
        QSizeF(150, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.moon"), QStringLiteral("Moon"),
        QStringLiteral("shape=moon;whiteSpace=wrap;html=1;") + yellow,
        QSizeF(90, 110), QStringLiteral("basic"));
    add(QStringLiteral("basic.noSymbol"), QStringLiteral("No Symbol"),
        QStringLiteral("shape=noSymbol;whiteSpace=wrap;html=1;fillColor=none;strokeColor=#b85450;"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.orthogonalTriangle"), QStringLiteral("Orthogonal Triangle"),
        QStringLiteral("shape=orthogonalTriangle;whiteSpace=wrap;html=1;") + green,
        QSizeF(110, 90), QStringLiteral("basic"));
    add(QStringLiteral("basic.acuteTriangle"), QStringLiteral("Acute Triangle"),
        QStringLiteral("shape=acuteTriangle;whiteSpace=wrap;html=1;") + green,
        QSizeF(110, 80), QStringLiteral("basic"));
    add(QStringLiteral("basic.obtuseTriangle"), QStringLiteral("Obtuse Triangle"),
        QStringLiteral("shape=obtuseTriangle;whiteSpace=wrap;html=1;") + green,
        QSizeF(110, 80), QStringLiteral("basic"));
    add(QStringLiteral("basic.ovalCallout"), QStringLiteral("Oval Callout"),
        QStringLiteral("shape=ovalCallout;whiteSpace=wrap;html=1;") + blue,
        QSizeF(150, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.pointedOval"), QStringLiteral("Pointed Oval"),
        QStringLiteral("shape=pointedOval;whiteSpace=wrap;html=1;") + purple,
        QSizeF(80, 110), QStringLiteral("basic"));
    add(QStringLiteral("basic.pyramid"), QStringLiteral("Pyramid"),
        QStringLiteral("shape=mxgraph.basic.pyramid;dx1=0.4;dx2=0.6;dy1=0.9;dy2=0.8;"
                       "whiteSpace=wrap;html=1;") + yellow,
        QSizeF(120, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.diagSnipRect"), QStringLiteral("Diag Snip Rect"),
        QStringLiteral("shape=diagSnipRect;whiteSpace=wrap;html=1;") + blue,
        QSizeF(140, 80), QStringLiteral("basic"));
    add(QStringLiteral("basic.diagRoundRect"), QStringLiteral("Diag Round Rect"),
        QStringLiteral("shape=diagRoundRect;whiteSpace=wrap;html=1;") + blue,
        QSizeF(140, 80), QStringLiteral("basic"));
    add(QStringLiteral("basic.cornerRoundRect"), QStringLiteral("Corner Round Rect"),
        QStringLiteral("shape=cornerRoundRect;whiteSpace=wrap;html=1;") + blue,
        QSizeF(140, 80), QStringLiteral("basic"));
    add(QStringLiteral("basic.threeCornerRoundRect"), QStringLiteral("Three Corner Round Rect"),
        QStringLiteral("shape=threeCornerRoundRect;whiteSpace=wrap;html=1;") + blue,
        QSizeF(140, 80), QStringLiteral("basic"));
    add(QStringLiteral("basic.plaque"), QStringLiteral("Plaque"),
        QStringLiteral("shape=plaque;whiteSpace=wrap;html=1;") + orange,
        QSizeF(140, 70), QStringLiteral("basic"));
    add(QStringLiteral("basic.frame"), QStringLiteral("Frame"),
        QStringLiteral("shape=frame;whiteSpace=wrap;html=1;fillColor=none;strokeColor=#6c8ebf;"),
        QSizeF(140, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.roundedFrame"), QStringLiteral("Rounded Frame"),
        QStringLiteral("shape=roundedFrame;whiteSpace=wrap;html=1;fillColor=none;strokeColor=#6c8ebf;"),
        QSizeF(140, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.plaqueFrame"), QStringLiteral("Plaque Frame"),
        QStringLiteral("shape=plaqueFrame;whiteSpace=wrap;html=1;fillColor=none;strokeColor=#6c8ebf;"),
        QSizeF(140, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.frameCorner"), QStringLiteral("Frame Corner"),
        QStringLiteral("shape=frameCorner;whiteSpace=wrap;html=1;fillColor=none;strokeColor=#6c8ebf;"),
        QSizeF(130, 90), QStringLiteral("basic"));
    add(QStringLiteral("basic.diagStripe"), QStringLiteral("Diag Stripe"),
        QStringLiteral("shape=diagStripe;whiteSpace=wrap;html=1;") + yellow,
        QSizeF(140, 80), QStringLiteral("basic"));
    add(QStringLiteral("basic.rectCallout"), QStringLiteral("Rect Callout"),
        QStringLiteral("shape=rectCallout;whiteSpace=wrap;html=1;") + blue,
        QSizeF(150, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.roundRectCallout"), QStringLiteral("Round Rect Callout"),
        QStringLiteral("shape=roundRectCallout;whiteSpace=wrap;html=1;") + blue,
        QSizeF(150, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.layeredRect"), QStringLiteral("Layered Rect"),
        QStringLiteral("shape=layeredRect;whiteSpace=wrap;html=1;") + green,
        QSizeF(150, 90), QStringLiteral("basic"));
    add(QStringLiteral("basic.smiley"), QStringLiteral("Smiley"),
        QStringLiteral("shape=mxgraph.basic.smiley;whiteSpace=wrap;html=1;") + yellow,
        QSizeF(110, 110), QStringLiteral("basic"));
    add(QStringLiteral("basic.sun"), QStringLiteral("Sun"),
        QStringLiteral("shape=mxgraph.basic.sun;whiteSpace=wrap;html=1;") + yellow,
        QSizeF(110, 110), QStringLiteral("basic"));
    add(QStringLiteral("basic.tick"), QStringLiteral("Tick"),
        QStringLiteral("shape=mxgraph.basic.tick;whiteSpace=wrap;html=1;"
                       "fillColor=#d5e8d4;strokeColor=#82b366;"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.pie"), QStringLiteral("Pie"),
        QStringLiteral("shape=mxgraph.basic.pie;startAngle=0.2;endAngle=0.9;"
                       "whiteSpace=wrap;html=1;") + blue,
        QSizeF(110, 110), QStringLiteral("basic"));
    add(QStringLiteral("basic.x"), QStringLiteral("X"),
        QStringLiteral("shape=mxgraph.basic.x;whiteSpace=wrap;html=1;") + red,
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("basic.wave"), QStringLiteral("Wave"),
        QStringLiteral("shape=mxgraph.basic.wave2;dy=0.3;whiteSpace=wrap;html=1;") + blue,
        QSizeF(160, 70), QStringLiteral("basic"));
    add(QStringLiteral("basic.button"), QStringLiteral("Button"),
        QStringLiteral("shape=button;whiteSpace=wrap;html=1;") + green,
        QSizeF(120, 50), QStringLiteral("basic"));

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
        QStringLiteral("shape=cloudVpc;whiteSpace=wrap;html=1;fillColor=none;strokeColor=#6c8ebf;"),
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

    add(QStringLiteral("basic.patternDiag"), QStringLiteral("Pattern Diag"),
        QStringLiteral("shape=patternFillRect;fillStyle=diag;step=8;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#6c8ebf;"),
        QSizeF(150, 80), QStringLiteral("basic"));
    add(QStringLiteral("basic.patternDiagRev"), QStringLiteral("Pattern Diag Rev"),
        QStringLiteral("shape=patternFillRect;fillStyle=diagRev;step=8;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#6c8ebf;"),
        QSizeF(150, 80), QStringLiteral("basic"));
    add(QStringLiteral("basic.patternVertical"), QStringLiteral("Pattern Vertical"),
        QStringLiteral("shape=patternFillRect;fillStyle=vert;step=8;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#6c8ebf;"),
        QSizeF(150, 80), QStringLiteral("basic"));
    add(QStringLiteral("basic.patternHorizontal"), QStringLiteral("Pattern Horizontal"),
        QStringLiteral("shape=patternFillRect;fillStyle=hor;step=8;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#6c8ebf;"),
        QSizeF(150, 80), QStringLiteral("basic"));
    add(QStringLiteral("basic.patternGrid"), QStringLiteral("Pattern Grid"),
        QStringLiteral("shape=patternFillRect;fillStyle=grid;step=8;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#6c8ebf;"),
        QSizeF(150, 80), QStringLiteral("basic"));
    add(QStringLiteral("basic.patternDiagGrid"), QStringLiteral("Pattern Diag Grid"),
        QStringLiteral("shape=patternFillRect;fillStyle=diagGrid;step=8;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#6c8ebf;"),
        QSizeF(150, 80), QStringLiteral("basic"));

    // --- misc ------------------------------------------------------------
    add(QStringLiteral("misc.container"), QStringLiteral("Container"),
        QStringLiteral("swimlane;startSize=0;html=1;fillColor=none;strokeColor=#666666;"),
        QSizeF(220, 160), QStringLiteral("misc"));
    add(QStringLiteral("misc.table"), QStringLiteral("Table"),
        QStringLiteral("shape=miscTable;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#6c8ebf;"),
        QSizeF(180, 120), QStringLiteral("misc"));
    add(QStringLiteral("misc.list"), QStringLiteral("List"),
        QStringLiteral("shape=miscList;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#6c8ebf;"),
        QSizeF(180, 120), QStringLiteral("misc"));
    add(QStringLiteral("misc.note"), QStringLiteral("Note"),
        QStringLiteral("shape=note;whiteSpace=wrap;html=1;fillColor=#fff2cc;strokeColor=#d6b656;"),
        QSizeF(120, 100), QStringLiteral("misc"));
    add(QStringLiteral("misc.callout"), QStringLiteral("Callout"),
        QStringLiteral("shape=callout;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(150, 90), QStringLiteral("misc"));

    // --- advanced layouts ------------------------------------------------
    add(QStringLiteral("advanced.container"), QStringLiteral("Container"),
        QStringLiteral("swimlane;startSize=0;html=1;fillColor=#f5f5f5;strokeColor=#666666;"),
        QSizeF(220, 160), QStringLiteral("advanced"));
    add(QStringLiteral("advanced.horizontal"), QStringLiteral("Horizontal"),
        QStringLiteral("swimlane;horizontal=0;startSize=30;html=1;fillColor=#f5f5f5;strokeColor=#666666;"),
        QSizeF(220, 160), QStringLiteral("advanced"));
    add(QStringLiteral("advanced.vertical"), QStringLiteral("Vertical"),
        QStringLiteral("swimlane;startSize=30;html=1;fillColor=#f5f5f5;strokeColor=#666666;"),
        QSizeF(220, 160), QStringLiteral("advanced"));
    add(QStringLiteral("advanced.poolHorizontal"), QStringLiteral("Pool Horizontal"),
        QStringLiteral("swimlane;horizontal=0;startSize=40;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(260, 180), QStringLiteral("advanced"));
    add(QStringLiteral("advanced.poolVertical"), QStringLiteral("Pool Vertical"),
        QStringLiteral("swimlane;startSize=40;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(260, 180), QStringLiteral("advanced"));
    add(QStringLiteral("advanced.treeHorizontal"), QStringLiteral("Tree Horizontal"),
        QStringLiteral("shape=advTreeH;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#6c8ebf;"),
        QSizeF(220, 150), QStringLiteral("advanced"));
    add(QStringLiteral("advanced.treeVertical"), QStringLiteral("Tree Vertical"),
        QStringLiteral("shape=advTreeV;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#6c8ebf;"),
        QSizeF(200, 190), QStringLiteral("advanced"));
    add(QStringLiteral("advanced.treeBalloon"), QStringLiteral("Tree Balloon"),
        QStringLiteral("shape=advTreeBalloon;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#6c8ebf;"),
        QSizeF(220, 190), QStringLiteral("advanced"));
    add(QStringLiteral("advanced.flowHorizontal"), QStringLiteral("Flow Horizontal"),
        QStringLiteral("shape=advFlowH;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;"),
        QSizeF(260, 110), QStringLiteral("advanced"));
    add(QStringLiteral("advanced.flowVertical"), QStringLiteral("Flow Vertical"),
        QStringLiteral("shape=advFlowV;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;"),
        QSizeF(160, 240), QStringLiteral("advanced"));

    // --- flowchart -------------------------------------------------------
    add(QStringLiteral("decision"), QStringLiteral("Decision"),
        QStringLiteral("rhombus;") + plain + orange, QSizeF(130, 90), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.data"), QStringLiteral("Data"),
        QStringLiteral("shape=parallelogram;perimeter=parallelogramPerimeter;fixedSize=1;")
            + plain + purple, QSizeF(140, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.document"), QStringLiteral("Document"),
        QStringLiteral("shape=document;boundedLbl=1;") + plain + blue,
        QSizeF(140, 90), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.multidocument"), QStringLiteral("Multidocument"),
        QStringLiteral("shape=mxgraph.flowchart.multi-document;") + plain + blue,
        QSizeF(140, 90), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.predefined"), QStringLiteral("Predefined"),
        QStringLiteral("shape=process;backgroundOutline=1;") + plain + blue,
        QSizeF(150, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.preparation"), QStringLiteral("Preparation"),
        QStringLiteral("shape=hexagon;perimeter=hexagonPerimeter2;fixedSize=1;") + plain + purple,
        QSizeF(140, 80), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.manualinput"), QStringLiteral("Manualinput"),
        QStringLiteral("shape=mxgraph.flowchart.manual_input;") + plain + yellow,
        QSizeF(140, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.manualoperation"), QStringLiteral("Manualoperation"),
        QStringLiteral("shape=mxgraph.flowchart.manual_operation;") + plain + yellow,
        QSizeF(140, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.delay"), QStringLiteral("Delay"),
        QStringLiteral("shape=mxgraph.flowchart.delay;") + plain + orange,
        QSizeF(130, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.display"), QStringLiteral("Display"),
        QStringLiteral("shape=mxgraph.flowchart.display;") + plain + blue,
        QSizeF(150, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.connector"), QStringLiteral("Connector"),
        QStringLiteral("shape=ellipse;") + plain + green,
        QSizeF(60, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.offpage"), QStringLiteral("Offpage"),
        QStringLiteral("shape=mxgraph.flowchart.off_page_reference;") + plain + yellow,
        QSizeF(100, 100), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.merge"), QStringLiteral("Merge"),
        QStringLiteral("shape=mxgraph.flowchart.merge_or_storage;") + plain + green,
        QSizeF(100, 80), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.extract"), QStringLiteral("Extract"),
        QStringLiteral("shape=mxgraph.flowchart.extract_or_measurement;") + plain + green,
        QSizeF(100, 80), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.or"), QStringLiteral("Or"),
        QStringLiteral("shape=ellipse;fcType=or;") + plain + green,
        QSizeF(70, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.summing"), QStringLiteral("Summing"),
        QStringLiteral("shape=ellipse;fcType=summing;") + plain + green,
        QSizeF(70, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.collate"), QStringLiteral("Collate"),
        QStringLiteral("shape=mxgraph.flowchart.collate;") + plain + purple,
        QSizeF(90, 90), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.sort"), QStringLiteral("Sort"),
        QStringLiteral("shape=rhombus;fcType=sort;") + plain + purple,
        QSizeF(100, 90), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.storeddata"), QStringLiteral("Storeddata"),
        QStringLiteral("shape=mxgraph.flowchart.stored_data;") + plain + blue,
        QSizeF(140, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.internalstorage"), QStringLiteral("Internalstorage"),
        QStringLiteral("shape=mxgraph.flowchart.internal_storage;") + plain + blue,
        QSizeF(130, 80), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.annotation"), QStringLiteral("Annotation"),
        QStringLiteral("shape=mxgraph.flowchart.annotation;align=left;verticalAlign=top;")
            + plain, QSizeF(120, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.looplimit"), QStringLiteral("Looplimit"),
        QStringLiteral("shape=mxgraph.flowchart.loop_limit;") + plain + blue,
        QSizeF(130, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.card"), QStringLiteral("Card"),
        QStringLiteral("shape=mxgraph.flowchart.card;") + plain + blue,
        QSizeF(130, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.directdata"), QStringLiteral("Directdata"),
        QStringLiteral("shape=cylinder3;boundedLbl=1;backgroundOutline=1;size=15;")
            + plain + blue, QSizeF(120, 80), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.sequentialdata"), QStringLiteral("Sequentialdata"),
        QStringLiteral("shape=mxgraph.flowchart.sequential_data;") + plain + blue,
        QSizeF(120, 80), QStringLiteral("flowchart"));

    // --- arrows ----------------------------------------------------------
    add(QStringLiteral("arrow.double"), QStringLiteral("Double"),
        QStringLiteral("shape=doubleArrow;") + plain + green,
        QSizeF(150, 60), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.left"), QStringLiteral("Left"),
        QStringLiteral("shape=singleArrow;direction=west;") + plain + green,
        QSizeF(140, 70), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.up"), QStringLiteral("Up"),
        QStringLiteral("shape=singleArrow;direction=north;") + plain + green,
        QSizeF(80, 120), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.down"), QStringLiteral("Down"),
        QStringLiteral("shape=singleArrow;direction=south;") + plain + green,
        QSizeF(80, 120), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.chevron"), QStringLiteral("Chevron"),
        QStringLiteral("shape=step;perimeter=stepPerimeter;fixedSize=1;") + plain + blue,
        QSizeF(150, 60), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.notched"), QStringLiteral("Notched"),
        QStringLiteral("shape=singleArrow;notch=0.3;") + plain + orange,
        QSizeF(150, 70), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.open"), QStringLiteral("Open"),
        QStringLiteral("shape=singleArrow;arrowWidth=0.5;arrowSize=0.35;") + plain + yellow,
        QSizeF(150, 70), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.thin"), QStringLiteral("Thin"),
        QStringLiteral("shape=singleArrow;arrowWidth=0.2;arrowSize=0.4;") + plain + green,
        QSizeF(170, 50), QStringLiteral("arrows"));

    add(QStringLiteral("arrow.tailed"), QStringLiteral("Tailed"),
        QStringLiteral("shape=arrTailed;") + plain + green,
        QSizeF(160, 70), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.sharp"), QStringLiteral("Sharp"),
        QStringLiteral("shape=arrSharp;") + plain + green,
        QSizeF(150, 70), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.slender"), QStringLiteral("Slender"),
        QStringLiteral("shape=arrSlender;") + plain + green,
        QSizeF(170, 50), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.slenderTwo"), QStringLiteral("Slender Two"),
        QStringLiteral("shape=arrSlenderTwo;") + plain + green,
        QSizeF(180, 50), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.wedge"), QStringLiteral("Wedge"),
        QStringLiteral("shape=arrWedge;") + plain + orange,
        QSizeF(120, 70), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.chevronBand"), QStringLiteral("Chevron Band"),
        QStringLiteral("shape=arrChevron;") + plain + blue,
        QSizeF(150, 60), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.notched2"), QStringLiteral("Notched2"),
        QStringLiteral("shape=arrNotched;") + plain + orange,
        QSizeF(160, 70), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.signalIn"), QStringLiteral("Signal In"),
        QStringLiteral("shape=arrSignalIn;") + plain + yellow,
        QSizeF(160, 80), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.notchedSignalIn"), QStringLiteral("Notched Signal In"),
        QStringLiteral("shape=arrNotchedSignalIn;") + plain + yellow,
        QSizeF(160, 80), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.jumpIn"), QStringLiteral("Jump In"),
        QStringLiteral("shape=arrJumpIn;") + plain + blue,
        QSizeF(160, 80), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.quad"), QStringLiteral("Quad"),
        QStringLiteral("shape=arrQuad;") + plain + purple,
        QSizeF(140, 140), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.triad"), QStringLiteral("Triad"),
        QStringLiteral("shape=arrTriad;") + plain + purple,
        QSizeF(140, 130), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.uturn"), QStringLiteral("Uturn"),
        QStringLiteral("shape=arrUturn;") + plain + green,
        QSizeF(140, 120), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.bend"), QStringLiteral("Bend"),
        QStringLiteral("shape=arrBend;") + plain + green,
        QSizeF(140, 120), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.bendDouble"), QStringLiteral("Bend Double"),
        QStringLiteral("shape=arrBendDouble;") + plain + green,
        QSizeF(140, 120), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.striped"), QStringLiteral("Striped"),
        QStringLiteral("shape=arrStriped;") + plain + yellow,
        QSizeF(170, 70), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.stylised"), QStringLiteral("Stylised"),
        QStringLiteral("shape=arrStylised;") + plain + blue,
        QSizeF(160, 70), QStringLiteral("arrows"));

    add(QStringLiteral("arrow.callout"), QStringLiteral("Callout"),
        QStringLiteral("shape=arrCalloutArrow;") + plain + blue,
        QSizeF(150, 120), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.calloutDouble"), QStringLiteral("Callout Double"),
        QStringLiteral("shape=arrCalloutDouble;") + plain + blue,
        QSizeF(150, 120), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.calloutDouble90"), QStringLiteral("Callout Double90"),
        QStringLiteral("shape=arrCalloutDouble90;") + plain + blue,
        QSizeF(150, 120), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.calloutQuad"), QStringLiteral("Callout Quad"),
        QStringLiteral("shape=arrCalloutQuad;") + plain + blue,
        QSizeF(150, 130), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.wedgeDashed"), QStringLiteral("Wedge Dashed"),
        QStringLiteral("shape=arrWedge;dashed=1;") + plain + orange,
        QSizeF(120, 70), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.tailedNotch"), QStringLiteral("Tailed Notch"),
        QStringLiteral("shape=arrTailed;") + plain + green,
        QSizeF(160, 70), QStringLiteral("arrows"));

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
        QStringLiteral("ellipse;fillColor=#000000;strokeColor=#000000;"),
        QSizeF(36, 36), QStringLiteral("uml"));
    add(QStringLiteral("uml.final"), QStringLiteral("Final"),
        QStringLiteral("shape=doubleEllipse;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(44, 44), QStringLiteral("uml"));
    add(QStringLiteral("uml.decision"), QStringLiteral("Decision"),
        QStringLiteral("rhombus;") + plain + yellow, QSizeF(90, 90), QStringLiteral("uml"));
    add(QStringLiteral("uml.fork"), QStringLiteral("Fork"),
        QStringLiteral("rounded=0;fillColor=#000000;strokeColor=#000000;"),
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
        QStringLiteral("rounded=1;dashed=1;fillColor=none;strokeColor=#666666;"),
        QSizeF(200, 140), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.annotation"), QStringLiteral("Annotation"),
        QStringLiteral("shape=mxgraph.flowchart.annotation;align=left;verticalAlign=top;")
            + plain, QSizeF(130, 60), QStringLiteral("bpmn"));

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
        QStringLiteral("rounded=0;fillColor=#ffffff;strokeColor=#999999;"),
        QSizeF(180, 40), QStringLiteral("mockup"));
    add(QStringLiteral("ui.window"), QStringLiteral("Window"),
        QStringLiteral("swimlane;startSize=26;") + blue, QSizeF(280, 180),
        QStringLiteral("mockup"));
    add(QStringLiteral("ui.navbar"), QStringLiteral("Navbar"),
        QStringLiteral("rounded=0;fillColor=#f5f5f5;strokeColor=#cccccc;"),
        QSizeF(320, 50), QStringLiteral("mockup"));
    add(QStringLiteral("ui.browser"), QStringLiteral("Browser"),
        QStringLiteral("swimlane;startSize=30;") + blue, QSizeF(320, 220),
        QStringLiteral("mockup"));

    add(QStringLiteral("android.phone"), QStringLiteral("Phone"),
        QStringLiteral("rounded=1;arcSize=8;androidType=phone;fillColor=#a4c639;strokeColor=#4a6b14;"),
        QSizeF(100, 190), QStringLiteral("software"));
    add(QStringLiteral("android.tablet"), QStringLiteral("Tablet"),
        QStringLiteral("rounded=1;arcSize=6;androidType=tablet;fillColor=#a4c639;strokeColor=#4a6b14;"),
        QSizeF(160, 220), QStringLiteral("software"));
    add(QStringLiteral("android.watch"), QStringLiteral("Watch"),
        QStringLiteral("rounded=1;arcSize=30;androidType=watch;fillColor=#a4c639;strokeColor=#4a6b14;"),
        QSizeF(100, 100), QStringLiteral("software"));

    add(QStringLiteral("image"), QStringLiteral("Image"),
        QStringLiteral("shape=image;verticalLabelPosition=bottom;verticalAlign=top;imageAspect=0;"
                       "html=1;strokeColor=none;fillColor=none;"),
        QSizeF(140, 100), QStringLiteral("basic"));

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
        QStringLiteral("shape=elec.junction;fillColor=#000000;strokeColor=#000000;"),
        QSizeF(24, 24), QStringLiteral("electrical"));

    // --- floor plan -------------------------------------------------------
    add(QStringLiteral("floor.wall"), QStringLiteral("Wall"),
        QStringLiteral("rounded=0;fillColor=#e8eaed;strokeColor=#5f6368;strokeWidth=2;"),
        QSizeF(240, 16), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.door"), QStringLiteral("Door"),
        QStringLiteral("shape=floor.door;fillColor=none;strokeColor=#5f6368;strokeWidth=2;"),
        QSizeF(90, 90), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.window"), QStringLiteral("Window"),
        QStringLiteral("shape=floor.window;fillColor=#dae8fc;strokeColor=#5f6368;strokeWidth=2;"),
        QSizeF(140, 16), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.stairs"), QStringLiteral("Stairs"),
        QStringLiteral("shape=floor.stairs;fillColor=#f5f5f5;strokeColor=#5f6368;"),
        QSizeF(120, 160), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.table"), QStringLiteral("Table"),
        QStringLiteral("rounded=1;fillColor=#fff2cc;strokeColor=#d6b656;"),
        QSizeF(160, 90), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.chair"), QStringLiteral("Chair"),
        QStringLiteral("rounded=1;fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(60, 60), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.bed"), QStringLiteral("Bed"),
        QStringLiteral("rounded=1;fillColor=#d5e8d4;strokeColor=#82b366;"),
        QSizeF(180, 120), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.sofa"), QStringLiteral("Sofa"),
        QStringLiteral("rounded=1;fillColor=#e1d5e7;strokeColor=#9673a6;"),
        QSizeF(180, 80), QStringLiteral("floorplan"));

    // --- infographic ------------------------------------------------------
    add(QStringLiteral("info.chevron"), QStringLiteral("Chevron"),
        QStringLiteral("shape=step;perimeter=stepPerimeter;fixedSize=1;fillColor=#1a73e8;"
                       "strokeColor=none;fontColor=#ffffff;html=1;"),
        QSizeF(180, 70), QStringLiteral("infographic"));
    add(QStringLiteral("info.ribbon"), QStringLiteral("Ribbon"),
        QStringLiteral("shape=info.ribbon;fillColor=#f29900;strokeColor=none;"
                       "fontColor=#ffffff;html=1;"),
        QSizeF(180, 70), QStringLiteral("infographic"));
    add(QStringLiteral("info.badge"), QStringLiteral("Badge"),
        QStringLiteral("shape=star;perimeter=starPerimeter;fillColor=#d93025;strokeColor=none;"
                       "fontColor=#ffffff;html=1;"),
        QSizeF(110, 110), QStringLiteral("infographic"));
    add(QStringLiteral("info.bubble"), QStringLiteral("Bubble"),
        QStringLiteral("shape=callout;perimeter=calloutPerimeter;fillColor=#e6f4ea;"
                       "strokeColor=#34a853;html=1;"),
        QSizeF(180, 100), QStringLiteral("infographic"));
    add(QStringLiteral("info.progress"), QStringLiteral("Progress"),
        QStringLiteral("rounded=1;arcSize=50;fillColor=#1a73e8;strokeColor=none;"),
        QSizeF(240, 24), QStringLiteral("infographic"));

    // --- SysML ------------------------------------------------------------
    add(QStringLiteral("sysml.block"), QStringLiteral("Block"),
        QStringLiteral("swimlane;fontStyle=1;startSize=28;fillColor=#dae8fc;"
                       "strokeColor=#6c8ebf;html=1;"),
        QSizeF(200, 120), QStringLiteral("sysml"));
    add(QStringLiteral("sysml.port"), QStringLiteral("Port"),
        QStringLiteral("fillColor=#ffffff;strokeColor=#6c8ebf;"), QSizeF(24, 24), QStringLiteral("sysml"));
    add(QStringLiteral("sysml.requirement"), QStringLiteral("Requirement"),
        QStringLiteral("shape=note;backgroundOutline=1;fillColor=#fff2cc;strokeColor=#d6b656;"
                       "html=1;"), QSizeF(180, 100), QStringLiteral("sysml"));
    add(QStringLiteral("sysml.constraint"), QStringLiteral("Constraint"),
        QStringLiteral("rounded=0;fillColor=#f8cecc;strokeColor=#b85450;html=1;"),
        QSizeF(170, 90), QStringLiteral("sysml"));
    add(QStringLiteral("sysml.item"), QStringLiteral("Item"),
        QStringLiteral("rounded=0;fillColor=#e1d5e7;strokeColor=#9673a6;html=1;"),
        QSizeF(140, 70), QStringLiteral("sysml"));

    // --- ArchiMate --------------------------------------------------------
    add(QStringLiteral("archimate.actor"), QStringLiteral("Actor"),
        QStringLiteral("rounded=1;arcSize=30;fillColor=#fff2cc;strokeColor=#d6b656;html=1;"),
        QSizeF(140, 70), QStringLiteral("archimate"));
    add(QStringLiteral("archimate.role"), QStringLiteral("Role"),
        QStringLiteral("rounded=1;arcSize=30;fillColor=#fff2cc;strokeColor=#d6b656;html=1;"),
        QSizeF(140, 70), QStringLiteral("archimate"));
    add(QStringLiteral("archimate.process"), QStringLiteral("Process"),
        QStringLiteral("rounded=1;arcSize=30;fillColor=#fff2cc;strokeColor=#d6b656;html=1;"),
        QSizeF(160, 70), QStringLiteral("archimate"));
    add(QStringLiteral("archimate.appcomponent"), QStringLiteral("Appcomponent"),
        QStringLiteral("rounded=1;arcSize=30;fillColor=#dae8fc;strokeColor=#6c8ebf;html=1;"),
        QSizeF(160, 70), QStringLiteral("archimate"));
    add(QStringLiteral("archimate.node"), QStringLiteral("Node"),
        QStringLiteral("shape=cube;fillColor=#d5e8d4;strokeColor=#82b366;boundedLbl=1;"
                       "backgroundOutline=1;html=1;"),
        QSizeF(160, 100), QStringLiteral("archimate"));
    add(QStringLiteral("archimate.device"), QStringLiteral("Device"),
        QStringLiteral("rounded=0;fillColor=#d5e8d4;strokeColor=#82b366;html=1;"),
        QSizeF(150, 80), QStringLiteral("archimate"));

    // --- Enterprise integration patterns ---------------------------------
    add(QStringLiteral("eip.channel"), QStringLiteral("Channel"),
        QStringLiteral("rounded=0;fillColor=#dae8fc;strokeColor=#6c8ebf;html=1;"),
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
        QStringLiteral("rounded=1;arcSize=25;fillColor=#0d6efd;strokeColor=#0d6efd;"
                       "fontColor=#ffffff;html=1;"),
        QSizeF(120, 42), QStringLiteral("bootstrap"));
    add(QStringLiteral("bs.panel"), QStringLiteral("Panel"),
        QStringLiteral("swimlane;startSize=34;fillColor=#ffffff;strokeColor=#dee2e6;html=1;"),
        QSizeF(240, 160), QStringLiteral("bootstrap"));
    add(QStringLiteral("bs.navbar"), QStringLiteral("Navbar"),
        QStringLiteral("rounded=0;fillColor=#212529;strokeColor=#212529;fontColor=#ffffff;html=1;"),
        QSizeF(320, 48), QStringLiteral("bootstrap"));
    add(QStringLiteral("bs.badge"), QStringLiteral("Badge"),
        QStringLiteral("rounded=1;arcSize=50;fillColor=#6c757d;strokeColor=#6c757d;"
                       "fontColor=#ffffff;html=1;"),
        QSizeF(70, 28), QStringLiteral("bootstrap"));
    add(QStringLiteral("bs.alert"), QStringLiteral("Alert"),
        QStringLiteral("rounded=1;arcSize=8;fillColor=#cff4fc;strokeColor=#b6effb;html=1;"),
        QSizeF(260, 56), QStringLiteral("bootstrap"));

    // --- Kubernetes -------------------------------------------------------
    add(QStringLiteral("k8s.pod"), QStringLiteral("Pod"),
        QStringLiteral("shape=hexagon;perimeter=hexagonPerimeter2;fixedSize=1;"
                       "fillColor=#326ce5;strokeColor=#1a4bb3;fontColor=#ffffff;html=1;"),
        QSizeF(110, 100), QStringLiteral("kubernetes"));
    add(QStringLiteral("k8s.service"), QStringLiteral("Service"),
        QStringLiteral("ellipse;fillColor=#326ce5;strokeColor=#1a4bb3;fontColor=#ffffff;html=1;"),
        QSizeF(100, 100), QStringLiteral("kubernetes"));
    add(QStringLiteral("k8s.deployment"), QStringLiteral("Deployment"),
        QStringLiteral("rounded=1;arcSize=10;fillColor=#326ce5;strokeColor=#1a4bb3;"
                       "fontColor=#ffffff;html=1;"),
        QSizeF(150, 90), QStringLiteral("kubernetes"));
    add(QStringLiteral("k8s.ingress"), QStringLiteral("Ingress"),
        QStringLiteral("shape=pentagon;perimeter=pentagonPerimeter;fillColor=#f29d38;"
                       "strokeColor=#b36b00;fontColor=#ffffff;html=1;"),
        QSizeF(130, 100), QStringLiteral("kubernetes"));
    add(QStringLiteral("k8s.configmap"), QStringLiteral("ConfigMap"),
        QStringLiteral("shape=note;backgroundOutline=1;fillColor=#fff2cc;strokeColor=#d6b656;"
                       "html=1;"),
        QSizeF(120, 100), QStringLiteral("kubernetes"));
    add(QStringLiteral("k8s.node"), QStringLiteral("Node"),
        QStringLiteral("shape=cube;fillColor=#e1d5e7;strokeColor=#9673a6;boundedLbl=1;"
                       "backgroundOutline=1;html=1;"),
        QSizeF(130, 110), QStringLiteral("kubernetes"));

    // --- iOS ---------------------------------------------------------------
    add(QStringLiteral("ios.phone"), QStringLiteral("iPhone"),
        QStringLiteral("shape=ios.phone;fillColor=#1b1d1e;strokeColor=#5f6368;"),
        QSizeF(100, 200), QStringLiteral("software"));
    add(QStringLiteral("ios.tablet"), QStringLiteral("iPad"),
        QStringLiteral("shape=ios.phone;fillColor=#1b1d1e;strokeColor=#5f6368;"),
        QSizeF(160, 220), QStringLiteral("software"));
    add(QStringLiteral("ios.watch"), QStringLiteral("Apple Watch"),
        QStringLiteral("shape=ios.phone;fillColor=#1b1d1e;strokeColor=#5f6368;"),
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
    const QString key = shapeKey(cell);
    return key == QLatin1String("text") || key == QLatin1String("label");
}

bool isImageShape(const Cell &cell)
{
    return shapeKey(cell) == QLatin1String("image")
        || cell.styleValue(QStringLiteral("image")).startsWith(QLatin1String("data:"));
}

bool isLineArt(const Cell &cell)
{
    const QString key = shapeKey(cell);
    return key == QLatin1String("umlactor") || key == QLatin1String("actor")
        || key == QLatin1String("line") || key == QLatin1String("connector")
        || key.startsWith(QLatin1String("mxgraph.electrical."));
}

bool isContainerShape(const Cell &cell)
{
    const QString key = shapeKey(cell);
    return cell.styleFlag(QStringLiteral("swimlane"))
        || key == QLatin1String("swimlane") || key == QLatin1String("group");
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

    if (key == QLatin1String("ellipse") || key == QLatin1String("circle")
        || key == QLatin1String("doubleEllipse"))
    {
        path.addEllipse(rect);

        if (key == QLatin1String("doubleEllipse"))
        {
            const double inset = qMin(w, h) * 0.1;
            path.addEllipse(rect.adjusted(inset, inset, -inset, -inset));
        }

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
        const double inset = qMin(w * 0.2, h * 0.5);
        path.moveTo(point(inset, 0));
        path.lineTo(point(w - inset, 0));
        path.lineTo(point(w, h / 2));
        path.lineTo(point(w - inset, h));
        path.lineTo(point(inset, h));
        path.lineTo(point(0, h / 2));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("parallelogram"))
    {
        const double skew = qMin(w * 0.2, h * 0.8);
        path.moveTo(point(skew, 0));
        path.lineTo(point(w, 0));
        path.lineTo(point(w - skew, h));
        path.lineTo(point(0, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("trapezoid"))
    {
        const double inset = qMin(w * 0.2, h * 0.8);
        path.moveTo(point(inset, 0));
        path.lineTo(point(w - inset, 0));
        path.lineTo(point(w, h));
        path.lineTo(point(0, h));
        path.closeSubpath();
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

    if (key == QLatin1String("cloud") || key == QLatin1String("cloudCallout"))
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

        if (key == QLatin1String("cloudCallout"))
        {
            // Three bubbles in the foreground path.
            path.addEllipse(QPointF(px(15.1), py(48.8)), w * 6.0 / 90.41, h * 2.7 / 61.4);
            path.addEllipse(QPointF(px(8.1), py(55.3)), w * 3.8 / 90.41, h * 1.8 / 61.4);
            path.addEllipse(QPointF(px(2.4), py(60.1)), w * 2.4 / 90.41, h * 1.3 / 61.4);
        }

        return path;
    }

    if (key == QLatin1String("document"))
    {
        const double wave = qMin(h * 0.15, 20.0);
        path.moveTo(point(0, 0));
        path.lineTo(point(w, 0));
        path.lineTo(point(w, h - wave));
        path.cubicTo(point(w * 0.75, h - wave * 2.4), point(w * 0.25, h + wave * 0.6),
                     point(0, h - wave));
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
        const double fold = qMin(qMin(w, h) * 0.22, 24.0);
        path.moveTo(point(0, 0));
        path.lineTo(point(w - fold, 0));
        path.lineTo(point(w, fold));
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

    if (key == QLatin1String("umlactor") || key == QLatin1String("actor"))
    {
        const double headRadius = qMin(w * 0.25, h * 0.14);
        const double cx = w / 2;
        path.addEllipse(QPointF(x + cx, y + headRadius), headRadius, headRadius);
        const double bodyTop = headRadius * 2;
        const double bodyBottom = h * 0.7;
        path.addRect(QRectF(x + cx - 3, y + bodyTop, 6, bodyBottom - bodyTop));
        path.moveTo(point(cx - w * 0.3, y + bodyTop + (bodyBottom - bodyTop) * 0.2));
        path.lineTo(point(cx + w * 0.3, y + bodyTop + (bodyBottom - bodyTop) * 0.2));
        path.moveTo(point(cx, y + bodyBottom));
        path.lineTo(point(cx - w * 0.28, y + h));
        path.moveTo(point(cx, y + bodyBottom));
        path.lineTo(point(cx + w * 0.28, y + h));
        return path;
    }

    if (key == QLatin1String("singlearrow") || key == QLatin1String("arrow")
        || key == QLatin1String("doublearrow"))
    {
        const bool doubleHeaded = key == QLatin1String("doublearrow");
        const double shaftRatio = qBound(0.1, Style::number(style, QStringLiteral("arrowWidth"), 0.4), 0.9);
        const double headRatio = qBound(0.15, Style::number(style, QStringLiteral("arrowSize"), 0.35), 0.85);
        const double shaft = h * shaftRatio;
        const double headLength = w * headRatio;
        QPainterPath east;
        east.moveTo(0, (h - shaft) / 2.0);

        if (doubleHeaded)
        {
            east.lineTo(headLength, (h - shaft) / 2.0);
            east.lineTo(headLength, 0);
            east.lineTo(0, h / 2.0);
            east.lineTo(headLength, h);
            east.lineTo(headLength, (h + shaft) / 2.0);
            east.lineTo(w - headLength, (h + shaft) / 2.0);
            east.lineTo(w - headLength, h);
            east.lineTo(w, h / 2.0);
            east.lineTo(w - headLength, 0);
            east.lineTo(w - headLength, (h - shaft) / 2.0);
        }
        else
        {
            east.lineTo(w - headLength, (h - shaft) / 2.0);
            east.lineTo(w - headLength, 0);
            east.lineTo(w, h / 2.0);
            east.lineTo(w - headLength, h);
            east.lineTo(w - headLength, (h + shaft) / 2.0);
        }

        east.lineTo(0, (h + shaft) / 2.0);
        east.closeSubpath();

        const QString direction = Style::value(style, QStringLiteral("direction"),
                                               QStringLiteral("east")).toLower();
        QTransform transform;

        if (direction == QLatin1String("west"))
        {
            transform.translate(w, h);
            transform.rotate(180);
        }
        else if (direction == QLatin1String("north"))
        {
            transform.translate(0, h);
            transform.rotate(-90);
            transform.scale(1.0, h / qMax(w, 0.001));
        }
        else if (direction == QLatin1String("south"))
        {
            transform.translate(w, 0);
            transform.rotate(90);
            transform.scale(1.0, h / qMax(w, 0.001));
        }

        if (!transform.isIdentity())
        {
            return transform.map(east);
        }

        return east;
    }

    if (key == QLatin1String("process"))
    {
        path.addRect(rect);
        return path;
    }

    if (key == QLatin1String("swimlane") || key == QLatin1String("group"))
    {
        path.addRect(rect);
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
        path.addRect(rect);
        const double dx = qBound(0.0, Style::number(probe.style, QStringLiteral("dx"), 20.0), w);
        const double dy = qBound(0.0, Style::number(probe.style, QStringLiteral("dy"), 20.0), h);
        path.moveTo(point(0, dy));
        path.lineTo(point(w, dy));
        path.moveTo(point(dx, 0));
        path.lineTo(point(dx, h));
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

    if (key == QLatin1String("arrCalloutArrow") || key == QLatin1String("arrCalloutDouble")
        || key == QLatin1String("arrCalloutDouble90") || key == QLatin1String("arrCalloutQuad"))
    {
        const double bodyH = h * 0.68;
        path.addRoundedRect(QRectF(x, y, w, bodyH), 8, 8);

        QPolygonF tail;
        tail << QPointF(x + w * 0.28, y + bodyH * 0.94)
             << QPointF(x + w * 0.16, y + h)
             << QPointF(x + w * 0.46, y + bodyH * 0.9);
        path.addPolygon(tail);
        path.closeSubpath();

        if (key != QLatin1String("arrCalloutArrow"))
        {
            QPolygonF second;
            second << QPointF(x + w * 0.62, y + bodyH * 0.94)
                   << QPointF(x + w * 0.74, y + h)
                   << QPointF(x + w * 0.44, y + bodyH * 0.9);
            path.addPolygon(second);
            path.closeSubpath();
        }

        if (key == QLatin1String("arrCalloutDouble90"))
        {
            QPolygonF side;
            side << QPointF(x + w * 0.92, y + bodyH * 0.5)
                 << QPointF(x + w, y + bodyH * 0.72)
                 << QPointF(x + w * 0.84, y + bodyH * 0.72);
            path.addPolygon(side);
            path.closeSubpath();
        }

        if (key == QLatin1String("arrCalloutQuad"))
        {
            QPolygonF top;
            top << QPointF(x + w * 0.5, y - h * 0.02)
                << QPointF(x + w * 0.62, y + h * 0.16)
                << QPointF(x + w * 0.38, y + h * 0.16);
            path.addPolygon(top);
            path.closeSubpath();
        }

        return path;
    }

    if (key == QLatin1String("arrTriad"))
    {
        const QPointF center(w / 2.0, h / 2.0);

        for (int i = 0; i < 3; ++i)
        {
            const double angle = -M_PI / 2.0 + i * 2.0 * M_PI / 3.0;
            const QPointF tip(center.x() + std::cos(angle) * w * 0.46,
                              center.y() + std::sin(angle) * h * 0.46);
            const QPointF back(center.x() + std::cos(angle) * w * 0.18,
                               center.y() + std::sin(angle) * h * 0.18);
            const QPointF normal(-std::sin(angle), std::cos(angle));
            QPolygonF head;
            head << tip << back + normal * w * 0.12 << back - normal * w * 0.12;
            path.addPolygon(head);
            path.closeSubpath();
        }

        path.addEllipse(center, w * 0.1, h * 0.1);
        return path;
    }

    if (key == QLatin1String("arrTailed") || key == QLatin1String("arrSharp")
        || key == QLatin1String("arrSlender") || key == QLatin1String("arrSlenderTwo")
        || key == QLatin1String("arrNotched") || key == QLatin1String("arrSignalIn")
        || key == QLatin1String("arrNotchedSignalIn") || key == QLatin1String("arrJumpIn")
        || key == QLatin1String("arrQuad")
        || key == QLatin1String("arrUturn") || key == QLatin1String("arrBend")
        || key == QLatin1String("arrBendDouble") || key == QLatin1String("arrStriped")
        || key == QLatin1String("arrStylised"))
    {
        if (key == QLatin1String("arrQuad"))
        {
            const QPointF c(w / 2.0, h / 2.0);
            const double dx = w * 0.5;
            const double dy = h * 0.5;
            const double s = qMin(dx, dy) * 0.28;
            QPolygonF p;
            p << QPointF(c.x(), c.y() - s) << QPointF(c.x() + s, c.y() - s)
              << QPointF(c.x() + s, c.y() - dy) << QPointF(c.x() + 2 * s, c.y() - dy)
              << QPointF(c.x(), c.y() - 2 * s)
              << QPointF(c.x() - 2 * s, c.y() - dy) << QPointF(c.x() - s, c.y() - dy)
              << QPointF(c.x() - s, c.y() - s) << QPointF(c.x() - dx, c.y() - s)
              << QPointF(c.x() - dx, c.y() + s) << QPointF(c.x() - s, c.y() + s)
              << QPointF(c.x() - s, c.y() + dy) << QPointF(c.x() - 2 * s, c.y() + dy)
              << QPointF(c.x(), c.y() + 2 * s)
              << QPointF(c.x() + 2 * s, c.y() + dy) << QPointF(c.x() + s, c.y() + dy)
              << QPointF(c.x() + s, c.y() + s) << QPointF(c.x() + dx, c.y() + s);
            path.addPolygon(p);
            path.closeSubpath();
            return path;
        }

        if (key == QLatin1String("arrUturn"))
        {
            const double band = qMin(w, h) * 0.22;
            path.moveTo(point(0, 0));
            path.lineTo(point(w * 0.35, 0));
            path.lineTo(point(w * 0.35, h - band));
            path.lineTo(point(w * 0.75, h - band));
            path.lineTo(point(w * 0.75, h * 0.35));
            path.lineTo(point(w, h * 0.35));
            path.lineTo(point(w * 0.6, h * 0.05));
            path.lineTo(point(w * 0.2, h * 0.35));
            path.lineTo(point(w * 0.45, h * 0.35));
            path.lineTo(point(w * 0.45, h));
            path.lineTo(point(0, h));
            path.closeSubpath();
            return path;
        }

        if (key == QLatin1String("arrBend") || key == QLatin1String("arrBendDouble"))
        {
            path.moveTo(point(0, h * 0.2));
            path.lineTo(point(w * 0.6, h * 0.2));
            path.lineTo(point(w * 0.6, h * 0.55));
            path.lineTo(point(w * 0.85, h * 0.55));
            path.lineTo(point(w * 0.55, h));
            path.lineTo(point(w * 0.25, h * 0.55));
            path.lineTo(point(w * 0.4, h * 0.55));
            path.lineTo(point(w * 0.4, h * 0.4));
            path.lineTo(point(0, h * 0.4));
            path.closeSubpath();

            if (key == QLatin1String("arrBendDouble"))
            {
                path.moveTo(point(0, h * 0.05));
                path.lineTo(point(w * 0.2, h * 0.3));
                path.lineTo(point(0, h * 0.55));
                path.closeSubpath();
            }

            return path;
        }

        const double head = w * (key == QLatin1String("arrSlender") ? 0.25 : 0.3);
        const double shaft = h * (key == QLatin1String("arrSlender") ? 0.18 : 0.32);
        const double top = (h - shaft) / 2.0;

        path.moveTo(point(0, top));
        path.lineTo(point(w - head, top));
        path.lineTo(point(w - head, 0));
        path.lineTo(point(w, h / 2.0));
        path.lineTo(point(w - head, h));
        path.lineTo(point(w - head, h - top));
        path.lineTo(point(0, h - top));
        path.closeSubpath();

        if (key == QLatin1String("arrTailed") || key == QLatin1String("arrTailed"))
        {
            path.moveTo(point(0, top));
            path.lineTo(point(w * 0.15, h * 0.5));
            path.lineTo(point(0, h - top));
        }

        if (key == QLatin1String("arrNotched") || key == QLatin1String("arrNotchedSignalIn"))
        {
            path.moveTo(point(w * 0.15, top));
            path.lineTo(point(w * 0.05, h * 0.5));
            path.lineTo(point(w * 0.15, h - top));
        }

        if (key == QLatin1String("arrSlenderTwo"))
        {
            path.moveTo(point(head, top));
            path.lineTo(point(0, h / 2.0));
            path.lineTo(point(head, h - top));
            path.closeSubpath();
        }

        if (key == QLatin1String("arrStriped"))
        {
            for (int i = 0; i < 3; ++i)
            {
                path.moveTo(point(w * (0.2 + i * 0.12), top));
                path.lineTo(point(w * (0.2 + i * 0.12), h - top));
            }
        }

        if (key == QLatin1String("arrStylised"))
        {
            path.moveTo(point(0, top));
            path.lineTo(point(w * 0.2, h * 0.5));
            path.lineTo(point(0, h - top));
        }

        return path;
    }

    if (key == QLatin1String("arrWedge"))
    {
        path.moveTo(point(0, h));
        path.lineTo(point(w, h / 2.0));
        path.lineTo(point(0, 0));
        path.lineTo(point(w * 0.25, h / 2.0));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("arrChevron"))
    {
        path.moveTo(point(0, 0));
        path.lineTo(point(w * 0.5, 0));
        path.lineTo(point(w, h / 2.0));
        path.lineTo(point(w * 0.5, h));
        path.lineTo(point(0, h));
        path.lineTo(point(w * 0.4, h / 2.0));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("advTreeH") || key == QLatin1String("advTreeV")
        || key == QLatin1String("advTreeBalloon"))
    {
        const bool vertical = key == QLatin1String("advTreeV");
        const bool balloon = key == QLatin1String("advTreeBalloon");
        const double nodeW = balloon ? w * 0.22 : (vertical ? w * 0.3 : w * 0.28);
        const double nodeH = balloon ? h * 0.22 : (vertical ? h * 0.22 : h * 0.24);
        const QRectF root = balloon
            ? QRectF(x + (w - nodeW) / 2.0, y + (h - nodeH) / 2.0, nodeW, nodeH)
            : (vertical ? QRectF(x + (w - nodeW) / 2.0, y, nodeW, nodeH)
                        : QRectF(x, y + (h - nodeH) / 2.0, nodeW, nodeH));
        path.addRoundedRect(root, 5, 5);

        for (int i = 0; i < 3; ++i)
        {
            QRectF child;

            if (balloon)
            {
                const double angle = -M_PI / 2.0 + i * 2.0 * M_PI / 3.0;
                child = QRectF(root.center().x() + std::cos(angle) * w * 0.34 - nodeW / 2.0,
                               root.center().y() + std::sin(angle) * h * 0.32 - nodeH / 2.0,
                               nodeW, nodeH);
            }
            else if (vertical)
            {
                child = QRectF(x + w * (0.05 + i * 0.32), y + h - nodeH, nodeW, nodeH);
            }
            else
            {
                child = QRectF(x + w - nodeW, y + h * (0.03 + i * 0.34), nodeW, nodeH);
            }

            path.addRoundedRect(child, 5, 5);
            const QPointF from = root.center();
            const QPointF to = child.center();
            path.moveTo(from);
            path.lineTo(to);
        }

        return path;
    }

    if (key == QLatin1String("advFlowH") || key == QLatin1String("advFlowV"))
    {
        const bool vertical = key == QLatin1String("advFlowV");
        const double nodeW = vertical ? w * 0.52 : w * 0.18;
        const double nodeH = vertical ? h * 0.16 : h * 0.42;

        for (int i = 0; i < 4; ++i)
        {
            const QRectF node = vertical
                ? QRectF(x + (w - nodeW) / 2.0, y + h * (0.02 + i * 0.25), nodeW, nodeH)
                : QRectF(x + w * (0.02 + i * 0.25), y + (h - nodeH) / 2.0, nodeW, nodeH);
            path.addRoundedRect(node, 6, 6);

            if (i > 0)
            {
                const QPointF previous = vertical
                    ? QPointF(x + w / 2.0, y + h * (0.02 + (i - 1) * 0.25) + nodeH)
                    : QPointF(x + w * (0.02 + (i - 1) * 0.25) + nodeW, y + h / 2.0);
                const QPointF current = vertical
                    ? QPointF(x + w / 2.0, y + h * (0.02 + i * 0.25))
                    : QPointF(x + w * (0.02 + i * 0.25), y + h / 2.0);
                path.moveTo(previous);
                path.lineTo(current);
            }
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

    if (key == QLatin1String("cross"))
    {
        const double t = qMin(w, h) * 0.32;
        const double left = (w - t) / 2.0;
        const double top = (h - t) / 2.0;
        path.moveTo(point(left, 0));
        path.lineTo(point(left + t, 0));
        path.lineTo(point(left + t, top));
        path.lineTo(point(w, top));
        path.lineTo(point(w, top + t));
        path.lineTo(point(left + t, top + t));
        path.lineTo(point(left + t, h));
        path.lineTo(point(left, h));
        path.lineTo(point(left, top + t));
        path.lineTo(point(0, top + t));
        path.lineTo(point(0, top));
        path.lineTo(point(left, top));
        path.closeSubpath();
        return path;
    }

    // --- flowchart -------------------------------------------------------
    if (key == QLatin1String("mxgraph.flowchart.manual_input"))
    {
        path.moveTo(point(0, h));
        path.lineTo(point(0, h * 0.25));
        path.lineTo(point(w, 0));
        path.lineTo(point(w, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mxgraph.flowchart.manual_operation"))
    {
        path.moveTo(point(0, 0));
        path.lineTo(point(w, 0));
        path.lineTo(point(w * 0.85, h));
        path.lineTo(point(w * 0.15, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mxgraph.flowchart.delay"))
    {
        path.moveTo(point(0, 0));
        path.lineTo(point(w - h / 2.0, 0));
        path.arcTo(QRectF(x + w - h, y, h, h), 90, -180);
        path.lineTo(point(0, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mxgraph.flowchart.display"))
    {
        path.moveTo(point(w * 0.15, 0));
        path.lineTo(point(w, 0));
        path.lineTo(point(w, h));
        path.lineTo(point(0, h));
        path.lineTo(point(w * 0.15, 0));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mxgraph.flowchart.off_page_reference"))
    {
        path.moveTo(point(0, 0));
        path.lineTo(point(w, 0));
        path.lineTo(point(w, h * 0.72));
        path.lineTo(point(w / 2.0, h));
        path.lineTo(point(0, h * 0.72));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mxgraph.flowchart.merge_or_storage"))
    {
        path.moveTo(point(0, 0));
        path.lineTo(point(w, 0));
        path.lineTo(point(w / 2.0, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mxgraph.flowchart.extract_or_measurement"))
    {
        path.moveTo(point(w / 2.0, 0));
        path.lineTo(point(w, h));
        path.lineTo(point(0, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mxgraph.flowchart.collate"))
    {
        path.moveTo(point(0, 0));
        path.lineTo(point(w, 0));
        path.lineTo(point(w / 2.0, h / 2.0));
        path.lineTo(point(w, h));
        path.lineTo(point(0, h));
        path.lineTo(point(w / 2.0, h / 2.0));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mxgraph.flowchart.stored_data"))
    {
        path.moveTo(point(0, 0));
        path.lineTo(point(w * 0.82, 0));
        path.quadTo(point(w, 0), point(w, h / 2.0));
        path.quadTo(point(w, h), point(w * 0.82, h));
        path.lineTo(point(0, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mxgraph.flowchart.sequential_data"))
    {
        path.addRoundedRect(rect, h / 2.0, h / 2.0);
        return path;
    }

    if (key == QLatin1String("mxgraph.flowchart.loop_limit"))
    {
        path.moveTo(point(0, 0));
        path.lineTo(point(w * 0.8, 0));
        path.lineTo(point(w, h * 0.2));
        path.lineTo(point(w, h));
        path.lineTo(point(0, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mxgraph.flowchart.card"))
    {
        path.moveTo(point(w * 0.18, 0));
        path.lineTo(point(w, 0));
        path.lineTo(point(w, h));
        path.lineTo(point(0, h));
        path.lineTo(point(0, h * 0.18));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("mxgraph.flowchart.internal_storage"))
    {
        path.addRect(rect);
        return path;
    }

    if (key == QLatin1String("mxgraph.flowchart.annotation"))
    {
        path.moveTo(point(0, 0));
        path.lineTo(point(0, h));
        path.moveTo(point(0, 0));
        path.lineTo(point(w, 0));
        return path;
    }

    if (key == QLatin1String("mxgraph.flowchart.multi-document"))
    {
        path.addRect(QRectF(x, y + h * 0.12, w * 0.92, h * 0.8));
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

    if (key == QLatin1String("text") || key == QLatin1String("label")
        || key == QLatin1String("image"))
    {
        path.addRect(rect);
        return path;
    }

    // Default: rectangle, optionally rounded.
    if (probe.styleFlag(QStringLiteral("rounded")) || key == QLatin1String("rounded"))
    {
        const double radius = qMin(w, h) * arcSizeFor(style);
        path.addRoundedRect(rect, radius, radius);
        return path;
    }

    path.addRect(rect);
    return path;
}

void applyDefinition(Cell &cell, const ShapeDefinition &definition)
{
    cell.style = Style::parse(definition.style);
    cell.geometry.setSize(definition.size);
}

} // namespace ShapeRegistry
} // namespace dq
