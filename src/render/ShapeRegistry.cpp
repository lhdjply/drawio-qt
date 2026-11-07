#include "render/ShapeRegistry.h"

#include <QPainterPath>
#include <QtMath>

namespace dq {
namespace ShapeRegistry {
namespace {

QString shapeKey(const Cell &cell)
{
    const QString shape = cell.styleValue(QStringLiteral("shape")).toLower();

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
    return {
        {QStringLiteral("rectangle"), QStringLiteral("矩形"),
         QStringLiteral("rounded=0;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;"),
         QSizeF(140, 70)},
        {QStringLiteral("rounded"), QStringLiteral("圆角矩形"),
         QStringLiteral("rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;arcSize=20;"),
         QSizeF(140, 70)},
        {QStringLiteral("ellipse"), QStringLiteral("椭圆"),
         QStringLiteral("ellipse;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;"),
         QSizeF(140, 80)},
        {QStringLiteral("circle"), QStringLiteral("圆形"),
         QStringLiteral("ellipse;aspect=fixed;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;"),
         QSizeF(90, 90)},
        {QStringLiteral("diamond"), QStringLiteral("菱形"),
         QStringLiteral("rhombus;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;"),
         QSizeF(120, 90)},
        {QStringLiteral("triangle"), QStringLiteral("三角形"),
         QStringLiteral("triangle;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;"),
         QSizeF(110, 90)},
        {QStringLiteral("hexagon"), QStringLiteral("六边形"),
         QStringLiteral("shape=hexagon;perimeter=hexagonPerimeter2;whiteSpace=wrap;html=1;fixedSize=1;"
                        "fillColor=#e1d5e7;strokeColor=#9673a6;"),
         QSizeF(140, 80)},
        {QStringLiteral("cylinder"), QStringLiteral("数据库"),
         QStringLiteral("shape=cylinder3;whiteSpace=wrap;html=1;boundedLbl=1;backgroundOutline=1;size=15;"
                        "fillColor=#bac8d3;strokeColor=#23445d;"),
         QSizeF(110, 100)},
        {QStringLiteral("cloud"), QStringLiteral("云形"),
         QStringLiteral("ellipse;shape=cloud;whiteSpace=wrap;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;"),
         QSizeF(150, 100)},
        {QStringLiteral("parallelogram"), QStringLiteral("平行四边形"),
         QStringLiteral("shape=parallelogram;perimeter=parallelogramPerimeter;whiteSpace=wrap;html=1;"
                        "fixedSize=1;fillColor=#fff2cc;strokeColor=#d6b656;"),
         QSizeF(140, 70)},
        {QStringLiteral("trapezoid"), QStringLiteral("梯形"),
         QStringLiteral("shape=trapezoid;perimeter=trapezoidPerimeter;whiteSpace=wrap;html=1;"
                        "fixedSize=1;fillColor=#fff2cc;strokeColor=#d6b656;"),
         QSizeF(140, 70)},
        {QStringLiteral("document"), QStringLiteral("文档"),
         QStringLiteral("shape=document;whiteSpace=wrap;html=1;boundedLbl=1;fillColor=#f8cecc;strokeColor=#b85450;"),
         QSizeF(140, 90)},
        {QStringLiteral("note"), QStringLiteral("便签"),
         QStringLiteral("shape=note;whiteSpace=wrap;html=1;backgroundOutline=1;darkOpacity=0.05;"
                        "fillColor=#fff2cc;strokeColor=#d6b656;"),
         QSizeF(120, 100)},
        {QStringLiteral("callout"), QStringLiteral("标注"),
         QStringLiteral("shape=callout;whiteSpace=wrap;html=1;perimeter=calloutPerimeter;"
                        "fillColor=#dae8fc;strokeColor=#6c8ebf;"),
         QSizeF(150, 90)},
        {QStringLiteral("actor"), QStringLiteral("角色"),
         QStringLiteral("shape=umlActor;verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                        "outlineConnect=0;fillColor=#dae8fc;strokeColor=#6c8ebf;"),
         QSizeF(60, 110)},
        {QStringLiteral("arrow"), QStringLiteral("箭头"),
         QStringLiteral("shape=singleArrow;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;"),
         QSizeF(140, 70)},
        {QStringLiteral("process"), QStringLiteral("流程"),
         QStringLiteral("shape=process;whiteSpace=wrap;html=1;backgroundOutline=1;"
                        "fillColor=#dae8fc;strokeColor=#6c8ebf;"),
         QSizeF(150, 70)},
        {QStringLiteral("step"), QStringLiteral("步骤"),
         QStringLiteral("shape=step;perimeter=stepPerimeter;whiteSpace=wrap;html=1;fixedSize=1;"
                        "fillColor=#dae8fc;strokeColor=#6c8ebf;"),
         QSizeF(150, 70)},
        {QStringLiteral("terminator"), QStringLiteral("起止"),
         QStringLiteral("rounded=1;arcSize=50;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;"),
         QSizeF(140, 60)},
        {QStringLiteral("swimlane"), QStringLiteral("泳道"),
         QStringLiteral("swimlane;html=1;startSize=23;fillColor=#f5f5f5;strokeColor=#666666;"),
         QSizeF(240, 160)},
        {QStringLiteral("text"), QStringLiteral("文本"),
         QStringLiteral("text;html=1;strokeColor=none;fillColor=none;align=center;"
                        "verticalAlign=middle;whiteSpace=wrap;rounded=0;"),
         QSizeF(120, 40)},
        {QStringLiteral("line"), QStringLiteral("直线"),
         QStringLiteral("line;strokeWidth=2;html=1;fillColor=none;strokeColor=#000000;"),
         QSizeF(120, 60)},
        {QStringLiteral("image"), QStringLiteral("图片"),
         QStringLiteral("shape=image;verticalLabelPosition=bottom;verticalAlign=top;imageAspect=0;"
                        "html=1;strokeColor=none;fillColor=none;"),
         QSizeF(140, 100)},
    };
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
        || key == QLatin1String("line") || key == QLatin1String("connector");
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
        || key == QLatin1String("doubleellipse"))
    {
        path.addEllipse(rect);

        if (key == QLatin1String("doubleellipse"))
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
        path.moveTo(point(w / 2, 0));
        path.lineTo(point(w, h));
        path.lineTo(point(0, h));
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
        const double inset = qMin(w * 0.12, h * 0.4);
        path.moveTo(point(inset, 0));
        path.lineTo(point(w - inset, 0));
        path.lineTo(point(w, h));
        path.lineTo(point(0, h));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("cylinder3") || key == QLatin1String("cylinder")
        || key == QLatin1String("database"))
    {
        const double cap = qMin(h * 0.18, 26.0);
        path.moveTo(point(0, cap / 2));
        path.cubicTo(point(0, 0), point(w, 0), point(w, cap / 2));
        path.lineTo(point(w, h - cap / 2));
        path.cubicTo(point(w, h), point(0, h), point(0, h - cap / 2));
        path.closeSubpath();
        return path;
    }

    if (key == QLatin1String("cloud"))
    {
        const double cw = w;
        const double ch = h;
        path.moveTo(point(cw * 0.25, ch * 0.35));
        path.cubicTo(point(cw * 0.05, ch * 0.35), point(cw * 0.0, ch * 0.6),
                     point(cw * 0.2, ch * 0.68));
        path.cubicTo(point(cw * 0.15, ch * 0.92), point(cw * 0.45, ch), point(cw * 0.55, ch * 0.82));
        path.cubicTo(point(cw * 0.65, ch), point(cw * 0.95, ch * 0.92), point(cw * 0.92, ch * 0.7));
        path.cubicTo(point(cw * 1.05, ch * 0.55), point(cw * 0.95, ch * 0.32),
                     point(cw * 0.75, ch * 0.35));
        path.cubicTo(point(cw * 0.68, ch * 0.12), point(cw * 0.35, ch * 0.08),
                     point(cw * 0.25, ch * 0.35));
        path.closeSubpath();
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
        const double tail = qMin(qMin(w, h) * 0.3, 36.0);
        const double bodyHeight = h - tail;
        path.addRoundedRect(QRectF(x, y, w, bodyHeight), 6, 6);
        path.moveTo(point(w * 0.25, bodyHeight));
        path.lineTo(point(w * 0.35, bodyHeight));
        path.lineTo(point(w * 0.2, h));
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

    if (key == QLatin1String("singlearrow") || key == QLatin1String("arrow"))
    {
        const double shaft = h * 0.4;
        const double headStart = w * 0.6;
        path.moveTo(point(0, (h - shaft) / 2));
        path.lineTo(point(headStart, (h - shaft) / 2));
        path.lineTo(point(headStart, 0));
        path.lineTo(point(w, h / 2));
        path.lineTo(point(headStart, h));
        path.lineTo(point(headStart, (h + shaft) / 2));
        path.lineTo(point(0, (h + shaft) / 2));
        path.closeSubpath();
        return path;
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
        const double depth = qMin(qMin(w, h) * 0.18, 20.0);
        path.moveTo(point(0, depth));
        path.lineTo(point(depth, 0));
        path.lineTo(point(w, 0));
        path.lineTo(point(w, h - depth));
        path.lineTo(point(w - depth, h));
        path.lineTo(point(0, h));
        path.closeSubpath();
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
