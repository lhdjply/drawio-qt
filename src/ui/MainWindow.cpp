#include "ui/MainWindow.h"

#include "app/AppSettings.h"
#include "app/UiTheme.h"
#include "app/I18n.h"
#include "app/UpdateChecker.h"
#include "core/Document.h"
#include "core/MxFileIO.h"
#include "io/Exporter.h"
#include "render/DiagramRenderer.h"
#include "ui/Dialogs.h"

#include <QSettings>
#include "ui/DiagramView.h"
#include "ui/FormatPanel.h"
#include "ui/LayerPanel.h"
#include "ui/ShapeLibraryDialog.h"
#include "ui/ShapeToolbox.h"
#include "ui/UpdateDialog.h"

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
#include <QPixmap>
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
{
    setAcceptDrops(true);
    setDockNestingEnabled(true);
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));

    auto *central = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    createFileTabs();
    centralLayout->addWidget(m_fileTabs->parentWidget());

    m_view = new DiagramView(central);
    centralLayout->addWidget(m_view, 1);

    createPageTabs();
    centralLayout->addWidget(m_pageTabs->parentWidget());

    setCentralWidget(central);

    m_fileTabs->blockSignals(true);
    m_document = new Document(this);
    m_view->setDocument(m_document);
    addDocumentTab(m_document);
    connectDocument(m_document);
    m_fileTabs->blockSignals(false);

    createActions();
    setupShortcuts();
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
            openFiles(paths);
        }
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
        box.setWindowTitle(QStringLiteral("File Changed"));
        box.setText(I18n::text(QStringLiteral("The file was changed by another program:\n%1\n\nReload?")).arg(path));
        box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        box.button(QMessageBox::Yes)->setText(QStringLiteral("Reload"));
        box.button(QMessageBox::No)->setText(QStringLiteral("Keep Current Content"));

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
    m_view->setGuidesVisible(m_guidesAction->isChecked());
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

    // 启动后自动检查一次更新：先让窗口画出来再查，只有真发现新版本才弹窗。
    if (AppSettings::instance().autoCheckUpdate())
    {
        QTimer::singleShot(3000, this, [this]()
        {
            checkForUpdates(false);
        });
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::checkForUpdates(bool manual)
{
    // 同一进程只安排一次启动检查：关掉再开新窗口不必再弹一遍
    if (!manual)
    {
        if (m_updateCheckScheduled)
        {
            return;
        }
        m_updateCheckScheduled = true;

        // 托盘/最小化状态没有窗口可弹，等窗口真的出来再说
        if (!isVisible())
        {
            return;
        }
    }

    auto *checker = new UpdateChecker(this);
    connect(checker, &UpdateChecker::updateAvailable, this,
            [this, checker, manual](const ReleaseInfo &release)
    {
        checker->deleteLater();
        if (!manual && !isVisible())
        {
            return;
        }
        UpdateDialog dialog(release, this);
        dialog.exec();
    });
    connect(checker, &UpdateChecker::failed, this,
            [this, checker, manual](const QString &reason)
    {
        checker->deleteLater();
        // 自动检查失败是后台的一次尝试，不打扰；手动检查必须给出可读原因
        if (manual)
        {
            QMessageBox::warning(this, QStringLiteral("Check for Updates"),
                                 I18n::text(QStringLiteral("Cannot check for updates:\n%1")).arg(reason));
        }
    });
    connect(checker, &UpdateChecker::upToDate, this,
            [this, checker, manual](const QString &current)
    {
        checker->deleteLater();
        if (manual)
        {
            QMessageBox::information(this, QStringLiteral("Check for Updates"),
                                     I18n::text(QStringLiteral("You are up to date (v%1).")).arg(current));
        }
    });

    checker->check();
}

// --- UI construction -----------------------------------------------------

void MainWindow::createFileTabs()
{
    auto *container = new QWidget(this);
    container->setObjectName(QStringLiteral("fileTabContainer"));
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(8, 4, 8, 0);
    layout->setSpacing(0);

    m_fileTabs = new QTabBar(container);
    m_fileTabs->setObjectName(QStringLiteral("fileTabs"));
    m_fileTabs->setDocumentMode(true);
    m_fileTabs->setDrawBase(false);
    m_fileTabs->setExpanding(false);
    m_fileTabs->setUsesScrollButtons(true);
    m_fileTabs->setTabsClosable(true);
    m_fileTabs->setElideMode(Qt::ElideRight);
    m_fileTabs->setMovable(false);
    layout->addWidget(m_fileTabs, 1);

    auto *addTab = new QToolButton(container);
    addTab->setObjectName(QStringLiteral("fileTabAdd"));
    addTab->setText(QStringLiteral("＋"));
    addTab->setToolTip(QStringLiteral("New Tab"));
    addTab->setAutoRaise(true);
    addTab->setFixedSize(26, 26);
    layout->addWidget(addTab);
    connect(addTab, &QToolButton::clicked, this, &MainWindow::newDocument);

    connect(m_fileTabs, &QTabBar::currentChanged, this, [this](int index)
    {
        if (index >= 0)
        {
            switchToDocument(index);
        }
    });
    connect(m_fileTabs, &QTabBar::tabCloseRequested, this, &MainWindow::closeDocumentTab);
}

Document *MainWindow::documentAt(int index) const
{
    if (index < 0 || index >= m_documents.size())
    {
        return nullptr;
    }

    return m_documents.at(index);
}

QString MainWindow::documentTabName(Document *document) const
{
    if (document == nullptr)
    {
        return QStringLiteral("Untitled");
    }

    const QString path = document->filePath();
    const QString name = path.isEmpty() ? QStringLiteral("Untitled")
                                        : QFileInfo(path).fileName();
    return document->isModified() ? name + QLatin1Char('*') : name;
}

int MainWindow::addDocumentTab(Document *document)
{
    m_documents.append(document);
    const QString rawName = documentTabName(document);
    const int index = m_fileTabs->addTab(I18n::text(rawName));
    m_fileTabs->setTabData(index, rawName);
    m_fileTabs->setTabToolTip(index, document->filePath().isEmpty()
                                      ? QStringLiteral("Untitled")
                                      : document->filePath());
    m_fileTabs->setCurrentIndex(index);
    return index;
}

void MainWindow::switchToDocument(int index)
{
    Document *document = documentAt(index);

    if (document == nullptr || document == m_document)
    {
        return;
    }

    if (m_view != nullptr)
    {
        m_view->commitEditing();
    }

    if (m_document != nullptr)
    {
        disconnect(m_document, nullptr, this, nullptr);
    }

    m_document = document;

    if (m_fileTabs != nullptr && m_fileTabs->currentIndex() != index)
    {
        QSignalBlocker blocker(m_fileTabs);
        m_fileTabs->setCurrentIndex(index);
    }

    if (m_view != nullptr)
    {
        m_view->setDocument(m_document);
    }

    connectDocument(m_document);

    if (m_formatPanel != nullptr)
    {
        m_formatPanel->setView(m_view);
    }

    if (m_layerPanel != nullptr)
    {
        m_layerPanel->setView(m_view);
    }

    updatePageTabs();
    updateTitle();
    updateActions();
    updateStatus();
    watchCurrentFile(!m_document->filePath().isEmpty());
}

void MainWindow::connectDocument(Document *document)
{
    if (document == nullptr)
    {
        return;
    }

    connect(document, &Document::modifiedChanged, this, [this, document](bool)
    {
        if (document == m_document)
        {
            updateTitle();
            updateActions();
        }
    });
    connect(document, &Document::pagesChanged, this, [this, document]()
    {
        if (document == m_document)
        {
            updatePageTabs();
            updateActions();
        }
    });
    connect(document, &Document::currentPageChanged, this, [this, document](int index)
    {
        if (document != m_document)
        {
            return;
        }

        updatePageTabs();
        m_view->refreshFromDocument();
        updateStatus();

        if (index >= 0 && m_pageTabs != nullptr)
        {
            QSignalBlocker blocker(m_pageTabs);
            m_pageTabs->setCurrentIndex(index);
        }
    });
    connect(document, &Document::filePathChanged, this, [this, document](const QString &)
    {
        if (document == m_document)
        {
            updateTitle();
        }
    });
}

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
    layout->addWidget(m_pageTabs, 1);

    auto *addButton = new QToolButton(container);
    addButton->setText(QStringLiteral("＋"));
    addButton->setToolTip(QStringLiteral("Add Page"));
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

    m_newAction = makeAction(QStringLiteral("New"), key(QStringLiteral("Ctrl+N")),
                             iconFor(QStringLiteral("new")), [this]()
    {
        newDocument();
    });
    m_newAction->setToolTip(QStringLiteral("New Drawing (Ctrl+N)"));

    m_newWindowAction = makeAction(QStringLiteral("New Window"),
                                   key(QStringLiteral("Ctrl+Shift+N")),
                                   QIcon(), [this]()
    {
        auto *window = new MainWindow();
        window->setWindowIcon(windowIcon());
        window->resize(size());
        window->show();
        window->newDocument();
    });

    m_openAction = makeAction(QStringLiteral("Open…"), key(QStringLiteral("Ctrl+O")),
                              iconFor(QStringLiteral("open")), [this]()
    {
        openDialog();
    });

    m_saveAction = makeAction(QStringLiteral("Save"), key(QStringLiteral("Ctrl+S")),
                              iconFor(QStringLiteral("save")), [this]()
    {
        save();
    });

    m_saveAsAction = makeAction(QStringLiteral("Save As…"),
                                key(QStringLiteral("Ctrl+Shift+S")), QIcon(), [this]()
    {
        saveAs();
    });

    m_exportAction = makeAction(QStringLiteral("Export…"), key(QStringLiteral("Ctrl+E")),
                                iconFor(QStringLiteral("export")), [this]()
    {
        openExportDialog();
    });

    m_printAction = makeAction(QStringLiteral("Print…"), key(QStringLiteral("Ctrl+P")),
                               iconFor(QStringLiteral("print")), [this]()
    {
        printDocument();
    });

    m_printPreviewAction = makeAction(QStringLiteral("Print Preview…"),
                                      key(QStringLiteral("Ctrl+Shift+P")), QIcon(),
                                      [this]()
    {
        printPreview();
    });

    m_pageSetupAction = makeAction(QStringLiteral("Page Setup…"),
                                   key(QStringLiteral("Ctrl+Alt+P")), QIcon(),
                                   [this]()
    {
        pageSetup();
    });

    m_closeAction = makeAction(QStringLiteral("Close Tab"), key(QStringLiteral("Ctrl+W")),
                               QIcon(), [this]()
    {
        if (m_fileTabs != nullptr)
        {
            closeDocumentTab(m_fileTabs->currentIndex());
        }
    });

    m_quitAction = makeAction(QStringLiteral("Quit"), key(QStringLiteral("Ctrl+Q")), QIcon(),
                              []()
    {
        QApplication::closeAllWindows();
    });

    m_undoAction = m_view->undoStack()->createUndoAction(this, QStringLiteral("Undo"));
    m_undoAction->setShortcut(key(QStringLiteral("Ctrl+Z")));
    m_undoAction->setShortcutContext(Qt::WindowShortcut);
    m_undoAction->setIcon(iconFor(QStringLiteral("undo")));

    m_redoAction = m_view->undoStack()->createRedoAction(this, QStringLiteral("Redo"));
    m_redoAction->setShortcuts({key(QStringLiteral("Ctrl+Shift+Z")),
                                key(QStringLiteral("Ctrl+Y"))});
    m_redoAction->setShortcutContext(Qt::WindowShortcut);
    m_redoAction->setIcon(iconFor(QStringLiteral("redo")));

    m_cutAction = makeAction(QStringLiteral("Cut"), key(QStringLiteral("Ctrl+X")), QIcon(),
                             [this]()
    {
        m_view->cutSelection();
    });

    m_copyAction = makeAction(QStringLiteral("Copy"), key(QStringLiteral("Ctrl+C")), QIcon(),
                              [this]()
    {
        m_view->copySelection();
    });

    m_pasteAction = makeAction(QStringLiteral("Paste"), key(QStringLiteral("Ctrl+V")), QIcon(),
                               [this]()
    {
        m_view->paste();
    });

    m_deleteAction = makeAction(QStringLiteral("Delete"), key(QStringLiteral("Delete")),
                                iconFor(QStringLiteral("delete")), [this]()
    {
        m_view->deleteSelection();
    });

    m_duplicateAction = makeAction(QStringLiteral("Duplicate"), key(QStringLiteral("Ctrl+D")),
                                   QIcon(), [this]()
    {
        m_view->duplicateSelection();
    });

    m_selectAllAction = makeAction(QStringLiteral("Select All"), key(QStringLiteral("Ctrl+A")),
                                   QIcon(), [this]()
    {
        m_view->selectAll();
    });

    m_zoomInAction = makeAction(QStringLiteral("Zoom In"), key(QStringLiteral("Ctrl++")),
                                iconFor(QStringLiteral("zoom-in")), [this]()
    {
        m_view->zoomIn();
    });
    m_zoomInAction->setShortcuts({key(QStringLiteral("Ctrl++")), key(QStringLiteral("Ctrl+="))});

    m_zoomOutAction = makeAction(QStringLiteral("Zoom Out"), key(QStringLiteral("Ctrl+-")),
                                 iconFor(QStringLiteral("zoom-out")), [this]()
    {
        m_view->zoomOut();
    });

    m_resetZoomAction = makeAction(QStringLiteral("Actual Size"), key(QStringLiteral("Ctrl+0")),
                                   QIcon(), [this]()
    {
        m_view->resetZoom();
    });

    m_fitAction = makeAction(QStringLiteral("Fit Window"), key(QStringLiteral("Ctrl+Shift+H")),
                             iconFor(QStringLiteral("fit")), [this]()
    {
        m_view->fitToWindow();
    });

    m_gridAction = makeAction(QStringLiteral("Show Grid"), key(QStringLiteral("Ctrl+Shift+G")),
                              iconFor(QStringLiteral("grid")), [this]()
    {
        const bool visible = m_gridAction->isChecked();
        m_view->setGridVisible(visible);
        AppSettings::instance().setGridVisible(visible);
    });
    m_gridAction->setCheckable(true);
    m_gridAction->setChecked(true);

    m_guidesAction = makeAction(QStringLiteral("Alignment Guides"),
                                 key(QStringLiteral("Ctrl+Shift+L")), QIcon(),
                                [this]()
    {
        const bool visible = m_guidesAction->isChecked();
        m_view->setGuidesVisible(visible);
        AppSettings::instance().setGuidesVisible(visible);
    });
    m_guidesAction->setCheckable(true);
    m_guidesAction->setChecked(AppSettings::instance().guidesVisible());

    m_pageViewAction = makeAction(QStringLiteral("Show Page Bounds"),
                                   key(QStringLiteral("Ctrl+Shift+B")), QIcon(),
                                  [this]()
    {
        const bool visible = m_pageViewAction->isChecked();
        m_view->setPageVisible(visible);
        AppSettings::instance().setPageVisible(visible);
        updateStatus();
    });
    m_pageViewAction->setCheckable(true);
    m_pageViewAction->setChecked(true);

    m_settingsAction = makeAction(QStringLiteral("Settings…"), key(QStringLiteral("Ctrl+,")), QIcon(),
                                  [this]()
    {
        openSettings();
    });

    m_fullScreenAction = makeAction(QStringLiteral("Full Screen"), key(QStringLiteral("F11")), QIcon(),
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

    m_toggleShapesAction = makeAction(QStringLiteral("Shape Panel"),
                                      key(QStringLiteral("Ctrl+Shift+K")), QIcon(),
                                      [this]()
    {
        if (auto *dock = findChild<QDockWidget *>(QStringLiteral("shapesDock")))
        {
            dock->setVisible(!dock->isVisible());
        }
    });
    m_toggleShapesAction->setCheckable(true);
    m_toggleShapesAction->setChecked(true);

    m_toggleFormatAction = makeAction(QStringLiteral("Format Panel"),
                                      key(QStringLiteral("Ctrl+Shift+F")), QIcon(),
                                      [this]()
    {
        if (auto *dock = findChild<QDockWidget *>(QStringLiteral("formatDock")))
        {
            dock->setVisible(!dock->isVisible());
        }
    });
    m_toggleFormatAction->setCheckable(true);
    m_toggleFormatAction->setChecked(true);

    m_groupAction = makeAction(QStringLiteral("Group"), key(QStringLiteral("Ctrl+G")),
                               iconFor(QStringLiteral("group")), [this]()
    {
        m_view->groupSelection();
    });

    m_ungroupAction = makeAction(QStringLiteral("Ungroup"),
                                 key(QStringLiteral("Ctrl+Shift+U")),
                                 iconFor(QStringLiteral("ungroup")), [this]()
    {
        m_view->ungroupSelection();
    });

    m_frontAction = makeAction(QStringLiteral("Bring to Front"),
                               key(QStringLiteral("Ctrl+Shift+]")), QIcon(), [this]()
    {
        m_view->changeZOrder(DiagramView::ZOrder::Front);
    });

    m_backAction = makeAction(QStringLiteral("Send to Back"),
                              key(QStringLiteral("Ctrl+Shift+[")), QIcon(), [this]()
    {
        m_view->changeZOrder(DiagramView::ZOrder::Back);
    });

    m_forwardAction = makeAction(QStringLiteral("Bring Forward"), key(QStringLiteral("Ctrl+]")),
                                 QIcon(), [this]()
    {
        m_view->changeZOrder(DiagramView::ZOrder::Forward);
    });

    m_backwardAction = makeAction(QStringLiteral("Send Backward"), key(QStringLiteral("Ctrl+[")),
                                  QIcon(), [this]()
    {
        m_view->changeZOrder(DiagramView::ZOrder::Backward);
    });

    m_addPageAction = makeAction(QStringLiteral("Add Page"),
                                  key(QStringLiteral("Ctrl+Shift+PgDown")), QIcon(), [this]()
    {
        addPage();
    });

    m_textAction = makeAction(QStringLiteral("Text"), key(QStringLiteral("Shift+T")),
                              iconFor(QStringLiteral("text")), [this]()
    {
        const QPointF center = m_view->toScene(QPointF(m_view->width() / 2.0,
                                                       m_view->height() / 2.0));
        m_view->addText(center);
    });

    m_connectorAction = makeAction(QStringLiteral("Connector"), key(QStringLiteral("C")),
                                   iconFor(QStringLiteral("connector")), [this]()
    {
        m_view->addEdgeFromSelection();
    });

    m_selectToolAction = makeAction(QStringLiteral("Select Tool"), key(QStringLiteral("V")),
                                    iconFor(QStringLiteral("select")), [this]()
    {
        m_view->setTool(DiagramView::Tool::Select);
    });
    m_selectToolAction->setCheckable(true);
    m_selectToolAction->setChecked(true);

    m_panToolAction = makeAction(QStringLiteral("Pan Tool"), key(QStringLiteral("H")),
                                 iconFor(QStringLiteral("pan")), [this]()
    {
        m_view->setTool(DiagramView::Tool::Pan);
    });
    m_panToolAction->setCheckable(true);

    auto *connectorTool = makeAction(QStringLiteral("Connector Tool"), key(QStringLiteral("L")),
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

void MainWindow::setupShortcuts()
{
    const auto add = [this](const QString &id, const QString &label, QAction *action)
    {
        if (action == nullptr)
        {
            return;
        }

        m_shortcutActions.insert(id, action);
        m_shortcuts.append(ShortcutSetting{id, label, action->shortcut(), action->shortcut()});
    };

    add(QStringLiteral("file.new"), QStringLiteral("New"), m_newAction);
    add(QStringLiteral("file.newWindow"), QStringLiteral("New Window"), m_newWindowAction);
    add(QStringLiteral("file.open"), QStringLiteral("Open…"), m_openAction);
    add(QStringLiteral("file.save"), QStringLiteral("Save"), m_saveAction);
    add(QStringLiteral("file.saveAs"), QStringLiteral("Save As…"), m_saveAsAction);
    add(QStringLiteral("file.export"), QStringLiteral("Export…"), m_exportAction);
    add(QStringLiteral("file.print"), QStringLiteral("Print…"), m_printAction);
    add(QStringLiteral("file.printPreview"), QStringLiteral("Print Preview…"), m_printPreviewAction);
    add(QStringLiteral("file.pageSetup"), QStringLiteral("Page Setup…"), m_pageSetupAction);
    add(QStringLiteral("file.close"), QStringLiteral("Close File"), m_closeAction);
    add(QStringLiteral("file.quit"), QStringLiteral("Quit"), m_quitAction);

    add(QStringLiteral("edit.undo"), QStringLiteral("Undo"), m_undoAction);
    add(QStringLiteral("edit.redo"), QStringLiteral("Redo"), m_redoAction);
    add(QStringLiteral("edit.cut"), QStringLiteral("Cut"), m_cutAction);
    add(QStringLiteral("edit.copy"), QStringLiteral("Copy"), m_copyAction);
    add(QStringLiteral("edit.paste"), QStringLiteral("Paste"), m_pasteAction);
    add(QStringLiteral("edit.delete"), QStringLiteral("Delete"), m_deleteAction);
    add(QStringLiteral("edit.duplicate"), QStringLiteral("Duplicate"), m_duplicateAction);
    add(QStringLiteral("edit.selectAll"), QStringLiteral("Select All"), m_selectAllAction);

    add(QStringLiteral("view.zoomIn"), QStringLiteral("Zoom In"), m_zoomInAction);
    add(QStringLiteral("view.zoomOut"), QStringLiteral("Zoom Out"), m_zoomOutAction);
    add(QStringLiteral("view.resetZoom"), QStringLiteral("Actual Size"), m_resetZoomAction);
    add(QStringLiteral("view.fit"), QStringLiteral("Fit Window"), m_fitAction);
    add(QStringLiteral("view.grid"), QStringLiteral("Show Grid"), m_gridAction);
    add(QStringLiteral("view.guides"), QStringLiteral("Alignment Guides"), m_guidesAction);
    add(QStringLiteral("view.page"), QStringLiteral("Show Page Bounds"), m_pageViewAction);
    add(QStringLiteral("view.fullScreen"), QStringLiteral("Full Screen"), m_fullScreenAction);
    add(QStringLiteral("view.shapes"), QStringLiteral("Shape Panel"), m_toggleShapesAction);
    add(QStringLiteral("view.format"), QStringLiteral("Format Panel"), m_toggleFormatAction);

    add(QStringLiteral("arrange.group"), QStringLiteral("Group"), m_groupAction);
    add(QStringLiteral("arrange.ungroup"), QStringLiteral("Ungroup"), m_ungroupAction);
    add(QStringLiteral("arrange.front"), QStringLiteral("Bring to Front"), m_frontAction);
    add(QStringLiteral("arrange.back"), QStringLiteral("Send to Back"), m_backAction);
    add(QStringLiteral("arrange.forward"), QStringLiteral("Bring Forward"), m_forwardAction);
    add(QStringLiteral("arrange.backward"), QStringLiteral("Send Backward"), m_backwardAction);

    add(QStringLiteral("insert.page"), QStringLiteral("Add Page"), m_addPageAction);
    add(QStringLiteral("insert.text"), QStringLiteral("Text"), m_textAction);
    add(QStringLiteral("insert.connector"), QStringLiteral("Connector"), m_connectorAction);
    add(QStringLiteral("tool.select"), QStringLiteral("Select Tool"), m_selectToolAction);
    add(QStringLiteral("tool.pan"), QStringLiteral("Pan Tool"), m_panToolAction);
    add(QStringLiteral("app.settings"), QStringLiteral("Settings…"), m_settingsAction);

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("drawio-qt"), QStringLiteral("drawio-qt"));

    for (ShortcutSetting &item : m_shortcuts)
    {
        const QString key = QStringLiteral("shortcuts/") + item.id;

        if (!settings.contains(key))
        {
            continue;
        }

        const QString saved = settings.value(key).toString();
        const QKeySequence sequence = QKeySequence::fromString(
            saved, QKeySequence::PortableText);

        if (saved.isEmpty() || sequence.toString().isEmpty())
        {
            // Older builds saved empty/invalid shortcuts before defaults existed.
            settings.remove(key);
            continue;
        }

        QAction *action = m_shortcutActions.value(item.id);

        if (action != nullptr)
        {
            action->setShortcut(sequence);
        }

        item.sequence = sequence;
    }
}

void MainWindow::editShortcuts()
{
    ShortcutDialog dialog(m_shortcuts, this);

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    m_shortcuts = dialog.settings();
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("drawio-qt"), QStringLiteral("drawio-qt"));

    for (const ShortcutSetting &item : m_shortcuts)
    {
        if (QAction *action = m_shortcutActions.value(item.id))
        {
            action->setShortcut(item.sequence);
        }

        settings.setValue(QStringLiteral("shortcuts/") + item.id,
                          item.sequence.toString(QKeySequence::PortableText));
    }

    settings.sync();
}

void MainWindow::openSettings()
{
    SettingsDialog dialog(m_shortcuts, AppSettings::instance().theme(),
                          AppSettings::instance().language(), this);

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    applyTheme(dialog.theme());

    const QString language = dialog.language();
    AppSettings::instance().setLanguage(language);

    if (language == QLatin1String("en"))
    {
        I18n::setLanguage(I18n::Language::English);
    }
    else if (language == QLatin1String("zh"))
    {
        I18n::setLanguage(I18n::Language::Chinese);
    }
    else
    {
        I18n::setLanguage(QLocale::system().language() == QLocale::English
                              ? I18n::Language::English : I18n::Language::Chinese);
    }

    if (m_toolbox != nullptr)
    {
        m_toolbox->reload();
    }

    if (m_layerPanel != nullptr)
    {
        m_layerPanel->refresh();
    }

    m_shortcuts = dialog.shortcutSettings();

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("drawio-qt"), QStringLiteral("drawio-qt"));

    for (const ShortcutSetting &item : m_shortcuts)
    {
        if (QAction *action = m_shortcutActions.value(item.id))
        {
            action->setShortcut(item.sequence);
        }

        settings.setValue(QStringLiteral("shortcuts/") + item.id,
                          item.sequence.toString(QKeySequence::PortableText));
    }

    settings.sync();
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("File"));
    fileMenu->addAction(m_newAction);
    fileMenu->addAction(m_newWindowAction);
    fileMenu->addAction(m_openAction);

    m_recentMenu = fileMenu->addMenu(QStringLiteral("Open Recent"));
    refreshRecentMenu();

    fileMenu->addSeparator();
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_saveAsAction);
    fileMenu->addSeparator();

    QMenu *exportMenu = fileMenu->addMenu(QStringLiteral("Export"));
    const QVector<QPair<QString, QString>> formats = {
        {QStringLiteral("PNG Image…"), QStringLiteral("png")},
        {QStringLiteral("SVG Vector Image…"), QStringLiteral("svg")},
        {QStringLiteral("PDF Document…"), QStringLiteral("pdf")},
        {QStringLiteral("JPEG Image…"), QStringLiteral("jpg")},
        {QStringLiteral("draw.io XML…"), QStringLiteral("xml")},
        {QStringLiteral("HTML Viewer…"), QStringLiteral("html")},
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

    QMenu *editMenu = menuBar()->addMenu(QStringLiteral("Edit"));
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
    editMenu->addSeparator();
    editMenu->addAction(m_settingsAction);

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("View"));
    viewMenu->setObjectName(QStringLiteral("viewMenu"));
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_resetZoomAction);
    viewMenu->addAction(m_fitAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_gridAction);
    viewMenu->addAction(m_guidesAction);
    viewMenu->addAction(m_pageViewAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_toggleShapesAction);
    viewMenu->addAction(m_toggleFormatAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_fullScreenAction);

    QMenu *insertMenu = menuBar()->addMenu(QStringLiteral("Insert"));

    insertMenu->addAction(m_textAction);
    insertMenu->addSeparator();
    insertMenu->addAction(m_connectorAction);

    QAction *libraryAction = insertMenu->addAction(QStringLiteral("Choose Shape Libraries…"));
    connect(libraryAction, &QAction::triggered, this, [this]()
    {
        ShapeLibraryDialog dialog(this);

        if (dialog.exec() == QDialog::Accepted)
        {
            ShapeLibraryDialog::setConfiguredCategories(dialog.enabledCategories());

            if (m_toolbox != nullptr)
            {
                m_toolbox->reload();
            }
        }
    });

    QAction *imageAction = insertMenu->addAction(QStringLiteral("Image…"));
    connect(imageAction, &QAction::triggered, this, &MainWindow::insertImage);

    insertMenu->addSeparator();
    insertMenu->addAction(m_addPageAction);


    QMenu *arrangeMenu = menuBar()->addMenu(QStringLiteral("Arrange"));

    QMenu *alignMenu = arrangeMenu->addMenu(QStringLiteral("Align"));
    const QVector<QPair<QString, DiagramView::Align>> alignEntries = {
        {QStringLiteral("Align Left"), DiagramView::Align::Left},
        {QStringLiteral("Center Horizontally"), DiagramView::Align::CenterHorizontal},
        {QStringLiteral("Align Right"), DiagramView::Align::Right},
        {QStringLiteral("Align Top"), DiagramView::Align::Top},
        {QStringLiteral("Center Vertically"), DiagramView::Align::CenterVertical},
        {QStringLiteral("Align Bottom"), DiagramView::Align::Bottom},
    };

    for (int i = 0; i < alignEntries.size(); ++i)
    {
        if (i == 3)
        {
            alignMenu->addSeparator();
        }

        const DiagramView::Align align = alignEntries.at(i).second;
        QAction *action = alignMenu->addAction(alignEntries.at(i).first);
        connect(action, &QAction::triggered, this, [this, align]()
        {
            m_view->alignSelection(align);
        });
        m_arrangeSelectionActions.append(action);
    }

    QMenu *layoutMenu = arrangeMenu->addMenu(QStringLiteral("Layout"));

    QAction *distributeH = layoutMenu->addAction(QStringLiteral("Distribute Horizontally"));
    connect(distributeH, &QAction::triggered, this, [this]()
    {
        m_view->distributeHorizontally();
    });
    m_arrangeSelectionActions.append(distributeH);

    QAction *distributeV = layoutMenu->addAction(QStringLiteral("Distribute Vertically"));
    connect(distributeV, &QAction::triggered, this, [this]()
    {
        m_view->distributeVertically();
    });
    m_arrangeSelectionActions.append(distributeV);

    layoutMenu->addSeparator();

    QAction *sameWidth = layoutMenu->addAction(QStringLiteral("Same Width"));
    connect(sameWidth, &QAction::triggered, this, [this]()
    {
        m_view->resizeSelection(DiagramView::SizeAction::SameWidth);
    });
    m_arrangeSelectionActions.append(sameWidth);

    QAction *sameHeight = layoutMenu->addAction(QStringLiteral("Same Height"));
    connect(sameHeight, &QAction::triggered, this, [this]()
    {
        m_view->resizeSelection(DiagramView::SizeAction::SameHeight);
    });
    m_arrangeSelectionActions.append(sameHeight);

    QAction *sameSize = layoutMenu->addAction(QStringLiteral("Same Size"));
    connect(sameSize, &QAction::triggered, this, [this]()
    {
        m_view->resizeSelection(DiagramView::SizeAction::SameSize);
    });
    m_arrangeSelectionActions.append(sameSize);

    arrangeMenu->addSeparator();
    arrangeMenu->addAction(m_groupAction);
    arrangeMenu->addAction(m_ungroupAction);

    arrangeMenu->addSeparator();
    arrangeMenu->addAction(m_frontAction);
    arrangeMenu->addAction(m_backAction);
    arrangeMenu->addAction(m_forwardAction);
    arrangeMenu->addAction(m_backwardAction);

    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("Help"));
    QAction *checkUpdates = helpMenu->addAction(QStringLiteral("Check for Updates…"));
    connect(checkUpdates, &QAction::triggered, this, [this]()
    {
        checkForUpdates(true);
    });
    QAction *about = helpMenu->addAction(QStringLiteral("About drawio-qt"));
    connect(about, &QAction::triggered, this, [this]()
    {
        showAboutDialog(this);
    });
}

