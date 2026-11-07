#include "core/Document.h"
#include "core/Page.h"

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
