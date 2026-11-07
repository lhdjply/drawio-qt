#include "ui/MainWindow.h"

#include "app/AppSettings.h"
#include "core/Document.h"
#include "core/MxFileIO.h"
#include "io/Exporter.h"
#include "render/DiagramRenderer.h"
#include "ui/Dialogs.h"
#include "ui/DiagramView.h"
#include "ui/FormatPanel.h"
#include "ui/ShapeToolbox.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBuffer>
#include <QCloseEvent>
#include <QCryptographicHash>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QAbstractButton>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPageSize>
#include <QPainter>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QScreen>
#include <QStyle>
#include <QUrl>
#include <QStatusBar>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>

namespace dq {
namespace {

QKeySequence key(const QString &text)
{
    return QKeySequence(text);
}

QIcon iconFor(const QString &name)
{
    return QIcon(QStringLiteral(":/icons/%1.svg").arg(name));
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_document(new Document(this))
{
    setAcceptDrops(true);
    setDockNestingEnabled(true);
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));

    auto *central = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    m_view = new DiagramView(central);
    centralLayout->addWidget(m_view, 1);

    createPageTabs();
    centralLayout->addWidget(m_pageTabs->parentWidget());

    setCentralWidget(central);

    m_view->setDocument(m_document);

    createActions();
    createMenus();
    createToolBars();
    createDocks();
    createStatusBar();

    m_formatPanel->setView(m_view);

    // --- wiring -----------------------------------------------------------
    connect(m_view, &DiagramView::selectionChanged, this, [this]()
    {
        updateActions();
        updateStatus();

        if (m_formatPanel != nullptr)
        {
            m_formatPanel->updateFromSelection();
        }
    });
    connect(m_view, &DiagramView::zoomChanged, this, [this](double)
    {
        updateStatus();
    });
    connect(m_view, &DiagramView::cursorPositionChanged, this, [this](const QPointF &point)
    {
        if (m_positionLabel != nullptr)
        {
            m_positionLabel->setText(QStringLiteral("x: %1  y: %2")
                                         .arg(qRound(point.x()))
                                         .arg(qRound(point.y())));
        }
    });
    connect(m_view, &DiagramView::modified, this, [this]()
    {
        m_document->setModified(true);
        updateTitle();
        updateActions();
    });
    connect(m_view, &DiagramView::contextMenuRequested, this,
            &MainWindow::showCanvasContextMenu);
    connect(m_view, &DiagramView::statusMessage, this, [this](const QString &message)
    {
        showStatusMessage(message);
    });
    connect(m_view, &DiagramView::filesDropped, this, [this](const QStringList &paths)
    {
        if (!paths.isEmpty())
        {
            openFile(paths.first());
        }
    });

    connect(m_document, &Document::modifiedChanged, this, [this](bool)
    {
        updateTitle();
        updateActions();
    });
    connect(m_document, &Document::pagesChanged, this, [this]()
    {
        updatePageTabs();
        updateActions();
    });
    connect(m_document, &Document::currentPageChanged, this, [this](int index)
    {
        updatePageTabs();
        m_view->refreshFromDocument();
        updateStatus();

        if (index >= 0 && m_pageTabs != nullptr)
        {
            QSignalBlocker blocker(m_pageTabs);
            m_pageTabs->setCurrentIndex(index);
        }
    });
    connect(m_document, &Document::filePathChanged, this, [this](const QString &)
    {
        updateTitle();
    });

    // --- auto save --------------------------------------------------------
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setInterval(20000);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::writeDraft);
    m_autoSaveTimer->start();

    // --- external change watcher ------------------------------------------
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path)
    {
        if (path != m_document->filePath())
        {
            return;
        }

        if (!m_document->isModified())
        {
            // Nothing to lose: reload silently.
            if (openFile(path))
            {
                m_view->fitToWindow();
            }
            return;
        }

        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("文件已更改"));
        box.setText(QStringLiteral("文件已被其他程序修改：\n%1\n\n是否重新加载？").arg(path));
        box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        box.button(QMessageBox::Yes)->setText(QStringLiteral("重新加载"));
        box.button(QMessageBox::No)->setText(QStringLiteral("保留当前内容"));

        if (box.exec() == QMessageBox::Yes)
        {
            if (openFile(path))
            {
                m_view->fitToWindow();
            }
        }
        else
        {
            watchCurrentFile(true);
        }
    });

    // --- initial state ----------------------------------------------------
    m_pageViewAction->setChecked(AppSettings::instance().pageVisible());
    m_gridAction->setChecked(AppSettings::instance().gridVisible());
    m_view->setGridVisible(AppSettings::instance().gridVisible());
    m_view->setPageVisible(AppSettings::instance().pageVisible());

    const QByteArray geometry = AppSettings::instance().windowGeometry();

    if (!geometry.isEmpty())
    {
        restoreGeometry(geometry);
    }
    else
    {
        resize(1280, 860);
    }

    const QByteArray state = AppSettings::instance().windowState();

    if (!state.isEmpty())
    {
        restoreState(state);
    }

    applyTheme(AppSettings::instance().theme());

    updateTitle();
    updateActions();
    updatePageTabs();
    updateStatus();
}

MainWindow::~MainWindow() = default;

// --- UI construction -----------------------------------------------------

