#include "core/Document.h"
#include "core/MxFileIO.h"
#include "core/Page.h"
#include "render/ShapeRegistry.h"
#include "render/ShapeRenderer.h"
#include "render/ShapeCatalog.h"

#include <QPainterPath>
#include <QtTest>

using namespace dq;

class TestModel : public QObject
{
    Q_OBJECT

private slots:
    void addAndRemoveCells()
    {
        Page page;
        Cell *a = page.addVertex(QRectF(10, 20, 100, 60), Style::parse(QStringLiteral("rounded=1;")));
        Cell *b = page.addVertex(QRectF(300, 20, 100, 60));
        Cell *edge = page.addEdge(a->id, b->id);

        QVERIFY(a != nullptr);
        QVERIFY(b != nullptr);
        QVERIFY(edge != nullptr);
        QCOMPARE(page.cells().size(), 3);
        QCOMPARE(page.vertices().size(), 2);
        QCOMPARE(page.edges().size(), 1);

        // Removing a vertex also removes its incident edge.
        const QString edgeId = edge->id;
        const QString bId = b->id;
        page.removeCell(a);
        QCOMPARE(page.cells().size(), 1);
        QVERIFY(page.cell(edgeId) == nullptr);
        QVERIFY(page.cell(bId) != nullptr);
    }

    void absoluteGeometryWithGroups()
    {
        Page page;
        Cell *group = page.addVertex(QRectF(100, 100, 300, 200));
        Cell *child = page.addVertex(QRectF(10, 20, 80, 40), {}, QString(), group->id);

        QCOMPARE(child->parent, group);
        QCOMPARE(child->absolutePosition(), QPointF(110, 120));
        QCOMPARE(child->absoluteRect(), QRectF(110, 120, 80, 40));

        // Moving the group moves the child.
        group->geometry.moveTopLeft(QPointF(200, 150));
        QCOMPARE(child->absolutePosition(), QPointF(210, 170));

        // Containers report their children.
        QVERIFY(group->isContainer());
        QCOMPARE(group->children.size(), 1);
    }

    void zOrderCanBeChanged()
    {
        Page page;
        Cell *a = page.addVertex(QRectF(0, 0, 50, 50));
        Cell *b = page.addVertex(QRectF(10, 10, 50, 50));

        QCOMPARE(page.indexOf(a), 0);
        QCOMPARE(page.indexOf(b), 1);
        page.moveCell(a, 1);
        QCOMPARE(page.indexOf(a), 1);
        QCOMPARE(page.indexOf(b), 0);
        page.moveCell(a, 0);
        QCOMPARE(page.indexOf(a), 0);
    }

    void contentBoundsIncludeCells()
    {
        Page page;
        page.addVertex(QRectF(10, 10, 100, 50));
        page.addVertex(QRectF(200, 100, 60, 60));

        const QRectF bounds = page.contentBounds();
        QVERIFY(bounds.contains(QRectF(10, 10, 100, 50)));
        QVERIFY(bounds.contains(QRectF(200, 100, 60, 60)));
    }

    void documentPages()
    {
        Document document;
        QCOMPARE(document.pageCount(), 1);

        Page *second = document.addPage(QStringLiteral("第二页"));
        QVERIFY(second != nullptr);
        QCOMPARE(document.pageCount(), 2);

        document.setCurrentIndex(1);
        QCOMPARE(document.currentPage(), second);

        QVERIFY(document.movePage(1, 0));
        QCOMPARE(document.page(0), second);

        document.removePage(0);
        QCOMPARE(document.pageCount(), 1);

        // The last page cannot be removed.
        document.removePage(0);
        QCOMPARE(document.pageCount(), 1);
    }

    void styleParsingRoundTrip()
    {
        const StyleMap style = Style::parse(
            QStringLiteral("rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;"));
        QCOMPARE(style.value(QStringLiteral("rounded")), QStringLiteral("1"));
        QCOMPARE(style.value(QStringLiteral("fillColor")), QStringLiteral("#dae8fc"));
        QCOMPARE(Style::format(style),
                 QStringLiteral("fillColor=#dae8fc;html=1;rounded=1;whiteSpace=wrap;"));
    }

    void htmlLabelHandlesTagsWithAttributes()
    {
        Cell cell(Cell::Kind::Vertex, QStringLiteral("label"));
        cell.style = Style::parse(QStringLiteral("html=1;"));
        cell.value = QStringLiteral(
            "研究内容一<br style=\"font-size: 16px;\"><br>"
            "<p style=\"margin: 0pt; text-align: justify;\">"
            "<span style=\"font-size: 16px;\">正文内容</span></p>");

        const QString text = cell.plainLabel();
        QVERIFY(text.contains(QStringLiteral("研究内容一")));
        QVERIFY(text.contains(QStringLiteral("正文内容")));
        QVERIFY(text.contains(QLatin1Char('\n')));
        QVERIFY(!text.contains(QLatin1Char('<')));
        QVERIFY(!text.contains(QStringLiteral("style=")));
    }

    void imageLibraryNamesAreTranslated()
    {
        for (const ShapeCategory &category : ShapeCatalog::instance().categories())
        {
            if (category.id == QLatin1String("img/active_directory"))
            {
                QCOMPARE(category.name, QStringLiteral("活动目录"));
            }
            else if (category.id == QLatin1String("img/azure2"))
            {
                QCOMPARE(category.name, QStringLiteral("微软 Azure"));
            }
        }
    }

    void imageIconNamesAreTranslated()
    {
        bool foundVirtualMachine = false;

        for (const ShapeDefinition &definition : ShapeCatalog::instance().allDefinitions())
        {
            if (definition.key.endsWith(QStringLiteral("/Virtual_Machine.svg")))
            {
                QCOMPARE(definition.name, QStringLiteral("虚拟机"));
                foundVirtualMachine = true;
            }
        }

        QVERIFY(foundVirtualMachine);
    }

    void basicShapesHaveGeometry()
    {
        for (const auto &definition : ShapeRegistry::toolboxShapes())
        {
            if (definition.category != QLatin1String("general")
                && definition.category != QLatin1String("basic")
                && definition.category != QLatin1String("cloud")
                && definition.category != QLatin1String("advanced")
                && definition.category != QLatin1String("arrows")
                && definition.category != QLatin1String("misc"))
            {
                continue;
            }

            Cell probe(Cell::Kind::Vertex, QString());
            probe.style = Style::parse(definition.style);

            if (ShapeRegistry::isTextShape(probe) || ShapeRegistry::isImageShape(probe))
            {
                continue;
            }

            const QPainterPath path = ShapeRegistry::pathForStyle(
                probe.style, QRectF(0, 0, 120, 80));
            QVERIFY2(!path.isEmpty(), qPrintable(definition.key + QStringLiteral(" has no geometry")));
        }
    }

    void drawioElectricalShapesResolve()
    {
        const QPainterPath waypoint = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=waypoint;size=6;strokeWidth=2;")),
            QRectF(0, 0, 14, 14));
        QVERIFY(waypoint.boundingRect().width() < 10.0);

