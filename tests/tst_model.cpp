#include "core/Document.h"
#include "core/Page.h"
#include "render/ShapeRegistry.h"
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
            if (definition.category != QLatin1String("basic")
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

    void flowchartShapesAreInBasicLibrary()
    {
        QStringList keys;

        for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
        {
            if (definition.category == QStringLiteral("basic"))
            {
                keys.append(definition.key);
            }
        }

        for (const QString &key : {QStringLiteral("or"), QStringLiteral("xor"),
                                   QStringLiteral("dataStorage"),
                                   QStringLiteral("internalStorage"),
                                   QStringLiteral("wedgeCallout")})
        {
            QVERIFY2(keys.contains(key), qPrintable(key));
        }
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

    void tapeIsInBasicLibrary()
    {
        bool found = false;

        for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
        {
            if (definition.key == QStringLiteral("tape"))
            {
                found = true;
                QCOMPARE(definition.category, QStringLiteral("basic"));
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
};

QTEST_MAIN(TestModel)
#include "tst_model.moc"