void MainWindow::createPageTabs()
{
    auto *container = new QWidget(this);
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(4);

    m_pageTabs = new QTabBar(container);
    m_pageTabs->setDrawBase(false);
    m_pageTabs->setExpanding(false);
    m_pageTabs->setTabsClosable(true);
    m_pageTabs->setMovable(true);
    m_pageTabs->setElideMode(Qt::ElideRight);
    m_pageTabs->setStyleSheet(QStringLiteral(
        "QTabBar::tab { padding: 3px 14px; margin-right: 2px; }"
        "QTabBar::close-button { background: transparent; }"
        "QTabBar::close-button:hover { background: rgba(0,0,0,0.12); border-radius: 2px; }"));

    layout->addWidget(m_pageTabs, 1);

    auto *addButton = new QToolButton(container);
    addButton->setText(QStringLiteral("＋"));
    addButton->setToolTip(QStringLiteral("添加页面"));
    addButton->setAutoRaise(true);
    layout->addWidget(addButton);

    connect(addButton, &QToolButton::clicked, this, &MainWindow::addPage);
    connect(m_pageTabs, &QTabBar::currentChanged, this, [this](int index)
    {
        if (index >= 0 && index < m_document->pageCount())
        {
            // Finish an in-progress label edit before the page changes.
            m_view->commitEditing();
            m_document->setCurrentIndex(index);
        }
    });
    connect(m_pageTabs, &QTabBar::tabCloseRequested, this, &MainWindow::removePage);
    connect(m_pageTabs, &QTabBar::tabBarDoubleClicked, this, &MainWindow::renamePage);
    connect(m_pageTabs, &QTabBar::tabMoved, this, [this](int from, int to)
    {
        Q_UNUSED(from);
        Q_UNUSED(to);
    });

    container->setObjectName(QStringLiteral("pageTabContainer"));
}