void MainWindow::createToolBars()
{
    auto *mainToolBar = addToolBar(QStringLiteral("Main Toolbar"));
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

    auto *toolBar = addToolBar(QStringLiteral("Tools"));
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

    auto *shapesDock = new QDockWidget(QStringLiteral("Shape"), this);
    shapesDock->setObjectName(QStringLiteral("shapesDock"));
    shapesDock->setWidget(m_toolbox);
    shapesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, shapesDock);

    m_formatPanel = new FormatPanel(this);

    auto *formatDock = new QDockWidget(QStringLiteral("Format"), this);
    formatDock->setObjectName(QStringLiteral("formatDock"));
    formatDock->setWidget(m_formatPanel);
    formatDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    formatDock->setMinimumWidth(290);
    addDockWidget(Qt::RightDockWidgetArea, formatDock);

    m_formatPanel->setMinimumWidth(270);
    resizeDocks({formatDock}, {290}, Qt::Horizontal);

    m_layerPanel = new LayerPanel(this);

    auto *layerDock = new QDockWidget(QStringLiteral("Layers"), this);
    layerDock->setObjectName(QStringLiteral("layerDock"));
    layerDock->setWidget(m_layerPanel);
    layerDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, layerDock);
    m_layerPanel->setView(m_view);

    if (QMenu *viewMenu = findChild<QMenu *>(QStringLiteral("viewMenu")))
    {
        viewMenu->addAction(layerDock->toggleViewAction());
    }

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
    statusBar()->showMessage(I18n::text(QStringLiteral("Ready")));
}