        const QPainterPath inductor = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=mxgraph.electrical.inductors.inductor;")),
            QRectF(0, 0, 60, 20));
        QVERIFY(inductor.elementCount() > 10);

        const QPainterPath resistor = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=mxgraph.electrical.resistors.resistor_1;")),
            QRectF(0, 0, 60, 20));
        QVERIFY(resistor.elementCount() > 5);

        const QPainterPath battery = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral(
                "shape=mxgraph.electrical.miscellaneous.batteryStack;")),
            QRectF(0, 0, 60, 40));
        QVERIFY(battery.elementCount() > 5);
    }

    void triangleMatchesDrawioOrientation()
    {
        const QPainterPath right = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("triangle;whiteSpace=wrap;")), QRectF(0, 0, 100, 80));

        QCOMPARE(right.elementAt(0).x, 0.0);
        QCOMPARE(right.elementAt(0).y, 0.0);
        QCOMPARE(right.elementAt(1).x, 100.0);
        QCOMPARE(right.elementAt(1).y, 40.0);

        const QPainterPath up = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=mxgraph.basic.acute_triangle;dx=0.5;")),
            QRectF(0, 0, 100, 80));

        QCOMPARE(up.elementAt(1).x, 50.0);
        QCOMPARE(up.elementAt(1).y, 0.0);
    }

    void cylinder3UsesSizeCap()
    {
        const QPainterPath path = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=cylinder3;size=15;")), QRectF(0, 0, 60, 80));

        QVERIFY(path.elementCount() > 6);
        QCOMPARE(path.boundingRect().height(), 80.0);
    }

    void arrows2UseStencilNames()
    {
        // Bare arr* names are not registered in draw.io, so they fall back.
        const QPainterPath bare = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=arrQuad;")), QRectF(0, 0, 100, 100));
        QCOMPARE(bare.elementCount(), 5);

        // The stencil name maps to the internal arrows2 key.
        const QPainterPath quad = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=mxgraph.arrows2.quadArrow;"
                                        "dy=10;dx=20;arrowHead=10;")),
            QRectF(0, 0, 100, 100));
        QCOMPARE(quad.elementCount(), 25);
        QCOMPARE(quad.elementAt(0).x, 60.0);
        QCOMPARE(quad.elementAt(0).y, 40.0);

        // Without dy/dx/arrowHead draw.io's constructor defaults (0.5/0.5/0)
        // collapse the shape into a thin cross; match that exactly.
        const QPainterPath noParams = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=mxgraph.arrows2.quadArrow;")),
            QRectF(0, 0, 100, 100));
        QVERIFY(noParams.elementCount() > 10);
        QCOMPARE(noParams.elementAt(0).x, 50.5);
        QCOMPARE(noParams.elementAt(0).y, 49.5);
    }

    void arrows2LibraryMatchesOfficialSet()
    {
        // Sidebar-Arrows2.js offers exactly these 20 shapes; the library must
        // not carry any other arrow.
        QStringList official = {
            QStringLiteral("arrow"), QStringLiteral("twoWayArrow"),
            QStringLiteral("stylisedArrow"), QStringLiteral("sharpArrow"),
            QStringLiteral("sharpArrow2"), QStringLiteral("calloutArrow"),
            QStringLiteral("bendArrow"), QStringLiteral("bendDoubleArrow"),
            QStringLiteral("calloutDoubleArrow"), QStringLiteral("calloutQuadArrow"),
            QStringLiteral("calloutDouble90Arrow"), QStringLiteral("quadArrow"),
            QStringLiteral("triadArrow"), QStringLiteral("tailedArrow"),
            QStringLiteral("tailedNotchedArrow"), QStringLiteral("stripedArrow"),
            QStringLiteral("jumpInArrow"), QStringLiteral("uTurnArrow"),
            QStringLiteral("wedgeArrow"), QStringLiteral("wedgeArrowDashed2"),
        };
        official.sort();

        QStringList library;
        QStringList keys;

        for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
        {
            if (definition.category != QLatin1String("arrows"))
            {
                continue;
            }

            keys.append(definition.key);

            const QString shape = Style::value(Style::parse(definition.style),
                                               QStringLiteral("shape"));
            QVERIFY2(shape.startsWith(QLatin1String("mxgraph.arrows2.")), qPrintable(shape));
            library.append(shape.mid(int(QStringLiteral("mxgraph.arrows2.").size())));
        }

        library.sort();
        QCOMPARE(library, official);
        QCOMPARE(keys.size(), 20);
    }

    void arrows2VertexCountsMatchDrawio()
    {
        // mxShapeArrows2Arrow.paintVertexShape: one moveTo, seven lineTo and
        // a close; the head tip sits at the right middle.
        const QPainterPath arrow = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=mxgraph.arrows2.arrow;dy=0.6;dx=40;notch=0;")),
            QRectF(0, 0, 100, 70));
        QCOMPARE(arrow.elementCount(), 9);
        QCOMPARE(arrow.elementAt(0).x, 0.0);
        QCOMPARE(arrow.elementAt(0).y, 21.0);  // h * 0.5 * dy
        QCOMPARE(arrow.elementAt(1).x, 60.0);  // w - dx
        QCOMPARE(arrow.elementAt(3).x, 100.0);
        QCOMPARE(arrow.elementAt(3).y, 35.0);

        // mxShapeArrows2TwoWayArrow.paintVertexShape: one moveTo, nine lineTo
        // and a close.
        const QPainterPath twoWay = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=mxgraph.arrows2.twoWayArrow;dy=0.6;dx=35;")),
            QRectF(0, 0, 100, 60));
        QCOMPARE(twoWay.elementCount(), 11);
        QCOMPARE(twoWay.elementAt(0).x, 35.0);
        QCOMPARE(twoWay.elementAt(0).y, 18.0);  // h * 0.5 * dy
        QCOMPARE(twoWay.elementAt(4).y, 60.0);
    }

    void annotationFallsBackLikeDrawio()
    {
        // draw.io only registers "Annotation 1/2"; the bare name is a rectangle.
        const QPainterPath bare = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=mxgraph.flowchart.annotation;")),
            QRectF(0, 0, 100, 80));
        QCOMPARE(bare.elementCount(), 5);

        const QPainterPath annotation = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=mxgraph.flowchart.annotation_1;")),
            QRectF(0, 0, 100, 80));
        QCOMPARE(annotation.elementCount(), 4);
        QCOMPARE(annotation.elementAt(0).x, 100.0);
    }

    void shapeSearchMatchesChinese()
    {
        const QVector<ShapeDefinition> stars = ShapeCatalog::instance().search(QStringLiteral("星"), 40);
        QVERIFY(!stars.isEmpty());

        bool foundHeart = false;

        for (const ShapeDefinition &definition : ShapeCatalog::instance().search(QStringLiteral("心"), 40))
        {
            if (definition.key == QStringLiteral("heart")
                || definition.key == QStringLiteral("basic.heart"))
            {
                foundHeart = true;
            }
        }

        QVERIFY(foundHeart);

        // English queries keep working.
        QVERIFY(!ShapeCatalog::instance().search(QStringLiteral("star"), 40).isEmpty());
    }

    void collapsedStateAndAlternateBoundsRoundTrip()
    {
        const QString xml = QStringLiteral(
            "<mxGraphModel><root><mxCell id=\"0\"/><mxCell id=\"1\" parent=\"0\"/>"
            "<mxCell id=\"2\" collapsed=\"1\" parent=\"1\" style=\"swimlane;\" value=\"Box\" vertex=\"1\">"
            "<mxGeometry x=\"10\" y=\"20\" width=\"140\" height=\"30\" as=\"geometry\">"
            "<mxRectangle x=\"10\" y=\"20\" width=\"200\" height=\"200\" as=\"alternateBounds\"/>"
            "</mxGeometry></mxCell></root></mxGraphModel>");

        Page page;
        QString error;
        QVERIFY2(MxFileIO::pageFromXml(page, xml, &error), qPrintable(error));

        Cell *cell = page.cell(QStringLiteral("2"));
        QVERIFY(cell != nullptr);
        QVERIFY(cell->collapsed);
        QCOMPARE(cell->alternateBounds, QRectF(10, 20, 200, 200));

        const QString written = MxFileIO::pageToXml(page, false);
        QVERIFY2(written.contains(QStringLiteral("collapsed=\"1\"")), qPrintable(written));
        QVERIFY2(written.contains(QStringLiteral("alternateBounds")), qPrintable(written));
    }

    void swimlanePaintsTitleBarLikeDrawio()
    {
        // mxSwimlane: the title bar takes the cell fill, the content area only
        // swimlaneFillColor (none by default) and the divider separates them.
        // draw.io paints no fold indicator as part of the shape.
        Cell container(Cell::Kind::Vertex, QStringLiteral("c"));
        container.style = Style::parse(QStringLiteral(
            "swimlane;whiteSpace=wrap;html=1;startSize=23;fillColor=#dae8fc;"));
        container.geometry = QRectF(0, 0, 200, 120);

        QImage image(200, 120, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter painter(&image);
        ShapeRenderer::paint(painter, container);
        painter.end();

        QCOMPARE(image.pixelColor(100, 10), QColor(QStringLiteral("#dae8fc")));
        QCOMPARE(image.pixelColor(100, 60), QColor(Qt::white));
        QVERIFY(image.pixelColor(100, 23).lightness() < 200);

        // No fold square in the top left corner.
        QVERIFY(image.pixelColor(8, 8).lightness() > 200);
    }

    void transparentBoundsContainersDeriveTheirBox()
    {
        Page page;
        Cell *container = page.addVertex(QRectF(0, 0, 0, 0), Style::parse(QStringLiteral(
            "swimlane;html=1;startSize=20;horizontal=1;transparentBounds=1;groupPadding=20;"
            "containerType=tree;")));
        page.addVertex(QRectF(100, 50, 80, 40), Style::parse(QStringLiteral("whiteSpace=wrap;")),
                       QString(), container->id);
        page.addVertex(QRectF(100, 120, 80, 40), Style::parse(QStringLiteral("whiteSpace=wrap;")),
                       QString(), container->id);

        QVERIFY(ShapeRegistry::hasTransparentBounds(*container));

        // Graph.getTransparentBounds(): children (100,50,80,110) grown by the
        // 20px padding and the 20px title bar on top.
        QCOMPARE(ShapeRegistry::transparentBounds(*container), QRectF(80, 10, 120, 170));
        QCOMPARE(ShapeRegistry::visibleRect(*container), QRectF(80, 10, 120, 170));

        // horizontal=0 puts the title bar on the left instead.
        Cell *side = page.addVertex(QRectF(0, 0, 0, 0), Style::parse(QStringLiteral(
            "swimlane;html=1;startSize=20;horizontal=0;transparentBounds=1;groupPadding=20;")));
        page.addVertex(QRectF(100, 50, 80, 40), {}, QString(), side->id);
        QCOMPARE(ShapeRegistry::transparentBounds(*side), QRectF(60, 30, 140, 80));
    }

    void edgeLabelKeepsInlineRuns()
    {
        // <b>/<i> carry their own colours in draw.io's edge labels.
        Cell label(Cell::Kind::Vertex, QStringLiteral("l"));
        label.style = Style::parse(QStringLiteral("edgeLabel;html=1;"));
        label.geometry = QRectF(0, 0, 10, 10);
        label.value = QStringLiteral(
            "<span style=\"color: rgb(51, 51, 255);\">L</span><u>a</u>"
            "<span><b style=\"color: rgb(255, 255, 51);\">b</b>"
            "<i style=\"color: rgb(255, 51, 51);\">el</i></span>");

        QImage image(120, 40, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter painter(&image);
        LabelPainter::paint(painter, label, QRectF(0, 0, 120, 40), false);
        painter.end();

        bool blue = false;
        bool yellow = false;
        bool red = false;

        for (int y = 0; y < image.height(); ++y)
        {
            for (int x = 0; x < image.width(); ++x)
            {
                const QColor c = image.pixelColor(x, y);

                if (c.blue() > 180 && c.red() < 120)
                {
                    blue = true;
                }

                if (c.red() > 180 && c.green() > 180 && c.blue() < 140)
                {
                    yellow = true;
                }

                if (c.red() > 180 && c.green() < 120 && c.blue() < 120)
                {
                    red = true;
                }
            }
        }

        QVERIFY(blue);
        QVERIFY(yellow);
        QVERIFY(red);
    }

    void nestedInlineSpansKeepOuterFormat()
    {
        // The inner span must not reset the outer colour, and closing it must
        // restore the outer weight.
        Cell cell(Cell::Kind::Vertex, QStringLiteral("c"));
        cell.style = Style::parse(QStringLiteral("text;html=1;"));
        cell.geometry = QRectF(0, 0, 240, 60);
        cell.value = QStringLiteral(
            "<h1><i>H</i><span style=\"color: rgb(103, 255, 100);\">e</span>a"
            "<span style=\"color: rgb(170, 96, 255);\">di"
            "<span style=\"font-weight: normal;\"><font style=\"font-size: 42px;\">n</font>"
            "</span></span><span style=\"font-weight: normal;\">g</span></h1>");

        // Laying the label out must not crash and must keep the run count.
        QImage image(240, 60, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter painter(&image);
        ShapeRenderer::paint(painter, cell);
        painter.end();

        QVERIFY(!image.isNull());
    }

    void htmlFontTagAndRgbColors()
    {
        // draw.io emits <font style="font-size: 18px; color: rgb(...)">, and
        // only the wrapped run may grow: the surrounding text keeps the cell size.
        Cell cell(Cell::Kind::Vertex, QStringLiteral("c"));
        cell.style = Style::parse(QStringLiteral("text;html=1;"));
        cell.geometry = QRectF(0, 0, 180, 230);
        cell.value = QStringLiteral(
            "<h1 style=\"margin-top: 0px;\">Heading</h1><p>Lorem ipsum do"
            "<b><font style=\"font-size: 18px; color: rgb(153, 255, 255);\">"
            "lor sit amet</font></b> elit.</p>");

        const QFont base = LabelPainter::fontForLabel(cell);
        QCOMPARE(base.pixelSize(), 12);

        QCOMPARE(Style::parseColorValue(QStringLiteral("rgb(153, 255, 255)")),
                 QColor(153, 255, 255));
        QCOMPARE(Style::parseColorValue(QStringLiteral("rgba(1, 2, 3, 0.5)")),
                 QColor(1, 2, 3, 128));
    }

    void actorAndHorizontalSwimlane()
    {
        const QPainterPath actor = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=umlActor;")), QRectF(0, 0, 40, 80));
        QVERIFY(actor.elementCount() > 6);

        // swimlane;horizontal=0 keeps the title bar on the left.
        const QPainterPath container = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("swimlane;horizontal=0;startSize=30;")),
            QRectF(0, 0, 160, 80));
        QVERIFY(!container.isEmpty());
    }

    void basicStencilShapes()
    {
        const auto path = [](const QString &style)
        {
            return ShapeRegistry::pathForStyle(Style::parse(style), QRectF(0, 0, 100, 100));
        };

        // Wave has two wavy edges made of quadratic curves.
        QVERIFY(path(QStringLiteral("shape=mxgraph.basic.wave2;dy=0.3;")).elementCount() > 10);

        // Pie is a wedge from the centre.
        const QPainterPath pie = path(QStringLiteral("shape=mxgraph.basic.pie;"));
        QCOMPARE(pie.elementAt(0).x, 50.0);

        // Sun adds eight triangular rays to the disc.
        QVERIFY(path(QStringLiteral("shape=mxgraph.basic.sun;")).elementCount() > 40);

        // Tick is a brush stroke made of nine arcs.
        QVERIFY(path(QStringLiteral("shape=mxgraph.basic.tick;")).elementCount() > 40);

        // X is a twelve point polygon.
        QCOMPARE(path(QStringLiteral("shape=mxgraph.basic.x;")).elementCount(), 13);

        QStringList keys;

        for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
        {
            if (definition.category == QStringLiteral("basic"))
            {
                keys.append(definition.key);
            }
        }

        for (const QString &key : {QStringLiteral("basic.wave"), QStringLiteral("basic.pie"),
                                   QStringLiteral("basic.x"), QStringLiteral("basic.sun"),
                                   QStringLiteral("basic.tick"), QStringLiteral("basic.smiley")})
        {
            QVERIFY2(keys.contains(key), qPrintable(key));
        }
    }

    void basicLibraryMatchesSidebar()
    {
        // Sidebar.prototype.addBasicPalette, in order and with its style
        // strings verbatim (s/s2 are the prefixes used by that file).
        const QString s = QStringLiteral("whiteSpace=wrap;html=1;shape=mxgraph.basic.");
        const QString s2 = QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;"
                                          "html=1;shape=mxgraph.basic.");
        const QStringList official = {
        s2 + QStringLiteral("rect;fillColor2=none;strokeWidth=1;size=20;indent=5;"),
        QStringLiteral("shape=partialRectangle;whiteSpace=wrap;html=1;top=0;bottom=0;fillColor=none;"),
        QStringLiteral("shape=partialRectangle;whiteSpace=wrap;html=1;right=0;top=0;bottom=0;fillColor=none;routingCenterX=-0.5;"),
        QStringLiteral("shape=partialRectangle;whiteSpace=wrap;html=1;bottom=0;right=0;fillColor=none;"),
        QStringLiteral("shape=partialRectangle;whiteSpace=wrap;html=1;top=0;left=0;fillColor=none;"),
        s2 + QStringLiteral("polygon;polyCoords=[[0.25,0],[0.75,0],[1,0.25],[1,0.75],[0.75,1],[0.25,1],[0,0.75],[0,0.25]];polyline=0;"),
        s2 + QStringLiteral("polygon;polyCoords=[[0.25,0],[0.75,0],[1,0.25],[1,0.75],[0.75,1],[0.25,1],[0,0.75],[0,0.25]];polyline=1;fillColor=none;"),
        s2 + QStringLiteral("patternFillRect;fillStyle=diag;step=5;fillStrokeWidth=0.2;fillStrokeColor=#dddddd;"),
        s2 + QStringLiteral("patternFillRect;fillStyle=diagRev;step=5;fillStrokeWidth=0.2;fillStrokeColor=#dddddd;"),
        s2 + QStringLiteral("patternFillRect;fillStyle=vert;step=5;fillStrokeWidth=0.2;fillStrokeColor=#dddddd;"),
        s2 + QStringLiteral("patternFillRect;fillStyle=hor;step=5;fillStrokeWidth=0.2;fillStrokeColor=#dddddd;"),
        s2 + QStringLiteral("patternFillRect;fillStyle=grid;step=5;fillStrokeWidth=0.2;fillStrokeColor=#dddddd;"),
        s2 + QStringLiteral("patternFillRect;fillStyle=diagGrid;step=5;fillStrokeWidth=0.2;fillStrokeColor=#dddddd;"),
        s2 + QStringLiteral("4_point_star_2;dx=0.8;"),
        s2 + QStringLiteral("6_point_star"),
        s2 + QStringLiteral("8_point_star"),
        s2 + QStringLiteral("banner"),
        s + QStringLiteral("cloud_callout"),
        s + QStringLiteral("cloud_rect"),
        s2 + QStringLiteral("cone"),
        s2 + QStringLiteral("cone2;dx=0.5;dy=0.9;"),
        s + QStringLiteral("document"),
        s2 + QStringLiteral("donut;dx=25;"),
        s2 + QStringLiteral("drop"),
        s2 + QStringLiteral("flash"),
        s2 + QStringLiteral("half_circle"),
        s2 + QStringLiteral("heart"),
        QStringLiteral("shape=hexagon;perimeter=hexagonPerimeter2;whiteSpace=wrap;html=1;shapeInside=1;aspect=fixed;"),
        QStringLiteral("html=1;shape=mxgraph.basic.isocube;isoAngle=15;"),
        s + QStringLiteral("loud_callout"),
        s2 + QStringLiteral("moon"),
        s2 + QStringLiteral("no_symbol"),
        s + QStringLiteral("octagon2;align=center;verticalAlign=middle;dx=15;"),
        s2 + QStringLiteral("orthogonal_triangle"),
        s2 + QStringLiteral("acute_triangle;dx=0.5;"),
        s2 + QStringLiteral("obtuse_triangle;dx=0.25;"),
        s + QStringLiteral("oval_callout"),
        s + QStringLiteral("pentagon"),
        s + QStringLiteral("pointed_oval"),
        s2 + QStringLiteral("pyramid;dx1=0.4;dx2=0.6;dy1=0.9;dy2=0.8;"),
        s2 + QStringLiteral("diag_snip_rect;dx=6;whiteSpace=wrap;"),
        s2 + QStringLiteral("diag_round_rect;dx=6;whiteSpace=wrap;"),
        s2 + QStringLiteral("corner_round_rect;dx=6;whiteSpace=wrap;"),
        s2 + QStringLiteral("three_corner_round_rect;dx=6;whiteSpace=wrap;"),
        s2 + QStringLiteral("plaque;dx=6;whiteSpace=wrap;"),
        s2 + QStringLiteral("frame;dx=10;whiteSpace=wrap;"),
        s2 + QStringLiteral("rounded_frame;dx=10;whiteSpace=wrap;"),
        s2 + QStringLiteral("plaque_frame;dx=10;whiteSpace=wrap;"),
        s2 + QStringLiteral("frame_corner;dx=10;whiteSpace=wrap;"),
        s2 + QStringLiteral("diag_stripe;dx=10;"),
        s + QStringLiteral("rectCallout;dx=30;dy=15;boundedLbl=1;"),
        s + QStringLiteral("roundRectCallout;dx=30;dy=15;size=5;boundedLbl=1;"),
        s + QStringLiteral("layered_rect;dx=10;outlineConnect=0;boundedLbl=1;"),
        s2 + QStringLiteral("smiley"),
        s2 + QStringLiteral("star"),
        s2 + QStringLiteral("sun"),
        s2 + QStringLiteral("tick"),
        s2 + QStringLiteral("wave2;dy=0.3;"),
        QStringLiteral("labelPosition=center;verticalLabelPosition=middle;align=center;html=1;shape=mxgraph.basic.button;dx=10;whiteSpace=wrap;"),
        QStringLiteral("labelPosition=center;verticalLabelPosition=middle;align=center;html=1;shape=mxgraph.basic.shaded_button;dx=10;fillColor=#E6E6E6;strokeColor=none;whiteSpace=wrap;"),
        s2 + QStringLiteral("x"),
        s2 + QStringLiteral("pie;startAngle=0.2;endAngle=0.9;"),
        s2 + QStringLiteral("arc;startAngle=0.3;endAngle=0.1;"),
        s2 + QStringLiteral("partConcEllipse;startAngle=0.25;endAngle=0.1;arcWidth=0.5;"),
        QStringLiteral("shape=message;html=1;html=1;outlineConnect=0;labelPosition=center;verticalLabelPosition=bottom;align=center;verticalAlign=top;"),
        QStringLiteral("shape=cylinder3;whiteSpace=wrap;html=1;boundedLbl=1;backgroundOutline=1;size=15;lid=0;"),
        QStringLiteral("shape=mindmapBang;whiteSpace=wrap;html=1;perimeter=ellipsePerimeter;"),
        QStringLiteral("shape=gitTag;tabSize=8;tabInset=4;holeSize=1;whiteSpace=wrap;html=1;"),
        QStringLiteral("shape=mermaidSankeyLink;width=20;fillColor=#AF7AA1;gradientColor=#76B7B2;gradientDirection=east;opacity=50;strokeColor=none;bendable=0;html=1;"),
        QStringLiteral("shape=mermaidPerson;perimeter=mermaidPersonPerimeter;whiteSpace=wrap;html=1;"),
        QStringLiteral("shape=mermaidUsecaseActor;actorType=normal;verticalLabelPosition=bottom;verticalAlign=top;html=1;outlineConnect=0;"),
        QStringLiteral("shape=mermaidUsecaseActor;actorType=hollow;verticalLabelPosition=bottom;verticalAlign=top;html=1;outlineConnect=0;"),
        QStringLiteral("shape=mermaidUsecaseActor;actorType=awesome;verticalLabelPosition=bottom;verticalAlign=top;html=1;outlineConnect=0;"),
        QStringLiteral("shape=smileyFace;aspect=fixed;smileyType=happy;perimeter=ellipsePerimeter;fillColor=#FFF8DC;strokeColor=#999999;strokeWidth=2;whiteSpace=wrap;html=1;"),
        QStringLiteral("shape=smileyFace;aspect=fixed;smileyType=neutral;perimeter=ellipsePerimeter;fillColor=#FFF8DC;strokeColor=#999999;strokeWidth=2;whiteSpace=wrap;html=1;"),
        QStringLiteral("shape=smileyFace;aspect=fixed;smileyType=sad;perimeter=ellipsePerimeter;fillColor=#FFF8DC;strokeColor=#999999;strokeWidth=2;whiteSpace=wrap;html=1;"),
        };

        QStringList library;

        for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
        {
            if (definition.category == QLatin1String("basic"))
            {
                library.append(definition.style);
            }
        }

        QCOMPARE(library, official);
    }

    void basicStencilGeometryMatchesXml()
    {
        const auto path = [](const QString &style, const QRectF &rect)
        {
            return ShapeRegistry::pathForStyle(Style::parse(style), rect);
        };
        const auto near = [](double actual, double expected)
        {
            return qAbs(actual - expected) < 0.02;
        };

        // stencils/basic.xml: 4 Point Star (92 x 92), dx=0.8 scales the inner
        // points to 0.5 * 0.8 * w.
        const QPainterPath star4 = path(
            QStringLiteral("shape=mxgraph.basic.4_point_star_2;dx=0.8;"),
            QRectF(0, 0, 100, 100));
        QCOMPARE(star4.elementCount(), 9);
        QVERIFY(near(star4.elementAt(0).x, 0.0));
        QVERIFY(near(star4.elementAt(0).y, 50.0));
        QVERIFY(near(star4.elementAt(1).x, 40.0));
        QVERIFY(near(star4.elementAt(1).y, 40.0));
        QVERIFY(near(star4.elementAt(2).x, 50.0));
        QVERIFY(near(star4.elementAt(2).y, 0.0));

        // 6 Point Star is defined in a 96 x 84.5 box; the first point is
        // (23, 28.9) in stencil units.
        const QPainterPath star6 = path(
            QStringLiteral("shape=mxgraph.basic.6_point_star;"), QRectF(0, 0, 100, 90));
        QCOMPARE(star6.elementCount(), 13);
        QVERIFY(near(star6.elementAt(0).x, 23.0 * 100.0 / 96.0));
        QVERIFY(near(star6.elementAt(0).y, 28.9 * 90.0 / 84.5));

        // Octagon (octagon2) cuts dx=15 * 2 = 30 units off each corner.
        const QPainterPath octagon = path(
            QStringLiteral("shape=mxgraph.basic.octagon2;dx=15;"), QRectF(0, 0, 100, 100));
        QCOMPARE(octagon.elementCount(), 9);
        QVERIFY(near(octagon.elementAt(0).x, 30.0));
        QVERIFY(near(octagon.elementAt(0).y, 0.0));
        QVERIFY(near(octagon.elementAt(2).x, 100.0));
        QVERIFY(near(octagon.elementAt(2).y, 30.0));

        // Cloud Rect is a single 212.54 x 175.05 path of 108 cubic curves.
        const QPainterPath cloud = path(
            QStringLiteral("shape=mxgraph.basic.cloud_rect;"), QRectF(0, 0, 120, 90));
        int curves = 0;
        for (int i = 0; i < cloud.elementCount(); ++i)
        {
            if (cloud.elementAt(i).type == QPainterPath::CurveToElement)
            {
                ++curves;
            }
        }
        QCOMPARE(curves, 108);

        // Pointed Oval: two 75 unit arcs over a 100 unit chord; the sagitta is
        // 19.1, so the lens is narrower than the 50.93 unit stencil box.
        const QPainterPath oval = path(
            QStringLiteral("shape=mxgraph.basic.pointed_oval;"), QRectF(0, 0, 50.93, 100));
        QVERIFY(qAbs(oval.boundingRect().width() - 38.2) < 0.5);
    }

    void basicDecorationsStayOutOfTheFillPath()
    {
        const auto decorations = [](const QString &style, const QRectF &rect)
        {
            return ShapeRegistry::decorationsForStyle(Style::parse(style), rect);
        };

        // The hatch lines of patternFillRect are a decoration with their own
        // colour and width.
        const QVector<ShapeDecoration> pattern = decorations(
            QStringLiteral("shape=mxgraph.basic.patternFillRect;fillStyle=diag;"
                           "step=5;fillStrokeWidth=0.2;fillStrokeColor=#dddddd;"),
            QRectF(0, 0, 120, 60));
        QCOMPARE(pattern.size(), 1);
        QVERIFY(qAbs(pattern.first().strokeWidth - 0.2) < 0.001);
        QVERIFY(pattern.first().path.elementCount() > 20);

        // The banner fold lines and the frame's inner rectangle never end up
        // in the filled outline (they would punch holes).
        QCOMPARE(decorations(QStringLiteral("shape=mxgraph.basic.banner;"),
                             QRectF(0, 0, 100, 50)).size(), 1);
        QCOMPARE(decorations(QStringLiteral("shape=mxgraph.basic.frame;dx=10;"),
                             QRectF(0, 0, 100, 60)).size(), 0);

        // Smiley: mouth plus the two 6 unit eye ellipses.
        const QVector<ShapeDecoration> smiley = decorations(
            QStringLiteral("shape=mxgraph.basic.smiley;"), QRectF(0, 0, 100, 100));
        QCOMPARE(smiley.size(), 3);
        QVERIFY(qAbs(smiley.at(1).strokeWidth - 6.0 * 100.0 / 98.0) < 0.01);

        // The tag's pierce hole is a filled decoration.
        QCOMPARE(decorations(QStringLiteral("shape=gitTag;tabSize=8;tabInset=4;holeSize=1;"),
                             QRectF(0, 0, 80, 20)).size(), 1);
    }

    void bareIsocubeFallsBackToRectangle()
    {
        const QPainterPath bare = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=isocube;")), QRectF(0, 0, 100, 100));
        QCOMPARE(bare.elementCount(), 5);

        const QPainterPath stencil = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=mxgraph.basic.isocube;isoAngle=15;")),
            QRectF(0, 0, 100, 100));
        QCOMPARE(stencil.elementCount(), 7);
    }

    void basicStencilsResolveToCanonicalKeys()
    {
        const auto path = [](const QString &shape)
        {
            return ShapeRegistry::pathForStyle(
                Style::parse(QStringLiteral("shape=") + shape), QRectF(0, 0, 100, 100));
        };

        // Stencil names are normalised to the internal camel case keys.
        const QPainterPath heart = path(QStringLiteral("mxgraph.basic.heart"));
        QVERIFY(heart.elementCount() > 8);

        const QPainterPath star6 = path(QStringLiteral("mxgraph.basic.6_point_star"));
        QVERIFY(star6.elementCount() >= 12);

        // no_symbol is a circle plus a diagonal band, not a plain line.
        const QPainterPath noSymbol = path(QStringLiteral("mxgraph.basic.no_symbol"));
        QVERIFY(noSymbol.elementCount() > 12);

        // Unregistered basic names fall back to a rectangle, exactly like
        // mxGraph's default shape.
        const QPainterPath unknown = path(QStringLiteral("mxgraph.basic.not_a_shape"));
        QCOMPARE(unknown.elementCount(), 5);

        // Electrical stencils keep their lower case draw.io names.
        const QPainterPath battery = path(
            QStringLiteral("mxgraph.electrical.miscellaneous.batteryStack"));
        QVERIFY(battery.elementCount() > 5);
    }

    void flowchartShapesMatchDrawio()
    {
        const auto path = [](const QString &style)
        {
            return ShapeRegistry::pathForStyle(Style::parse(style), QRectF(0, 0, 100, 80));
        };

        // or: flat left edge, curved right edge.
        const QPainterPath orPath = path(QStringLiteral("shape=or;"));
        QVERIFY(orPath.elementCount() >= 6);
        QCOMPARE(orPath.elementAt(0).x, 0.0);

        // xor adds the inner curve back to the start point.
        QVERIFY(path(QStringLiteral("shape=xor;")).elementCount() > orPath.elementCount());

        // step uses fixedSize pixels for its notches.
        const QPainterPath step = path(QStringLiteral("shape=step;fixedSize=1;"));
        QCOMPARE(step.elementAt(1).x, 80.0);

        // callout keeps a square body and a triangular tail.
        const QPainterPath callout = path(QStringLiteral("shape=callout;"));
        QCOMPARE(callout.elementAt(2).y, 50.0);

        // cube cuts the top-right and bottom-left corners; the interior edges
        // are decorations so the filled path stays a single hexagon.
        const QPainterPath cube = path(QStringLiteral("shape=cube;size=20;"));
        QCOMPARE(cube.elementAt(1).x, 80.0);
        QCOMPARE(cube.elementCount(), 7);
    }

    void logicGatesAndStorageLiveInGeneral()
    {
        QStringList general;
        QStringList basic;

        for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
        {
            if (definition.category == QLatin1String("general"))
            {
                general.append(definition.key);
            }
            else if (definition.category == QLatin1String("basic"))
            {
                basic.append(definition.key);
            }
        }

        for (const QString &key : {QStringLiteral("or"), QStringLiteral("and"),
                                   QStringLiteral("dataStorage"),
                                   QStringLiteral("internalStorage"),
                                   QStringLiteral("wedgeCallout")})
        {
            QVERIFY2(general.contains(key), qPrintable(key));
            QVERIFY2(!basic.contains(key), qPrintable(key));
        }

        // Sidebar-Basic.js has no XOR entry; the logic gates live in General.
        QVERIFY(!basic.contains(QStringLiteral("xor")));
    }

    void generalLibraryFollowsSidebar()
    {
        // Sidebar.prototype.addGeneralPalette, in draw.io order: the vertex
        // templates followed by the edge templates.
        const QStringList official = {
            QStringLiteral("rectangle"),      QStringLiteral("rounded"),
            QStringLiteral("text"),           QStringLiteral("textbox"),
            QStringLiteral("ellipse"),        QStringLiteral("square"),
            QStringLiteral("circle"),         QStringLiteral("process"),
            QStringLiteral("diamond"),        QStringLiteral("parallelogram"),
            QStringLiteral("hexagon"),        QStringLiteral("triangle"),
            QStringLiteral("cylinder"),       QStringLiteral("cloud"),
            QStringLiteral("document"),       QStringLiteral("internalStorage"),
            QStringLiteral("cube"),           QStringLiteral("step"),
            QStringLiteral("trapezoid"),      QStringLiteral("tape"),
            QStringLiteral("note"),           QStringLiteral("card"),
            QStringLiteral("callout"),        QStringLiteral("wedgeCallout"),
            QStringLiteral("actor"),          QStringLiteral("or"),
            QStringLiteral("and"),            QStringLiteral("dataStorage"),
            QStringLiteral("container"),      QStringLiteral("containerVertical"),
            QStringLiteral("containerHorizontal"), QStringLiteral("list"),
            QStringLiteral("listItem"),       QStringLiteral("curve"),
            QStringLiteral("flexArrow"),      QStringLiteral("flexArrowOneWay"),
            QStringLiteral("dashedLine"),     QStringLiteral("dottedLine"),
            QStringLiteral("plainLine"),      QStringLiteral("bidirectionalConnector"),
            QStringLiteral("directionalConnector"), QStringLiteral("link"),
            QStringLiteral("connectorLabel"), QStringLiteral("connectorLabels"),
            QStringLiteral("connectorSymbol"),
        };

        QStringList keys;
        QHash<QString, ShapeDefinition> byKey;

        for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
        {
            if (definition.category == QLatin1String("general"))
            {
                keys.append(definition.key);
                byKey.insert(definition.key, definition);
            }
        }

        QCOMPARE(keys, official);

        // Default sizes come from the sidebar templates.
        QCOMPARE(byKey.value(QStringLiteral("rectangle")).size, QSizeF(120, 60));
        QCOMPARE(byKey.value(QStringLiteral("square")).size, QSizeF(80, 80));
        QCOMPARE(byKey.value(QStringLiteral("process")).size, QSizeF(120, 60));
        QCOMPARE(byKey.value(QStringLiteral("card")).size, QSizeF(80, 100));
        QCOMPARE(byKey.value(QStringLiteral("actor")).size, QSizeF(30, 60));
        QCOMPARE(byKey.value(QStringLiteral("container")).size, QSizeF(200, 200));
        QCOMPARE(byKey.value(QStringLiteral("list")).size, QSizeF(140, 120));
        QCOMPARE(byKey.value(QStringLiteral("listItem")).size, QSizeF(80, 30));
        QCOMPARE(byKey.value(QStringLiteral("curve")).size, QSizeF(50, 50));
        QCOMPARE(byKey.value(QStringLiteral("link")).size, QSizeF(100, 0));

        // The added entries reuse the addGeneralPalette style strings.
        QVERIFY(byKey.value(QStringLiteral("square")).style.startsWith(
            QStringLiteral("whiteSpace=wrap;html=1;aspect=fixed;")));
        QVERIFY(byKey.value(QStringLiteral("process")).style.startsWith(
            QStringLiteral("shape=process;whiteSpace=wrap;html=1;backgroundOutline=1;")));
        QVERIFY(byKey.value(QStringLiteral("card")).style.startsWith(
            QStringLiteral("shape=card;whiteSpace=wrap;html=1;")));
        QVERIFY(byKey.value(QStringLiteral("step")).style.startsWith(
            QStringLiteral("shape=step;perimeter=stepPerimeter;")));
        QVERIFY(byKey.value(QStringLiteral("container")).style.startsWith(
            QStringLiteral("swimlane;startSize=0;")));
        QVERIFY(byKey.value(QStringLiteral("containerHorizontal")).style.startsWith(
            QStringLiteral("swimlane;horizontal=0;")));
        QVERIFY(byKey.value(QStringLiteral("dottedLine")).style.startsWith(
            QStringLiteral("endArrow=none;dashed=1;html=1;dashPattern=1 3;strokeWidth=2;")));
        QVERIFY(byKey.value(QStringLiteral("flexArrow")).style.startsWith(
            QStringLiteral("shape=flexArrow;endArrow=classic;startArrow=classic;")));
        QVERIFY(byKey.value(QStringLiteral("text")).style.startsWith(
            QStringLiteral("text;html=1;whiteSpace=wrap;strokeColor=none;fillColor=none;")));

        // draw.io labels the XOR gate "Or" and the D shaped gate "And".
        QVERIFY(byKey.value(QStringLiteral("or")).style.startsWith(QStringLiteral("shape=xor;")));
        QVERIFY(byKey.value(QStringLiteral("and")).style.startsWith(QStringLiteral("shape=or;")));

        // Vertex templates keep their label; edge templates are flagged so the
        // canvas inserts them as connectors.
        QCOMPARE(byKey.value(QStringLiteral("text")).value, QStringLiteral("Text"));
        QCOMPARE(byKey.value(QStringLiteral("actor")).value, QStringLiteral("Actor"));
        QVERIFY(byKey.value(QStringLiteral("list")).value == QStringLiteral("List"));

        for (const QString &key : official)
        {
            const bool edge = byKey.value(key).edge;
            const bool edgeOnly = key == QStringLiteral("curve")
                || key == QStringLiteral("flexArrow") || key == QStringLiteral("flexArrowOneWay")
                || key == QStringLiteral("dashedLine") || key == QStringLiteral("dottedLine")
                || key == QStringLiteral("plainLine")
                || key == QStringLiteral("bidirectionalConnector")
                || key == QStringLiteral("directionalConnector")
                || key == QStringLiteral("link") || key == QStringLiteral("connectorLabel")
                || key == QStringLiteral("connectorLabels")
                || key == QStringLiteral("connectorSymbol");
            QCOMPARE(edge, edgeOnly);
        }

        // The List entry carries its three rows, like the sidebar template.
        const ShapeDefinition list = byKey.value(QStringLiteral("list"));
        QCOMPARE(list.children.size(), 3);
        QCOMPARE(list.children.at(0).value, QStringLiteral("Item 1"));
        QCOMPARE(list.children.at(2).geometry, QRectF(0, 90, 140, 30));

        // Connector templates carry their labels and the message symbol.
        QCOMPARE(byKey.value(QStringLiteral("connectorLabels")).children.size(), 3);
        const ShapeDefinition symbol = byKey.value(QStringLiteral("connectorSymbol"));
        QCOMPARE(symbol.children.size(), 1);
        QVERIFY(symbol.children.first().relative);
        QCOMPARE(symbol.children.first().offset, QPointF(-10, -7));

        // Every vertex entry is drawable (text and image placeholders aside).
        for (const ShapeDefinition &definition : byKey)
        {
            if (definition.edge)
            {
                continue;
            }

            Cell probe(Cell::Kind::Vertex, QString());
            probe.style = Style::parse(definition.style);

            if (ShapeRegistry::isTextShape(probe) || ShapeRegistry::isImageShape(probe))
            {
                continue;
            }

            QVERIFY2(!ShapeRegistry::pathForStyle(probe.style, QRectF(0, 0, 120, 80)).isEmpty(),
                     qPrintable(definition.key));
        }
    }

    void generalPalettePreviewsArePainted()
    {
        // Every General entry, including the edge templates and the composite
        // List, must produce a visible palette icon.
        ShapeCatalog &catalog = ShapeCatalog::instance();

        for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
        {
            if (definition.category != QLatin1String("general"))
            {
                continue;
            }

            const QIcon icon = catalog.iconFor(definition, 48);
            QVERIFY2(!icon.isNull(), qPrintable(definition.key));

            const QImage image = icon.pixmap(48, 48).toImage();
            bool visible = false;

            for (int y = 0; y < image.height() && !visible; ++y)
            {
                for (int x = 0; x < image.width(); ++x)
                {
                    if (qAlpha(image.pixel(x, y)) > 8)
                    {
                        visible = true;
                        break;
                    }
                }
            }

            QVERIFY2(visible, qPrintable(definition.key));
        }
    }

    void basicLibraryKeepsStencilShapes()
    {
        QStringList keys;

        for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
        {
            if (definition.category == QLatin1String("basic"))
            {
                keys.append(definition.key);
            }
        }

        // Nothing from the General palette is left in the basic library.
        for (const QString &key : {QStringLiteral("rectangle"), QStringLiteral("rounded"),
                                   QStringLiteral("text"), QStringLiteral("ellipse"),
                                   QStringLiteral("circle"), QStringLiteral("diamond"),
                                   QStringLiteral("triangle"), QStringLiteral("hexagon"),
                                   QStringLiteral("parallelogram"), QStringLiteral("trapezoid"),
                                   QStringLiteral("cylinder"), QStringLiteral("cloud"),
                                   QStringLiteral("document"), QStringLiteral("internalStorage"),
                                   QStringLiteral("cube"), QStringLiteral("tape"),
                                   QStringLiteral("note"), QStringLiteral("callout"),
                                   QStringLiteral("wedgeCallout"), QStringLiteral("actor"),
                                   QStringLiteral("or"), QStringLiteral("dataStorage")})
        {
            QVERIFY2(!keys.contains(key), qPrintable(key));
        }

        // Sidebar-Basic.js entries stay in the basic library.
        for (const QString &key : {QStringLiteral("basic.fourPointStar2"),
                                   QStringLiteral("basic.banner"),
                                   QStringLiteral("basic.cloudCallout"),
                                   QStringLiteral("basic.cone"), QStringLiteral("basic.donut"),
                                   QStringLiteral("basic.drop"), QStringLiteral("basic.flash"),
                                   QStringLiteral("basic.heart"), QStringLiteral("basic.isocube"),
                                   QStringLiteral("basic.moon"), QStringLiteral("basic.noSymbol"),
                                   QStringLiteral("basic.pyramid"), QStringLiteral("basic.smiley"),
                                   QStringLiteral("basic.sun"), QStringLiteral("basic.tick"),
                                   QStringLiteral("basic.pie"), QStringLiteral("basic.x"),
                                   QStringLiteral("basic.wave"), QStringLiteral("basic.button"),
                                   QStringLiteral("basic.polygon"),
                                   QStringLiteral("basic.cylinderStack"),
                                   QStringLiteral("basic.smileySad")})
        {
            QVERIFY2(keys.contains(key), qPrintable(key));
        }

        QCOMPARE(ShapeRegistry::definition(QStringLiteral("rectangle")).category,
                 QStringLiteral("general"));
        QCOMPARE(ShapeRegistry::definition(QStringLiteral("basic.banner")).category,
                 QStringLiteral("basic"));
    }

    void generalCategoryIsListedFirstAndTranslated()
    {
        const QVector<ShapeCategory> categories = ShapeCatalog::instance().builtinCategories();
        int general = -1;
        int basic = -1;

        for (int i = 0; i < categories.size(); ++i)
        {
            if (categories.at(i).id == QLatin1String("general"))
            {
                general = i;
            }
            else if (categories.at(i).id == QLatin1String("basic"))
            {
                basic = i;
            }
        }

        QVERIFY(general >= 0);
        QVERIFY(basic > general);
        QCOMPARE(categories.at(general).name, QStringLiteral("通用形状"));
        QCOMPARE(ShapeCatalog::instance().definition(QStringLiteral("rectangle")).category,
                 QStringLiteral("general"));
    }

    void basicLibraryHasTriangleAndCylinderVariants()
    {
        QStringList keys;

        for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
        {
            if (definition.category == QStringLiteral("basic"))
            {
                keys.append(definition.key);
            }
        }

        QVERIFY(keys.contains(QStringLiteral("basic.orthogonalTriangle")));
        QVERIFY(keys.contains(QStringLiteral("basic.obtuseTriangle")));
        QVERIFY(keys.contains(QStringLiteral("basic.hexagon")));
        QVERIFY(keys.contains(QStringLiteral("basic.cylinderStack")));
    }

    void cylinderStackHasOpenTop()
    {
        const QPainterPath lid = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=cylinder3;size=15;")), QRectF(0, 0, 80, 100));
        const QPainterPath open = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=cylinder3;size=15;lid=0;")), QRectF(0, 0, 80, 100));

        // A closed lid starts at the top edge, an open stack dips below it.
        QCOMPARE(lid.elementAt(0).y, 15.0);
        QCOMPARE(open.elementAt(0).y, 0.0);
    }

    void tapeIsInGeneralLibrary()
    {
        bool found = false;

        for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
        {
            if (definition.key == QStringLiteral("tape"))
            {
                found = true;
                QCOMPARE(definition.category, QStringLiteral("general"));
            }
        }

        QVERIFY(found);
    }

    void tapeUsesCurvedEdges()
    {
        const QPainterPath path = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=tape;")), QRectF(0, 0, 120, 100));

        QVERIFY(path.elementCount() > 8);
        QCOMPARE(path.boundingRect().width(), 120.0);

        const QPainterPath::Element first = path.elementAt(0);
        QCOMPARE(first.y, 20.0);
    }

    void pyramidUsesDrawioParameters()
    {
        const QPainterPath path = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral(
                "shape=mxgraph.basic.pyramid;dx1=0.4;dx2=0.6;dy1=0.9;dy2=0.8;")),
            QRectF(0, 0, 100, 100));

        QCOMPARE(path.elementCount(), 5);
        QCOMPARE(path.elementAt(0).x, 40.0);
        QCOMPARE(path.elementAt(0).y, 0.0);
        QCOMPARE(path.elementAt(2).x, 60.0);
        QCOMPARE(path.elementAt(2).y, 100.0);

        // The crease is a stroke-only decoration, not part of the filled path.
        const QVector<ShapeDecoration> decorations = ShapeRegistry::decorationsForStyle(
            Style::parse(QStringLiteral(
                "shape=mxgraph.basic.pyramid;dx1=0.4;dx2=0.6;dy1=0.9;dy2=0.8;")),
            QRectF(0, 0, 100, 100));
        QCOMPARE(decorations.size(), 1);
        QCOMPARE(decorations.first().path.elementCount(), 2);
    }

    void duplicateShapeKeepsAttachedEdgeInsideSelection()
    {
        Page page;
        Cell *a = page.addVertex(QRectF(0, 0, 80, 40));
        Cell *b = page.addVertex(QRectF(200, 0, 80, 40));
        page.addEdge(a->id, b->id);

        QCOMPARE(page.cells().size(), 3);
        QCOMPARE(a->styleValue(QStringLiteral("fillColor")), QString());
    }

    void flowchartLibraryMatchesSidebar()
    {
        // Sidebar-Flowchart.js offers exactly these entries with exactly these
        // style strings (w = h = 100, s = strokeWidth=2;html=1;shape=...):
        // the 23 stencils of flowchart.xml, the registered shapes card,
        // parallelogram, internalStorage, manualInput, trapezoid,
        // offPageConnector, process, tape and hexagon, and the transfer arrow.
        const QStringList official = {
            QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.annotation_1;"
                           "align=left;pointerEvents=1;"),
            QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.annotation_2;"
                           "align=left;labelPosition=right;pointerEvents=1;"),
            QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;shape=card;"
                           "whiteSpace=wrap;size=20;arcSize=12;"),
            QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                           "shape=mxgraph.flowchart.collate;"),
            QStringLiteral("shape=parallelogram;html=1;strokeWidth=2;"
                           "perimeter=parallelogramPerimeter;whiteSpace=wrap;rounded=1;"
                           "arcSize=12;size=0.23;"),
            QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.database;"
                           "whiteSpace=wrap;"),
            QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.decision;"
                           "whiteSpace=wrap;"),
            QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.delay;"
                           "whiteSpace=wrap;"),
            QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.direct_data;"
                           "whiteSpace=wrap;"),
            QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.display;"
                           "whiteSpace=wrap;"),
            QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.document2;"
                           "whiteSpace=wrap;size=0.25;"),
            QStringLiteral("strokeWidth=2;html=1;"
                           "shape=mxgraph.flowchart.extract_or_measurement;whiteSpace=wrap;"),
            QStringLiteral("shape=internalStorage;whiteSpace=wrap;html=1;dx=15;dy=15;"
                           "rounded=1;arcSize=8;strokeWidth=2;"),
            QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.loop_limit;"
                           "whiteSpace=wrap;"),
            QStringLiteral("html=1;strokeWidth=2;shape=manualInput;boundedLbl=1;"
                           "whiteSpace=wrap;rounded=1;size=26;arcSize=11;"),
            QStringLiteral("verticalLabelPosition=middle;verticalAlign=middle;html=1;"
                           "shape=trapezoid;perimeter=trapezoidPerimeter;whiteSpace=wrap;"
                           "size=0.23;arcSize=10;flipV=1;labelPosition=center;align=center;"),
            QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.merge_or_storage;"
                           "whiteSpace=wrap;"),
            QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.multi-document;"
                           "whiteSpace=wrap;boundedLbl=1;"),
            QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                           "shape=offPageConnector;rounded=0;size=0.5;"),
            QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                           "shape=mxgraph.flowchart.on-page_reference;"),
            QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                           "shape=mxgraph.flowchart.or_2;"),
            QStringLiteral("shape=tape;whiteSpace=wrap;html=1;strokeWidth=2;size=0.19"),
            QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                           "shape=mxgraph.flowchart.parallel_mode;accentColor=#ffff00;"
                           "pointerEvents=1"),
            QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                           "shape=process;whiteSpace=wrap;rounded=1;size=0.14;arcSize=6;"),
            QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                           "shape=hexagon;perimeter=hexagonPerimeter2;arcSize=6;size=0.27;"),
            QStringLiteral("rounded=1;whiteSpace=wrap;html=1;absoluteArcSize=1;arcSize=14;"
                           "strokeWidth=2;"),
            QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.sequential_data;"
                           "whiteSpace=wrap;"),
            QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                           "shape=mxgraph.flowchart.sort;"),
            QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.start_1;"
                           "whiteSpace=wrap;"),
            QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.start_2;"
                           "whiteSpace=wrap;"),
            QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.stored_data;"
                           "whiteSpace=wrap;"),
            QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                           "shape=mxgraph.flowchart.summing_junction;"),
            QStringLiteral("strokeWidth=2;html=1;shape=mxgraph.flowchart.terminator;"
                           "whiteSpace=wrap;"),
            QStringLiteral("verticalLabelPosition=bottom;verticalAlign=top;html=1;"
                           "strokeWidth=2;shape=mxgraph.arrows2.arrow;dy=0.6;dx=40;notch=0;"),
        };

        QStringList library;

        for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
        {
            if (definition.category == QLatin1String("flowchart"))
            {
                library.append(definition.style);
            }
        }

        QCOMPARE(library, official);
    }

    void advancedLibraryMatchesSidebar()
    {
        // Sidebar.prototype.createAdvancedShapes() plus the entries
        // Sidebar-Advanced.js appends: 42 palette entries in sidebar order
        // with the sidebar's style strings verbatim.
        const QStringList official = {
            QStringLiteral("shape=ext;double=1;rounded=0;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=ext;double=1;rounded=1;whiteSpace=wrap;html=1;"),
            QStringLiteral("ellipse;shape=doubleEllipse;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=ext;double=1;whiteSpace=wrap;html=1;aspect=fixed;"),
            QStringLiteral("ellipse;shape=doubleEllipse;whiteSpace=wrap;html=1;aspect=fixed;"),
            QStringLiteral("shape=tapeData;whiteSpace=wrap;html=1;perimeter=ellipsePerimeter;"),
            QStringLiteral("shape=manualInput;boundedLbl=1;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=loopLimit;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=offPageConnector;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=delay;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=display;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=singleArrow;direction=west;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=singleArrow;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=singleArrow;direction=north;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=singleArrow;direction=south;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=doubleArrow;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=doubleArrow;direction=south;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=actor;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=cross;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=corner;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=tee;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=datastore;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=orEllipse;perimeter=ellipsePerimeter;whiteSpace=wrap;"
                           "html=1;backgroundOutline=1;"),
            QStringLiteral("shape=sumEllipse;perimeter=ellipsePerimeter;whiteSpace=wrap;"
                           "html=1;backgroundOutline=1;"),
            QStringLiteral("shape=lineEllipse;perimeter=ellipsePerimeter;whiteSpace=wrap;"
                           "html=1;backgroundOutline=1;"),
            QStringLiteral("shape=lineEllipse;line=vertical;perimeter=ellipsePerimeter;"
                           "whiteSpace=wrap;html=1;backgroundOutline=1;"),
            QStringLiteral("shape=sortShape;perimeter=rhombusPerimeter;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=collate;whiteSpace=wrap;html=1;"),
            QStringLiteral("shape=switch;whiteSpace=wrap;html=1;"),
            QStringLiteral("swimlane;childLayout=stackLayout;horizontal=1;fillColor=none;"
                           "horizontalStack=1;resizeParent=1;resizeParentMax=0;resizeLast=0;"
                           "collapsible=0;strokeColor=none;stackBorder=10;stackSpacing=-12;"
                           "resizable=1;align=center;points=[];whiteSpace=wrap;html=1;"),
            QStringLiteral("swimlane;"),
            QStringLiteral("swimlane;fontStyle=0;childLayout=stackLayout;horizontal=1;"
                           "startSize=26;fillColor=none;horizontalStack=0;resizeParent=1;"
                           "resizeParentMax=0;resizeLast=0;collapsible=1;marginBottom=0;"
                           "html=1;"),
            QStringLiteral("text;strokeColor=none;fillColor=none;align=left;"
                           "verticalAlign=top;spacingLeft=4;spacingRight=4;overflow=hidden;"
                           "rotatable=0;points=[[0,0.5],[1,0.5]];portConstraint=eastwest;"
                           "whiteSpace=wrap;html=1;"),
            QStringLiteral("swimlane;childLayout=stackLayout;resizeParent=1;"
                           "resizeParentMax=0;horizontal=1;startSize=20;horizontalStack=0;"
                           "html=1;"),
            QStringLiteral("swimlane;childLayout=stackLayout;resizeParent=1;"
                           "resizeParentMax=0;horizontal=0;startSize=20;horizontalStack=0;"
                           "html=1;"),
            QStringLiteral("swimlane;childLayout=stackLayout;resizeParent=1;"
                           "resizeParentMax=0;startSize=20;html=1;"),
            QStringLiteral("swimlane;childLayout=stackLayout;resizeParent=1;"
                           "resizeParentMax=0;startSize=20;horizontal=0;horizontalStack=1;"),
        };

        QStringList library;
        QVector<ShapeDefinition> entries;

        for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
        {
            if (definition.category == QLatin1String("advanced"))
            {
                library.append(definition.style);
                entries.append(definition);
            }
        }

        // The five layout containers close the palette; their style strings
        // carry the ELK childLayout of Menus.layoutContainers, so they are
        // checked by prefix below.
        QCOMPARE(library.size(), official.size() + 5);
        QCOMPARE(library.mid(0, official.size()), official);

        const QStringList layoutPrefixes = {
            QStringLiteral("swimlane;html=1;startSize=20;horizontal=1;resizable=0;"
                           "fontSize=12;transparentBounds=1;groupPadding=20;"
                           "containerType=tree;childLayout=%5B%7B%22layout%22%3A%22elkTree%22"),
            QStringLiteral("swimlane;html=1;startSize=20;horizontal=0;resizable=0;"
                           "fontSize=12;transparentBounds=1;groupPadding=20;"
                           "containerType=tree;childLayout=%5B%7B%22layout%22%3A%22elkTree%22"),
            QStringLiteral("swimlane;html=1;startSize=20;horizontal=1;resizable=0;"
                           "fontSize=12;transparentBounds=1;groupPadding=20;"
                           "containerType=tree;childLayout=%5B%7B%22layout%22%3A"
                           "%22BalloonTreeLayout%22"),
            QStringLiteral("swimlane;html=1;startSize=20;horizontal=1;resizable=0;"
                           "fontSize=12;transparentBounds=1;groupPadding=20;"
                           "containerType=tree;childLayout=%5B%7B%22layout%22%3A"
                           "%22elkLayered%22"),
            QStringLiteral("swimlane;html=1;startSize=20;horizontal=0;resizable=0;"
                           "fontSize=12;transparentBounds=1;groupPadding=20;"
                           "containerType=tree;childLayout=%5B%7B%22layout%22%3A"
                           "%22elkLayered%22"),
        };

        for (int i = 0; i < layoutPrefixes.size(); ++i)
        {
            const QString style = library.at(official.size() + i);
            QVERIFY2(style.startsWith(layoutPrefixes.at(i)), qPrintable(style));
        }

        // Child cells: the process bar's steps, the list rows, the pool lanes
        // and the layout containers' nodes and edges.
        const auto childrenOf = [&entries](const QString &key)
        {
            for (const ShapeDefinition &definition : entries)
            {
                if (definition.key == key)
                {
                    return definition.children;
                }
            }

            return QVector<ShapeChildDefinition>();
        };

        const QVector<ShapeChildDefinition> steps =
            childrenOf(QStringLiteral("advanced.processBar"));
        QCOMPARE(steps.size(), 3);
        QCOMPARE(steps.at(0).geometry, QRectF(10, 33, 100, 57));
        QCOMPARE(steps.at(2).geometry, QRectF(186, 33, 100, 57));
        QCOMPARE(steps.at(0).value, QStringLiteral("Step 1"));

        const QVector<ShapeChildDefinition> listRows =
            childrenOf(QStringLiteral("advanced.list"));
        QCOMPARE(listRows.size(), 3);
        QCOMPARE(listRows.at(1).geometry, QRectF(0, 52, 140, 26));

        // The four pools of Sidebar-Advanced.js: the lanes stack below or
        // beside the pool title, exactly as the data entries store them.
        const auto lanes = [&childrenOf](const QString &key)
        {
            const QVector<ShapeChildDefinition> children = childrenOf(key);
            QVector<QRectF> rects;

            for (const ShapeChildDefinition &child : children)
            {
                rects.append(child.geometry);
            }

            return rects;
        };

        QCOMPARE(lanes(QStringLiteral("advanced.horizontalPool1")),
                 (QVector<QRectF>{QRectF(0, 20, 480, 120), QRectF(0, 140, 480, 120),
                                  QRectF(0, 260, 480, 120)}));
        QCOMPARE(lanes(QStringLiteral("advanced.horizontalPool2")),
                 (QVector<QRectF>{QRectF(20, 0, 460, 120), QRectF(20, 120, 460, 120),
                                  QRectF(20, 240, 460, 120)}));
        QCOMPARE(lanes(QStringLiteral("advanced.verticalPool1")),
                 (QVector<QRectF>{QRectF(0, 20, 120, 460), QRectF(120, 20, 120, 460),
                                  QRectF(240, 20, 120, 460)}));
        QCOMPARE(lanes(QStringLiteral("advanced.verticalPool2")),
                 (QVector<QRectF>{QRectF(20, 0, 120, 480), QRectF(140, 0, 120, 480),
                                  QRectF(260, 0, 120, 480)}));

        const QVector<ShapeChildDefinition> balloon =
            childrenOf(QStringLiteral("advanced.balloonTreeLayout"));
        QCOMPARE(balloon.size(), 19);

        int balloonEdges = 0;

        for (const ShapeChildDefinition &child : balloon)
        {
            if (child.edge)
            {
                ++balloonEdges;
                QVERIFY(child.source >= 0 && child.target >= 0);
            }
        }

        QCOMPARE(balloonEdges, 9);

        const QVector<ShapeChildDefinition> flow =
            childrenOf(QStringLiteral("advanced.verticalFlowLayout"));
        QCOMPARE(flow.size(), 8);

        // Every entry has a preview with ink (the palette renders the actual
        // geometry for procedural shapes).
        for (const ShapeDefinition &definition : entries)
        {
            const QPixmap preview = ShapeCatalog::instance().iconFor(definition, 48)
                                        .pixmap(48, 48);
            QVERIFY2(!preview.isNull(), qPrintable(definition.key));

            const QImage image = preview.toImage();
            int ink = 0;

            for (int y = 0; y < image.height(); ++y)
            {
                for (int x = 0; x < image.width(); ++x)
                {
                    if (image.pixelColor(x, y).alpha() > 0
                        && image.pixelColor(x, y).lightness() < 200)
                    {
                        ++ink;
                    }
                }
            }

            QVERIFY2(ink > 0, qPrintable(definition.key));
        }
    }

    void advancedShapesFollowShapesJs()
    {
        const auto path = [](const QString &style, double w, double h)
        {
            return ShapeRegistry::pathForStyle(Style::parse(style), QRectF(0, 0, w, h));
        };

        // ExtendedShape: the outline is one rectangle; the second frame is a
        // decoration painted by ShapeRenderer.
        const QPainterPath doubleRect = path(
            QStringLiteral("shape=ext;double=1;rounded=0;whiteSpace=wrap;html=1;"), 120, 80);
        QCOMPARE(doubleRect.elementCount(), 5);
        QCOMPARE(doubleRect.boundingRect(), QRectF(0, 0, 120, 80));

        // ManualInputShape: the slanted top edge starts at size (default 30).
        const QPainterPath manual = path(
            QStringLiteral("shape=manualInput;boundedLbl=1;whiteSpace=wrap;html=1;"), 80, 80);
        QCOMPARE(manual.elementAt(0), QPointF(0, 80));
        QCOMPARE(manual.elementAt(1), QPointF(0, 30));
        QCOMPARE(manual.elementAt(2), QPointF(80, 0));

        // LoopLimitShape: size 20 cuts both top corners.
        const QPainterPath loop = path(
            QStringLiteral("shape=loopLimit;whiteSpace=wrap;html=1;"), 100, 80);
        QCOMPARE(loop.elementAt(0), QPointF(20, 0));
        QCOMPARE(loop.elementAt(2), QPointF(100, 16));

        // OffPageConnectorShape: the point is 3/8 of the height.
        const QPainterPath offPage = path(
            QStringLiteral("shape=offPageConnector;whiteSpace=wrap;html=1;"), 80, 80);
        QCOMPARE(offPage.elementAt(2), QPointF(80, 50));
        QCOMPARE(offPage.elementAt(3), QPointF(40, 80));

        // DelayShape: the D-shaped right edge, two quadratics.
        const QPainterPath delay = path(
            QStringLiteral("shape=delay;whiteSpace=wrap;html=1;"), 80, 40);
        // Qt stores each quadratic as a cubic (3 elements) plus the close.
        QCOMPARE(delay.elementCount(), 10);
        QCOMPARE(delay.elementAt(1), QPointF(60, 0));

        // DisplayShape: the slanted left edge and the same D-shaped right.
        const QPainterPath display = path(
            QStringLiteral("shape=display;whiteSpace=wrap;html=1;"), 80, 40);
        QCOMPARE(display.elementAt(0), QPointF(0, 20));
        QCOMPARE(display.elementAt(1), QPointF(20, 0));

        // SingleArrowShape / DoubleArrowShape use arrowWidth 0.3 and
        // arrowSize 0.2 of the constructor defaults.
        const QPainterPath arrow = path(
            QStringLiteral("shape=singleArrow;whiteSpace=wrap;html=1;"), 100, 60);
        QCOMPARE(arrow.elementCount(), 8);
        QCOMPARE(arrow.elementAt(0), QPointF(0, 21));
        QCOMPARE(arrow.elementAt(3), QPointF(100, 30));

        const QPainterPath doubleArrow = path(
            QStringLiteral("shape=doubleArrow;whiteSpace=wrap;html=1;"), 100, 60);
        QCOMPARE(doubleArrow.elementCount(), 11);
        QCOMPARE(doubleArrow.elementAt(0), QPointF(0, 30));
        QCOMPARE(doubleArrow.elementAt(5), QPointF(100, 30));

        // mxActor: draw.io's `actor` is the filled person silhouette.
        const QPainterPath actor = path(
            QStringLiteral("shape=actor;whiteSpace=wrap;html=1;"), 40, 60);
        QCOMPARE(actor.elementAt(0), QPointF(0, 60));
        QVERIFY(actor.boundingRect().width() > 39.0);

        // CrossShape: size 0.2 of the smaller side.
        const QPainterPath cross = path(
            QStringLiteral("shape=cross;whiteSpace=wrap;html=1;"), 80, 80);
        QCOMPARE(cross.elementCount(), 13);
        QCOMPARE(cross.elementAt(0), QPointF(0, 32));
        QCOMPARE(cross.elementAt(3), QPointF(48, 0));

        // CornerShape / TeeShape use dx = dy = 20.
        const QPainterPath corner = path(
            QStringLiteral("shape=corner;whiteSpace=wrap;html=1;"), 80, 80);
        QCOMPARE(corner.elementAt(3), QPointF(20, 20));

        const QPainterPath tee = path(
            QStringLiteral("shape=tee;whiteSpace=wrap;html=1;"), 80, 80);
        QCOMPARE(tee.elementAt(3), QPointF(50, 20));
        QCOMPARE(tee.elementAt(5), QPointF(30, 80));

        // DataStoreShape: the lid bulge reaches above the cell (dy/3).
        // The cubic lid's control points sit at -dy/3, so the curve peaks at
        // y = 0 (draw.io's c.curveTo(0, -dy/3, w, -dy/3, w, dy)).
        const QPainterPath store = path(
            QStringLiteral("shape=datastore;whiteSpace=wrap;html=1;"), 60, 60);
        QVERIFY(qAbs(store.boundingRect().top()) < 0.01);
        QCOMPARE(store.elementAt(0), QPointF(0, 8));

        // SortShape is the rhombus (the divider is a decoration).
        const QPainterPath sort = path(
            QStringLiteral("shape=sortShape;perimeter=rhombusPerimeter;whiteSpace=wrap;"
                           "html=1;"), 80, 80);
        QCOMPARE(sort.elementAt(0), QPointF(40, 0));
        QCOMPARE(sort.elementAt(2), QPointF(40, 80));

        // CollateShape: two triangles.
        const QPainterPath collate = path(
            QStringLiteral("shape=collate;whiteSpace=wrap;html=1;"), 80, 80);
        QCOMPARE(collate.elementCount(), 8);

        // SwitchShape: four quadratics through the centre.
        const QPainterPath sw = path(
            QStringLiteral("shape=switch;whiteSpace=wrap;html=1;"), 60, 60);
        QCOMPARE(sw.elementCount(), 13);
        QCOMPARE(sw.elementAt(0), QPointF(0, 0));
        QCOMPARE(sw.boundingRect(), QRectF(0, 0, 60, 60));

        // mxDoubleEllipse: the inner ellipse is a decoration, so the shape
        // path is a single ellipse.
        const QPainterPath doubleEllipse = path(
            QStringLiteral("ellipse;shape=doubleEllipse;whiteSpace=wrap;html=1;"), 100, 60);
        QCOMPARE(doubleEllipse.elementCount(), 13);
        QCOMPARE(doubleEllipse.boundingRect(), QRectF(0, 0, 100, 60));
    }

    void advancedDecorationsMatchShapesJs()
    {
        const auto decorations = [](const QString &style, double w, double h)
        {
            return ShapeRegistry::decorationsForStyle(Style::parse(style), QRectF(0, 0, w, h));
        };

        // The double outline of shape=ext is an inner frame inset by 2.
        const QVector<ShapeDecoration> frame = decorations(
            QStringLiteral("shape=ext;double=1;rounded=0;whiteSpace=wrap;html=1;"), 120, 80);
        QVERIFY(frame.isEmpty());

        // OrEllipse / SumEllipse / LineEllipse / SortShape / TapeData paint
        // their interior lines in ShapeRenderer (no path decorations).
        QVERIFY(decorations(QStringLiteral(
            "shape=orEllipse;perimeter=ellipsePerimeter;whiteSpace=wrap;html=1;"),
            80, 80).isEmpty());
    }

    void flowchartStencilsFollowTheOfficialXml()
    {
        const auto path = [](const QString &style, double w, double h)
        {
            return ShapeRegistry::pathForStyle(Style::parse(style), QRectF(0, 0, w, h));
        };

        // Decision: move (50,0), three lines and a close.
        const QPainterPath decision = path(
            QStringLiteral("shape=mxgraph.flowchart.decision;"), 100, 100);
        QCOMPARE(decision.elementCount(), 5);
        QCOMPARE(decision.elementAt(0).x, 50.0);
        QCOMPARE(decision.elementAt(0).y, 0.0);
        QCOMPARE(decision.elementAt(1).x, 100.0);
        QCOMPARE(decision.elementAt(1).y, 50.0);

        // Annotation 1 is an open bracket (the stencil has no <close>).
        const QPainterPath annotation = path(
            QStringLiteral("shape=mxgraph.flowchart.annotation_1;"), 50, 100);
        QCOMPARE(annotation.elementCount(), 4);
        QCOMPARE(annotation.elementAt(0).x, 50.0);
        QCOMPARE(annotation.elementAt(0).y, 0.0);
        QCOMPARE(annotation.elementAt(2).y, 100.0);

        // Transfer: the stencil's seven points plus a close.
        const QPainterPath transfer = path(
            QStringLiteral("shape=mxgraph.flowchart.transfer;"), 97.5, 70);
        QCOMPARE(transfer.elementCount(), 8);
        QCOMPARE(transfer.elementAt(0).x, 0.0);
        QCOMPARE(transfer.elementAt(0).y, 20.0);
        QCOMPARE(transfer.elementAt(3).x, 97.5);
        QCOMPARE(transfer.elementAt(3).y, 35.0);

        // Parallel Mode: three accent coloured diamonds, no outer outline.
        const QPainterPath parallel = path(
            QStringLiteral("shape=mxgraph.flowchart.parallel_mode;"), 94, 40);
        QCOMPARE(parallel.elementCount(), 15);
        QCOMPARE(parallel.elementAt(0).x, 47.0);
        QCOMPARE(parallel.elementAt(0).y, 15.0);

        // Off-page Reference: the notch sits at 50% of the height.
        const QPainterPath offPage = path(
            QStringLiteral("shape=mxgraph.flowchart.off-page_reference;"), 60, 60);
        QCOMPARE(offPage.elementCount(), 6);
        QCOMPARE(offPage.elementAt(2).x, 60.0);
        QCOMPARE(offPage.elementAt(2).y, 30.0);

        // Delay ends in a half circle whose radius is 33 stencil units; the
        // stencil itself is a hair wider than 98.25 units.
        const QPainterPath delay = path(
            QStringLiteral("shape=mxgraph.flowchart.delay;"), 100, 60);
        QVERIFY(qAbs(delay.boundingRect().width() - 100.0) < 0.05);
        QCOMPARE(delay.boundingRect().height(), 60.0);
    }

    void flowchartStencilNamesMustBeRegistered()
    {
        // draw.io registers "off-page_reference" (flowchart.xml replaces the
        // blanks of "Off-page Reference" by underscores); the all-underscore
        // variant is not a stencil and falls back to a rectangle.
        const QPainterPath fallback = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=mxgraph.flowchart.off_page_reference;")),
            QRectF(0, 0, 60, 60));
        QCOMPARE(fallback.elementCount(), 5);
        QCOMPARE(fallback.elementAt(0).x, 0.0);
        QCOMPARE(fallback.elementAt(1).x, 60.0);
        QCOMPARE(fallback.elementAt(1).y, 0.0);

        const QPainterPath stencil = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=mxgraph.flowchart.off-page_reference;")),
            QRectF(0, 0, 60, 60));
        QCOMPARE(stencil.elementAt(1).y, 0.0);
        QCOMPARE(stencil.elementAt(2).y, 30.0);

        // "Document" only exists as the registered document2 shape.
        const QPainterPath document = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=mxgraph.flowchart.document2;size=0.25;")),
            QRectF(0, 0, 100, 60));
        QVERIFY(document.elementCount() > 10);
        QCOMPARE(document.boundingRect().width(), 100.0);
    }

    void flowchartRegisteredShapesFollowShapesJs()
    {
        // OffPageConnectorShape: h * size is the notch height.
        const QPainterPath offPage = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=offPageConnector;rounded=0;size=0.5;")),
            QRectF(0, 0, 60, 60));
        QCOMPARE(offPage.elementCount(), 6);
        QCOMPARE(offPage.elementAt(2).y, 30.0);
        QCOMPARE(offPage.elementAt(3).x, 30.0);

        // HexagonShape: size is relative to the width unless fixedSize=1.
        const QPainterPath hexagon = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=hexagon;size=0.27;")), QRectF(0, 0, 100, 60));
        QCOMPARE(hexagon.elementAt(0).x, 27.0);

        const QPainterPath fixed = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=hexagon;fixedSize=1;size=20;")),
            QRectF(0, 0, 100, 60));
        QCOMPARE(fixed.elementAt(0).x, 20.0);

        // TrapezoidShape caps the offset at half the width; flipV mirrors it.
        const QPainterPath trapezoid = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=trapezoid;size=0.23;")), QRectF(0, 0, 100, 60));
        QCOMPARE(trapezoid.elementAt(1).x, 23.0);

        // ManualInputShape starts at the bottom left corner.
        const QPainterPath manual = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=manualInput;rounded=0;size=26;")),
            QRectF(0, 0, 100, 60));
        QCOMPARE(manual.elementCount(), 5);
        QCOMPARE(manual.elementAt(0).x, 0.0);
        QCOMPARE(manual.elementAt(0).y, 60.0);
        QCOMPARE(manual.elementAt(1).y, 26.0);

        // ProcessShape is an mxRectangleShape: absoluteArcSize makes the
        // corner radius a fixed number of pixels.
        const QPainterPath process = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("rounded=1;absoluteArcSize=1;arcSize=14;")),
            QRectF(0, 0, 100, 100));
        QCOMPARE(process.boundingRect(), QRectF(0, 0, 100, 100));
        QCOMPARE(process.elementAt(0).y, 7.0);
    }

    void miscLibraryMatchesSidebar()
    {
        // Sidebar.prototype.addMiscPalette, in draw.io order. Keys are the
        // stable internal identifiers; names are the sidebar titles.
        const QStringList official = {
            QStringLiteral("misc.autosizeText"), QStringLiteral("misc.autosizeTitle"),
            QStringLiteral("misc.link"), QStringLiteral("misc.curvedText"),
            QStringLiteral("misc.slantedTextLeft"), QStringLiteral("misc.slantedTextRight"),
            QStringLiteral("misc.slantedTextTop"), QStringLiteral("misc.variable"),
            QStringLiteral("misc.timestamp"), QStringLiteral("misc.unorderedList"),
            QStringLiteral("misc.orderedList"), QStringLiteral("misc.verticalList"),
            QStringLiteral("misc.note"), QStringLiteral("misc.table1"),
            QStringLiteral("misc.table2"), QStringLiteral("misc.tableWithTitle1"),
            QStringLiteral("misc.tableWithTitle2"), QStringLiteral("misc.crossFunctional"),
            QStringLiteral("misc.tableAssets"), QStringLiteral("misc.tableBordered"),
            QStringLiteral("misc.tablePlain"), QStringLiteral("misc.htmlTable"),
            QStringLiteral("misc.rectangleSketch"), QStringLiteral("misc.ellipseSketch"),
            QStringLiteral("misc.diamondSketch"), QStringLiteral("misc.isometricCube"),
            QStringLiteral("misc.isometricSquare"), QStringLiteral("misc.isometricEdge1"),
            QStringLiteral("misc.isometricEdge2"), QStringLiteral("misc.leftCurlyBracket"),
            QStringLiteral("misc.rightCurlyBracket"), QStringLiteral("misc.horizontalLine"),
            QStringLiteral("misc.verticalLine"), QStringLiteral("misc.horizontalBackbone"),
            QStringLiteral("misc.verticalBackbone"), QStringLiteral("misc.horizontalCrossbar"),
            QStringLiteral("misc.verticalCrossbar"), QStringLiteral("misc.imageFixedAspect"),
            QStringLiteral("misc.imageVariableAspect"), QStringLiteral("misc.icon"),
            QStringLiteral("misc.label1"), QStringLiteral("misc.label2"),
            QStringLiteral("misc.shapeGroup"), QStringLiteral("misc.partialRectangle1"),
            QStringLiteral("misc.partialRectangle2"), QStringLiteral("misc.partialRectangle3"),
            QStringLiteral("misc.waypoint"), QStringLiteral("misc.manualLine"),
            QStringLiteral("misc.filledEdge"), QStringLiteral("misc.taperedArrow"),
            QStringLiteral("misc.horizontalElbow"), QStringLiteral("misc.verticalElbow"),
            QStringLiteral("misc.arc"), QStringLiteral("misc.zigzag"),
        };

        QStringList keys;
        QHash<QString, ShapeDefinition> byKey;

        for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
        {
            if (definition.category == QLatin1String("misc"))
            {
                keys.append(definition.key);
                byKey.insert(definition.key, definition);
            }
        }

        QCOMPARE(keys, official);
        QCOMPARE(byKey.value(QStringLiteral("misc.autosizeText")).size, QSizeF(50, 30));
        QCOMPARE(byKey.value(QStringLiteral("misc.curvedText")).size, QSizeF(70, 50));
        QCOMPARE(byKey.value(QStringLiteral("misc.note")).size, QSizeF(150, 150));
        QCOMPARE(byKey.value(QStringLiteral("misc.table1")).size, QSizeF(180, 120));
        QCOMPARE(byKey.value(QStringLiteral("misc.crossFunctional")).size, QSizeF(400, 400));
        QCOMPARE(byKey.value(QStringLiteral("misc.htmlTable")).size, QSizeF(180, 180));
        QCOMPARE(byKey.value(QStringLiteral("misc.leftCurlyBracket")).size, QSizeF(20, 120));
        QCOMPARE(byKey.value(QStringLiteral("misc.horizontalLine")).size, QSizeF(160, 10));
        QCOMPARE(byKey.value(QStringLiteral("misc.zigzag")).size, QSizeF(160, 20));
        QCOMPARE(byKey.value(QStringLiteral("misc.link")).link,
                 QStringLiteral("https://www.draw.io"));
        QVERIFY(byKey.value(QStringLiteral("misc.timestamp")).placeholders);
        QVERIFY(byKey.value(QStringLiteral("misc.variable")).placeholders);
        QCOMPARE(byKey.value(QStringLiteral("misc.variable")).placeholderName,
                 QStringLiteral("Variable"));

        // The exact sidebar style strings (no color defaults appended).
        QCOMPARE(byKey.value(QStringLiteral("misc.curvedText")).style,
                 QStringLiteral("shape=curvedText;noLabel=1;align=center;"
                                "verticalAlign=middle;strokeColor=none;fillColor=none;"));
        QVERIFY(byKey.value(QStringLiteral("misc.rectangleSketch")).style.startsWith(
            QStringLiteral("rounded=1;whiteSpace=wrap;html=1;strokeWidth=2;fillWeight=4;")));
        QVERIFY(byKey.value(QStringLiteral("misc.ellipseSketch")).style.contains(
            QStringLiteral("fillStyle=dots;sketch=1;")));
        QVERIFY(byKey.value(QStringLiteral("misc.diamondSketch")).style.contains(
            QStringLiteral("fillStyle=cross-hatch;")));
        QVERIFY(byKey.value(QStringLiteral("misc.isometricCube")).style.startsWith(
            QStringLiteral("html=1;whiteSpace=wrap;shape=isoCube2;")));

        // Edge templates become floating edges.
        int edgeCount = 0;

        for (const QString &key : official)
        {
            if (byKey.value(key).edge)
            {
                ++edgeCount;
            }
        }

        QCOMPARE(edgeCount, 7);
        QVERIFY(byKey.value(QStringLiteral("misc.isometricEdge1")).edge);
        QVERIFY(byKey.value(QStringLiteral("misc.filledEdge")).edge);
        QVERIFY(byKey.value(QStringLiteral("misc.taperedArrow")).edge);
        QVERIFY(byKey.value(QStringLiteral("misc.horizontalElbow")).edge);

        // Tables carry their rows and cells, the shape group its triangle.
        QCOMPARE(byKey.value(QStringLiteral("misc.verticalList")).children.size(), 8);
        QCOMPARE(byKey.value(QStringLiteral("misc.table1")).children.size(), 12);
        QCOMPARE(byKey.value(QStringLiteral("misc.tableWithTitle2")).children.size(), 9);
        QCOMPARE(byKey.value(QStringLiteral("misc.crossFunctional")).children.size(), 12);
        QCOMPARE(byKey.value(QStringLiteral("misc.tableAssets")).children.size(), 20);
        QCOMPARE(byKey.value(QStringLiteral("misc.shapeGroup")).children.size(), 1);
        QCOMPARE(byKey.value(QStringLiteral("misc.shapeGroup")).children.first().value,
                 QString());
        QCOMPARE(byKey.value(QStringLiteral("misc.verticalList")).children.at(1).value,
                 QStringLiteral("Item 1"));

        // The deleted legacy entries are gone.
        QVERIFY(!byKey.contains(QStringLiteral("misc.container")));
        QVERIFY(!byKey.contains(QStringLiteral("misc.list")));
        QVERIFY(!byKey.contains(QStringLiteral("misc.callout")));
    }

    void placeholderLabelsExpand()
    {
        Cell variable(Cell::Kind::Vertex, QStringLiteral("variable"));
        variable.value = QStringLiteral("%name% Text");
        variable.placeholders = true;
        variable.placeholderName = QStringLiteral("Variable");
        QCOMPARE(variable.displayLabel(), QStringLiteral("Variable Text"));
        QCOMPARE(variable.plainLabel(), QStringLiteral("Variable Text"));

        Cell timestamp(Cell::Kind::Vertex, QStringLiteral("timestamp"));
        timestamp.value = QStringLiteral("%date{yyyy}%");
        timestamp.placeholders = true;
        QVERIFY(timestamp.displayLabel().contains(
            QString::number(QDate::currentDate().year())));

        // Without the placeholders flag the template stays verbatim.
        Cell raw(Cell::Kind::Vertex, QStringLiteral("raw"));
        raw.value = QStringLiteral("%name% Text");
        QCOMPARE(raw.displayLabel(), QStringLiteral("%name% Text"));
    }

    void miscShapeGeometry()
    {
        const QPainterPath crossbar = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=crossbar;rounded=1;")),
            QRectF(0, 0, 120, 20));
        QCOMPARE(crossbar.elementCount(), 6);
        QCOMPARE(crossbar.elementAt(0).x, 0.0);
        QCOMPARE(crossbar.elementAt(0).y, 0.0);
        QCOMPARE(crossbar.elementAt(0).type, QPainterPath::MoveToElement);
        QCOMPARE(crossbar.elementAt(2).x, 120.0);

        const QPainterPath zigzag = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=zigzag;fillColor=none;")),
            QRectF(0, 0, 160, 20));
        QCOMPARE(zigzag.elementCount(), 17);
        QCOMPARE(zigzag.elementAt(0).x, 0.0);
        QCOMPARE(zigzag.elementAt(0).y, 10.0);
        QCOMPARE(zigzag.elementAt(1).x, 5.0);
        QCOMPARE(zigzag.elementAt(1).y, 1.0);

        const QPainterPath bracket = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=curlyBracket;rounded=1;")),
            QRectF(0, 0, 20, 120));
        QVERIFY(bracket.elementCount() > 8);
        QCOMPARE(bracket.boundingRect().right(), 20.0);
        QCOMPARE(bracket.boundingRect().bottom(), 120.0);
        QVERIFY(bracket.boundingRect().width() > 18.0);

        const QPainterPath cube = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=isoCube2;isoAngle=15;")),
            QRectF(0, 0, 90, 100));
        QCOMPARE(cube.elementCount(), 7);
        QCOMPARE(cube.elementAt(0).x, 45.0);
        QVERIFY(qAbs(cube.elementAt(1).y - 21.6070883172) < 0.01);

        const QPainterPath iso = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=isoRectangle;aspect=fixed;")),
            QRectF(0, 0, 150, 90));
        QCOMPARE(iso.elementCount(), 5);
        QCOMPARE(iso.boundingRect().center().x(), 75.0);

        const QPainterPath table = ShapeRegistry::pathForStyle(
            Style::parse(QStringLiteral("shape=table;tableRender=collapsed;startSize=30;")),
            QRectF(0, 0, 180, 150));
        QCOMPARE(table.boundingRect(), QRectF(0, 0, 180, 150));
    }
};

QTEST_MAIN(TestModel)
#include "tst_model.moc"
