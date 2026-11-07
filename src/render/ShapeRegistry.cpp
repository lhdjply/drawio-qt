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

    add(QStringLiteral("rectangle"), QStringLiteral("矩形"),
        QStringLiteral("rounded=0;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(140, 70), QStringLiteral("basic"));
    add(QStringLiteral("rounded"), QStringLiteral("圆角矩形"),
        QStringLiteral("rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;arcSize=20;"),
        QSizeF(140, 70), QStringLiteral("basic"));
    add(QStringLiteral("ellipse"), QStringLiteral("椭圆"),
        QStringLiteral("ellipse;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;"),
        QSizeF(140, 80), QStringLiteral("basic"));
    add(QStringLiteral("circle"), QStringLiteral("圆形"),
        QStringLiteral("ellipse;aspect=fixed;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;"),
        QSizeF(90, 90), QStringLiteral("basic"));
    add(QStringLiteral("diamond"), QStringLiteral("菱形"),
        QStringLiteral("rhombus;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;"),
        QSizeF(120, 90), QStringLiteral("basic"));
    add(QStringLiteral("triangle"), QStringLiteral("三角形"),
        QStringLiteral("triangle;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;"),
        QSizeF(110, 90), QStringLiteral("basic"));
    add(QStringLiteral("hexagon"), QStringLiteral("六边形"),
        QStringLiteral("shape=hexagon;perimeter=hexagonPerimeter2;whiteSpace=wrap;html=1;fixedSize=1;"
                       "fillColor=#e1d5e7;strokeColor=#9673a6;"),
        QSizeF(140, 80), QStringLiteral("basic"));
    add(QStringLiteral("parallelogram"), QStringLiteral("平行四边形"),
        QStringLiteral("shape=parallelogram;perimeter=parallelogramPerimeter;whiteSpace=wrap;html=1;"
                       "fixedSize=1;fillColor=#fff2cc;strokeColor=#d6b656;"),
        QSizeF(140, 70), QStringLiteral("basic"));
    add(QStringLiteral("trapezoid"), QStringLiteral("梯形"),
        QStringLiteral("shape=trapezoid;perimeter=trapezoidPerimeter;whiteSpace=wrap;html=1;"
                       "fixedSize=1;fillColor=#fff2cc;strokeColor=#d6b656;"),
        QSizeF(140, 70), QStringLiteral("basic"));
    add(QStringLiteral("pentagon"), QStringLiteral("五边形"),
        QStringLiteral("shape=pentagon;perimeter=pentagonPerimeter;whiteSpace=wrap;html=1;"
                       "fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(110, 100), QStringLiteral("basic"));
    add(QStringLiteral("octagon"), QStringLiteral("八边形"),
        QStringLiteral("shape=octagon;perimeter=octagonPerimeter;whiteSpace=wrap;html=1;"
                       "fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(110, 100), QStringLiteral("basic"));
    add(QStringLiteral("star"), QStringLiteral("星形"),
        QStringLiteral("shape=star;perimeter=starPerimeter;whiteSpace=wrap;html=1;"
                       "fillColor=#fff2cc;strokeColor=#d6b656;"),
        QSizeF(110, 100), QStringLiteral("basic"));
    add(QStringLiteral("cross"), QStringLiteral("十字"),
        QStringLiteral("shape=cross;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;"),
        QSizeF(100, 100), QStringLiteral("basic"));
    add(QStringLiteral("cube"), QStringLiteral("立方体"),
        QStringLiteral("shape=cube;whiteSpace=wrap;html=1;boundedLbl=1;backgroundOutline=1;"
                       "darkOpacity=0.05;fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(110, 100), QStringLiteral("basic"));
    add(QStringLiteral("doubleEllipse"), QStringLiteral("双椭圆"),
        QStringLiteral("shape=doubleEllipse;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;"),
        QSizeF(130, 80), QStringLiteral("basic"));
    add(QStringLiteral("cylinder"), QStringLiteral("数据库"),
        QStringLiteral("shape=cylinder3;whiteSpace=wrap;html=1;boundedLbl=1;backgroundOutline=1;size=15;"
                       "fillColor=#bac8d3;strokeColor=#23445d;"),
        QSizeF(110, 100), QStringLiteral("basic"));
    add(QStringLiteral("cloud"), QStringLiteral("云形"),
        QStringLiteral("ellipse;shape=cloud;whiteSpace=wrap;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;"),
        QSizeF(150, 100), QStringLiteral("basic"));
    add(QStringLiteral("document"), QStringLiteral("文档"),
        QStringLiteral("shape=document;whiteSpace=wrap;html=1;boundedLbl=1;fillColor=#f8cecc;strokeColor=#b85450;"),
        QSizeF(140, 90), QStringLiteral("basic"));
    add(QStringLiteral("note"), QStringLiteral("便签"),
        QStringLiteral("shape=note;whiteSpace=wrap;html=1;backgroundOutline=1;darkOpacity=0.05;"
                       "fillColor=#fff2cc;strokeColor=#d6b656;"),
        QSizeF(120, 100), QStringLiteral("basic"));
    add(QStringLiteral("callout"), QStringLiteral("标注"),
        QStringLiteral("shape=callout;whiteSpace=wrap;html=1;perimeter=calloutPerimeter;"
                       "fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(150, 90), QStringLiteral("basic"));
    add(QStringLiteral("swimlane"), QStringLiteral("泳道"),
        QStringLiteral("swimlane;html=1;startSize=23;fillColor=#f5f5f5;strokeColor=#666666;"),
        QSizeF(240, 160), QStringLiteral("basic"));
    add(QStringLiteral("text"), QStringLiteral("文本"),
        QStringLiteral("text;html=1;strokeColor=none;fillColor=none;align=center;"
                       "verticalAlign=middle;whiteSpace=wrap;rounded=0;"),
        QSizeF(120, 40), QStringLiteral("basic"));
    add(QStringLiteral("line"), QStringLiteral("直线"),
        QStringLiteral("line;strokeWidth=2;html=1;fillColor=none;strokeColor=#000000;"),
        QSizeF(120, 60), QStringLiteral("basic"));

    add(QStringLiteral("actor"), QStringLiteral("角色"),
        QStringLiteral("shape=umlActor;verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                       "outlineConnect=0;fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(60, 110), QStringLiteral("uml"));
    add(QStringLiteral("arrow"), QStringLiteral("箭头"),
        QStringLiteral("shape=singleArrow;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;"),
        QSizeF(140, 70), QStringLiteral("arrows"));
    add(QStringLiteral("process"), QStringLiteral("流程"),
        QStringLiteral("shape=process;whiteSpace=wrap;html=1;backgroundOutline=1;"
                       "fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(150, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("step"), QStringLiteral("步骤"),
        QStringLiteral("shape=step;perimeter=stepPerimeter;whiteSpace=wrap;html=1;fixedSize=1;"
                       "fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(150, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("terminator"), QStringLiteral("起止"),
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

    // --- flowchart -------------------------------------------------------
    add(QStringLiteral("decision"), QStringLiteral("判定"),
        QStringLiteral("rhombus;") + plain + orange, QSizeF(130, 90), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.data"), QStringLiteral("数据"),
        QStringLiteral("shape=parallelogram;perimeter=parallelogramPerimeter;fixedSize=1;")
            + plain + purple, QSizeF(140, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.document"), QStringLiteral("文档"),
        QStringLiteral("shape=document;boundedLbl=1;") + plain + blue,
        QSizeF(140, 90), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.multidocument"), QStringLiteral("多文档"),
        QStringLiteral("shape=mxgraph.flowchart.multi-document;") + plain + blue,
        QSizeF(140, 90), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.predefined"), QStringLiteral("预定义流程"),
        QStringLiteral("shape=process;backgroundOutline=1;") + plain + blue,
        QSizeF(150, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.preparation"), QStringLiteral("准备"),
        QStringLiteral("shape=hexagon;perimeter=hexagonPerimeter2;fixedSize=1;") + plain + purple,
        QSizeF(140, 80), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.manualinput"), QStringLiteral("手动输入"),
        QStringLiteral("shape=mxgraph.flowchart.manual_input;") + plain + yellow,
        QSizeF(140, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.manualoperation"), QStringLiteral("手动操作"),
        QStringLiteral("shape=mxgraph.flowchart.manual_operation;") + plain + yellow,
        QSizeF(140, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.delay"), QStringLiteral("延迟"),
        QStringLiteral("shape=mxgraph.flowchart.delay;") + plain + orange,
        QSizeF(130, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.display"), QStringLiteral("显示"),
        QStringLiteral("shape=mxgraph.flowchart.display;") + plain + blue,
        QSizeF(150, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.connector"), QStringLiteral("连接点"),
        QStringLiteral("shape=ellipse;") + plain + green,
        QSizeF(60, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.offpage"), QStringLiteral("跨页引用"),
        QStringLiteral("shape=mxgraph.flowchart.off_page_reference;") + plain + yellow,
        QSizeF(100, 100), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.merge"), QStringLiteral("合并"),
        QStringLiteral("shape=mxgraph.flowchart.merge_or_storage;") + plain + green,
        QSizeF(100, 80), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.extract"), QStringLiteral("提取"),
        QStringLiteral("shape=mxgraph.flowchart.extract_or_measurement;") + plain + green,
        QSizeF(100, 80), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.or"), QStringLiteral("或"),
        QStringLiteral("shape=ellipse;fcType=or;") + plain + green,
        QSizeF(70, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.summing"), QStringLiteral("汇总"),
        QStringLiteral("shape=ellipse;fcType=summing;") + plain + green,
        QSizeF(70, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.collate"), QStringLiteral("整理"),
        QStringLiteral("shape=mxgraph.flowchart.collate;") + plain + purple,
        QSizeF(90, 90), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.sort"), QStringLiteral("排序"),
        QStringLiteral("shape=rhombus;fcType=sort;") + plain + purple,
        QSizeF(100, 90), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.storeddata"), QStringLiteral("存储数据"),
        QStringLiteral("shape=mxgraph.flowchart.stored_data;") + plain + blue,
        QSizeF(140, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.internalstorage"), QStringLiteral("内部存储"),
        QStringLiteral("shape=mxgraph.flowchart.internal_storage;") + plain + blue,
        QSizeF(130, 80), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.annotation"), QStringLiteral("注释"),
        QStringLiteral("shape=mxgraph.flowchart.annotation;align=left;verticalAlign=top;")
            + plain, QSizeF(120, 60), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.looplimit"), QStringLiteral("循环上限"),
        QStringLiteral("shape=mxgraph.flowchart.loop_limit;") + plain + blue,
        QSizeF(130, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.card"), QStringLiteral("卡片"),
        QStringLiteral("shape=mxgraph.flowchart.card;") + plain + blue,
        QSizeF(130, 70), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.directdata"), QStringLiteral("直接数据"),
        QStringLiteral("shape=cylinder3;boundedLbl=1;backgroundOutline=1;size=15;")
            + plain + blue, QSizeF(120, 80), QStringLiteral("flowchart"));
    add(QStringLiteral("fc.sequentialdata"), QStringLiteral("顺序数据"),
        QStringLiteral("shape=mxgraph.flowchart.sequential_data;") + plain + blue,
        QSizeF(120, 80), QStringLiteral("flowchart"));

    // --- arrows ----------------------------------------------------------
    add(QStringLiteral("arrow.double"), QStringLiteral("双向箭头"),
        QStringLiteral("shape=doubleArrow;") + plain + green,
        QSizeF(150, 60), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.left"), QStringLiteral("左箭头"),
        QStringLiteral("shape=singleArrow;direction=west;") + plain + green,
        QSizeF(140, 70), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.up"), QStringLiteral("上箭头"),
        QStringLiteral("shape=singleArrow;direction=north;") + plain + green,
        QSizeF(80, 120), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.down"), QStringLiteral("下箭头"),
        QStringLiteral("shape=singleArrow;direction=south;") + plain + green,
        QSizeF(80, 120), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.chevron"), QStringLiteral("V 形箭头"),
        QStringLiteral("shape=step;perimeter=stepPerimeter;fixedSize=1;") + plain + blue,
        QSizeF(150, 60), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.notched"), QStringLiteral("凹口箭头"),
        QStringLiteral("shape=singleArrow;notch=0.3;") + plain + orange,
        QSizeF(150, 70), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.open"), QStringLiteral("空心箭头"),
        QStringLiteral("shape=singleArrow;arrowWidth=0.5;arrowSize=0.35;") + plain + yellow,
        QSizeF(150, 70), QStringLiteral("arrows"));
    add(QStringLiteral("arrow.thin"), QStringLiteral("细箭头"),
        QStringLiteral("shape=singleArrow;arrowWidth=0.2;arrowSize=0.4;") + plain + green,
        QSizeF(170, 50), QStringLiteral("arrows"));

    // --- UML -------------------------------------------------------------
    add(QStringLiteral("uml.class"), QStringLiteral("类"),
        QStringLiteral("swimlane;fontStyle=1;childLayout=stackLayout;horizontal=1;"
                       "startSize=26;") + blue + QStringLiteral("html=1;"),
        QSizeF(220, 140), QStringLiteral("uml"));
    add(QStringLiteral("uml.interface"), QStringLiteral("接口"),
        QStringLiteral("ellipse;") + plain + green, QSizeF(100, 100), QStringLiteral("uml"));
    add(QStringLiteral("uml.package"), QStringLiteral("包"),
        QStringLiteral("shape=mxgraph.uml.package;") + plain + yellow,
        QSizeF(150, 110), QStringLiteral("uml"));
    add(QStringLiteral("uml.component"), QStringLiteral("组件"),
        QStringLiteral("shape=mxgraph.uml.component;") + plain + blue,
        QSizeF(160, 100), QStringLiteral("uml"));
    add(QStringLiteral("uml.note"), QStringLiteral("UML 注释"),
        QStringLiteral("shape=note;backgroundOutline=1;") + plain + yellow,
        QSizeF(130, 100), QStringLiteral("uml"));
    add(QStringLiteral("uml.usecase"), QStringLiteral("用例"),
        QStringLiteral("ellipse;") + plain + green, QSizeF(150, 80), QStringLiteral("uml"));
    add(QStringLiteral("uml.state"), QStringLiteral("状态"),
        QStringLiteral("rounded=1;arcSize=40;") + plain + green,
        QSizeF(140, 70), QStringLiteral("uml"));
    add(QStringLiteral("uml.initial"), QStringLiteral("初始状态"),
        QStringLiteral("ellipse;fillColor=#000000;strokeColor=#000000;"),
        QSizeF(36, 36), QStringLiteral("uml"));
    add(QStringLiteral("uml.final"), QStringLiteral("终止状态"),
        QStringLiteral("shape=doubleEllipse;fillColor=#ffffff;strokeColor=#000000;"),
        QSizeF(44, 44), QStringLiteral("uml"));
    add(QStringLiteral("uml.decision"), QStringLiteral("UML 判定"),
        QStringLiteral("rhombus;") + plain + yellow, QSizeF(90, 90), QStringLiteral("uml"));
    add(QStringLiteral("uml.fork"), QStringLiteral("同步条"),
        QStringLiteral("rounded=0;fillColor=#000000;strokeColor=#000000;"),
        QSizeF(140, 12), QStringLiteral("uml"));
    add(QStringLiteral("uml.object"), QStringLiteral("对象"),
        QStringLiteral("swimlane;fontStyle=4;startSize=26;") + blue + QStringLiteral("html=1;"),
        QSizeF(200, 100), QStringLiteral("uml"));
    add(QStringLiteral("uml.activity"), QStringLiteral("活动"),
        QStringLiteral("rounded=1;arcSize=45;") + plain + green,
        QSizeF(150, 60), QStringLiteral("uml"));
    add(QStringLiteral("uml.node"), QStringLiteral("节点"),
        QStringLiteral("shape=cube;boundedLbl=1;backgroundOutline=1;darkOpacity=0.05;")
            + plain + blue, QSizeF(140, 100), QStringLiteral("uml"));
    add(QStringLiteral("uml.artifact"), QStringLiteral("制品"),
        QStringLiteral("shape=note;backgroundOutline=1;") + plain + blue,
        QSizeF(120, 90), QStringLiteral("uml"));
    add(QStringLiteral("uml.database"), QStringLiteral("UML 数据库"),
        QStringLiteral("shape=cylinder3;boundedLbl=1;backgroundOutline=1;size=15;")
            + plain + blue, QSizeF(110, 100), QStringLiteral("uml"));

    // --- ER --------------------------------------------------------------
    add(QStringLiteral("er.entity"), QStringLiteral("实体"),
        QStringLiteral("swimlane;fontStyle=1;startSize=28;") + green + QStringLiteral("html=1;"),
        QSizeF(180, 90), QStringLiteral("er"));
    add(QStringLiteral("er.weakentity"), QStringLiteral("弱实体"),
        QStringLiteral("shape=mxgraph.er.weak_entity;fontStyle=1;") + green,
        QSizeF(180, 90), QStringLiteral("er"));
    add(QStringLiteral("er.relationship"), QStringLiteral("关系"),
        QStringLiteral("rhombus;") + plain + orange, QSizeF(140, 90), QStringLiteral("er"));
    add(QStringLiteral("er.attribute"), QStringLiteral("属性"),
        QStringLiteral("ellipse;") + plain + yellow, QSizeF(130, 70), QStringLiteral("er"));
    add(QStringLiteral("er.keyattribute"), QStringLiteral("主键属性"),
        QStringLiteral("ellipse;fontStyle=4;") + plain + yellow,
        QSizeF(140, 70), QStringLiteral("er"));
    add(QStringLiteral("er.multivalued"), QStringLiteral("多值属性"),
        QStringLiteral("shape=doubleEllipse;") + plain + yellow,
        QSizeF(140, 80), QStringLiteral("er"));
    add(QStringLiteral("er.derived"), QStringLiteral("派生属性"),
        QStringLiteral("ellipse;dashed=1;") + plain + yellow,
        QSizeF(140, 70), QStringLiteral("er"));
    add(QStringLiteral("er.associative"), QStringLiteral("关联实体"),
        QStringLiteral("shape=rhombus;") + plain + purple,
        QSizeF(150, 100), QStringLiteral("er"));

    // --- BPMN ------------------------------------------------------------
    add(QStringLiteral("bpmn.start"), QStringLiteral("开始事件"),
        QStringLiteral("ellipse;strokeWidth=1.5;") + white,
        QSizeF(56, 56), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.intermediate"), QStringLiteral("中间事件"),
        QStringLiteral("shape=doubleEllipse;strokeWidth=1.5;") + white,
        QSizeF(64, 64), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.end"), QStringLiteral("结束事件"),
        QStringLiteral("ellipse;strokeWidth=4;") + white,
        QSizeF(56, 56), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.task"), QStringLiteral("任务"),
        QStringLiteral("rounded=1;arcSize=12;") + white, QSizeF(150, 80), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.usertask"), QStringLiteral("用户任务"),
        QStringLiteral("rounded=1;arcSize=12;bpmnType=user;") + white, QSizeF(150, 80), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.servicetask"), QStringLiteral("服务任务"),
        QStringLiteral("rounded=1;arcSize=12;bpmnType=service;") + white, QSizeF(150, 80), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.scripttask"), QStringLiteral("脚本任务"),
        QStringLiteral("rounded=1;arcSize=12;bpmnType=script;") + white, QSizeF(150, 80), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.subprocess"), QStringLiteral("子流程"),
        QStringLiteral("rounded=1;arcSize=12;bpmnType=subprocess;") + white, QSizeF(170, 90), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.exclusive"), QStringLiteral("排他网关"),
        QStringLiteral("rhombus;bpmnType=exclusive;") + white, QSizeF(80, 80), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.parallel"), QStringLiteral("并行网关"),
        QStringLiteral("rhombus;bpmnType=parallel;") + white, QSizeF(80, 80), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.inclusive"), QStringLiteral("包容网关"),
        QStringLiteral("rhombus;bpmnType=inclusive;") + white, QSizeF(80, 80), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.pool"), QStringLiteral("池"),
        QStringLiteral("swimlane;horizontal=0;startSize=30;") + white + QStringLiteral("html=1;"),
        QSizeF(420, 180), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.lane"), QStringLiteral("泳道"),
        QStringLiteral("swimlane;horizontal=0;startSize=26;") + white + QStringLiteral("html=1;"),
        QSizeF(420, 120), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.dataobject"), QStringLiteral("数据对象"),
        QStringLiteral("shape=note;backgroundOutline=1;") + white,
        QSizeF(70, 90), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.datastore"), QStringLiteral("数据存储"),
        QStringLiteral("shape=mxgraph.bpmn.data_store;") + white,
        QSizeF(100, 80), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.message"), QStringLiteral("消息"),
        QStringLiteral("shape=mxgraph.bpmn.message;") + white,
        QSizeF(90, 60), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.group"), QStringLiteral("分组"),
        QStringLiteral("rounded=1;dashed=1;fillColor=none;strokeColor=#666666;"),
        QSizeF(200, 140), QStringLiteral("bpmn"));
    add(QStringLiteral("bpmn.annotation"), QStringLiteral("BPMN 注释"),
        QStringLiteral("shape=mxgraph.flowchart.annotation;align=left;verticalAlign=top;")
            + plain, QSizeF(130, 60), QStringLiteral("bpmn"));

    // --- network ---------------------------------------------------------
    add(QStringLiteral("net.server"), QStringLiteral("服务器"),
        QStringLiteral("rounded=0;netType=server;") + blue, QSizeF(100, 120), QStringLiteral("network"));
    add(QStringLiteral("net.router"), QStringLiteral("路由器"),
        QStringLiteral("shape=cylinder3;boundedLbl=1;backgroundOutline=1;size=18;") + blue,
        QSizeF(120, 70), QStringLiteral("network"));
    add(QStringLiteral("net.switch"), QStringLiteral("交换机"),
        QStringLiteral("rounded=0;netType=switch;") + blue, QSizeF(130, 60), QStringLiteral("network"));
    add(QStringLiteral("net.firewall"), QStringLiteral("防火墙"),
        QStringLiteral("shape=mxgraph.networks.firewall;") + red,
        QSizeF(120, 80), QStringLiteral("network"));
    add(QStringLiteral("net.cloud"), QStringLiteral("网络云"),
        QStringLiteral("ellipse;shape=cloud;") + plain + purple,
        QSizeF(160, 100), QStringLiteral("network"));
    add(QStringLiteral("net.database"), QStringLiteral("网络数据库"),
        QStringLiteral("shape=cylinder3;boundedLbl=1;backgroundOutline=1;size=15;") + blue,
        QSizeF(100, 110), QStringLiteral("network"));
    add(QStringLiteral("net.storage"), QStringLiteral("存储"),
        QStringLiteral("shape=mxgraph.networks.storage;") + blue,
        QSizeF(100, 110), QStringLiteral("network"));
    add(QStringLiteral("net.workstation"), QStringLiteral("工作站"),
        QStringLiteral("rounded=0;netType=workstation;") + blue, QSizeF(120, 90), QStringLiteral("network"));
    add(QStringLiteral("net.laptop"), QStringLiteral("笔记本"),
        QStringLiteral("shape=mxgraph.networks.laptop;") + blue,
        QSizeF(130, 80), QStringLiteral("network"));
    add(QStringLiteral("net.phone"), QStringLiteral("电话"),
        QStringLiteral("rounded=1;arcSize=20;") + blue, QSizeF(60, 100), QStringLiteral("network"));
    add(QStringLiteral("net.printer"), QStringLiteral("打印机"),
        QStringLiteral("shape=mxgraph.networks.printer;") + blue,
        QSizeF(110, 80), QStringLiteral("network"));
    add(QStringLiteral("net.wireless"), QStringLiteral("无线接入点"),
        QStringLiteral("rounded=1;arcSize=15;netType=wireless;") + blue, QSizeF(90, 90), QStringLiteral("network"));
    add(QStringLiteral("net.vpn"), QStringLiteral("VPN 网关"),
        QStringLiteral("shape=mxgraph.networks.vpn;") + green,
        QSizeF(110, 110), QStringLiteral("network"));
    add(QStringLiteral("net.loadbalancer"), QStringLiteral("负载均衡"),
        QStringLiteral("ellipse;netType=loadbalancer;") + blue, QSizeF(100, 100), QStringLiteral("network"));
    add(QStringLiteral("net.users"), QStringLiteral("用户群"),
        QStringLiteral("shape=mxgraph.networks.users;") + blue,
        QSizeF(120, 90), QStringLiteral("network"));

    // --- data flow / C4 / UI kits ---------------------------------------
    add(QStringLiteral("dfd.process"), QStringLiteral("处理"),
        QStringLiteral("ellipse;") + green, QSizeF(120, 80), QStringLiteral("dfd"));
    add(QStringLiteral("dfd.datastore"), QStringLiteral("数据存储"),
        QStringLiteral("shape=mxgraph.dfd.data_store;") + blue,
        QSizeF(160, 50), QStringLiteral("dfd"));
    add(QStringLiteral("dfd.external"), QStringLiteral("外部实体"),
        QStringLiteral("rounded=0;") + yellow, QSizeF(140, 70), QStringLiteral("dfd"));
    add(QStringLiteral("dfd.processgane"), QStringLiteral("Gane 处理"),
        QStringLiteral("rounded=1;arcSize=10;") + green, QSizeF(140, 80), QStringLiteral("dfd"));

    add(QStringLiteral("c4.person"), QStringLiteral("C4 人员"),
        QStringLiteral("shape=umlActor;verticalLabelPosition=bottom;verticalAlign=top;")
            + blue, QSizeF(70, 120), QStringLiteral("c4"));
    add(QStringLiteral("c4.system"), QStringLiteral("C4 软件系统"),
        QStringLiteral("rounded=1;arcSize=6;") + blue, QSizeF(200, 120), QStringLiteral("c4"));
    add(QStringLiteral("c4.container"), QStringLiteral("C4 容器"),
        QStringLiteral("rounded=1;arcSize=6;") + green, QSizeF(200, 110), QStringLiteral("c4"));
    add(QStringLiteral("c4.component"), QStringLiteral("C4 组件"),
        QStringLiteral("rounded=1;arcSize=6;") + yellow, QSizeF(180, 100), QStringLiteral("c4"));

    add(QStringLiteral("ui.button"), QStringLiteral("按钮"),
        QStringLiteral("rounded=1;arcSize=20;") + blue, QSizeF(110, 40), QStringLiteral("mockup"));
    add(QStringLiteral("ui.input"), QStringLiteral("输入框"),
        QStringLiteral("rounded=0;fillColor=#ffffff;strokeColor=#999999;"),
        QSizeF(180, 40), QStringLiteral("mockup"));
    add(QStringLiteral("ui.window"), QStringLiteral("窗口"),
        QStringLiteral("swimlane;startSize=26;") + blue, QSizeF(280, 180),
        QStringLiteral("mockup"));
    add(QStringLiteral("ui.navbar"), QStringLiteral("导航栏"),
        QStringLiteral("rounded=0;fillColor=#f5f5f5;strokeColor=#cccccc;"),
        QSizeF(320, 50), QStringLiteral("mockup"));
    add(QStringLiteral("ui.browser"), QStringLiteral("浏览器"),
        QStringLiteral("swimlane;startSize=30;") + blue, QSizeF(320, 220),
        QStringLiteral("mockup"));

    add(QStringLiteral("android.phone"), QStringLiteral("Android 手机"),
        QStringLiteral("rounded=1;arcSize=8;androidType=phone;fillColor=#a4c639;strokeColor=#4a6b14;"),
        QSizeF(100, 190), QStringLiteral("software"));
    add(QStringLiteral("android.tablet"), QStringLiteral("Android 平板"),
        QStringLiteral("rounded=1;arcSize=6;androidType=tablet;fillColor=#a4c639;strokeColor=#4a6b14;"),
        QSizeF(160, 220), QStringLiteral("software"));
    add(QStringLiteral("android.watch"), QStringLiteral("Android 手表"),
        QStringLiteral("rounded=1;arcSize=30;androidType=watch;fillColor=#a4c639;strokeColor=#4a6b14;"),
        QSizeF(100, 100), QStringLiteral("software"));

    add(QStringLiteral("image"), QStringLiteral("图片"),
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
        {"elec.resistor", "电阻", "elec.resistor", false, QSizeF(120, 50)},
        {"elec.resistorus", "电阻（锯齿）", "elec.resistorus", false, QSizeF(120, 60)},
        {"elec.capacitor", "电容", "elec.capacitor", false, QSizeF(80, 70)},
        {"elec.capacitorpolar", "极性电容", "elec.capacitorpolar", false, QSizeF(80, 70)},
        {"elec.inductor", "电感", "elec.inductor", false, QSizeF(120, 50)},
        {"elec.diode", "二极管", "elec.diode", true, QSizeF(110, 60)},
        {"elec.zener", "稳压二极管", "elec.zener", true, QSizeF(110, 70)},
        {"elec.led", "发光二极管", "elec.led", true, QSizeF(110, 70)},
        {"elec.transistornpn", "NPN 三极管", "elec.transistornpn", false, QSizeF(100, 100)},
        {"elec.transistorpnp", "PNP 三极管", "elec.transistorpnp", false, QSizeF(100, 100)},
        {"elec.battery", "电池", "elec.battery", false, QSizeF(120, 60)},
        {"elec.ground", "接地", "elec.ground", false, QSizeF(80, 70)},
        {"elec.groundchassis", "机壳地", "elec.groundchassis", false, QSizeF(80, 70)},
        {"elec.lamp", "灯泡", "elec.lamp", false, QSizeF(80, 80)},
        {"elec.motor", "电机", "elec.motor", false, QSizeF(90, 90)},
        {"elec.switchspst", "开关（单刀单掷）", "elec.switchspst", false, QSizeF(120, 60)},
        {"elec.pushbutton", "按钮", "elec.pushbutton", false, QSizeF(120, 70)},
        {"elec.transformer", "变压器", "elec.transformer", false, QSizeF(130, 90)},
        {"elec.fuse", "保险丝", "elec.fuse", false, QSizeF(120, 50)},
        {"elec.acsource", "交流源", "elec.acsource", false, QSizeF(90, 90)},
        {"elec.voltagesource", "电压源", "elec.voltagesource", false, QSizeF(90, 90)},
        {"elec.ammeter", "电流表", "elec.ammeter", false, QSizeF(80, 80)},
        {"elec.voltmeter", "电压表", "elec.voltmeter", false, QSizeF(80, 80)},
        {"elec.antenna", "天线", "elec.antenna", false, QSizeF(100, 100)},
        {"elec.speaker", "扬声器", "elec.speaker", false, QSizeF(100, 80)},
        {"elec.microphone", "麦克风", "elec.microphone", false, QSizeF(80, 100)},
        {"elec.opamp", "运算放大器", "elec.opamp", true, QSizeF(110, 100)},
        {"elec.gate.and", "与门", "elec.gate.and", true, QSizeF(120, 80)},
        {"elec.gate.or", "或门", "elec.gate.or", true, QSizeF(120, 80)},
        {"elec.gate.not", "非门", "elec.gate.not", true, QSizeF(110, 80)},
        {"elec.gate.nand", "与非门", "elec.gate.nand", true, QSizeF(130, 80)},
        {"elec.gate.nor", "或非门", "elec.gate.nor", true, QSizeF(130, 80)},
        {"elec.gate.xor", "异或门", "elec.gate.xor", true, QSizeF(130, 80)},
    };

    for (const ElecDef &entry : electrical)
    {
        add(QString::fromLatin1(entry.key), QString::fromUtf8(entry.name),
            QStringLiteral("shape=%1;%2").arg(QString::fromLatin1(entry.shape),
                                             entry.filled ? solidArt : lineArt),
            entry.size, QStringLiteral("electrical"));
    }

    add(QStringLiteral("elec.junction"), QStringLiteral("节点"),
        QStringLiteral("shape=elec.junction;fillColor=#000000;strokeColor=#000000;"),
        QSizeF(24, 24), QStringLiteral("electrical"));

    // --- floor plan -------------------------------------------------------
    add(QStringLiteral("floor.wall"), QStringLiteral("墙体"),
        QStringLiteral("rounded=0;fillColor=#e8eaed;strokeColor=#5f6368;strokeWidth=2;"),
        QSizeF(240, 16), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.door"), QStringLiteral("门"),
        QStringLiteral("shape=floor.door;fillColor=none;strokeColor=#5f6368;strokeWidth=2;"),
        QSizeF(90, 90), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.window"), QStringLiteral("窗"),
        QStringLiteral("shape=floor.window;fillColor=#dae8fc;strokeColor=#5f6368;strokeWidth=2;"),
        QSizeF(140, 16), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.stairs"), QStringLiteral("楼梯"),
        QStringLiteral("shape=floor.stairs;fillColor=#f5f5f5;strokeColor=#5f6368;"),
        QSizeF(120, 160), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.table"), QStringLiteral("桌子"),
        QStringLiteral("rounded=1;fillColor=#fff2cc;strokeColor=#d6b656;"),
        QSizeF(160, 90), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.chair"), QStringLiteral("椅子"),
        QStringLiteral("rounded=1;fillColor=#dae8fc;strokeColor=#6c8ebf;"),
        QSizeF(60, 60), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.bed"), QStringLiteral("床"),
        QStringLiteral("rounded=1;fillColor=#d5e8d4;strokeColor=#82b366;"),
        QSizeF(180, 120), QStringLiteral("floorplan"));
    add(QStringLiteral("floor.sofa"), QStringLiteral("沙发"),
        QStringLiteral("rounded=1;fillColor=#e1d5e7;strokeColor=#9673a6;"),
        QSizeF(180, 80), QStringLiteral("floorplan"));

    // --- infographic ------------------------------------------------------
    add(QStringLiteral("info.chevron"), QStringLiteral("信息图箭头"),
        QStringLiteral("shape=step;perimeter=stepPerimeter;fixedSize=1;fillColor=#1a73e8;"
                       "strokeColor=none;fontColor=#ffffff;html=1;"),
        QSizeF(180, 70), QStringLiteral("infographic"));
    add(QStringLiteral("info.ribbon"), QStringLiteral("缎带"),
        QStringLiteral("shape=info.ribbon;fillColor=#f29900;strokeColor=none;"
                       "fontColor=#ffffff;html=1;"),
        QSizeF(180, 70), QStringLiteral("infographic"));
    add(QStringLiteral("info.badge"), QStringLiteral("徽章"),
        QStringLiteral("shape=star;perimeter=starPerimeter;fillColor=#d93025;strokeColor=none;"
                       "fontColor=#ffffff;html=1;"),
        QSizeF(110, 110), QStringLiteral("infographic"));
    add(QStringLiteral("info.bubble"), QStringLiteral("气泡"),
        QStringLiteral("shape=callout;perimeter=calloutPerimeter;fillColor=#e6f4ea;"
                       "strokeColor=#34a853;html=1;"),
        QSizeF(180, 100), QStringLiteral("infographic"));
    add(QStringLiteral("info.progress"), QStringLiteral("进度条"),
        QStringLiteral("rounded=1;arcSize=50;fillColor=#1a73e8;strokeColor=none;"),
        QSizeF(240, 24), QStringLiteral("infographic"));

    // --- SysML ------------------------------------------------------------
    add(QStringLiteral("sysml.block"), QStringLiteral("模块"),
        QStringLiteral("swimlane;fontStyle=1;startSize=28;fillColor=#dae8fc;"
                       "strokeColor=#6c8ebf;html=1;"),
        QSizeF(200, 120), QStringLiteral("sysml"));
    add(QStringLiteral("sysml.port"), QStringLiteral("端口"),
        QStringLiteral("fillColor=#ffffff;strokeColor=#6c8ebf;"), QSizeF(24, 24), QStringLiteral("sysml"));
    add(QStringLiteral("sysml.requirement"), QStringLiteral("需求"),
        QStringLiteral("shape=note;backgroundOutline=1;fillColor=#fff2cc;strokeColor=#d6b656;"
                       "html=1;"), QSizeF(180, 100), QStringLiteral("sysml"));
    add(QStringLiteral("sysml.constraint"), QStringLiteral("约束"),
        QStringLiteral("rounded=0;fillColor=#f8cecc;strokeColor=#b85450;html=1;"),
        QSizeF(170, 90), QStringLiteral("sysml"));
    add(QStringLiteral("sysml.item"), QStringLiteral("条目"),
        QStringLiteral("rounded=0;fillColor=#e1d5e7;strokeColor=#9673a6;html=1;"),
        QSizeF(140, 70), QStringLiteral("sysml"));

    // --- ArchiMate --------------------------------------------------------
    add(QStringLiteral("archimate.actor"), QStringLiteral("业务参与者"),
        QStringLiteral("rounded=1;arcSize=30;fillColor=#fff2cc;strokeColor=#d6b656;html=1;"),
        QSizeF(140, 70), QStringLiteral("archimate"));
    add(QStringLiteral("archimate.role"), QStringLiteral("业务角色"),
        QStringLiteral("rounded=1;arcSize=30;fillColor=#fff2cc;strokeColor=#d6b656;html=1;"),
        QSizeF(140, 70), QStringLiteral("archimate"));
    add(QStringLiteral("archimate.process"), QStringLiteral("业务流程"),
        QStringLiteral("rounded=1;arcSize=30;fillColor=#fff2cc;strokeColor=#d6b656;html=1;"),
        QSizeF(160, 70), QStringLiteral("archimate"));
    add(QStringLiteral("archimate.appcomponent"), QStringLiteral("应用组件"),
        QStringLiteral("rounded=1;arcSize=30;fillColor=#dae8fc;strokeColor=#6c8ebf;html=1;"),
        QSizeF(160, 70), QStringLiteral("archimate"));
    add(QStringLiteral("archimate.node"), QStringLiteral("技术节点"),
        QStringLiteral("shape=cube;fillColor=#d5e8d4;strokeColor=#82b366;boundedLbl=1;"
                       "backgroundOutline=1;html=1;"),
        QSizeF(160, 100), QStringLiteral("archimate"));
    add(QStringLiteral("archimate.device"), QStringLiteral("设备"),
        QStringLiteral("rounded=0;fillColor=#d5e8d4;strokeColor=#82b366;html=1;"),
        QSizeF(150, 80), QStringLiteral("archimate"));

    // --- Enterprise integration patterns ---------------------------------
    add(QStringLiteral("eip.channel"), QStringLiteral("消息通道"),
        QStringLiteral("rounded=0;fillColor=#dae8fc;strokeColor=#6c8ebf;html=1;"),
        QSizeF(200, 24), QStringLiteral("eip"));
    add(QStringLiteral("eip.router"), QStringLiteral("消息路由"),
        QStringLiteral("shape=eip.router;fillColor=#ffffff;strokeColor=#000000;html=1;"),
        QSizeF(180, 90), QStringLiteral("eip"));
    add(QStringLiteral("eip.splitter"), QStringLiteral("拆分器"),
        QStringLiteral("shape=eip.splitter;fillColor=#ffffff;strokeColor=#000000;html=1;"),
        QSizeF(160, 90), QStringLiteral("eip"));
    add(QStringLiteral("eip.aggregator"), QStringLiteral("聚合器"),
        QStringLiteral("shape=eip.splitter;direction=west;fillColor=#ffffff;"
                       "strokeColor=#000000;html=1;"),
        QSizeF(160, 90), QStringLiteral("eip"));

    // --- Bootstrap --------------------------------------------------------
    add(QStringLiteral("bs.button"), QStringLiteral("Bootstrap 按钮"),
        QStringLiteral("rounded=1;arcSize=25;fillColor=#0d6efd;strokeColor=#0d6efd;"
                       "fontColor=#ffffff;html=1;"),
        QSizeF(120, 42), QStringLiteral("bootstrap"));
    add(QStringLiteral("bs.panel"), QStringLiteral("Bootstrap 面板"),
        QStringLiteral("swimlane;startSize=34;fillColor=#ffffff;strokeColor=#dee2e6;html=1;"),
        QSizeF(240, 160), QStringLiteral("bootstrap"));
    add(QStringLiteral("bs.navbar"), QStringLiteral("Bootstrap 导航栏"),
        QStringLiteral("rounded=0;fillColor=#212529;strokeColor=#212529;fontColor=#ffffff;html=1;"),
        QSizeF(320, 48), QStringLiteral("bootstrap"));
    add(QStringLiteral("bs.badge"), QStringLiteral("Bootstrap 徽标"),
        QStringLiteral("rounded=1;arcSize=50;fillColor=#6c757d;strokeColor=#6c757d;"
                       "fontColor=#ffffff;html=1;"),
        QSizeF(70, 28), QStringLiteral("bootstrap"));
    add(QStringLiteral("bs.alert"), QStringLiteral("Bootstrap 提示"),
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
        const double rotation = Style::number(style, QStringLiteral("rotation"), 0.0);

        if (qAbs(qAbs(rotation) - 90.0) < 1.0)
        {
            // The cell is a tall thin box rotated by ±90; draw the coil across
            // its height so the rotation turns it into a vertical coil.
            const double length = h;
            const double left = (w - length) / 2.0;
            const double cy = h / 2.0;
            const double bump = length * 0.24;

            path.moveTo(QPointF(x + left, y + cy));
            path.lineTo(QPointF(x + left + length * 0.04, y + cy));

            for (int i = 0; i < 4; ++i)
            {
                path.arcTo(QRectF(x + left + length * 0.04 + i * bump,
                                  y + cy - bump / 2.0, bump, bump),
                           180, -180);
            }

            path.moveTo(QPointF(x + left + length * 0.96, y + cy));
            path.lineTo(QPointF(x + left + length, y + cy));
            return path;
        }

        const double cy = h / 2.0;
        const double bump = w * 0.2;
        path.moveTo(point(0, cy));
        path.lineTo(point(w * 0.1, cy));

        for (int i = 0; i < 4; ++i)
        {
            path.arcTo(QRectF(x + w * 0.1 + i * bump, y + cy - bump / 2.0,
                              bump, bump),
                       180, -180);
        }

        path.moveTo(point(w * 0.9, cy));
        path.lineTo(point(w, cy));
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