// --- state ---------------------------------------------------------------

void MainWindow::updateTitle()
{
    QString title = m_document->title();

    if (m_document->filePath().isEmpty())
    {
        const QString base = I18n::text(QStringLiteral("Untitled Drawing"));
        title = QStringLiteral("%1 - drawio-qt").arg(
            m_document->isModified() ? QStringLiteral("*") + base : base);
    }

    setWindowTitle(title);

    if (m_fileTabs != nullptr && m_fileTabs->currentIndex() >= 0)
    {
        const int index = m_fileTabs->currentIndex();
        m_fileTabs->setTabText(index, I18n::text(documentTabName(m_document)));
        m_fileTabs->setTabData(index, documentTabName(m_document));
        m_fileTabs->setTabToolTip(index, m_document->filePath().isEmpty()
                                         ? QStringLiteral("Untitled")
                                         : m_document->filePath());
    }
}

void MainWindow::updateActions()
{
    if (m_saveAction == nullptr)
    {
        return;
    }

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

    bool hasSelectedGroup = false;

    for (Cell *cell : m_view->selectedCellObjects())
    {
        if (cell->isGroup())
        {
            hasSelectedGroup = true;
            break;
        }
    }

    m_ungroupAction->setEnabled(hasSelectedGroup);

    for (QAction *action : m_arrangeSelectionActions)
    {
        action->setEnabled(selectedCount >= 2);
    }

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
        const QString rawName = page != nullptr ? page->name
                                                : QStringLiteral("Page %1").arg(i + 1);
        m_pageTabs->addTab(I18n::text(rawName));
        m_pageTabs->setTabData(i, rawName);
    }

    m_pageTabs->setCurrentIndex(m_document->currentIndex());

    if (m_layerPanel != nullptr)
    {
        m_layerPanel->refresh();
    }
}

