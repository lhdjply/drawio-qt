#include "core/Page.h"
#include "render/EdgeRouter.h"

#include <QtTest>

using namespace dq;

class TestRouter : public QObject
{
    Q_OBJECT

private slots:
    void orthogonalRouteIsAxisAligned()
    {
        Page page;
        Cell *a = page.addVertex(QRectF(100, 100, 100, 60));
        Cell *b = page.addVertex(QRectF(400, 300, 100, 60));
        Cell *edge = page.addEdge(a->id, b->id);

        const RoutedEdge route = EdgeRouter::route(page, *edge);

        QVERIFY(route.points.size() >= 3);
        QVERIFY(route.sourceCell == a);
        QVERIFY(route.targetCell == b);

        // The first point sits on A's outline, the last one on B's outline.
        QVERIFY(route.start().x() >= 99.0 && route.start().x() <= 201.0);
        QVERIFY(route.start().y() >= 99.0 && route.start().y() <= 161.0);
        QVERIFY(route.end().x() >= 399.0 && route.end().x() <= 501.0);
        QVERIFY(route.end().y() >= 299.0 && route.end().y() <= 361.0);

        for (int i = 1; i < route.points.size(); ++i)
        {
            const QPointF delta = route.points.at(i) - route.points.at(i - 1);
            const bool horizontal = qAbs(delta.y()) < 0.01;
            const bool vertical = qAbs(delta.x()) < 0.01;
            QVERIFY2(horizontal || vertical, "orthogonal segments must be axis aligned");
        }
    }

    void waypointsAreVisited()
    {
        Page page;
        Cell *a = page.addVertex(QRectF(0, 0, 80, 40));
        Cell *b = page.addVertex(QRectF(300, 0, 80, 40));
        Cell *edge = page.addEdge(a->id, b->id);
        edge->waypoints = {QPointF(150, -80), QPointF(220, 100)};

        const RoutedEdge route = EdgeRouter::route(page, *edge);
        bool firstFound = false;
        bool secondFound = false;

        for (const QPointF &point : route.points)
        {
            if (QLineF(point, QPointF(150, -80)).length() < 0.01)
            {
                firstFound = true;
            }

            if (QLineF(point, QPointF(220, 100)).length() < 0.01)
            {
                secondFound = true;
            }
        }

        QVERIFY(firstFound);
        QVERIFY(secondFound);
    }

    void straightStyleProducesTwoPoints()
    {
        Page page;
        Cell *a = page.addVertex(QRectF(0, 0, 80, 40));
        Cell *b = page.addVertex(QRectF(200, 120, 80, 40));
        Cell *edge = page.addEdge(a->id, b->id,
                                  Style::parse(QStringLiteral("edgeStyle=segmentEdgeStyle;")));

        const RoutedEdge route = EdgeRouter::route(page, *edge);
        QCOMPARE(route.points.size(), 2);
    }

    void freeEdgeUsesGivenPoints()
    {
        Page page;
        Cell *edge = page.addEdge(QString(), QString());
        edge->sourcePoint = QPointF(10, 10);
        edge->targetPoint = QPointF(200, 140);

        const RoutedEdge route = EdgeRouter::route(page, *edge);
        QCOMPARE(route.start(), QPointF(10, 10));
        QCOMPARE(route.end(), QPointF(200, 140));
    }

    void exitConstraintsAreHonoured()
    {
        Page page;
        Cell *a = page.addVertex(QRectF(0, 0, 100, 100));
        Cell *b = page.addVertex(QRectF(300, 0, 100, 100));
        Cell *edge = page.addEdge(a->id, b->id,
                                  Style::parse(QStringLiteral(
                                      "edgeStyle=orthogonalEdgeStyle;exitX=1;exitY=0.25;"
                                      "entryX=0;entryY=0.75;")));

        const RoutedEdge route = EdgeRouter::route(page, *edge);
        QCOMPARE(route.start(), QPointF(100, 25));
        QCOMPARE(route.end(), QPointF(300, 75));
    }

    void simplifyRemovesCollinearPoints()
    {
        const QVector<QPointF> points = {QPointF(0, 0), QPointF(10, 0), QPointF(20, 0),
                                         QPointF(20, 20)};
        const QVector<QPointF> simplified = EdgeRouter::simplify(points);
        QCOMPARE(simplified.size(), 3);
        QCOMPARE(simplified.first(), QPointF(0, 0));
        QCOMPARE(simplified.last(), QPointF(20, 20));
    }

    void ellipseConnectionPointLiesOnOutline()
    {
        Page page;
        Cell *ellipse = page.addVertex(QRectF(0, 0, 100, 60),
                                       Style::parse(QStringLiteral("ellipse;")));
        QPointF direction;
        const QPointF point = EdgeRouter::connectionPoint(*ellipse, QPointF(200, 30), &direction);

        QVERIFY(point.x() > 90.0 && point.x() <= 101.0);
        QVERIFY(qAbs(point.y() - 30.0) < 2.0);
        QVERIFY(direction.x() > 0.0);
    }
};

QTEST_MAIN(TestRouter)
#include "tst_router.moc"
