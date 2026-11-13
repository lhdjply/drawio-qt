#include "app/AppSettings.h"
#include "app/UiTheme.h"
#include "core/Document.h"
#include "core/MxFileIO.h"
#include "ui/DiagramView.h"
#include "ui/FormatPanel.h"
#include "ui/ShapeToolbox.h"
#include "ui/ShapeLibraryDialog.h"
#include "render/ShapeCatalog.h"
#include "render/DiagramRenderer.h"
#include "render/EdgeRenderer.h"
#include "render/EdgeRouter.h"
#include "ui/OverlayEditor.h"
#include "ui/MainWindow.h"
#include "ui/LayerPanel.h"
#include "ui/Dialogs.h"
#include "ui/FormatPanel.h"
#include "app/I18n.h"

#include <QApplication>
#include <QColorDialog>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>
#include <QKeyEvent>
#include <QMenu>
#include <QMenuBar>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QImage>
#include <QTimer>
#include <QUndoStack>
#include <QListWidget>
#include <QCheckBox>
#include <QTabBar>
#include <QTreeWidget>
#include <QSpinBox>
#include <QToolButton>
#include <QScrollArea>
#include <QSettings>
#include "ui/Dialogs.h"
#include "ui/FormatPanel.h"
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextEdit>
#include <QtTest>

using namespace dq;

