#include "app/AppSettings.h"
#include "app/UiTheme.h"
#include "core/Document.h"
#include "core/MxFileIO.h"
#include "ui/DiagramView.h"
#include "ui/FormatPanel.h"
#include "ui/ShapeToolbox.h"
#include "ui/ShapeLibraryDialog.h"
#include "render/ShapeCatalog.h"
#include "render/EdgeRouter.h"
#include "ui/OverlayEditor.h"
#include "ui/MainWindow.h"
#include "ui/LayerPanel.h"
#include "ui/Dialogs.h"
#include "ui/FormatPanel.h"
#include "app/I18n.h"

#include <QApplication>
#include <QComboBox>
#include <QGroupBox>
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
#include <QToolButton>
#include <QSettings>
#include "ui/Dialogs.h"
#include "ui/FormatPanel.h"
#include <QTabWidget>
#include <QTemporaryDir>
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
        QVERIFY(value.contains(QStringLiteral("font-size: 11pt")));
        QVERIFY(value.contains(QStringLiteral("fontSize")) == false);
    }

    void defaultShapeLibrariesAreFocused()
    {
        const QStringList defaults = ShapeLibraryDialog::defaultCategories();
        QCOMPARE(defaults, (QStringList{QStringLiteral("basic"), QStringLiteral("arrows"),
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
};

QTEST_MAIN(TestUiSmoke)
#include "tst_ui_smoke.moc"
