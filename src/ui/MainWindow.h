#pragma once

#include "core/Page.h"

#include <QMainWindow>

class QAction;
class QActionGroup;
class QFileSystemWatcher;
class QLabel;
class QMenu;
class QPrinter;
class QTabBar;
class QTimer;

namespace dq {

class DiagramView;
class Document;
class FormatPanel;
class ShapeToolbox;

/// One editor window. Each window owns a Document, the canvas, the shape
/// palette, the format panel and the per-window actions, page tabs and
/// auto-save timer.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    bool openFile(const QString &path);
    void newDocument();

    Document *document() const { return m_document; }
    DiagramView *view() const { return m_view; }

    void openExportDialog(const QString &presetFormat = QString());

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createDocks();
    void createStatusBar();
    void createPageTabs();

    void updateTitle();
    void updateActions();
    void updatePageTabs();
    void updateStatus();

    bool maybeSave();
    bool saveTo(const QString &path);
    bool save();
    void saveAs();
    void openDialog();
    void openRecent(const QString &path);
    void refreshRecentMenu();
    void printDocument();
    void printPreview();
    void pageSetup();
    void insertImage();

    void addPage();
    void removePage(int index);
    void renamePage(int index);

    void applyTheme(const QString &theme);

    void watchCurrentFile(bool watch);
    void checkDraftRecovery();
    void writeDraft();
    void removeDraft();
    QString draftPathFor(const QString &path) const;

    void showCanvasContextMenu(const QPoint &globalPos, const QString &cellId);
    void showStatusMessage(const QString &message, int timeout = 4000);
    void renderToPrinter(QPrinter &printer);

    Document *m_document = nullptr;
    DiagramView *m_view = nullptr;
    ShapeToolbox *m_toolbox = nullptr;
    FormatPanel *m_formatPanel = nullptr;
    QTabBar *m_pageTabs = nullptr;
    QTimer *m_autoSaveTimer = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;
    QMenu *m_recentMenu = nullptr;
    QActionGroup *m_toolGroup = nullptr;

    QLabel *m_zoomLabel = nullptr;
    QLabel *m_positionLabel = nullptr;
    QLabel *m_selectionLabel = nullptr;

    QAction *m_newAction = nullptr;
    QAction *m_newWindowAction = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_saveAsAction = nullptr;
    QAction *m_exportAction = nullptr;
    QAction *m_printAction = nullptr;
    QAction *m_printPreviewAction = nullptr;
    QAction *m_pageSetupAction = nullptr;
    QAction *m_closeAction = nullptr;
    QAction *m_quitAction = nullptr;

    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;
    QAction *m_cutAction = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_pasteAction = nullptr;
    QAction *m_deleteAction = nullptr;
    QAction *m_duplicateAction = nullptr;
    QAction *m_selectAllAction = nullptr;

    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_resetZoomAction = nullptr;
    QAction *m_fitAction = nullptr;
    QAction *m_gridAction = nullptr;
    QAction *m_pageViewAction = nullptr;
    QAction *m_darkAction = nullptr;
    QAction *m_fullScreenAction = nullptr;
    QAction *m_toggleShapesAction = nullptr;
    QAction *m_toggleFormatAction = nullptr;

    QAction *m_groupAction = nullptr;
    QAction *m_ungroupAction = nullptr;
    QAction *m_frontAction = nullptr;
    QAction *m_backAction = nullptr;
    QAction *m_forwardAction = nullptr;
    QAction *m_backwardAction = nullptr;

    QAction *m_addPageAction = nullptr;
    QAction *m_textAction = nullptr;
    QAction *m_connectorAction = nullptr;
    QAction *m_selectToolAction = nullptr;
    QAction *m_panToolAction = nullptr;

};

} // namespace dq