/// Builds a real editor window offscreen, loads the bundled example and grabs
/// a screenshot. This catches layout/rendering regressions without a display.
class TestUiSmoke : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        UiTheme::applyFusionStyle();
        UiTheme::applyColorScheme(QStringLiteral("light"));
    }

    void windowRendersExample()
    {
        MainWindow window;
        window.resize(1280, 820);
        window.show();

        const QString example = QStringLiteral(DRAWIO_QT_SOURCE_DIR)
            + QStringLiteral("/resources/assets/examples/basic-flow.drawio");
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

        // Show the electrical library in the generated screenshot.
        if (auto *toolbox = window.findChild<ShapeToolbox *>())
        {
            if (auto *combo = toolbox->findChild<QComboBox *>())
            {
                const int index = combo->findData(QStringLiteral("electrical"));

                if (index >= 0)
                {
                    combo->setCurrentIndex(index);
                }
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

    void darkThemeRenders()
    {
        // The window applies its theme on construction, so set the stored
        // preference (not just the palette) before creating it.
        AppSettings::instance().setTheme(QStringLiteral("dark"));

        MainWindow window;
        window.resize(1100, 720);
        window.show();
        (void)QTest::qWaitForWindowExposed(&window);

        const QString example = QStringLiteral(DRAWIO_QT_SOURCE_DIR)
            + QStringLiteral("/resources/assets/examples/basic-flow.drawio");
        QVERIFY(window.openFile(example));
        window.view()->setDarkTheme(true);
        QCoreApplication::processEvents();

        const QImage screenshot = window.grab().toImage();
        QVERIFY(!screenshot.isNull());

        const QString outputPath = QStringLiteral(DRAWIO_QT_BINARY_DIR)
            + QStringLiteral("/ui-dark.png");
        QVERIFY(screenshot.save(outputPath));

        QVERIFY(screenshot.pixelColor(screenshot.width() / 2, 4).lightness() < 110
                || screenshot.pixelColor(4, screenshot.height() / 2).lightness() < 110);

        // Restore the light theme for the remaining tests.
        AppSettings::instance().setTheme(QStringLiteral("light"));
        UiTheme::applyColorScheme(QStringLiteral("light"));
    }

    void droppingShapeAddsCell()
    {
        MainWindow window;
        window.resize(1000, 700);
        window.show();
        (void)QTest::qWaitForWindowExposed(&window);

        DiagramView *view = window.view();
        QVERIFY(view != nullptr);
        const int before = view->page()->cells().size();

        QMimeData mime;
        mime.setData(QStringLiteral("application/x-drawio-shape"),
                     QByteArrayLiteral("rectangle"));

        const QPoint position(320, 260);
        QDragEnterEvent enter(position, Qt::CopyAction, &mime, Qt::LeftButton,
                              Qt::NoModifier);
        QApplication::sendEvent(view, &enter);
        QVERIFY(enter.isAccepted());

        QDragMoveEvent move(position, Qt::CopyAction, &mime, Qt::LeftButton,
                            Qt::NoModifier);
        QApplication::sendEvent(view, &move);

        QDropEvent drop(QPointF(position), Qt::CopyAction, &mime, Qt::LeftButton,
                        Qt::NoModifier);
        QApplication::sendEvent(view, &drop);
        QCoreApplication::processEvents();

        QCOMPARE(view->page()->cells().size(), before + 1);
        QCOMPARE(view->selection().size(), 1);
    }

    void droppingImageLibraryShapeAddsCell()
    {
        MainWindow window;
        window.resize(1000, 700);
        window.show();
        (void)QTest::qWaitForWindowExposed(&window);

        DiagramView *view = window.view();
        const int before = view->page()->cells().size();

        QMimeData mime;
        mime.setData(QStringLiteral("application/x-drawio-shape"),
                     QByteArrayLiteral("img/lib/active_directory/database_cube.svg"));

        const QPoint position(360, 300);
        QDragEnterEvent enter(position, Qt::CopyAction, &mime, Qt::LeftButton,
                              Qt::NoModifier);
        QApplication::sendEvent(view, &enter);
        QVERIFY(enter.isAccepted());

        QDropEvent drop(QPointF(position), Qt::CopyAction, &mime, Qt::LeftButton,
                        Qt::NoModifier);
        QApplication::sendEvent(view, &drop);
        QCoreApplication::processEvents();

        QCOMPARE(view->page()->cells().size(), before + 1);
        Cell *cell = view->page()->cell(*view->selection().begin());
        QVERIFY(cell != nullptr);
        QCOMPARE(cell->styleValue(QStringLiteral("image")),
                 QStringLiteral("img/lib/active_directory/database_cube.svg"));
    }

    void arrangeFunctionsLiveInTheMenu()
    {
        MainWindow window;
        window.show();
        (void)QTest::qWaitForWindowExposed(&window);

        QMenu *arrangeMenu = nullptr;

        for (QAction *action : window.menuBar()->actions())
        {
            if (action->text().startsWith(QStringLiteral("Arrange")))
            {
                arrangeMenu = action->menu();
                break;
            }
        }

        QVERIFY(arrangeMenu != nullptr);

        QMenu *alignMenu = nullptr;
        QMenu *layoutMenu = nullptr;

        for (QAction *action : arrangeMenu->actions())
        {
            if (action->menu() == nullptr)
            {
                continue;
            }

            if (action->text() == QStringLiteral("Align"))
            {
                alignMenu = action->menu();
            }
            else if (action->text() == QStringLiteral("Layout"))
            {
                layoutMenu = action->menu();
            }
        }

        QVERIFY(alignMenu != nullptr);
        QVERIFY(layoutMenu != nullptr);
        QVERIFY(alignMenu->actions().size() >= 6);
        QVERIFY(layoutMenu->actions().size() >= 5);

        QStringList arrangeTexts;

        for (QAction *action : arrangeMenu->actions())
        {
            arrangeTexts.append(action->text());
        }

        QVERIFY(arrangeTexts.contains(QStringLiteral("Group")));
        QVERIFY(arrangeTexts.contains(QStringLiteral("Ungroup")));

        // The right hand format panel no longer contains an arrange section.
        auto *panel = window.findChild<FormatPanel *>();
        QVERIFY(panel != nullptr);

        for (QGroupBox *group : panel->findChildren<QGroupBox *>())
        {
            QVERIFY(group->title() != QStringLiteral("排列"));
        }
    }

    void advancedPaletteEntriesInsertTheirChildren()
    {
        MainWindow window;
        window.resize(1000, 700);
        window.show();

        DiagramView *view = window.view();
        QVERIFY(view != nullptr);

        int inserted = 0;

        for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
        {
            if (definition.category != QLatin1String("advanced"))
            {
                continue;
            }

            Cell *cell = view->addShape(definition, QPointF(300, 300));
            QVERIFY2(cell != nullptr, qPrintable(definition.key));
            ++inserted;

            int vertices = 0;
            int edges = 0;

            for (const ShapeChildDefinition &child : definition.children)
            {
                child.edge ? ++edges : ++vertices;
            }

            QCOMPARE(cell->children.size(), vertices + edges);

            // The seeded nodes and connectors are children of the container.
            for (Cell *child : cell->children)
            {
                QCOMPARE(child->parent, cell);

                if (child->isEdge())
                {
                    QVERIFY(cell->page->cell(child->sourceId) != nullptr);
                    QVERIFY(cell->page->cell(child->targetId) != nullptr);
                }
            }

            // transparentBounds containers keep a zero sized stored geometry;
            // the visible box is derived from the children and must surround
            // them, wherever the entry was dropped.
            if (definition.key.endsWith(QStringLiteral("Layout")))
            {
                QCOMPARE(cell->geometry.size(), QSizeF(0, 0));

                const QRectF box = ShapeRegistry::visibleRect(*cell);
                QVERIFY(!box.isEmpty());
                QVERIFY2(box.contains(QPointF(300, 300)), qPrintable(definition.key));

                for (Cell *child : cell->children)
                {
                    if (child->isVertex())
                    {
                        QVERIFY2(box.contains(child->absoluteRect()), qPrintable(definition.key));
                    }
                }
            }
        }

        QCOMPARE(inserted, 42);
    }

    void layoutContainerIsHitTestable()
    {
        MainWindow window;
        window.resize(1000, 700);
        window.show();

        DiagramView *view = window.view();
        QVERIFY(view != nullptr);

        // Drop a live layout container: its stored geometry is (0,0,0,0) and
        // the visible box comes from the children, so hit testing and the
        // marquee must use that box.
        Cell *container = view->addShape(
            ShapeRegistry::definition(QStringLiteral("advanced.verticalTreeLayout")),
            QPointF(300, 300));
        QVERIFY(container != nullptr);
        QVERIFY(ShapeRegistry::hasTransparentBounds(*container));

        const QRectF box = ShapeRegistry::visibleRect(*container);
        QVERIFY(!box.isEmpty());

        // A click inside the derived box (outside the child nodes) selects the
        // container itself.
        view->selectCell(QString());
        QCOMPARE(view->cellAt(box.bottomLeft() + QPointF(6, -6)), container);

        // A rubber band over the box selects the container.
        const QSet<QString> marquee = view->cellsInMarquee(box.adjusted(-5, -5, 5, 5));
        QVERIFY2(marquee.contains(container->id), "marquee must select the container");

        // The selection bounds cover the whole derived box.
        view->selectCell(container->id);
        const QRectF selection = view->selectionBounds();
        QVERIFY(selection.contains(box.topLeft()));
        QVERIFY(selection.contains(box.bottomRight()));
    }

    void rotatingAContainerCarriesItsChildren()
    {
        MainWindow window;
        window.resize(1000, 700);
        window.show();

        DiagramView *view = window.view();
        QVERIFY(view != nullptr);

        Cell *container = view->addShape(
            ShapeRegistry::definition(QStringLiteral("advanced.verticalTreeLayout")),
            QPointF(300, 300));
        QVERIFY(container != nullptr);

        const QRectF box = ShapeRegistry::visibleRect(*container);
        const QPointF pivot = box.center();

        QVector<Cell *> children;

        for (Cell *child : container->children)
        {
            if (child->isVertex())
            {
                children.append(child);
            }
        }

        QVERIFY(!children.isEmpty());

        QVector<QPointF> before;

        for (Cell *child : children)
        {
            before.append(child->geometry.center());
        }

        view->selectCell(container->id);
        view->setRotation(90.0);

        // The container itself is rotated and every child orbits the derived
        // box centre by the same angle (mxVertexHandler.rotateCell).
        QCOMPARE(container->effectiveRotation(), 90.0);

        for (int i = 0; i < children.size(); ++i)
        {
            const QPointF center = children.at(i)->geometry.center();
            const QPointF offset = before.at(i) - pivot;
            const QPointF expected(pivot.x() - offset.y(), pivot.y() + offset.x());

            QVERIFY2(QLineF(center, expected).length() < 0.01,
                     qPrintable(QString::number(i)));
            QCOMPARE(children.at(i)->effectiveRotation(), 90.0);
        }
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
        QVERIFY(window.undoAction() != nullptr);
        QVERIFY(window.undoAction()->isEnabled());

        view->undoStack()->undo();
        QCOMPARE(view->page()->cell(id)->value, QString());
        QVERIFY(window.redoAction() != nullptr);
        QVERIFY(window.redoAction()->isEnabled());
    }

    void htmlInlineFontSurvivesEditing()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *cell = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                    QPointF(120, 120));
        QVERIFY(cell != nullptr);
        const QString id = cell->id;
        cell->style = Style::parse(QStringLiteral("html=1;fontSize=28;"));
        cell->value = QStringLiteral(
            "<span style=\"font-size: 11pt;\">原文字</span>");

        view->startEditing(id, true);
        auto *editor = view->findChild<OverlayEditor *>();
        QVERIFY(editor != nullptr);
        QVERIFY(editor->font().pixelSize() < 20);
        editor->setPlainText(QStringLiteral("新文字"));
        view->commitEditing();

        const QString value = view->page()->cell(id)->value;
        QVERIFY(value.contains(QStringLiteral("新文字")));
        QVERIFY(value.contains(QStringLiteral("font-size:")));
        QVERIFY(value.contains(QStringLiteral("fontSize")) == false);
    }

    void defaultShapeLibrariesAreFocused()
    {
        const QStringList defaults = ShapeLibraryDialog::defaultCategories();
        QCOMPARE(defaults, (QStringList{QStringLiteral("general"), QStringLiteral("basic"),
                                        QStringLiteral("arrows"),
                                        QStringLiteral("flowchart")}));
    }

    void shapeLibraryDialogReportsNone()
    {
        ShapeLibraryDialog dialog;
        auto *list = dialog.findChild<QListWidget *>();
        QVERIFY(list != nullptr);

        for (int i = 0; i < list->count(); ++i)
        {
            QListWidgetItem *item = list->item(i);

            if (item->flags() & Qt::ItemIsUserCheckable)
            {
                item->setCheckState(Qt::Unchecked);
            }
        }

        QCOMPARE(dialog.enabledCategories(), QStringList{QStringLiteral("__none__")});
    }

    void rotationHandleRotatesShape()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *cell = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                    QPointF(120, 120));
        QVERIFY(cell != nullptr);
        view->selectCell(cell->id);

        const QRectF sceneBounds = view->selectionBounds();
        const QPointF viewTopLeft = view->toView(sceneBounds.topLeft());
        const QPointF viewBottomRight = view->toView(sceneBounds.bottomRight());
        const QRectF viewBounds = QRectF(viewTopLeft, viewBottomRight).normalized();
        const QPointF handle(viewBounds.center().x(), viewBounds.top() - 24.0);
        const QPointF target = handle + QPointF(60, 60);

        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, handle.toPoint());
        QTest::mouseMove(view, target.toPoint());
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, target.toPoint());

        QVERIFY(qAbs(cell->effectiveRotation()) > 1.0);
        QVERIFY(!cell->styleValue(QStringLiteral("rotation")).isEmpty());
    }

    void rotationSerializedInStyle()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *cell = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                    QPointF(120, 120));
        QVERIFY(cell != nullptr);
        view->selectCell(cell->id);
        view->setRotation(45);

        const QString xml = MxFileIO::pageToXml(*view->page(), false);
        QVERIFY(xml.contains(QStringLiteral("rotation=45")));
        QVERIFY(!xml.contains(QStringLiteral("rotation=\"45\"")));
    }

    void draggingConnectorAddsWaypoint()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *a = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                 QPointF(100, 200));
        Cell *b = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                 QPointF(400, 200));
        QVERIFY(a != nullptr && b != nullptr);

        Cell *edge = view->page()->addEdge(a->id, b->id,
                                           Style::parse(QStringLiteral(
                                               "edgeStyle=none;endArrow=none;")));
        QVERIFY(edge != nullptr);
        view->selectCell(edge->id);

        const RoutedEdge route = EdgeRouter::route(*view->page(), *edge);
        const QPointF middle = route.pointAt(0.5);
        const QPointF start = view->toView(middle);
        const QPointF target = start + QPointF(0, 40);

        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, start.toPoint());
        QVERIFY(!edge->waypoints.isEmpty());
        const QPointF before = edge->waypoints.first();
        QMouseEvent moveEvent(QEvent::MouseMove, target, view->mapToGlobal(target.toPoint()),
                              Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(view, &moveEvent);
        QVERIFY(edge->waypoints.first() != before);
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, target.toPoint());
    }

    void rotatedResizeUsesRotatedHandles()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        view->setZoom(1.0);
        Cell *cell = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                    QPointF(100, 100));
        QVERIFY(cell != nullptr);
        cell->geometry = QRectF(100, 100, 100, 60);
        view->selectCell(cell->id);
        view->setRotation(45);

        const QRectF rect = cell->absoluteRect();
        QTransform transform;
        transform.translate(rect.center().x(), rect.center().y());
        transform.rotate(45);
        transform.translate(-rect.center().x(), -rect.center().y());
        const QPointF br = transform.map(rect.bottomRight());
        const QPointF start = view->toView(br);
        const QPointF target = start + QPointF(24, 24);

        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, start.toPoint());
        QTest::mouseMove(view, target.toPoint());
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, target.toPoint());

        QVERIFY(cell->geometry.width() != 100.0 || cell->geometry.height() != 60.0);
    }

    void layerFileLoadsIntoPanel()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("layers.drawio"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QStringLiteral(
            "<mxfile><diagram id=\"p\" name=\"p\"><mxGraphModel><root>"
            "<mxCell id=\"0\"/><mxCell id=\"L1\" parent=\"0\" value=\"图层1\"/>"
            "<mxCell id=\"L2\" parent=\"0\" value=\"图层2\"/>"
            "<mxCell id=\"v\" parent=\"L1\" vertex=\"1\" value=\"box\">"
            "<mxGeometry x=\"10\" y=\"10\" width=\"40\" height=\"30\" as=\"geometry\"/>"
            "</mxCell></root></mxGraphModel></diagram></mxfile>").toUtf8());
        file.close();

        MainWindow window;
        window.show();
        QVERIFY(window.openFile(path));
        QCOMPARE(window.document()->currentPage()->layers.size(), 2);

        auto *panel = window.findChild<LayerPanel *>();
        QVERIFY(panel != nullptr);
        auto *tree = panel->findChild<QTreeWidget *>();
        QVERIFY(tree != nullptr);
        QCOMPARE(tree->topLevelItemCount(), 2);
        int objectCount = 0;

        for (int i = 0; i < tree->topLevelItemCount(); ++i)
        {
            objectCount += tree->topLevelItem(i)->childCount();
        }

        QCOMPARE(objectCount, 1);
    }

    void layersUpdateWhenShapesAdded()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        auto *panel = window.findChild<LayerPanel *>();
        QVERIFY(panel != nullptr);
        auto *tree = panel->findChild<QTreeWidget *>();
        QVERIFY(tree != nullptr);
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->childCount(), 0);

        Cell *shape = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                     QPointF(120, 120));
        QVERIFY(shape != nullptr);
        QCOMPARE(tree->topLevelItem(0)->childCount(), 1);
        QVERIFY(tree->currentItem() != nullptr);
        QCOMPARE(tree->currentItem()->data(0, Qt::UserRole).toString(), shape->id);
    }

    void lockedLayerPreventsEditing()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        view->addLayer();
        const QString layerId = view->page()->activeLayerId;
        view->setLayerLocked(layerId, true);
        QVERIFY(view->isLayerLocked(layerId));

        Cell *blocked = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                       QPointF(120, 120));
        QVERIFY(blocked == nullptr);

        view->setLayerLocked(layerId, false);
        QVERIFY(!view->isLayerLocked(layerId));
        Cell *created = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                       QPointF(120, 120));
        QVERIFY(created != nullptr);
    }

    void lockedLayerCannotBeDeleted()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        view->addLayer();
        const QString layerId = view->page()->activeLayerId;
        view->setLayerLocked(layerId, true);
        QCOMPARE(view->page()->layers.size(), 2);

        view->removeLayer(layerId);
        QCOMPARE(view->page()->layers.size(), 2);
    }

    void shortcutsAreConfigurable()
    {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                           QStringLiteral("drawio-qt"), QStringLiteral("drawio-qt"));
        settings.setValue(QStringLiteral("shortcuts/file.new"), QStringLiteral("Ctrl+Alt+N"));
        settings.sync();

        MainWindow window;
        window.show();

        bool found = false;

        for (QAction *action : window.findChildren<QAction *>())
        {
            if (action->text() == QStringLiteral("New"))
            {
                found = true;
                QCOMPARE(action->shortcut(), QKeySequence(QStringLiteral("Ctrl+Alt+N")));
                break;
            }
        }

        QVERIFY(found);
        settings.remove(QStringLiteral("shortcuts/file.new"));
        settings.sync();

        QVector<ShortcutSetting> items = {
            {QStringLiteral("a"), QStringLiteral("功能 A"), QKeySequence(QStringLiteral("Ctrl+A")),
             QKeySequence(QStringLiteral("Ctrl+A"))},
            {QStringLiteral("b"), QStringLiteral("功能 B"), QKeySequence(QStringLiteral("Ctrl+B")),
             QKeySequence(QStringLiteral("Ctrl+B"))}};
        ShortcutDialog dialog(items);
        QCOMPARE(dialog.findChildren<ShortcutEdit *>().size(), 2);

        ShortcutEdit edit;
        edit.show();
        edit.setFocus();
        QTest::keyClick(&edit, Qt::Key_Delete, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(edit.keySequence(), QKeySequence(QStringLiteral("Ctrl+Shift+Del")));
    }

    void settingsDialogHasAppearanceAndShortcuts()
    {
        QVector<ShortcutSetting> items = {
            {QStringLiteral("a"), QStringLiteral("功能 A"), QKeySequence(QStringLiteral("Ctrl+A")),
             QKeySequence(QStringLiteral("Ctrl+A"))}};
        SettingsDialog dialog(items, QStringLiteral("dark"), QStringLiteral("zh"));
        QCOMPARE(dialog.theme(), QStringLiteral("dark"));
        QCOMPARE(dialog.findChildren<QTabWidget *>().size(), 1);
        QCOMPARE(dialog.findChildren<ShortcutEdit *>().size(), 1);
    }

    void englishTranslationAppliesToMenus()
    {
        MainWindow window;
        window.show();

        dq::I18n::setLanguage(dq::I18n::Language::English);
        dq::I18n::retranslate(&window);

        bool foundNew = false;

        for (QAction *action : window.findChildren<QAction *>())
        {
            if (action->text() == QStringLiteral("New"))
            {
                foundNew = true;
                break;
            }
        }

        QVERIFY(foundNew);

        dq::I18n::setLanguage(dq::I18n::Language::Chinese);
        QCOMPARE(dq::I18n::text(QStringLiteral("New")), QStringLiteral("新建"));
        dq::I18n::retranslate(&window);

        bool foundChinese = false;

        for (QAction *action : window.findChildren<QAction *>())
        {
            if (action->text() == QStringLiteral("新建"))
            {
                foundChinese = true;
                break;
            }
        }

        QVERIFY(foundChinese);

        bool pageTabTranslated = false;

        for (QTabBar *tabBar : window.findChildren<QTabBar *>())
        {
            for (int i = 0; i < tabBar->count(); ++i)
            {
                if (tabBar->tabText(i) == QStringLiteral("第 1 页"))
                {
                    pageTabTranslated = true;
                }
            }
        }

        QVERIFY(pageTabTranslated);

        window.document()->addPage();

        bool secondPageTranslated = false;

        for (QTabBar *tabBar : window.findChildren<QTabBar *>())
        {
            for (int i = 0; i < tabBar->count(); ++i)
            {
                if (tabBar->tabText(i) == QStringLiteral("第 2 页"))
                {
                    secondPageTranslated = true;
                }
            }
        }

        QVERIFY(secondPageTranslated);

        QStringList menuTexts;
        for (QAction *action : window.menuBar()->actions())
        {
            menuTexts.append(action->text());
        }
        QVERIFY(menuTexts.contains(QStringLiteral("编辑")));
        QVERIFY(menuTexts.contains(QStringLiteral("文件")));

        dq::I18n::setLanguage(dq::I18n::Language::English);
        dq::I18n::retranslate(&window);

        menuTexts.clear();

        for (QAction *action : window.menuBar()->actions())
        {
            menuTexts.append(action->text());
        }

        QVERIFY(menuTexts.contains(QStringLiteral("File")));
        QVERIFY(menuTexts.contains(QStringLiteral("Edit")));
    }

    void fillAndBorderCanBeDisabled()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *shape = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                     QPointF(120, 120));
        QVERIFY(shape != nullptr);

        auto *panel = window.findChild<FormatPanel *>();
        QVERIFY(panel != nullptr);
        auto *fill = panel->findChild<QCheckBox *>(QStringLiteral("fillEnabled"));
        auto *stroke = panel->findChild<QCheckBox *>(QStringLiteral("strokeEnabled"));
        QVERIFY(fill != nullptr && stroke != nullptr);
        QVERIFY(fill->isChecked() && stroke->isChecked());

        fill->setChecked(false);
        QCOMPARE(shape->styleValue(QStringLiteral("fillColor")), QStringLiteral("none"));
        stroke->setChecked(false);
        QCOMPARE(shape->styleValue(QStringLiteral("strokeColor")), QStringLiteral("none"));

        fill->setChecked(true);
        QVERIFY(!shape->styleValue(QStringLiteral("fillColor")).isEmpty());
        QVERIFY(shape->styleValue(QStringLiteral("fillColor")) != QStringLiteral("none"));
    }

    void lightDarkColorsShowInFormatPanel()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *shape = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                     QPointF(120, 120));
        QVERIFY(shape != nullptr);
        shape->setStyleValue(QStringLiteral("fillColor"),
                             QStringLiteral("light-dark(#FF0000,#FF0000)"));
        shape->setStyleValue(QStringLiteral("strokeColor"),
                             QStringLiteral("light-dark(#4B33FF,#4B33FF)"));
        view->selectCell(shape->id);

        auto *panel = window.findChild<FormatPanel *>();
        QVERIFY(panel != nullptr);
        auto *fill = panel->findChild<QToolButton *>(QStringLiteral("fillColor"));
        auto *stroke = panel->findChild<QToolButton *>(QStringLiteral("strokeColor"));
        QVERIFY(fill != nullptr && stroke != nullptr);
        QCOMPARE(fill->text(), QStringLiteral("#FF0000"));
        QCOMPARE(stroke->text(), QStringLiteral("#4B33FF"));
    }

    void gradientCanBeToggled()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *shape = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                     QPointF(120, 120));
        QVERIFY(shape != nullptr);
        shape->setStyleValue(QStringLiteral("gradientColor"), QStringLiteral("#FFFF33"));
        view->selectCell(shape->id);

        auto *panel = window.findChild<FormatPanel *>();
        QVERIFY(panel != nullptr);
        auto *gradient = panel->findChild<QCheckBox *>(QStringLiteral("gradientEnabled"));
        QVERIFY(gradient != nullptr);
        QVERIFY(gradient->isChecked());

        gradient->setChecked(false);
        QVERIFY(shape->styleValue(QStringLiteral("gradientColor")).isEmpty());

        gradient->setChecked(true);
        QVERIFY(!shape->styleValue(QStringLiteral("gradientColor")).isEmpty());

        auto *direction = panel->findChild<QComboBox *>(QStringLiteral("gradientDirection"));
        QVERIFY(direction != nullptr);
        const int east = direction->findData(QStringLiteral("east"));
        QVERIFY(east >= 0);
        direction->setCurrentIndex(east);
        QCOMPARE(shape->styleValue(QStringLiteral("gradientDirection")), QStringLiteral("east"));
    }

    void fontFamilyCanBeSet()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *shape = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                     QPointF(120, 120));
        QVERIFY(shape != nullptr);
        view->selectCell(shape->id);

        auto *panel = window.findChild<FormatPanel *>();
        QVERIFY(panel != nullptr);
        auto *family = panel->findChild<QComboBox *>(QStringLiteral("fontFamily"));
        QVERIFY(family != nullptr);
        QVERIFY(family->count() > 0);

        auto *panelScroll = panel->findChild<QScrollArea *>();
        QVERIFY(panelScroll != nullptr);
        QCOMPARE(panelScroll->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);

        family->setCurrentIndex(0);
        emit family->activated(0);
        QCOMPARE(shape->styleValue(QStringLiteral("fontFamily")), family->itemData(0).toString());
    }

    void floatingEdgeCanBeDragged()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *cell = view->addShape(
            ShapeRegistry::definition(QStringLiteral("arrow.wedge")), QPointF(200, 200));
        QVERIFY(cell != nullptr);
        QVERIFY(cell->isEdge());

        const QPointF startPoint = cell->sourcePoint;
        const QPointF targetPoint = cell->targetPoint;
        view->selectCell(cell->id);

        const QPoint mid = view->toView((startPoint + targetPoint) / 2.0).toPoint();
        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, mid);
        QTest::mouseMove(view, mid + QPoint(40, 30));
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, mid + QPoint(40, 30));

        QVERIFY(cell->sourcePoint != startPoint);
        QVERIFY(cell->targetPoint != targetPoint);
    }

    void clickingCanvasEndsEditing()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *shape = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                     QPointF(200, 200));
        QVERIFY(shape != nullptr);
        shape->value = QStringLiteral("<div>abcdef</div>");

        view->startEditing(shape->id, true);
        QVERIFY(view->isEditing());

        // A click on the canvas (outside the label and the format panel) ends it.
        const QPoint empty = view->toView(QPointF(600, 500)).toPoint();
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, empty);
        QCoreApplication::processEvents();

        QVERIFY(!view->isEditing());
    }

    void zeroStartSizeContainerKeepsItsBox()
    {
        MainWindow window;
        window.resize(900, 700);
        window.show();

        DiagramView *view = window.view();
        Page *page = view->page();

        Cell *container = page->addVertex(QRectF(60, 290, 50, 40),
                                          Style::parse(QStringLiteral("swimlane;startSize=0;")),
                                          QString());
        container->collapsed = true;
        container->alternateBounds = QRectF(60, 290, 200, 200);

        const QPoint fold = view->toView(QPointF(68, 298)).toPoint();
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, fold);
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, fold);

        // Nothing to fold to: the 50x40 box stays.
        QCOMPARE(container->geometry.size(), QSizeF(50, 40));
    }

    void collapseRestoresFoldedSize()
    {
        MainWindow window;
        window.resize(900, 700);
        window.show();

        DiagramView *view = window.view();
        Page *page = view->page();

        Cell *container = page->addVertex(QRectF(100, 100, 140, 30),
                                          Style::parse(QStringLiteral("swimlane;whiteSpace=wrap;html=1;")),
                                          QStringLiteral("Box"));
        container->collapsed = true;
        container->alternateBounds = QRectF(100, 100, 200, 200);

        const QPoint fold = view->toView(QPointF(108, 108)).toPoint();

        // Expand, then fold again: the folded box must match the opened state.
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, fold);
        QVERIFY(!container->collapsed);
        QCOMPARE(container->geometry.size(), QSizeF(200, 200));

        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, fold);
        QVERIFY(container->collapsed);
        QCOMPARE(container->geometry.size(), QSizeF(140, 30));
    }

    void expandRestoresContainerSize()
    {
        MainWindow window;
        window.resize(900, 700);
        window.show();

        DiagramView *view = window.view();
        Page *page = view->page();

        Cell *container = page->addVertex(QRectF(100, 100, 200, 120),
                                          Style::parse(QStringLiteral("swimlane;whiteSpace=wrap;html=1;")),
                                          QStringLiteral("Box"));
        QVERIFY(container != nullptr);
        const QPoint fold = view->toView(QPointF(108, 108)).toPoint();

        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, fold);
        QVERIFY(container->collapsed);

        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, fold);
        QVERIFY(!container->collapsed);
        QCOMPARE(container->geometry.height(), 120.0);
        QCOMPARE(container->geometry.width(), 200.0);
    }

    void foldButtonTogglesContainer()
    {
        MainWindow window;
        window.resize(900, 700);
        window.show();

        DiagramView *view = window.view();
        Page *page = view->page();

        Cell *container = page->addVertex(QRectF(100, 100, 200, 120),
                                          Style::parse(QStringLiteral("swimlane;whiteSpace=wrap;html=1;")),
                                          QStringLiteral("Box"));
        QVERIFY(container != nullptr);
        QVERIFY(!container->collapsed);

        const QPoint fold = view->toView(QPointF(108, 108)).toPoint();
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, fold);

        QVERIFY(container->collapsed);

        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, fold);
        QVERIFY(!container->collapsed);
    }

    void marqueeSelectsConnectors()
    {
        MainWindow window;
        window.resize(900, 700);
        window.show();

        DiagramView *view = window.view();
        Page *page = view->page();

        Cell *edge = page->addEdge(QString(), QString(), Style::parse(QStringLiteral("endArrow=classic;html=1;")));
        edge->sourcePoint = QPointF(100, 100);
        edge->targetPoint = QPointF(300, 100);

        view->selectAll();
        view->clearSelection();

        // Rubber band across the middle of the connector.
        const QPoint from = view->toView(QPointF(150, 60)).toPoint();
        const QPoint to = view->toView(QPointF(250, 140)).toPoint();
        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(view, to);
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, to);

        QVERIFY(view->selection().contains(edge->id));
    }

    void selectAllSkipsEdgeLabels()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Page *page = view->page();

        Cell *edge = page->addEdge(QString(), QString(), Style::parse(QStringLiteral("endArrow=classic;html=1;")));
        edge->sourcePoint = QPointF(100, 100);
        edge->targetPoint = QPointF(300, 100);

        Cell *label = page->addVertex(QRectF(), Style::parse(QStringLiteral("edgeLabel;html=1;")),
                                      QStringLiteral("Label"));
        label->parentId = edge->id;

        // Symbols (shape=message) are child cells with a relative offset too.
        Cell *symbol = page->addVertex(QRectF(-10, -7, 20, 14),
                                       Style::parse(QStringLiteral("shape=message;html=1;")),
                                       QString());
        symbol->parentId = edge->id;

        Cell *rect = page->addVertex(QRectF(20, 20, 60, 40),
                                     Style::parse(QStringLiteral("whiteSpace=wrap;html=1;")),
                                     QString());
        QVERIFY(rect != nullptr);

        view->selectAll();
        const QSet<QString> selection = view->selection();

        QVERIFY(selection.contains(edge->id));
        QVERIFY(selection.contains(rect->id));
        QVERIFY(!selection.contains(label->id));
        QVERIFY(!selection.contains(symbol->id));
    }

    void edgeLabelCanBeEdited()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Page *page = view->page();

        Cell *edge = page->addEdge(QString(), QString(), Style::parse(QStringLiteral("endArrow=classic;html=1;")));
        edge->sourcePoint = QPointF(100, 100);
        edge->targetPoint = QPointF(300, 100);

        Cell *label = page->addVertex(QRectF(), Style::parse(QStringLiteral("edgeLabel;html=1;")),
                                      QStringLiteral("Label"));
        label->parentId = edge->id;

        // The label is hit testable even though its geometry is empty.
        const QRectF bounds = EdgeRenderer::childBounds(EdgeRouter::route(*page, *edge), *label);
        QVERIFY(!bounds.isEmpty());

        view->startEditing(label->id, true);
        QVERIFY(view->isEditing());

        QTextEdit *editor = view->findChild<QTextEdit *>();
        QVERIFY(editor != nullptr);
        QVERIFY(editor->isVisible());
        editor->selectAll();
        editor->insertPlainText(QStringLiteral("Renamed"));
        view->commitEditing();

        QCOMPARE(page->cell(label->id)->value, QStringLiteral("Renamed"));
    }

    void headingCanBeUnbolded()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *shape = view->addShape(ShapeRegistry::definition(QStringLiteral("textbox")),
                                     QPointF(200, 200));
        QVERIFY(shape != nullptr);
        shape->value = QStringLiteral("<h1>Heading</h1>");

        view->startEditing(shape->id, true);
        QTextEdit *editor = view->findChild<QTextEdit *>();
        QVERIFY(editor != nullptr);

        editor->selectAll();
        QVERIFY(view->applyEditorFontStyle(1, false));
        view->commitEditing();

        QVERIFY2(shape->value.contains(QStringLiteral("font-weight: normal")),
                 qPrintable(shape->value));
    }

    void textColorSurvivesCommit()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *shape = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                     QPointF(200, 200));
        QVERIFY(shape != nullptr);
        shape->value = QStringLiteral("<div>abcdef</div>");

        view->startEditing(shape->id, true);
        QTextEdit *editor = view->findChild<QTextEdit *>();
        QVERIFY(editor != nullptr);

        QTextCursor cursor = editor->textCursor();
        cursor.setPosition(0);
        cursor.setPosition(3, QTextCursor::KeepAnchor);
        editor->setTextCursor(cursor);

        QVERIFY(view->applyEditorFontColor(QStringLiteral("#ff0000")));
        view->commitEditing();

        QVERIFY2(shape->value.contains(QStringLiteral("color: #ff0000")),
                 qPrintable(shape->value));
    }

    void fontSizeSurvivesCommit()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *shape = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                     QPointF(200, 200));
        QVERIFY(shape != nullptr);
        shape->value = QStringLiteral("<div>abcdef</div>");

        view->startEditing(shape->id, true);
        QVERIFY(view->isEditing());

        QTextEdit *editor = view->findChild<QTextEdit *>();
        QVERIFY(editor != nullptr);

        QTextCursor cursor = editor->textCursor();
        cursor.setPosition(0);
        cursor.setPosition(3, QTextCursor::KeepAnchor);
        editor->setTextCursor(cursor);

        QVERIFY(view->applyEditorFontSize(20));
        view->commitEditing();

        QVERIFY2(shape->value.contains(QStringLiteral("font-size: 20px")),
                 qPrintable(shape->value));
        QVERIFY2(shape->value.contains(QStringLiteral("<b>") ) == false, qPrintable(shape->value));
    }

    void clickingFormatPanelKeepsEditing()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *shape = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                     QPointF(200, 200));
        QVERIFY(shape != nullptr);
        shape->value = QStringLiteral("<div>abcdef</div>");

        view->startEditing(shape->id, true);
        QVERIFY(view->isEditing());

        // Moving focus into the format panel must not commit the label.
        QWidget *panel = window.findChild<QWidget *>(QStringLiteral("formatPanel"));
        QVERIFY(panel != nullptr);
        QSpinBox *size = panel->findChild<QSpinBox *>();
        QVERIFY(size != nullptr);
        size->setFocus();
        QCoreApplication::processEvents();

        QVERIFY(view->isEditing());
    }

    void pageSheetGrowsWithContent()
    {
        Page page;
        page.settings.page = true;
        page.settings.pageWidth = 300;
        page.settings.pageHeight = 200;

        Cell *shape = page.addVertex(QRectF(20, 20, 80, 40),
                                     Style::parse(QStringLiteral("whiteSpace=wrap;html=1;")), QString());

        // Only the first page while everything fits.
        QCOMPARE(DiagramRenderer::pageSheet(page), QRectF(0, 0, 300, 200));

        // Moving it to the second page leaves page 1 empty, so only the page
        // that holds content is drawn.
        shape->geometry = QRectF(360, 260, 80, 40);
        QCOMPARE(DiagramRenderer::pageSheet(page), QRectF(300, 200, 300, 200));
    }

    void colorDialogIsStyled()
    {
        MainWindow window;
        window.show();

        QScopedPointer<QColorDialog> dialog(
            ColorDialog::create(QColor(QStringLiteral("#dae8fc")), &window,
                                QStringLiteral("Choose Color")));
        QVERIFY(dialog != nullptr);
        QVERIFY(dialog->testOption(QColorDialog::DontUseNativeDialog));
        QVERIFY(dialog->testOption(QColorDialog::ShowAlphaChannel));
        QCOMPARE(dialog->currentColor(), QColor(QStringLiteral("#dae8fc")));
        QVERIFY(!dialog->styleSheet().isEmpty());

        QPushButton *primary = nullptr;

        for (QPushButton *button : dialog->findChildren<QPushButton *>())
        {
            if (button->objectName() == QLatin1String("primaryButton"))
            {
                primary = button;
            }
        }

        QVERIFY(primary != nullptr);

        // Text only buttons, like the rest of the application's dialogs.
        QVERIFY(primary->icon().isNull());

        for (QPushButton *button : dialog->findChildren<QPushButton *>())
        {
            if (button->text() == QStringLiteral("Cancel"))
            {
                QVERIFY(button->icon().isNull());
            }
        }
    }

    void formatPanelHasStyleAndTextTabs()
    {
        MainWindow window;
        window.show();

        QWidget *panel = window.findChild<QWidget *>(QStringLiteral("formatPanel"));
        QVERIFY(panel != nullptr);

        auto *tabs = panel->findChild<QTabWidget *>(QStringLiteral("formatTabs"));
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->count(), 2);

        // Style keeps the geometry and paint sections, Text keeps the text one.
        auto groupNames = [](QWidget *page)
        {
            QStringList names;

            for (QGroupBox *group : page->findChildren<QGroupBox *>())
            {
                names.append(group->title());
            }

            return names;
        };

        const QStringList style = groupNames(tabs->widget(0));
        const QStringList text = groupNames(tabs->widget(1));

        QVERIFY2(style.contains(QStringLiteral("Position and Size")), qPrintable(style.join(",")));
        QVERIFY2(style.contains(QStringLiteral("Fill")), qPrintable(style.join(",")));
        QVERIFY2(text.contains(QStringLiteral("Text")), qPrintable(text.join(",")));
        QVERIFY2(!style.contains(QStringLiteral("Text")), qPrintable(style.join(",")));
    }

    void formattingAppliesToSelectionOnly()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *shape = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                     QPointF(200, 200));
        QVERIFY(shape != nullptr);
        shape->value = QStringLiteral("<div>abcdef</div>");

        view->startEditing(shape->id, true);
        QVERIFY(view->isEditing());

        // Select the first three characters and bold only them.
        QTextEdit *editor = view->findChild<QTextEdit *>();

        if (editor == nullptr)
        {
            return;
        }

        QTextCursor cursor = editor->textCursor();
        cursor.setPosition(0);
        cursor.setPosition(3, QTextCursor::KeepAnchor);
        editor->setTextCursor(cursor);

        QVERIFY(view->applyEditorFontStyle(1, true));
        view->commitEditing();

        QVERIFY2(shape->value.contains(QStringLiteral("<b>abc</b>")),
                 qPrintable(shape->value));
        QVERIFY2(shape->value.contains(QStringLiteral("def")), qPrintable(shape->value));
    }

    void newShapesUseTwoPixelStroke()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *shape = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                     QPointF(150, 150));
        QVERIFY(shape != nullptr);
        QCOMPARE(shape->styleValue(QStringLiteral("strokeWidth")), QStringLiteral("2"));
        QCOMPARE(shape->styleValue(QStringLiteral("fillColor")), QStringLiteral("#ffffff"));
    }

    void directionAndFlipAreApplied()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *shape = view->addShape(ShapeRegistry::definition(QStringLiteral("arrow.arrow")),
                                     QPointF(200, 200));
        QVERIFY(shape != nullptr);

        // direction=north rotates the paint box and the shape by 90 degrees.
        shape->setStyleValue(QStringLiteral("direction"), QStringLiteral("north"));
        const QImage north = view->grab().toImage();

        shape->setStyleValue(QStringLiteral("direction"), QString());
        shape->setStyleValue(QStringLiteral("flipH"), QStringLiteral("1"));
        const QImage flipped = view->grab().toImage();

        QVERIFY(north != flipped);
    }

    void wedgeArrowInsertsAsEdge()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        const int before = view->page()->cells().size();

        Cell *cell = view->addShape(
            ShapeRegistry::definition(QStringLiteral("arrow.wedge")), QPointF(200, 200));
        QVERIFY(cell != nullptr);
        QVERIFY(cell->isEdge());
        QCOMPARE(view->page()->cells().size(), before + 1);
        QVERIFY(!cell->sourcePoint.isNull());
        QVERIFY(!cell->targetPoint.isNull());
    }

    void htmlLabelEditorKeepsHeadingFormatting()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *cell = view->addShape(ShapeRegistry::definition(QStringLiteral("textbox")),
                                    QPointF(250, 250));
        QVERIFY(cell != nullptr);
        QVERIFY(cell->value.contains(QStringLiteral("<h1")));
        const QString original = cell->value;

        view->startEditing(cell->id, false);
        QVERIFY(view->isEditing());

        QTextEdit *editor = view->findChild<QTextEdit *>();
        QVERIFY(editor != nullptr);

        // The Heading is shown as a level 1 block with a bold run.
        const QTextBlock heading = editor->document()->firstBlock();
        QCOMPARE(heading.text(), QStringLiteral("Heading"));
        QCOMPARE(heading.blockFormat().headingLevel(), 1);
        bool bold = false;

        for (QTextBlock::iterator it = heading.begin(); !it.atEnd(); ++it)
        {
            if (it.fragment().charFormat().fontWeight() >= QFont::Bold)
            {
                bold = true;
            }
        }

        QVERIFY(bold);

        // Committing without changes keeps the markup untouched.
        view->commitEditing();
        QVERIFY(!view->isEditing());
        QCOMPARE(cell->value, original);

        // Editing the heading text keeps the heading block and its weight.
        view->startEditing(cell->id, true);
        editor = view->findChild<QTextEdit *>();
        QVERIFY(editor != nullptr);
        QTextCursor cursor(editor->document()->firstBlock());
        cursor.select(QTextCursor::BlockUnderCursor);
        cursor.insertText(QStringLiteral("Title"));
        view->commitEditing();
        QVERIFY(cell->value.contains(QStringLiteral("<h1")));
        QVERIFY(cell->value.contains(QStringLiteral("Title")));
        QVERIFY(!cell->value.contains(QStringLiteral("Heading")));
    }

    void htmlLabelEditorFormatsSelectedText()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *cell = view->addShape(ShapeRegistry::definition(QStringLiteral("textbox")),
                                    QPointF(250, 250));
        QVERIFY(cell != nullptr);

        view->startEditing(cell->id, false);
        QTextEdit *editor = view->findChild<QTextEdit *>();
        QVERIFY(editor != nullptr);

        // Bold "ipsum" in the paragraph with Ctrl+B.
        QTextBlock paragraph = editor->document()->firstBlock().next();
        QVERIFY(paragraph.text().startsWith(QStringLiteral("Lorem ipsum")));
        QTextCursor cursor(paragraph);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 6);
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 5);
        editor->setTextCursor(cursor);
        QTest::keyClick(editor, Qt::Key_B, Qt::ControlModifier);

        // Italic and underline "dolor" with Ctrl+I and Ctrl+U.
        cursor = QTextCursor(paragraph);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 12);
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 5);
        editor->setTextCursor(cursor);
        QTest::keyClick(editor, Qt::Key_I, Qt::ControlModifier);
        QTest::keyClick(editor, Qt::Key_U, Qt::ControlModifier);

        view->commitEditing();

        QVERIFY(cell->value.contains(QStringLiteral("<b>ipsum</b>")));
        QVERIFY(cell->value.contains(QStringLiteral("dolor")));
        QVERIFY(cell->value.contains(QStringLiteral("<i>dolor</i>")));
        QVERIFY(cell->value.contains(QStringLiteral("<u>")));
        QVERIFY(cell->value.contains(QStringLiteral("<h1")));
    }

    void formatPanelBoldAppliesToLabelSelection()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        auto *panel = window.findChild<FormatPanel *>();
        QVERIFY(panel != nullptr);

        Cell *cell = view->addShape(ShapeRegistry::definition(QStringLiteral("textbox")),
                                    QPointF(250, 250));
        QVERIFY(cell != nullptr);

        view->startEditing(cell->id, false);
        QTextEdit *editor = view->findChild<QTextEdit *>();
        QVERIFY(editor != nullptr);

        QTextBlock paragraph = editor->document()->firstBlock().next();
        QTextCursor cursor(paragraph);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 6);
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 5);
        editor->setTextCursor(cursor);

        QToolButton *bold = nullptr;

        for (QToolButton *button : panel->findChildren<QToolButton *>())
        {
            if (button->toolTip() == QStringLiteral("Bold"))
            {
                bold = button;
            }
        }

        QVERIFY(bold != nullptr);
        bold->setChecked(true);
        view->commitEditing();

        // Only the selected word is bold; the cell itself keeps its fontStyle.
        QVERIFY(cell->value.contains(QStringLiteral("<b>ipsum</b>")));
        QVERIFY(!cell->value.contains(QStringLiteral("<b>Lorem")));
        QVERIFY(cell->styleValue(QStringLiteral("fontStyle")).isEmpty());
    }

    void generalEdgeTemplatesInsertAsEdges()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();

        for (const QString &key : {QStringLiteral("curve"), QStringLiteral("flexArrow"),
                                   QStringLiteral("dashedLine"), QStringLiteral("dottedLine"),
                                   QStringLiteral("plainLine"), QStringLiteral("link"),
                                   QStringLiteral("directionalConnector")})
        {
            const int before = view->page()->cells().size();
            Cell *cell = view->addShape(ShapeRegistry::definition(key), QPointF(200, 200));
            QVERIFY2(cell != nullptr, qPrintable(key));
            QVERIFY2(cell->isEdge(), qPrintable(key));
            QCOMPARE(view->page()->cells().size(), before + 1);
            QVERIFY(!cell->sourcePoint.isNull());
            QVERIFY(!cell->targetPoint.isNull());
        }

        // The curve template also carries its waypoints.
        Cell *curve = view->addShape(ShapeRegistry::definition(QStringLiteral("curve")),
                                     QPointF(300, 300));
        QVERIFY(curve != nullptr);
        QCOMPARE(curve->waypoints.size(), 2);
    }

    void generalCompositeTemplatesInsertChildren()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();

        // The List container comes with its three item rows.
        const int before = view->page()->cells().size();
        Cell *list = view->addShape(ShapeRegistry::definition(QStringLiteral("list")),
                                    QPointF(250, 250));
        QVERIFY(list != nullptr);
        QVERIFY(list->isVertex());
        QCOMPARE(view->page()->cells().size(), before + 4);
        QCOMPARE(list->children.size(), 3);
        QCOMPARE(list->children.at(1)->value, QStringLiteral("Item 2"));
        QCOMPARE(list->children.at(1)->absolutePosition().y(),
                 list->absolutePosition().y() + 60.0);

        // Connector templates insert edge labels and the message symbol.
        Cell *labels = view->addShape(
            ShapeRegistry::definition(QStringLiteral("connectorLabels")), QPointF(300, 300));
        QVERIFY(labels != nullptr);
        QVERIFY(labels->isEdge());
        QCOMPARE(labels->children.size(), 3);
        QVERIFY(labels->children.first()->isEdgeChild());
        QVERIFY(labels->children.first()->relativeGeometry);
        QCOMPARE(labels->children.first()->value, QStringLiteral("Label"));

        Cell *symbol = view->addShape(
            ShapeRegistry::definition(QStringLiteral("connectorSymbol")), QPointF(350, 350));
        QVERIFY(symbol != nullptr);
        QCOMPARE(symbol->children.size(), 1);
        QCOMPARE(symbol->children.first()->geometry.size(), QSizeF(20, 14));
        QCOMPARE(symbol->children.first()->geometryOffset, QPointF(-10, -7));
    }

    void textTemplateInsertsItsLabel()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *text = view->addShape(ShapeRegistry::definition(QStringLiteral("text")),
                                    QPointF(120, 120));
        QVERIFY(text != nullptr);
        QCOMPARE(text->value, QStringLiteral("Text"));
    }

    void layerCanBeDeletedFromPanel()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        view->addLayer();
        QCOMPARE(view->page()->layers.size(), 2);

        auto *panel = window.findChild<LayerPanel *>();
        QVERIFY(panel != nullptr);
        QToolButton *remove = nullptr;

        for (QToolButton *button : panel->findChildren<QToolButton *>())
        {
            if (button->text() == QStringLiteral("－"))
            {
                remove = button;
                break;
            }
        }

        QVERIFY(remove != nullptr);
        QVERIFY(remove->isEnabled());
        remove->click();
        QCOMPARE(view->page()->layers.size(), 1);
    }

    void layerObjectsAreTopmostFirst()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        auto *panel = window.findChild<LayerPanel *>();
        QVERIFY(panel != nullptr);
        auto *tree = panel->findChild<QTreeWidget *>();
        QVERIFY(tree != nullptr);

        Cell *a = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                 QPointF(100, 100));
        Cell *b = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                 QPointF(200, 100));
        QVERIFY(a != nullptr && b != nullptr);

        QTreeWidgetItem *layerItem = tree->topLevelItem(0);
        QCOMPARE(layerItem->childCount(), 2);
        QCOMPARE(layerItem->child(0)->data(0, Qt::UserRole).toString(), b->id);

        view->selectCell(a->id);
        view->moveObject(a->id, 1);
        QCOMPARE(tree->topLevelItem(0)->child(0)->data(0, Qt::UserRole).toString(), a->id);
    }

    void objectsMoveWithinLayer()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *a = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                 QPointF(100, 100));
        Cell *b = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                 QPointF(200, 100));
        QVERIFY(a != nullptr && b != nullptr);

        const int beforeA = view->page()->indexOf(a);
        const int beforeB = view->page()->indexOf(b);
        QVERIFY(beforeB > beforeA);

        view->moveObject(b->id, -1);
        QVERIFY(view->page()->indexOf(b) < beforeB);
        QVERIFY(view->page()->indexOf(a) == beforeA + 1);
    }

    void layersCanBeManaged()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Page *page = view->page();
        QVERIFY(page != nullptr);
        QCOMPARE(page->layers.size(), 1);

        view->addLayer();
        QCOMPARE(page->layers.size(), 2);
        const QString layerId = page->activeLayerId;
        QVERIFY(page->layer(layerId) != nullptr);

        Cell *shape = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                     QPointF(120, 120));
        QVERIFY(shape != nullptr);
        QCOMPARE(shape->parentId, layerId);

        view->setLayerVisible(layerId, false);
        QVERIFY(!page->layer(layerId)->visible);
        view->setLayerVisible(layerId, true);
        QVERIFY(page->layer(layerId)->visible);

        view->renameLayer(layerId, QStringLiteral("我的图层"));
        QCOMPARE(page->layer(layerId)->name, QStringLiteral("我的图层"));

        const QString shapeId = shape->id;
        view->removeLayer(layerId);
        QCOMPARE(page->layers.size(), 1);
        QVERIFY(page->cell(shapeId) == nullptr);
    }

    void altEnterInsertsLineBreak()
    {
        OverlayEditor editor;
        editor.show();
        editor.begin(QRect(0, 0, 120, 80), QStringLiteral("第一行"), true);

        QSignalSpy committed(&editor, &OverlayEditor::committed);
        QTest::keyClick(&editor, Qt::Key_Return, Qt::AltModifier);
        QVERIFY(editor.toPlainText().contains(QLatin1Char('\n')));
        QCOMPARE(committed.count(), 0);
    }

    void closingLastTabDoesNotCrash()
    {
        MainWindow window;
        window.show();

        auto *tabs = window.findChild<QTabBar *>(QStringLiteral("fileTabs"));
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->count(), 1);

        QVERIFY(QMetaObject::invokeMethod(tabs, "tabCloseRequested", Q_ARG(int, 0)));
        QCoreApplication::processEvents();
        QVERIFY(!window.isVisible());
    }

    void opensMultipleFilesInTabs()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString first = dir.filePath(QStringLiteral("first.drawio"));
        const QString second = dir.filePath(QStringLiteral("second.drawio"));
        const QString xml = QStringLiteral(
            "<mxfile><diagram id=\"p\" name=\"p\"><mxGraphModel><root>"
            "<mxCell id=\"0\"/><mxCell id=\"1\" parent=\"0\"/>"
            "</root></mxGraphModel></diagram></mxfile>");

        for (const QString &path : {first, second})
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(xml.toUtf8());
        }

        MainWindow window;
        window.show();
        QVERIFY(window.openFile(first));
        QVERIFY(window.openFile(second));

        auto *tabs = window.findChild<QTabBar *>(QStringLiteral("fileTabs"));
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->count(), 2);
        QCOMPARE(QFileInfo(window.document()->filePath()).fileName(),
                 QStringLiteral("second.drawio"));
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

    void draggingSnapsToAlignmentGuides()
    {
        MainWindow window;
        window.resize(1000, 700);
        window.show();
        (void)QTest::qWaitForWindowExposed(&window);

        DiagramView *view = window.view();
        view->page()->settings.grid = false; // isolate guide snapping from grid snapping
        view->setGuidesVisible(true);

        Cell *anchor = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                      QPointF(100, 100));
        Cell *moving = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                      QPointF(300, 220));
        view->setSelection({moving->id});

        const double anchorLeft = anchor->absoluteRect().left();
        const double movingLeft = moving->absoluteRect().left();
        const QPointF start = moving->absoluteRect().center();

        // Stop 3 px short of the alignment: the guide should pull it in.
        const QPointF end = start + QPointF(anchorLeft - movingLeft + 3.0, 0);

        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier,
                          view->toView(start).toPoint());
        QTest::mouseMove(view, view->toView(end).toPoint());
        QCoreApplication::processEvents();

        // Capture the guide while the drag is still active.
        const QImage guideShot = window.grab().toImage();
        QVERIFY(!guideShot.isNull());
        guideShot.save(QStringLiteral(DRAWIO_QT_BINARY_DIR) + QStringLiteral("/guides.png"));

        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier,
                            view->toView(end).toPoint());
        QCoreApplication::processEvents();

        QCOMPARE(moving->absoluteRect().left(), anchorLeft);
    }

    void resizingSnapsToAlignmentGuides()
    {
        MainWindow window;
        window.resize(1000, 700);
        window.show();
        (void)QTest::qWaitForWindowExposed(&window);

        DiagramView *view = window.view();
        view->page()->settings.grid = false;
        view->setGuidesVisible(true);

        Cell *anchor = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                      QPointF(600, 100));
        Cell *moving = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                      QPointF(360, 300));
        view->setSelection({moving->id});

        // Drag the right edge to within 3 px of the anchor's right edge.
        const QRectF before = moving->absoluteRect();
        const QPointF corner = view->toView(before.bottomRight());
        const double targetX = anchor->absoluteRect().right();
        const QPointF end = view->toView(QPointF(targetX - 3.0, before.bottom()));

        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, corner.toPoint());
        QTest::mouseMove(view, end.toPoint());
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, end.toPoint());
        QCoreApplication::processEvents();

        QCOMPARE(moving->absoluteRect().right(), anchor->absoluteRect().right());
    }

    void connectorShowsTargetAnchorsAndUsesThem()
    {
        MainWindow window;
        window.resize(1280, 800);
        window.show();
        (void)QTest::qWaitForWindowExposed(&window);

        DiagramView *view = window.view();
        view->page()->settings.grid = false;

        Cell *source = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                      QPointF(180, 250));
        Cell *target = view->addShape(ShapeRegistry::definition(QStringLiteral("rectangle")),
                                      QPointF(520, 250));
        view->setTool(DiagramView::Tool::Connector);

        const QRectF sourceRect = source->absoluteRect();
        const QRectF targetRect = target->absoluteRect();

        const QPointF start(sourceRect.right() - 2.0, sourceRect.center().y());
        const QPointF end(targetRect.left() + 3.0, targetRect.center().y());

        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier,
                          view->toView(start).toPoint());
        QTest::mouseMove(view, view->toView(end).toPoint());
        QCoreApplication::processEvents();

        // The target anchors must be visible while the connector is dragged.
        const QImage preview = window.grab().toImage();
        QVERIFY(!preview.isNull());
        QVERIFY(preview.save(QStringLiteral(DRAWIO_QT_BINARY_DIR)
                             + QStringLiteral("/connector-points.png")));

        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier,
                            view->toView(end).toPoint());
        QCoreApplication::processEvents();

        QCOMPARE(view->page()->cells().size(), 3);
        QCOMPARE(view->selection().size(), 1);

        Cell *edge = view->page()->cell(*view->selection().begin());
        QVERIFY(edge != nullptr);
        QVERIFY(edge->isEdge());
        QCOMPARE(edge->sourceId, source->id);
        QCOMPARE(edge->targetId, target->id);
        QCOMPARE(edge->styleValue(QStringLiteral("exitX")), QStringLiteral("1"));
        QCOMPARE(edge->styleValue(QStringLiteral("exitY")), QStringLiteral("0.5"));
        QCOMPARE(edge->styleValue(QStringLiteral("entryX")), QStringLiteral("0"));
        QCOMPARE(edge->styleValue(QStringLiteral("entryY")), QStringLiteral("0.5"));
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

    void draggingTransparentContainerMovesChildren()
    {
        MainWindow window;
        window.resize(900, 700);
        window.show();

        DiagramView *view = window.view();
        Page *page = view->page();

        Cell *container = page->addVertex(QRectF(),
                                          Style::parse(QStringLiteral("swimlane;startSize=20;"
                                                                      "transparentBounds=1;groupPadding=20;")),
                                          QStringLiteral("Layout"));
        Cell *child = page->addVertex(QRectF(40, 40, 100, 40),
                                      Style::parse(QStringLiteral("whiteSpace=wrap;html=1;")),
                                      QStringLiteral("Node"));
        child->parentId = container->id;

        view->selectCell(container->id, false);

        const QPoint from = view->toView(QPointF(60, 60)).toPoint();
        const QPoint to = view->toView(QPointF(160, 120)).toPoint();
        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(view, to);
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, to);

        // The container geometry stays at the origin, the child moved instead.
        QCOMPARE(container->geometry, QRectF());
        QVERIFY2(child->geometry.left() > 40.0, qPrintable(QString::number(child->geometry.left())));
    }

    void ctrlHoverShowsHandCursor()
    {
        MainWindow window;
        window.resize(900, 700);
        window.show();

        DiagramView *view = window.view();
        Page *page = view->page();

        Cell *linked = page->addVertex(QRectF(200, 40, 80, 40),
                                       Style::parse(QStringLiteral("whiteSpace=wrap;html=1;")),
                                       QStringLiteral("B"));
        linked->link = QStringLiteral("https://example.com/doc");

        const QPoint point = view->toView(QPointF(240, 60)).toPoint();

        // Hover without Ctrl keeps the arrow (clear any modifier state left by
        // the previous test first).
        QKeyEvent clear(QEvent::KeyRelease, Qt::Key_Control, Qt::NoModifier);
        QCoreApplication::sendEvent(view, &clear);
        QTest::mouseMove(view, point);
        QCOMPARE(view->cursor().shape(), Qt::ArrowCursor);

        // Hovering with Ctrl over the linked cell shows the hand.
        QTest::mouseMove(view, point);
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Control, Qt::NoModifier);
        QCoreApplication::sendEvent(view, &press);
        QCOMPARE(view->cursor().shape(), Qt::PointingHandCursor);

        QKeyEvent release(QEvent::KeyRelease, Qt::Key_Control, Qt::NoModifier);
        QCoreApplication::sendEvent(view, &release);
        QCOMPARE(view->cursor().shape(), Qt::ArrowCursor);
    }

    void ctrlClickOpensLink()
    {
        MainWindow window;
        window.resize(900, 700);
        window.show();

        DiagramView *view = window.view();
        Page *page = view->page();

        Cell *plain = page->addVertex(QRectF(40, 40, 80, 40),
                                      Style::parse(QStringLiteral("whiteSpace=wrap;html=1;")),
                                      QStringLiteral("A"));
        Cell *linked = page->addVertex(QRectF(200, 40, 80, 40),
                                       Style::parse(QStringLiteral("whiteSpace=wrap;html=1;")),
                                       QStringLiteral("B"));
        linked->link = QStringLiteral("https://example.com/doc");

        QString opened;
        QObject::connect(view, &DiagramView::linkActivated, view,
                         [&opened](const QString &url) { opened = url; });

        // Ctrl+click on a linked cell requests the link.
        const QPoint linkedPoint = view->toView(QPointF(240, 60)).toPoint();
        QTest::mousePress(view, Qt::LeftButton, Qt::ControlModifier, linkedPoint);
        QTest::mouseRelease(view, Qt::LeftButton, Qt::ControlModifier, linkedPoint);
        QCOMPARE(opened, QStringLiteral("https://example.com/doc"));

        // Ctrl+click on a plain cell starts a connector instead.
        opened.clear();
        const QPoint plainPoint = view->toView(QPointF(80, 60)).toPoint();
        QTest::mousePress(view, Qt::LeftButton, Qt::ControlModifier, plainPoint);
        QTest::mouseRelease(view, Qt::LeftButton, Qt::ControlModifier, plainPoint);
        QVERIFY(opened.isEmpty());
        Q_UNUSED(plain);
    }

    void miscLinkAndTimestampAreEditable()
    {
        MainWindow window;
        window.show();

        DiagramView *view = window.view();
        Cell *link = view->addShape(ShapeRegistry::definition(QStringLiteral("misc.link")),
                                    QPointF(180, 160));
        QVERIFY(link != nullptr);
        QCOMPARE(link->value, QStringLiteral("Link"));
        QCOMPARE(link->link, QStringLiteral("https://www.draw.io"));

        view->setCellLink(link->id, QStringLiteral("https://example.com/doc"));
        QCOMPARE(view->page()->cell(link->id)->link,
                 QStringLiteral("https://example.com/doc"));

        // The link survives a save/load cycle (UserObject metadata).
        const QString xml = MxFileIO::documentToXml(*view->document(), false);
        QVERIFY(xml.contains(QStringLiteral("UserObject")));
        QVERIFY(xml.contains(QStringLiteral("https://example.com/doc")));
        Document reopened;
        QVERIFY(MxFileIO::documentFromXml(reopened, xml));
        Cell *loaded = reopened.currentPage()->cell(link->id);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->link, QStringLiteral("https://example.com/doc"));
        QCOMPARE(loaded->value, QStringLiteral("Link"));

        Cell *timestamp = view->addShape(
            ShapeRegistry::definition(QStringLiteral("misc.timestamp")),
            QPointF(360, 160));
        QVERIFY(timestamp != nullptr);
        QVERIFY(timestamp->placeholders);
        QVERIFY(timestamp->displayLabel().contains(
            QString::number(QDate::currentDate().year())));
        QVERIFY(!timestamp->displayLabel().contains(QStringLiteral("%date")));

        Cell *variable = view->addShape(
            ShapeRegistry::definition(QStringLiteral("misc.variable")),
            QPointF(360, 220));
        QVERIFY(variable != nullptr);
        QCOMPARE(variable->displayLabel(), QStringLiteral("Variable Text"));
    }
};

QTEST_MAIN(TestUiSmoke)
#include "tst_ui_smoke.moc"