void MainWindow::createActions()
{
    auto makeAction = [this](const QString &text, const QKeySequence &shortcut,
                             const QIcon &icon, auto slot)
    {
        auto *action = new QAction(text, this);

        if (!shortcut.isEmpty())
        {
            action->setShortcut(shortcut);
            action->setShortcutContext(Qt::WindowShortcut);
        }

        if (!icon.isNull())
        {
            action->setIcon(icon);
        }

        connect(action, &QAction::triggered, this, slot);
        return action;
    };

    m_newAction = makeAction(QStringLiteral("新建"), key(QStringLiteral("Ctrl+N")),
                             iconFor(QStringLiteral("new")), [this]()
    {
        newDocument();
    });
    m_newAction->setToolTip(QStringLiteral("新建绘图 (Ctrl+N)"));

    m_newWindowAction = makeAction(QStringLiteral("新建窗口"),
                                   key(QStringLiteral("Ctrl+Shift+N")),
                                   QIcon(), [this]()
    {
        auto *window = new MainWindow();
        window->setWindowIcon(windowIcon());
        window->resize(size());
        window->show();
        window->newDocument();
    });

    m_openAction = makeAction(QStringLiteral("打开…"), key(QStringLiteral("Ctrl+O")),
                              iconFor(QStringLiteral("open")), [this]()
    {
        openDialog();
    });

    m_saveAction = makeAction(QStringLiteral("保存"), key(QStringLiteral("Ctrl+S")),
                              iconFor(QStringLiteral("save")), [this]()
    {
        save();
    });

    m_saveAsAction = makeAction(QStringLiteral("另存为…"),
                                key(QStringLiteral("Ctrl+Shift+S")), QIcon(), [this]()
    {
        saveAs();
    });

    m_exportAction = makeAction(QStringLiteral("导出…"), key(QStringLiteral("Ctrl+E")),
                                iconFor(QStringLiteral("export")), [this]()
    {
        openExportDialog();
    });

    m_printAction = makeAction(QStringLiteral("打印…"), key(QStringLiteral("Ctrl+P")),
                               iconFor(QStringLiteral("print")), [this]()
    {
        printDocument();
    });

    m_printPreviewAction = makeAction(QStringLiteral("打印预览…"), QKeySequence(), QIcon(),
                                      [this]()
    {
        printPreview();
    });

    m_pageSetupAction = makeAction(QStringLiteral("页面设置…"), QKeySequence(), QIcon(),
                                   [this]()
    {
        pageSetup();
    });

    m_closeAction = makeAction(QStringLiteral("关闭窗口"), key(QStringLiteral("Ctrl+W")),
                               QIcon(), [this]()
    {
        close();
    });

    m_quitAction = makeAction(QStringLiteral("退出"), key(QStringLiteral("Ctrl+Q")), QIcon(),
                              []()
    {
        QApplication::closeAllWindows();
    });

    m_undoAction = m_view->undoStack()->createUndoAction(this, QStringLiteral("撤销"));
    m_undoAction->setShortcut(key(QStringLiteral("Ctrl+Z")));
    m_undoAction->setShortcutContext(Qt::WindowShortcut);
    m_undoAction->setIcon(iconFor(QStringLiteral("undo")));

    m_redoAction = m_view->undoStack()->createRedoAction(this, QStringLiteral("重做"));
    m_redoAction->setShortcuts({key(QStringLiteral("Ctrl+Shift+Z")),
                                key(QStringLiteral("Ctrl+Y"))});
    m_redoAction->setShortcutContext(Qt::WindowShortcut);
    m_redoAction->setIcon(iconFor(QStringLiteral("redo")));

    m_cutAction = makeAction(QStringLiteral("剪切"), key(QStringLiteral("Ctrl+X")), QIcon(),
                             [this]()
    {
        m_view->cutSelection();
    });

    m_copyAction = makeAction(QStringLiteral("复制"), key(QStringLiteral("Ctrl+C")), QIcon(),
                              [this]()
    {
        m_view->copySelection();
    });

    m_pasteAction = makeAction(QStringLiteral("粘贴"), key(QStringLiteral("Ctrl+V")), QIcon(),
                               [this]()
    {
        m_view->paste();
    });

    m_deleteAction = makeAction(QStringLiteral("删除"), key(QStringLiteral("Delete")),
                                iconFor(QStringLiteral("delete")), [this]()
    {
        m_view->deleteSelection();
    });

    m_duplicateAction = makeAction(QStringLiteral("复制副本"), key(QStringLiteral("Ctrl+D")),
                                   QIcon(), [this]()
    {
        m_view->duplicateSelection();
    });

    m_selectAllAction = makeAction(QStringLiteral("全选"), key(QStringLiteral("Ctrl+A")),
                                   QIcon(), [this]()
    {
        m_view->selectAll();
    });

    m_zoomInAction = makeAction(QStringLiteral("放大"), key(QStringLiteral("Ctrl++")),
                                iconFor(QStringLiteral("zoom-in")), [this]()
    {
        m_view->zoomIn();
    });
    m_zoomInAction->setShortcuts({key(QStringLiteral("Ctrl++")), key(QStringLiteral("Ctrl+="))});

    m_zoomOutAction = makeAction(QStringLiteral("缩小"), key(QStringLiteral("Ctrl+-")),
                                 iconFor(QStringLiteral("zoom-out")), [this]()
    {
        m_view->zoomOut();
    });

    m_resetZoomAction = makeAction(QStringLiteral("实际大小"), key(QStringLiteral("Ctrl+0")),
                                   QIcon(), [this]()
    {
        m_view->resetZoom();
    });

    m_fitAction = makeAction(QStringLiteral("适应窗口"), key(QStringLiteral("Ctrl+Shift+H")),
                             iconFor(QStringLiteral("fit")), [this]()
    {
        m_view->fitToWindow();
    });

    m_gridAction = makeAction(QStringLiteral("显示网格"), key(QStringLiteral("Ctrl+Shift+G")),
                              iconFor(QStringLiteral("grid")), [this]()
    {
        const bool visible = m_gridAction->isChecked();
        m_view->setGridVisible(visible);
        AppSettings::instance().setGridVisible(visible);
    });
    m_gridAction->setCheckable(true);
    m_gridAction->setChecked(true);

    m_pageViewAction = makeAction(QStringLiteral("显示页面边界"), QKeySequence(), QIcon(),
                                  [this]()
    {
        const bool visible = m_pageViewAction->isChecked();
        m_view->setPageVisible(visible);
        AppSettings::instance().setPageVisible(visible);
        updateStatus();
    });
    m_pageViewAction->setCheckable(true);
    m_pageViewAction->setChecked(true);

    m_darkAction = makeAction(QStringLiteral("深色模式"), QKeySequence(), QIcon(), [this]()
    {
        applyTheme(m_darkAction->isChecked() ? QStringLiteral("dark")
                                             : QStringLiteral("light"));
    });
    m_darkAction->setCheckable(true);

    m_fullScreenAction = makeAction(QStringLiteral("全屏"), key(QStringLiteral("F11")), QIcon(),
                                    [this]()
    {
        if (isFullScreen())
        {
            showNormal();
        }
        else
        {
            showFullScreen();
        }
    });
    m_fullScreenAction->setCheckable(true);

    m_toggleShapesAction = makeAction(QStringLiteral("形状面板"), QKeySequence(), QIcon(),
                                      [this]()
    {
        if (auto *dock = findChild<QDockWidget *>(QStringLiteral("shapesDock")))
        {
            dock->setVisible(!dock->isVisible());
        }
    });
    m_toggleShapesAction->setCheckable(true);
    m_toggleShapesAction->setChecked(true);

    m_toggleFormatAction = makeAction(QStringLiteral("格式面板"), QKeySequence(), QIcon(),
                                      [this]()
    {
        if (auto *dock = findChild<QDockWidget *>(QStringLiteral("formatDock")))
        {
            dock->setVisible(!dock->isVisible());
        }
    });
    m_toggleFormatAction->setCheckable(true);
    m_toggleFormatAction->setChecked(true);

    m_groupAction = makeAction(QStringLiteral("组合"), key(QStringLiteral("Ctrl+G")),
                               iconFor(QStringLiteral("group")), [this]()
    {
        m_view->groupSelection();
    });

    m_ungroupAction = makeAction(QStringLiteral("取消组合"),
                                 key(QStringLiteral("Ctrl+Shift+U")),
                                 iconFor(QStringLiteral("ungroup")), [this]()
    {
        m_view->ungroupSelection();
    });

    m_frontAction = makeAction(QStringLiteral("置于顶层"),
                               key(QStringLiteral("Ctrl+Shift+]")), QIcon(), [this]()
    {
        m_view->changeZOrder(DiagramView::ZOrder::Front);
    });

    m_backAction = makeAction(QStringLiteral("置于底层"),
                              key(QStringLiteral("Ctrl+Shift+[")), QIcon(), [this]()
    {
        m_view->changeZOrder(DiagramView::ZOrder::Back);
    });

    m_forwardAction = makeAction(QStringLiteral("上移一层"), key(QStringLiteral("Ctrl+]")),
                                 QIcon(), [this]()
    {
        m_view->changeZOrder(DiagramView::ZOrder::Forward);
    });

    m_backwardAction = makeAction(QStringLiteral("下移一层"), key(QStringLiteral("Ctrl+[")),
                                  QIcon(), [this]()
    {
        m_view->changeZOrder(DiagramView::ZOrder::Backward);
    });

    m_addPageAction = makeAction(QStringLiteral("添加页面"), QKeySequence(), QIcon(), [this]()
    {
        addPage();
    });

    m_textAction = makeAction(QStringLiteral("文本"), key(QStringLiteral("Shift+T")),
                              iconFor(QStringLiteral("text")), [this]()
    {
        const QPointF center = m_view->toScene(QPointF(m_view->width() / 2.0,
                                                       m_view->height() / 2.0));
        m_view->addText(center);
    });

    m_connectorAction = makeAction(QStringLiteral("连接线"), key(QStringLiteral("C")),
                                   iconFor(QStringLiteral("connector")), [this]()
    {
        m_view->addEdgeFromSelection();
    });

    m_selectToolAction = makeAction(QStringLiteral("选择工具"), key(QStringLiteral("V")),
                                    iconFor(QStringLiteral("select")), [this]()
    {
        m_view->setTool(DiagramView::Tool::Select);
    });
    m_selectToolAction->setCheckable(true);
    m_selectToolAction->setChecked(true);

    m_panToolAction = makeAction(QStringLiteral("平移工具"), key(QStringLiteral("H")),
                                 iconFor(QStringLiteral("pan")), [this]()
    {
        m_view->setTool(DiagramView::Tool::Pan);
    });
    m_panToolAction->setCheckable(true);

    auto *connectorTool = makeAction(QStringLiteral("连线工具"), key(QStringLiteral("L")),
                                     iconFor(QStringLiteral("connector")), [this]()
    {
        m_view->setTool(DiagramView::Tool::Connector);
    });
    connectorTool->setCheckable(true);

    m_toolGroup = new QActionGroup(this);
    m_toolGroup->setExclusive(true);
    m_toolGroup->addAction(m_selectToolAction);
    m_toolGroup->addAction(m_panToolAction);
    m_toolGroup->addAction(connectorTool);
    connectorTool->setObjectName(QStringLiteral("connectorToolAction"));
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    fileMenu->addAction(m_newAction);
    fileMenu->addAction(m_newWindowAction);
    fileMenu->addAction(m_openAction);

    m_recentMenu = fileMenu->addMenu(QStringLiteral("打开最近的文件"));
    refreshRecentMenu();

    QMenu *examplesMenu = fileMenu->addMenu(QStringLiteral("打开内置示例"));

    const QVector<QPair<QString, QString>> examples = {
        {QStringLiteral("流程图"), QStringLiteral(":/examples/basic-flow.drawio")},
        {QStringLiteral("UML 类图"), QStringLiteral(":/examples/uml-class.drawio")},
        {QStringLiteral("网络拓扑"), QStringLiteral(":/examples/network.drawio")},
    };

    for (const auto &entry : examples)
    {
        QAction *action = examplesMenu->addAction(entry.first);
        const QString resource = entry.second;
        connect(action, &QAction::triggered, this, [this, resource]()
        {
            if (!maybeSave())
            {
                return;
            }

            QFile file(resource);

            if (!file.open(QIODevice::ReadOnly))
            {
                QMessageBox::warning(this, QStringLiteral("示例"),
                                     QStringLiteral("无法读取内置示例：%1").arg(resource));
                return;
            }

            QString error;

            if (!m_document->loadFromXmlString(QString::fromUtf8(file.readAll()), &error))
            {
                QMessageBox::warning(this, QStringLiteral("示例"), error);
                return;
            }

            m_document->setFilePath(QString());
            m_document->setModified(false);
            m_view->undoStack()->clear();
            m_view->refreshFromDocument();
            m_view->fitToWindow();
            updateTitle();
            updatePageTabs();
            updateActions();
        });
    }

    fileMenu->addSeparator();
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_saveAsAction);
    fileMenu->addSeparator();

    QMenu *exportMenu = fileMenu->addMenu(QStringLiteral("导出"));
    const QVector<QPair<QString, QString>> formats = {
        {QStringLiteral("PNG 图像…"), QStringLiteral("png")},
        {QStringLiteral("SVG 矢量图…"), QStringLiteral("svg")},
        {QStringLiteral("PDF 文档…"), QStringLiteral("pdf")},
        {QStringLiteral("JPEG 图像…"), QStringLiteral("jpg")},
        {QStringLiteral("draw.io XML…"), QStringLiteral("xml")},
        {QStringLiteral("HTML 查看器…"), QStringLiteral("html")},
    };

    for (const auto &entry : formats)
    {
        QAction *action = exportMenu->addAction(entry.first);
        const QString format = entry.second;
        connect(action, &QAction::triggered, this, [this, format]()
        {
            openExportDialog(format);
        });
    }

    exportMenu->addSeparator();
    exportMenu->addAction(m_exportAction);

    fileMenu->addAction(m_printAction);
    fileMenu->addAction(m_printPreviewAction);
    fileMenu->addAction(m_pageSetupAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_closeAction);
    fileMenu->addAction(m_quitAction);

    QMenu *editMenu = menuBar()->addMenu(QStringLiteral("编辑(&E)"));
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
    editMenu->addSeparator();
    editMenu->addAction(m_cutAction);
    editMenu->addAction(m_copyAction);
    editMenu->addAction(m_pasteAction);
    editMenu->addAction(m_duplicateAction);
    editMenu->addAction(m_deleteAction);
    editMenu->addSeparator();
    editMenu->addAction(m_selectAllAction);
    editMenu->addSeparator();
    editMenu->addAction(m_groupAction);
    editMenu->addAction(m_ungroupAction);

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_resetZoomAction);
    viewMenu->addAction(m_fitAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_gridAction);
    viewMenu->addAction(m_pageViewAction);
    viewMenu->addAction(m_darkAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_toggleShapesAction);
    viewMenu->addAction(m_toggleFormatAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_fullScreenAction);

    QMenu *insertMenu = menuBar()->addMenu(QStringLiteral("插入(&I)"));

    QMenu *shapesMenu = insertMenu->addMenu(QStringLiteral("形状"));
    for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
    {
        QAction *action = shapesMenu->addAction(definition.name);
        connect(action, &QAction::triggered, this, [this, definition]()
        {
            const QPointF center = m_view->toScene(QPointF(m_view->width() / 2.0,
                                                           m_view->height() / 2.0));
            m_view->addShape(definition, center);
        });
    }

    insertMenu->addAction(m_textAction);
    insertMenu->addSeparator();
    insertMenu->addAction(m_connectorAction);

    QAction *imageAction = insertMenu->addAction(QStringLiteral("图片…"));
    connect(imageAction, &QAction::triggered, this, &MainWindow::insertImage);

    insertMenu->addSeparator();
    insertMenu->addAction(m_addPageAction);


    QMenu *arrangeMenu = menuBar()->addMenu(QStringLiteral("排列(&A)"));
    arrangeMenu->addAction(m_frontAction);
    arrangeMenu->addAction(m_backAction);
    arrangeMenu->addAction(m_forwardAction);
    arrangeMenu->addAction(m_backwardAction);

    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    QAction *shortcuts = helpMenu->addAction(QStringLiteral("键盘快捷键"));
    connect(shortcuts, &QAction::triggered, this, [this]()
    {
        showShortcutsDialog(this);
    });
    QAction *about = helpMenu->addAction(QStringLiteral("关于 drawio-qt"));
    connect(about, &QAction::triggered, this, [this]()
    {
        showAboutDialog(this);
    });
}