void MainWindow::updateStatus()
{
    m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(m_view->zoom() * 100.0)));

    const int count = m_view->selection().size();
    m_selectionLabel->setText(count == 0 ? QString()
                                        : I18n::text(QStringLiteral("%1 object(s) selected")).arg(count));

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
        QMessageBox::critical(this, QStringLiteral("Save Failed"), error);
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
    showStatusMessage(I18n::text(QStringLiteral("Saved: %1")).arg(path));
    return true;
}

void MainWindow::saveAs()
{
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save As"),
        m_document->filePath().isEmpty()
            ? (m_document->suggestedSavePath().isEmpty()
                   ? AppSettings::instance().lastSaveDir() + QLatin1Char('/')
                         + QStringLiteral("Untitled.drawio")
                   : m_document->suggestedSavePath())
            : m_document->filePath(),
        QStringLiteral("draw.io Files (*.drawio);;XML Files (*.xml)"));

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
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Open"),
        AppSettings::instance().lastOpenDir(),
        QStringLiteral("draw.io Files (*.drawio *.dio *.xml *.svg *.png *.pdf);;"
                       "All Files (*)"));

    if (!paths.isEmpty())
    {
        openFiles(paths);
    }
}

void MainWindow::openFiles(const QStringList &paths)
{
    for (const QString &path : paths)
    {
        if (!path.isEmpty())
        {
            openFile(path);
        }
    }
}

