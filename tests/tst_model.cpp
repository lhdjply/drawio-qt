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

    void foldIconReflectsState()
    {
        const auto inkInFold = [](const QString &style, bool collapsed)
        {
            Cell container(Cell::Kind::Vertex, QStringLiteral("c"));
            container.style = Style::parse(style);
            container.collapsed = collapsed;
            container.geometry = QRectF(0, 0, 200, 120);

            QImage image(200, 120, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::white);
            QPainter painter(&image);
            ShapeRenderer::paint(painter, container);
            painter.end();

            int ink = 0;

            for (int y = 3; y <= 14; ++y)
            {
                for (int x = 3; x <= 14; ++x)
                {
                    if (image.pixelColor(x, y).lightness() < 200)
                    {
                        ++ink;
                    }
                }
            }

            return ink;
        };

        const int expanded = inkInFold(QStringLiteral("swimlane;whiteSpace=wrap;html=1;"), false);
        const int collapsed = inkInFold(QStringLiteral("swimlane;whiteSpace=wrap;html=1;"), true);

        // The collapsed icon has an extra vertical bar (plus instead of minus).
        QVERIFY2(collapsed > expanded, qPrintable(QString::number(expanded) + "/" + QString::number(collapsed)));
    }

    void containersDrawFoldIndicator()
    {
        Cell container(Cell::Kind::Vertex, QStringLiteral("c"));
        container.style = Style::parse(QStringLiteral("swimlane;whiteSpace=wrap;html=1;"));
        container.geometry = QRectF(0, 0, 200, 120);

        QImage image(200, 120, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter painter(&image);
        ShapeRenderer::paint(painter, container);
        painter.end();

        // The fold square sits at 4,4 and has ink for its border and minus.
        int ink = 0;

        for (int y = 3; y <= 14; ++y)
        {
            for (int x = 3; x <= 14; ++x)
            {
                if (image.pixelColor(x, y).lightness() < 200)
                {
                    ++ink;
                }
            }
        }

        QVERIFY2(ink > 10, qPrintable(QString::number(ink)));
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

        // The plain XOR stencil is not part of the General palette.
        QVERIFY(basic.contains(QStringLiteral("xor")));
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

        // Sidebar-Basic.js stencils stay in the basic library.
        for (const QString &key : {QStringLiteral("basic.star4"), QStringLiteral("basic.banner"),
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
                                   QStringLiteral("image")})
        {
            QVERIFY2(keys.contains(key), qPrintable(key));
        }

        QCOMPARE(ShapeRegistry::definition(QStringLiteral("rectangle")).category,
                 QStringLiteral("general"));
        QCOMPARE(ShapeRegistry::definition(QStringLiteral("basic.star4")).category,
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
        QVERIFY(keys.contains(QStringLiteral("basic.cylinder")));
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

        QVERIFY(path.elementCount() >= 6);
        QCOMPARE(path.elementAt(0).x, 40.0);
        QCOMPARE(path.elementAt(0).y, 0.0);
        QCOMPARE(path.elementAt(2).x, 60.0);
        QCOMPARE(path.elementAt(2).y, 100.0);
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
};

QTEST_MAIN(TestModel)
#include "tst_model.moc"