void MainWindow::createToolBars()
{
    auto *mainToolBar = addToolBar(QStringLiteral("主工具栏"));
    mainToolBar->setObjectName(QStringLiteral("mainToolBar"));
    mainToolBar->setMovable(false);
    mainToolBar->setIconSize(QSize(20, 20));
    mainToolBar->addAction(m_newAction);
    mainToolBar->addAction(m_openAction);
    mainToolBar->addAction(m_saveAction);
    mainToolBar->addSeparator();
    mainToolBar->addAction(m_undoAction);
    mainToolBar->addAction(m_redoAction);
    mainToolBar->addSeparator();
    mainToolBar->addAction(m_deleteAction);
    mainToolBar->addSeparator();
    mainToolBar->addAction(m_zoomInAction);
    mainToolBar->addAction(m_zoomOutAction);
    mainToolBar->addAction(m_fitAction);
    mainToolBar->addAction(m_gridAction);
    mainToolBar->addSeparator();
    mainToolBar->addAction(m_printAction);
    mainToolBar->addAction(m_exportAction);

    auto *toolBar = addToolBar(QStringLiteral("工具"));
    toolBar->setObjectName(QStringLiteral("toolsToolBar"));
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(20, 20));
    toolBar->addAction(m_selectToolAction);
    toolBar->addAction(findChild<QAction *>(QStringLiteral("connectorToolAction")));
    toolBar->addAction(m_panToolAction);
    toolBar->addSeparator();
    toolBar->addAction(m_textAction);
    toolBar->addAction(m_groupAction);
    toolBar->addAction(m_ungroupAction);
}