void MainWindow::closeDocumentTab(int index)
{
    Document *document = documentAt(index);

    if (document == nullptr)
    {
        return;
    }

    if (document != m_document)
    {
        switchToDocument(index);
    }

    if (!maybeSave())
    {
        return;
    }

    disconnect(document, nullptr, this, nullptr);

    if (document == m_document)
    {
        m_document = nullptr;
    }

    m_documents.removeAt(index);
    m_fileTabs->removeTab(index);
    document->deleteLater();

    if (m_documents.isEmpty())
    {
        close();
        return;
    }

    const int next = qBound(0, m_fileTabs->currentIndex(), m_documents.size() - 1);
    m_fileTabs->setCurrentIndex(next);
    switchToDocument(next);
}

bool MainWindow::openFile(const QString &path)
{
    if (!maybeSave())
    {
        return false;
    }

    const bool blank = m_document != nullptr && m_document->filePath().isEmpty()
        && m_document->pageCount() == 1 && !m_document->isModified();

    if (!blank)
    {
        switchToDocument(addDocumentTab(new Document(this)));
    }

    QString error;

    if (!m_document->load(path, &error))
    {
        QMessageBox::critical(this, QStringLiteral("Open Failed"),
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
    showStatusMessage(I18n::text(QStringLiteral("Opened: %1")).arg(path));
    return true;
}

void MainWindow::openRecent(const QString &path)
{
    if (!QFileInfo::exists(path))
    {
        AppSettings::instance().removeRecentFile(path);
        refreshRecentMenu();
        QMessageBox::information(this, QStringLiteral("File Not Found"),
                                 I18n::text(QStringLiteral("File not found:\n%1")).arg(path));
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
        QAction *empty = m_recentMenu->addAction(QStringLiteral("(None)"));
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
    QAction *clear = m_recentMenu->addAction(QStringLiteral("Clear List"));
    connect(clear, &QAction::triggered, this, [this]()
    {
        AppSettings::instance().clearRecentFiles();
        refreshRecentMenu();
    });
}

void MainWindow::newDocument()
{
    if (m_fileTabs == nullptr)
    {
        return;
    }

    Document *document = new Document(this);
    switchToDocument(addDocumentTab(document));
    m_view->undoStack()->clear();
    m_view->refreshFromDocument();
    m_view->fitToWindow();
    updateTitle();
    updatePageTabs();
    updateActions();
    watchCurrentFile(false);
    showStatusMessage(QStringLiteral("New Drawing"));
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
        QMessageBox::critical(this, QStringLiteral("Export Failed"), result.error);
        return;
    }

    QFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        QMessageBox::critical(this, QStringLiteral("Export Failed"),
                              I18n::text(QStringLiteral("Cannot write: %1\n%2")).arg(path, file.errorString()));
        return;
    }

    if (file.write(result.data) != result.data.size())
    {
        QMessageBox::critical(this, QStringLiteral("Export Failed"),
                              I18n::text(QStringLiteral("Failed to write file: %1")).arg(path));
        return;
    }

    file.close();

    AppSettings::instance().setLastExportDir(QFileInfo(path).absolutePath());
    AppSettings::instance().sync();
    showStatusMessage(I18n::text(QStringLiteral("Exported: %1")).arg(path));
}

void MainWindow::insertImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Insert Image"), AppSettings::instance().lastOpenDir(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.svg *.gif *.webp);;All Files (*)"));

    if (path.isEmpty())
    {
        return;
    }

    QImage image(path);

    if (image.isNull())
    {
        QMessageBox::warning(this, QStringLiteral("Insert Image"),
                             I18n::text(QStringLiteral("Cannot read image: %1")).arg(path));
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

    m_view->beginSnapshot(QStringLiteral("Insert Image"));
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

    m_view->beginSnapshot(QStringLiteral("Page Setup"));
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
    dialog.setWindowTitle(QStringLiteral("Print"));

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
    preview.setWindowTitle(QStringLiteral("Print Preview"));
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
        QMessageBox::information(this, QStringLiteral("Delete Page"),
                                 QStringLiteral("A document must keep at least one page."));
        return;
    }

    const Page *page = m_document->page(index);

    if (page == nullptr)
    {
        return;
    }

    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Delete Page"));
    box.setText(I18n::text(QStringLiteral("Delete page “%1”?")).arg(page->name));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.button(QMessageBox::Yes)->setText(QStringLiteral("Delete"));
    box.button(QMessageBox::No)->setText(QStringLiteral("Cancel"));

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
    const QString name = QInputDialog::getText(this, QStringLiteral("Rename Page"),
                                               QStringLiteral("Page name:"), QLineEdit::Normal,
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
    const bool dark = theme == QLatin1String("dark")
        || (theme != QLatin1String("light") && UiTheme::isDarkScheme());

    m_view->setDarkTheme(dark);
    UiTheme::applyColorScheme(theme);

    if (m_formatPanel != nullptr)
    {
        m_formatPanel->applyTheme(dark);
    }

    updateActionIcons();
    updateTabStyle();

    AppSettings::instance().setTheme(theme);
}

