#include "core/Document.h"
#include "ui/DiagramView.h"
#include "ui/OverlayEditor.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QFileInfo>
#include <QImage>
#include <QTimer>
#include <QUndoStack>
#include <QtTest>

using namespace dq;

/// Builds a real editor window offscreen, loads the bundled example and grabs
/// a screenshot. This catches layout/rendering regressions without a display.
class TestUiSmoke : public QObject
{
    Q_OBJECT

private slots:
    void windowRendersExample()
    {
        MainWindow window;
        window.resize(1280, 820);
        window.show();

        const QString example = QStringLiteral(DRAWIO_QT_SOURCE_DIR)
            + QStringLiteral("/resources/examples/basic-flow.drawio");
        QVERIFY(QFileInfo::exists(example));
        QVERIFY(window.openFile(example));

        QVERIFY(window.document() != nullptr);
        QCOMPARE(window.document()->pageCount(), 1);
        QCOMPARE(window.document()->currentPage()->vertices().size(), 6);

        // Select a shape so the format panel and selection handles are shown
        // in the generated screenshot.
        for (Cell *cell : window.document()->currentPage()->vertices())
        {
            if (cell->value == QStringLiteral("提交订单"))
            {
                window.view()->setSelection({cell->id});
                break;
            }
        }

        // Let the event loop lay out docks and render once.
        (void)QTest::qWaitForWindowExposed(&window);
        QCoreApplication::processEvents();

        const QImage screenshot = window.grab().toImage();
        QVERIFY(!screenshot.isNull());
        QVERIFY(screenshot.width() >= 1000);

        const QString outputPath = QStringLiteral(DRAWIO_QT_BINARY_DIR)
            + QStringLiteral("/ui-smoke.png");
        QVERIFY(screenshot.save(outputPath));

        // The canvas is not empty: some pixels differ from the background.
        const QColor background = screenshot.pixelColor(10, 10);
        bool foundDifferent = false;

        for (int y = 20; y < screenshot.height() - 20 && !foundDifferent; y += 7)
        {
            for (int x = 20; x < screenshot.width() - 20; x += 7)
            {
                if (screenshot.pixelColor(x, y) != background)
                {
                    foundDifferent = true;
                    break;
                }
            }
        }

        QVERIFY(foundDifferent);
    }

    void canvasSelectionAndUndo()
    {
        MainWindow window;
        window.resize(1000, 700);
        window.show();

        DiagramView *view = window.view();
        QVERIFY(view != nullptr);

        const ShapeDefinition rectangle = ShapeRegistry::definition(QStringLiteral("rectangle"));
        Cell *cell = view->addShape(rectangle, QPointF(200, 200));
        QVERIFY(cell != nullptr);
        QCOMPARE(view->selection().size(), 1);

        const double beforeX = cell->geometry.x();
        const QString id = cell->id;
        view->moveSelectionBy(QPointF(60, 0));
        QCOMPARE(view->page()->cell(id)->geometry.x(), beforeX + 60.0);

        QVERIFY(view->undoStack()->canUndo());
        view->undoStack()->undo();
        QCOMPARE(view->page()->cell(id)->geometry.x(), beforeX);

        view->undoStack()->redo();
        QCOMPARE(view->page()->cell(id)->geometry.x(), beforeX + 60.0);

        // Style change is undoable too.
        view->applyStyle(QStringLiteral("fillColor"), QStringLiteral("#ff0000"));
        QCOMPARE(view->page()->cell(id)->styleValue(QStringLiteral("fillColor")),
                 QStringLiteral("#ff0000"));
        view->undoStack()->undo();
        QVERIFY(view->page()->cell(id)->styleValue(QStringLiteral("fillColor"))
                != QStringLiteral("#ff0000"));
    }