void MainWindow::createDocks()
{
    m_toolbox = new ShapeToolbox(this);

    auto *shapesDock = new QDockWidget(QStringLiteral("形状"), this);
    shapesDock->setObjectName(QStringLiteral("shapesDock"));
    shapesDock->setWidget(m_toolbox);
    shapesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, shapesDock);

    m_formatPanel = new FormatPanel(this);

    auto *formatDock = new QDockWidget(QStringLiteral("格式"), this);
    formatDock->setObjectName(QStringLiteral("formatDock"));
    formatDock->setWidget(m_formatPanel);
    formatDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, formatDock);

    m_formatPanel->setMinimumWidth(230);

    connect(m_toolbox, &ShapeToolbox::shapeActivated, this,
            [this](const ShapeDefinition &definition)
    {
        const QPointF center = m_view->toScene(QPointF(m_view->width() / 2.0,
                                                       m_view->height() / 2.0));
        m_view->addShape(definition, center);
    });

    connect(shapesDock, &QDockWidget::visibilityChanged, this, [this](bool visible)
    {
        m_toggleShapesAction->setChecked(visible);
    });
    connect(formatDock, &QDockWidget::visibilityChanged, this, [this](bool visible)
    {
        m_toggleFormatAction->setChecked(visible);
    });
}

void MainWindow::createStatusBar()
{
    m_zoomLabel = new QLabel(QStringLiteral("100%"), this);
    m_zoomLabel->setMinimumWidth(60);

    m_positionLabel = new QLabel(QStringLiteral("x: 0  y: 0"), this);
    m_positionLabel->setMinimumWidth(120);

    m_selectionLabel = new QLabel(QString(), this);

    statusBar()->addPermanentWidget(m_selectionLabel);
    statusBar()->addPermanentWidget(m_positionLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);
    statusBar()->showMessage(QStringLiteral("就绪"));
}

// --- state ---------------------------------------------------------------

void MainWindow::updateTitle()
{
    setWindowTitle(m_document->title());
}

void MainWindow::updateActions()
{
    const bool hasDocument = m_document != nullptr && m_document->currentPage() != nullptr;
    const bool hasSelection = m_view->hasSelection();
    const int selectedCount = m_view->selection().size();

    m_saveAction->setEnabled(hasDocument);
    m_saveAsAction->setEnabled(hasDocument);
    m_exportAction->setEnabled(hasDocument);
    m_printAction->setEnabled(hasDocument);
    m_printPreviewAction->setEnabled(hasDocument);
    m_pageSetupAction->setEnabled(hasDocument);
    m_closeAction->setEnabled(true);
    m_selectAllAction->setEnabled(hasDocument);
    m_pasteAction->setEnabled(hasDocument);

    m_cutAction->setEnabled(hasSelection);
    m_copyAction->setEnabled(hasSelection);
    m_deleteAction->setEnabled(hasSelection);
    m_duplicateAction->setEnabled(hasSelection);

    m_frontAction->setEnabled(hasSelection);
    m_backAction->setEnabled(hasSelection);
    m_forwardAction->setEnabled(hasSelection);
    m_backwardAction->setEnabled(hasSelection);

    m_groupAction->setEnabled(selectedCount >= 2);
    m_ungroupAction->setEnabled(hasSelection);

    m_connectorAction->setEnabled(selectedCount >= 2);
}

void MainWindow::updatePageTabs()
{
    if (m_pageTabs == nullptr)
    {
        return;
    }

    QSignalBlocker blocker(m_pageTabs);
    m_pageTabs->setCurrentIndex(-1);

    while (m_pageTabs->count() > 0)
    {
        m_pageTabs->removeTab(0);
    }

    for (int i = 0; i < m_document->pageCount(); ++i)
    {
        const Page *page = m_document->page(i);
        m_pageTabs->addTab(page != nullptr ? page->name : QStringLiteral("页面 %1").arg(i + 1));
    }

    m_pageTabs->setCurrentIndex(m_document->currentIndex());
}

void MainWindow::updateStatus()
{
    m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(m_view->zoom() * 100.0)));

    const int count = m_view->selection().size();
    m_selectionLabel->setText(count == 0 ? QString()
                                        : QStringLiteral("已选择 %1 个对象").arg(count));

    if (m_document->currentPage() != nullptr)
    {
        m_pageViewAction->setChecked(m_view->isPageVisible());
    }
}

// --- file operations -----------------------------------------------------

bool MainWindow::save()
{
    if (m_document->filePath().isEmpty())
    {
        saveAs();
        return !m_document->isModified();
    }

    return saveTo(m_document->filePath());
}

bool MainWindow::saveTo(const QString &path)
{
    m_view->commitEditing();
    watchCurrentFile(false);

    QString error;

    if (!m_document->save(path, &error))
    {
        watchCurrentFile(true);
        QMessageBox::critical(this, QStringLiteral("保存失败"), error);
        return false;
    }

    AppSettings::instance().addRecentFile(path);
    AppSettings::instance().setLastSaveDir(QFileInfo(path).absolutePath());
    AppSettings::instance().sync();
    refreshRecentMenu();
    removeDraft();
    watchCurrentFile(true);
    updateTitle();
    updateActions();
    showStatusMessage(QStringLiteral("已保存：%1").arg(path));
    return true;
}