void MainWindow::updateActionIcons()
{
    if (m_view == nullptr)
    {
        return;
    }

    const QVector<QPair<QAction *, QString>> icons = {
        {m_newAction, QStringLiteral("new")},
        {m_openAction, QStringLiteral("open")},
        {m_saveAction, QStringLiteral("save")},
        {m_exportAction, QStringLiteral("export")},
        {m_printAction, QStringLiteral("print")},
        {m_undoAction, QStringLiteral("undo")},
        {m_redoAction, QStringLiteral("redo")},
        {m_deleteAction, QStringLiteral("delete")},
        {m_zoomInAction, QStringLiteral("zoom-in")},
        {m_zoomOutAction, QStringLiteral("zoom-out")},
        {m_fitAction, QStringLiteral("fit")},
        {m_gridAction, QStringLiteral("grid")},
        {m_groupAction, QStringLiteral("group")},
        {m_ungroupAction, QStringLiteral("ungroup")},
        {m_selectToolAction, QStringLiteral("select")},
        {m_panToolAction, QStringLiteral("pan")},
        {m_connectorAction, QStringLiteral("connector")},
        {m_textAction, QStringLiteral("text")},
    };

    for (const auto &entry : icons)
    {
        if (entry.first != nullptr)
        {
            entry.first->setIcon(QIcon(QStringLiteral(":/icons/%1.svg").arg(entry.second)));
        }
    }
}

