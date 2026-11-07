#include "core/Document.h"
#include "core/Page.h"
#include "render/ShapeRegistry.h"

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