void MainWindow::saveAs()
{
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("另存为"),
        m_document->filePath().isEmpty()
            ? (m_document->suggestedSavePath().isEmpty()
                   ? AppSettings::instance().lastSaveDir() + QLatin1Char('/')
                         + QStringLiteral("未命名绘图.drawio")
                   : m_document->suggestedSavePath())
            : m_document->filePath(),
        QStringLiteral("draw.io 文件 (*.drawio);;XML 文件 (*.xml)"));

    if (path.isEmpty())
    {
        return;
    }

    if (QFileInfo(path).suffix().isEmpty())
    {
        path += QStringLiteral(".drawio");
    }

    saveTo(path);
}

void MainWindow::openDialog()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开"),
        AppSettings::instance().lastOpenDir(),
        QStringLiteral("draw.io 文件 (*.drawio *.dio *.xml *.svg *.png *.pdf);;"
                       "所有文件 (*)"));

    if (!path.isEmpty())
    {
        openFile(path);
    }
}

bool MainWindow::openFile(const QString &path)
{
    if (!maybeSave())
    {
        return false;
    }

    QString error;

    if (!m_document->load(path, &error))
    {
        QMessageBox::critical(this, QStringLiteral("打开失败"),
                              QStringLiteral("%1\n\n%2").arg(path, error));
        return false;
    }

    m_view->undoStack()->clear();
    m_view->refreshFromDocument();
    m_view->fitToWindow();

    AppSettings::instance().addRecentFile(path);
    AppSettings::instance().setLastOpenDir(QFileInfo(path).absolutePath());
    AppSettings::instance().sync();
    refreshRecentMenu();

    updateTitle();
    updatePageTabs();
    updateActions();
    watchCurrentFile(true);
    checkDraftRecovery();
    showStatusMessage(QStringLiteral("已打开：%1").arg(path));
    return true;
}

void MainWindow::openRecent(const QString &path)
{
    if (!QFileInfo::exists(path))
    {
        AppSettings::instance().removeRecentFile(path);
        refreshRecentMenu();
        QMessageBox::information(this, QStringLiteral("文件不存在"),
                                 QStringLiteral("找不到文件：\n%1").arg(path));
        return;
    }

    openFile(path);
}

void MainWindow::refreshRecentMenu()
{
    if (m_recentMenu == nullptr)
    {
        return;
    }

    m_recentMenu->clear();
    const QStringList files = AppSettings::instance().recentFiles();

    if (files.isEmpty())
    {
        QAction *empty = m_recentMenu->addAction(QStringLiteral("（无）"));
        empty->setEnabled(false);
        return;
    }

    for (const QString &file : files)
    {
        QAction *action = m_recentMenu->addAction(QFileInfo(file).fileName());
        action->setToolTip(file);
        connect(action, &QAction::triggered, this, [this, file]()
        {
            openRecent(file);
        });
    }

    m_recentMenu->addSeparator();
    QAction *clear = m_recentMenu->addAction(QStringLiteral("清除列表"));
    connect(clear, &QAction::triggered, this, [this]()
    {
        AppSettings::instance().clearRecentFiles();
        refreshRecentMenu();
    });
}

void MainWindow::newDocument()
{
    if (!maybeSave())
    {
        return;
    }

    m_view->undoStack()->clear();
    m_document->newDocument();
    m_view->refreshFromDocument();
    m_view->fitToWindow();
    updateTitle();
    updatePageTabs();
    updateActions();
    watchCurrentFile(false);
    showStatusMessage(QStringLiteral("新建绘图"));
}

// --- export / print ------------------------------------------------------

void MainWindow::openExportDialog(const QString &presetFormat)
{
    if (m_document->currentPage() == nullptr)
    {
        return;
    }

    ExportDialog dialog(m_document, this);

    if (!presetFormat.isEmpty())
    {
        dialog.setPresetFormat(presetFormat);
    }

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    QString path = dialog.filePath();
    ExportOptions options = dialog.options();
    const QString suffix = defaultSuffixForFormat(options.format);

    if (QFileInfo(path).suffix().isEmpty())
    {
        path += QLatin1Char('.') + suffix;
    }

    const ExportResult result = Exporter::exportDocument(*m_document, options);

    if (!result.ok)
    {
        QMessageBox::critical(this, QStringLiteral("导出失败"), result.error);
        return;
    }

    QFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        QMessageBox::critical(this, QStringLiteral("导出失败"),
                              QStringLiteral("无法写入：%1\n%2").arg(path, file.errorString()));
        return;
    }

    if (file.write(result.data) != result.data.size())
    {
        QMessageBox::critical(this, QStringLiteral("导出失败"),
                              QStringLiteral("写入文件失败：%1").arg(path));
        return;
    }

    file.close();

    AppSettings::instance().setLastExportDir(QFileInfo(path).absolutePath());
    AppSettings::instance().sync();
    showStatusMessage(QStringLiteral("已导出：%1").arg(path));
}

void MainWindow::insertImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("插入图片"), AppSettings::instance().lastOpenDir(),
        QStringLiteral("图片 (*.png *.jpg *.jpeg *.svg *.gif *.webp);;所有文件 (*)"));

    if (path.isEmpty())
    {
        return;
    }

    QImage image(path);

    if (image.isNull())
    {
        QMessageBox::warning(this, QStringLiteral("插入图片"),
                             QStringLiteral("无法读取图片：%1").arg(path));
        return;
    }

    const QByteArray bytes = [&path]()
    {
        QFile file(path);

        if (!file.open(QIODevice::ReadOnly))
        {
            return QByteArray();
        }

        return file.readAll();
    }();

    if (bytes.isEmpty())
    {
        return;
    }

    const QString mime = QFileInfo(path).suffix().toLower() == QLatin1String("svg")
        ? QStringLiteral("image/svg+xml") : QStringLiteral("image/png");

    const QString dataUri = QStringLiteral("data:%1;base64,%2")
        .arg(mime, QString::fromLatin1(bytes.toBase64()));

    QSizeF size = image.size();

    if (size.width() > 400 || size.height() > 400)
    {
        size.scale(400, 400, Qt::KeepAspectRatio);
    }

    const QPointF center = m_view->toScene(QPointF(m_view->width() / 2.0,
                                                   m_view->height() / 2.0));

    StyleMap style = Style::parse(QStringLiteral(
        "shape=image;verticalLabelPosition=bottom;verticalAlign=top;html=1;"
        "strokeColor=none;fillColor=none;"));
    style.insert(QStringLiteral("image"), dataUri);

    Page *page = m_document->currentPage();

    if (page == nullptr)
    {
        return;
    }

    m_view->beginSnapshot(QStringLiteral("插入图片"));
    page->addVertex(QRectF(center - QPointF(size.width() / 2.0, size.height() / 2.0), size),
                    style);
    m_view->commitSnapshot();
    m_view->update();
}