    void inlineEditingCommitsLabel()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *cell = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                    QPointF(120, 120));
        QVERIFY(cell != nullptr);
        const QString id = cell->id;

        view->startEditing(id, true);
        auto *editor = view->findChild<OverlayEditor *>();
        QVERIFY(editor != nullptr);
        QVERIFY(editor->isVisible());

        editor->setPlainText(QStringLiteral("测试标签"));
        view->commitEditing();

        QCOMPARE(view->page()->cell(id)->value, QStringLiteral("测试标签"));
        QVERIFY(!view->isEditing());

        view->undoStack()->undo();
        QCOMPARE(view->page()->cell(id)->value, QString());
    }

    void saveAndReopenDocument()
    {
        const QString path = QStringLiteral(DRAWIO_QT_BINARY_DIR)
            + QStringLiteral("/roundtrip.drawio");

        {
            MainWindow window;
            window.show();
            DiagramView *view = window.view();
            Cell *cell = view->addShape(
                ShapeRegistry::definition(QStringLiteral("ellipse")), QPointF(200, 150));
            view->setCellValue(cell->id, QStringLiteral("已保存"));

            QString error;
            QVERIFY2(window.document()->save(path, &error), qPrintable(error));
            QVERIFY(QFileInfo::exists(path));
        }

        MainWindow reopened;
        reopened.show();
        QVERIFY(reopened.openFile(path));
        QCOMPARE(reopened.document()->currentPage()->vertices().size(), 1);
        QCOMPARE(reopened.document()->currentPage()->vertices().first()->value,
                 QStringLiteral("已保存"));
        QVERIFY(!reopened.document()->isModified());
    }

    void connectShapesWithMouse()
    {
        MainWindow window;
        window.resize(1000, 700);
        window.show();
        (void)QTest::qWaitForWindowExposed(&window);

        DiagramView *view = window.view();
        Cell *a = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                 QPointF(180, 160));
        Cell *b = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                 QPointF(520, 360));
        const QString aId = a->id;
        const QString bId = b->id;
        const int before = view->page()->cells().size();

        view->setSelection({});
        view->clearSelection();

        // Hover the right edge of A first: the connection handle only exists
        // while the shape is hovered.
        const QPointF handleScene = QPointF(a->absoluteRect().right(),
                                            a->absoluteRect().center().y());
        const QPointF handleView = view->toView(handleScene);
        QTest::mouseMove(view, handleView.toPoint());
        QCoreApplication::processEvents();

        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, handleView.toPoint());
        const QPointF targetView = view->toView(b->absoluteRect().center());
        QTest::mouseMove(view, targetView.toPoint());
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, targetView.toPoint());
        QCoreApplication::processEvents();

        QCOMPARE(view->page()->cells().size(), before + 1);
        QVERIFY(view->selection().size() == 1);

        const QString edgeId = *view->selection().begin();
        Cell *edge = view->page()->cell(edgeId);
        QVERIFY(edge != nullptr);
        QVERIFY(edge->isEdge());
        QCOMPARE(edge->sourceId, aId);
        QCOMPARE(edge->targetId, bId);
    }

    void resizeWithMouseHandles()
    {
        MainWindow window;
        window.resize(1000, 700);
        window.show();
        (void)QTest::qWaitForWindowExposed(&window);

        DiagramView *view = window.view();
        Cell *cell = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                    QPointF(200, 200));
        const QString id = cell->id;
        view->setSelection({id});
        QCoreApplication::processEvents();

        const QRectF before = cell->absoluteRect();
        const QPointF corner = view->toView(before.bottomRight());
        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, corner.toPoint());
        QTest::mouseMove(view, corner.toPoint() + QPoint(60, 30));
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier,
                            corner.toPoint() + QPoint(60, 30));
        QCoreApplication::processEvents();

        const QRectF after = view->page()->cell(id)->absoluteRect();
        QVERIFY(after.width() > before.width());
        QVERIFY(after.height() > before.height());

        view->undoStack()->undo();
        QCOMPARE(view->page()->cell(id)->absoluteRect().size(), before.size());
    }

    void marqueeSelectsIntersectingShapes()
    {
        MainWindow window;
        window.resize(1000, 700);
        window.show();
        (void)QTest::qWaitForWindowExposed(&window);

        DiagramView *view = window.view();
        view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                       QPointF(180, 160));
        view->addShape(ShapeRegistry::definition(QStringLiteral("ellipse")),
                       QPointF(380, 300));
        view->clearSelection();

        const QPoint from = view->toView(QPointF(20, 20)).toPoint();
        const QPoint to = view->toView(QPointF(600, 420)).toPoint();
        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(view, to);
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, to);
        QCoreApplication::processEvents();

        QCOMPARE(view->selection().size(), 2);
    }

    void groupAndUngroup()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *a = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                 QPointF(120, 120));
        Cell *b = view->addShape(ShapeRegistry::definition(QStringLiteral("ellipse")),
                                 QPointF(360, 240));
        const QString aId = a->id;
        const QString bId = b->id;
        const QPointF aPosition = a->absolutePosition();
        const QPointF bPosition = b->absolutePosition();

        view->setSelection({aId, bId});
        view->groupSelection();

        QCOMPARE(view->page()->cells().size(), 3);
        QCOMPARE(view->selection().size(), 1);

        const QString groupId = *view->selection().begin();
        Cell *group = view->page()->cell(groupId);
        QVERIFY(group != nullptr);
        QCOMPARE(group->children.size(), 2);
        QCOMPARE(view->page()->cell(aId)->parent, group);

        // Grouping must not move anything.
        QCOMPARE(view->page()->cell(aId)->absolutePosition(), aPosition);
        QCOMPARE(view->page()->cell(bId)->absolutePosition(), bPosition);

        view->setSelection({groupId});
        view->ungroupSelection();

        QCOMPARE(view->page()->cells().size(), 2);
        QVERIFY(view->page()->cell(groupId) == nullptr);
        QCOMPARE(view->page()->cell(aId)->absolutePosition(), aPosition);
        QCOMPARE(view->page()->cell(bId)->absolutePosition(), bPosition);

        // The whole sequence is undoable.
        view->undoStack()->undo();
        QCOMPARE(view->page()->cells().size(), 3);
        view->undoStack()->undo();
        QCOMPARE(view->page()->cells().size(), 2);
    }

    void multiPageDocumentPersists()
    {
        const QString path = QStringLiteral(DRAWIO_QT_BINARY_DIR)
            + QStringLiteral("/multipage.drawio");

        {
            MainWindow window;
            window.show();
            DiagramView *view = window.view();
            view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                           QPointF(100, 100));

            window.document()->addPage(QStringLiteral("第二页"));
            window.document()->setCurrentIndex(1);
            view->refreshFromDocument();
            view->addShape(ShapeRegistry::definition(QStringLiteral("ellipse")),
                           QPointF(200, 200));

            QString error;
            QVERIFY2(window.document()->save(path, &error), qPrintable(error));
        }

        MainWindow reopened;
        reopened.show();
        QVERIFY(reopened.openFile(path));
        QCOMPARE(reopened.document()->pageCount(), 2);
        QCOMPARE(reopened.document()->page(0)->vertices().size(), 1);
        QCOMPARE(reopened.document()->page(1)->vertices().size(), 1);
        QCOMPARE(reopened.document()->page(1)->name, QStringLiteral("第二页"));

        // Switching pages through the tab bar keeps the canvas in sync.
        reopened.document()->setCurrentIndex(0);
        QCOMPARE(reopened.view()->page(), reopened.document()->page(0));
    }

    void clipboardAndDuplicate()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        const ShapeDefinition ellipse = ShapeRegistry::definition(QStringLiteral("ellipse"));
        view->addShape(ellipse, QPointF(150, 150));
        const int before = view->page()->cells().size();

        view->duplicateSelection();
        QCOMPARE(view->page()->cells().size(), before + 1);

        view->copySelection();
        view->paste();
        QCOMPARE(view->page()->cells().size(), before + 2);
    }
};

QTEST_MAIN(TestUiSmoke)
#include "tst_ui_smoke.moc"