void MainWindow::updateTabStyle()
{
    if (m_pageTabs == nullptr)
    {
        return;
    }

    const bool dark = m_view != nullptr && m_view->isDarkTheme();
    const QString closeIcon = dark ? QStringLiteral(":/icons/tab-close-dark.svg")
                                   : QStringLiteral(":/icons/tab-close.svg");

    m_pageTabs->setStyleSheet(
        QStringLiteral("QTabBar::tab { padding: 3px 14px; margin-right: 2px; }"
                       "QTabBar::close-button { image: url(%1);"
                       " subcontrol-position: right; margin-left: 4px; }"
                       "QTabBar::close-button:hover { background: rgba(127,127,127,0.25);"
                       " border-radius: 2px; }")
            .arg(closeIcon));

    if (m_fileTabs != nullptr)
    {
        QWidget *container = m_fileTabs->parentWidget();

        if (container != nullptr)
        {
            container->setStyleSheet(
                dark ? QStringLiteral("#fileTabContainer { background: #1a1d21;"
                                      " border-bottom: 1px solid #33383f; }")
                     : QStringLiteral("#fileTabContainer { background: #f3f5f8;"
                                      " border-bottom: 1px solid #dde2e8; }"));
        }

        const QString tabStyle = dark
            ? QStringLiteral(
                  "QTabBar::tab { background: #262b31; color: #9aa4b0;"
                  " border: 1px solid #3a4048; border-bottom: none;"
                  " border-top-left-radius: 8px; border-top-right-radius: 8px;"
                  " padding: 8px 20px; margin: 6px 3px 0 0; min-width: 110px; }"
                  "QTabBar::tab:selected { background: #1e2227; color: #8ab4f8;"
                  " border-color: #4a5563; font-weight: 600; }"
                  "QTabBar::tab:hover:!selected { background: #2d333a; color: #c7d0da; }"
                  "QTabBar::close-button { image: url(%1); subcontrol-position: right;"
                  " margin-left: 8px; }"
                  "QTabBar::close-button:hover { background: rgba(255,255,255,0.12);"
                  " border-radius: 3px; }"
                  "#fileTabAdd { border: none; border-radius: 4px; color: #9aa4b0;"
                  " font-size: 17px; margin-top: 5px; }"
                  "#fileTabAdd:hover { background: #2d333a; color: #ffffff; }")
            : QStringLiteral(
                  "QTabBar::tab { background: #e9edf3; color: #5b6573;"
                  " border: 1px solid #d8dee6; border-bottom: none;"
                  " border-top-left-radius: 8px; border-top-right-radius: 8px;"
                  " padding: 8px 20px; margin: 6px 3px 0 0; min-width: 110px; }"
                  "QTabBar::tab:selected { background: #ffffff; color: #1a73e8;"
                  " border-color: #d8dee6; font-weight: 600; }"
                  "QTabBar::tab:hover:!selected { background: #dfe5ee; color: #374151; }"
                  "QTabBar::close-button { image: url(%1); subcontrol-position: right;"
                  " margin-left: 8px; }"
                  "QTabBar::close-button:hover { background: rgba(0,0,0,0.08);"
                  " border-radius: 3px; }"
                  "#fileTabAdd { border: none; border-radius: 4px; color: #5b6573;"
                  " font-size: 17px; margin-top: 5px; }"
                  "#fileTabAdd:hover { background: #dfe5ee; color: #1a73e8; }");

        m_fileTabs->setStyleSheet(tabStyle.arg(closeIcon));
    }
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
    box.setWindowTitle(QStringLiteral("Autosave Found"));
    box.setText(QStringLiteral("An autosave from the previous session was found. Recover it?"));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.button(QMessageBox::Yes)->setText(QStringLiteral("Restore"));
    box.button(QMessageBox::No)->setText(QStringLiteral("Ignore"));

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
        showStatusMessage(QStringLiteral("Recovered autosaved content"));
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

        QAction *edit = menu.addAction(QStringLiteral("Edit Text"));
        connect(edit, &QAction::triggered, this, [this]()
        {
            m_view->startEditingSelection();
        });

        QAction *connectAction = menu.addAction(QStringLiteral("Connect Selected Shapes"));
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
    box.setWindowTitle(QStringLiteral("Unsaved Changes"));
    box.setText(I18n::text(QStringLiteral("Document “%1” has unsaved changes. Save it?"))
                    .arg(m_document->displayName()));
    box.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    box.button(QMessageBox::Save)->setText(QStringLiteral("Save"));
    box.button(QMessageBox::Discard)->setText(QStringLiteral("Don't Save"));
    box.button(QMessageBox::Cancel)->setText(QStringLiteral("Cancel"));
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
    if (!m_documents.isEmpty())
    {
        for (int index = 0; index < m_documents.size(); ++index)
        {
            switchToDocument(index);

            if (!maybeSave())
            {
                event->ignore();
                return;
            }
        }
    }

    AppSettings::instance().setWindowGeometry(saveGeometry());
    AppSettings::instance().setWindowState(saveState());
    AppSettings::instance().sync();

    if (m_autoSaveTimer != nullptr)
    {
        m_autoSaveTimer->stop();
    }

    if (m_document != nullptr)
    {
        watchCurrentFile(false);
    }

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
    QStringList paths;

    for (const QUrl &url : event->mimeData()->urls())
    {
        if (url.isLocalFile())
        {
            paths.append(url.toLocalFile());
        }
    }

    if (!paths.isEmpty())
    {
        openFiles(paths);
        event->acceptProposedAction();
    }
}

void MainWindow::showStatusMessage(const QString &message, int timeout)
{
    statusBar()->showMessage(message, timeout);
}

} // namespace dq