void MainWindow::pageSetup()
{
    Page *page = m_document->currentPage();

    if (page == nullptr)
    {
        return;
    }

    PageSetupDialog dialog(*page, this);

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    m_view->beginSnapshot(QStringLiteral("页面设置"));
    page->settings = dialog.settings();
    m_view->commitSnapshot();
    m_view->update();
    updateStatus();
}

void MainWindow::printDocument()
{
    Page *page = m_document->currentPage();

    if (page == nullptr)
    {
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setFullPage(true);
    printer.setPageSize(QPageSize(QSizeF(page->settings.pageWidth * 25.4 / 96.0,
                                         page->settings.pageHeight * 25.4 / 96.0),
                                  QPageSize::Millimeter, QStringLiteral("drawio-qt"),
                                  QPageSize::ExactMatch));
    printer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);

    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(QStringLiteral("打印"));

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    renderToPrinter(printer);
}

void MainWindow::printPreview()
{
    Page *page = m_document->currentPage();

    if (page == nullptr)
    {
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setFullPage(true);
    printer.setPageSize(QPageSize(QSizeF(page->settings.pageWidth * 25.4 / 96.0,
                                         page->settings.pageHeight * 25.4 / 96.0),
                                  QPageSize::Millimeter, QStringLiteral("drawio-qt"),
                                  QPageSize::ExactMatch));
    printer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);

    QPrintPreviewDialog preview(&printer, this);
    preview.setWindowTitle(QStringLiteral("打印预览"));
    connect(&preview, &QPrintPreviewDialog::paintRequested, this,
            [this](QPrinter *target)
    {
        renderToPrinter(*target);
    });
    preview.exec();
}

void MainWindow::renderToPrinter(QPrinter &printer)
{
    QPainter painter;

    if (!painter.begin(&printer))
    {
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF deviceRect = printer.pageLayout().fullRectPixels(printer.resolution());
    bool firstPage = true;

    for (int index = 0; index < m_document->pageCount(); ++index)
    {
        Page *page = m_document->page(index);

        if (page == nullptr)
        {
            continue;
        }

        QRectF modelRect = page->settings.page
            ? QRectF(0, 0, page->settings.pageWidth, page->settings.pageHeight)
            : DiagramRenderer::contentBounds(*page);

        if (modelRect.isEmpty())
        {
            modelRect = QRectF(0, 0, 400, 300);
        }

        const double scale = qMin(deviceRect.width() / modelRect.width(),
                                  deviceRect.height() / modelRect.height());

        if (!firstPage)
        {
            printer.newPage();
        }

        painter.save();
        painter.translate(deviceRect.topLeft());
        painter.translate((deviceRect.width() - modelRect.width() * scale) / 2.0,
                          (deviceRect.height() - modelRect.height() * scale) / 2.0);
        painter.scale(scale, scale);
        painter.translate(-modelRect.topLeft());

        RenderOptions options;
        options.grid = false;
        options.page = false;
        options.dark = false;
        DiagramRenderer::paint(painter, *page, options);
        painter.restore();

        firstPage = false;
    }

    painter.end();
}

// --- pages ---------------------------------------------------------------

void MainWindow::addPage()
{
    Page *page = m_document->addPage();

    if (page == nullptr)
    {
        return;
    }

    m_document->setCurrentIndex(m_document->pageCount() - 1);
    m_document->setModified(true);
    updatePageTabs();
    m_view->refreshFromDocument();
    m_view->fitToWindow();
}

void MainWindow::removePage(int index)
{
    if (m_document->pageCount() <= 1)
    {
        QMessageBox::information(this, QStringLiteral("删除页面"),
                                 QStringLiteral("文档至少需要保留一个页面。"));
        return;
    }

    const Page *page = m_document->page(index);

    if (page == nullptr)
    {
        return;
    }

    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("删除页面"));
    box.setText(QStringLiteral("确定要删除页面“%1”吗？").arg(page->name));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.button(QMessageBox::Yes)->setText(QStringLiteral("删除"));
    box.button(QMessageBox::No)->setText(QStringLiteral("取消"));

    if (box.exec() != QMessageBox::Yes)
    {
        return;
    }

    m_view->undoStack()->clear();
    m_document->removePage(index);
    updatePageTabs();
    m_view->refreshFromDocument();
}

void MainWindow::renamePage(int index)
{
    Page *page = m_document->page(index);

    if (page == nullptr)
    {
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("重命名页面"),
                                               QStringLiteral("页面名称："), QLineEdit::Normal,
                                               page->name, &ok);

    if (ok && !name.trimmed().isEmpty())
    {
        page->name = name.trimmed();
        m_document->setModified(true);
        updatePageTabs();
    }
}

// --- view / theme --------------------------------------------------------

void MainWindow::applyTheme(const QString &theme)
{
    const bool dark = theme == QLatin1String("dark");

    m_darkAction->setChecked(dark);
    m_view->setDarkTheme(dark);

    if (dark)
    {
        QPalette palette;
        palette.setColor(QPalette::Window, QColor(0x1B, 0x1D, 0x1E));
        palette.setColor(QPalette::WindowText, QColor(0xE8, 0xEA, 0xED));
        palette.setColor(QPalette::Base, QColor(0x24, 0x27, 0x2B));
        palette.setColor(QPalette::AlternateBase, QColor(0x2C, 0x30, 0x35));
        palette.setColor(QPalette::Text, QColor(0xE8, 0xEA, 0xED));
        palette.setColor(QPalette::Button, QColor(0x2C, 0x30, 0x35));
        palette.setColor(QPalette::ButtonText, QColor(0xE8, 0xEA, 0xED));
        palette.setColor(QPalette::Highlight, QColor(0x1A, 0x73, 0xE8));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        qApp->setPalette(palette);
    }
    else
    {
        qApp->setPalette(QApplication::style()->standardPalette());
    }

    AppSettings::instance().setTheme(theme);
}

// --- drafts --------------------------------------------------------------

QString MainWindow::draftPathFor(const QString &path) const
{
    const QByteArray hash = QCryptographicHash::hash(path.toUtf8(),
                                                     QCryptographicHash::Sha1).toHex();
    return AppSettings::draftDirectory() + QLatin1Char('/')
        + QString::fromLatin1(hash) + QStringLiteral(".drawio");
}

void MainWindow::writeDraft()
{
    if (!AppSettings::instance().autoSaveEnabled() || !m_document->isModified()
        || m_document->filePath().isEmpty())
    {
        return;
    }

    const QString path = draftPathFor(m_document->filePath());
    QFile file(path);

    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        file.write(m_document->toXmlString().toUtf8());
        file.close();
    }
}

void MainWindow::removeDraft()
{
    if (m_document->filePath().isEmpty())
    {
        return;
    }

    QFile::remove(draftPathFor(m_document->filePath()));
}

void MainWindow::checkDraftRecovery()
{
    const QString path = draftPathFor(m_document->filePath());
    const QFileInfo draftInfo(path);

    if (!draftInfo.exists() || !QFileInfo::exists(m_document->filePath()))
    {
        return;
    }

    if (draftInfo.lastModified() <= QFileInfo(m_document->filePath()).lastModified())
    {
        return;
    }

    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("发现自动保存"));
    box.setText(QStringLiteral("发现该文件上次会话的自动保存内容，是否恢复？"));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.button(QMessageBox::Yes)->setText(QStringLiteral("恢复"));
    box.button(QMessageBox::No)->setText(QStringLiteral("忽略"));

    if (box.exec() != QMessageBox::Yes)
    {
        return;
    }

    QFile file(path);

    if (!file.open(QIODevice::ReadOnly))
    {
        return;
    }

    const QString xml = QString::fromUtf8(file.readAll());
    file.close();

    QString error;
    const QString filePath = m_document->filePath();

    if (m_document->loadFromXmlString(xml, &error))
    {
        m_document->setFilePath(filePath);
        m_document->setModified(true);
        m_view->refreshFromDocument();
        m_view->fitToWindow();
        updateTitle();
        updatePageTabs();
        showStatusMessage(QStringLiteral("已恢复自动保存内容"));
    }
}

// --- watcher -------------------------------------------------------------

void MainWindow::watchCurrentFile(bool watch)
{
    const QStringList watched = m_watcher->files();

    if (!watched.isEmpty())
    {
        m_watcher->removePaths(watched);
    }

    const QString path = m_document->filePath();

    if (watch && !path.isEmpty() && QFileInfo::exists(path))
    {
        m_watcher->addPath(path);
    }
}

// --- context menu / close ------------------------------------------------

void MainWindow::showCanvasContextMenu(const QPoint &globalPos, const QString &cellId)
{
    Q_UNUSED(cellId);

    QMenu menu(this);

    if (m_view->hasSelection())
    {
        menu.addAction(m_cutAction);
        menu.addAction(m_copyAction);
        menu.addAction(m_pasteAction);
        menu.addAction(m_duplicateAction);
        menu.addAction(m_deleteAction);
        menu.addSeparator();

        QAction *edit = menu.addAction(QStringLiteral("编辑文本"));
        connect(edit, &QAction::triggered, this, [this]()
        {
            m_view->startEditingSelection();
        });

        QAction *connectAction = menu.addAction(QStringLiteral("连接所选形状"));
        connectAction->setEnabled(m_view->selection().size() >= 2);
        connect(connectAction, &QAction::triggered, this, [this]()
        {
            m_view->addEdgeFromSelection();
        });

        menu.addSeparator();
        menu.addAction(m_frontAction);
        menu.addAction(m_backAction);
        menu.addAction(m_forwardAction);
        menu.addAction(m_backwardAction);
        menu.addSeparator();
        menu.addAction(m_groupAction);
        menu.addAction(m_ungroupAction);
    }
    else
    {
        menu.addAction(m_pasteAction);
        menu.addAction(m_selectAllAction);
        menu.addSeparator();
        menu.addAction(m_fitAction);
        menu.addAction(m_pageSetupAction);
    }

    menu.exec(globalPos);
}

bool MainWindow::maybeSave()
{
    m_view->commitEditing();

    if (!m_document->isModified())
    {
        return true;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("未保存的更改"));
    box.setText(QStringLiteral("文档“%1”有未保存的更改，是否保存？")
                    .arg(m_document->displayName()));
    box.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    box.button(QMessageBox::Save)->setText(QStringLiteral("保存"));
    box.button(QMessageBox::Discard)->setText(QStringLiteral("不保存"));
    box.button(QMessageBox::Cancel)->setText(QStringLiteral("取消"));
    box.setDefaultButton(QMessageBox::Save);

    const int result = box.exec();

    if (result == QMessageBox::Save)
    {
        return save();
    }

    if (result == QMessageBox::Discard)
    {
        removeDraft();
        return true;
    }

    return false;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!maybeSave())
    {
        event->ignore();
        return;
    }

    AppSettings::instance().setWindowGeometry(saveGeometry());
    AppSettings::instance().setWindowState(saveState());
    AppSettings::instance().sync();

    m_autoSaveTimer->stop();
    watchCurrentFile(false);
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    for (const QUrl &url : event->mimeData()->urls())
    {
        if (url.isLocalFile())
        {
            openFile(url.toLocalFile());
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::showStatusMessage(const QString &message, int timeout)
{
    statusBar()->showMessage(message, timeout);
}

} // namespace dq
