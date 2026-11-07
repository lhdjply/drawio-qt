#pragma once

#include "core/Document.h"
#include "render/ShapeRegistry.h"

#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QWidget>

class QUndoStack;

namespace dq {

class OverlayEditor;

/// The diagram canvas. Paints the page through DiagramRenderer and implements
/// selection, moving, resizing, connector creation, inline label editing,
/// clipboard, z-order and undo/redo with page snapshots.
class DiagramView : public QWidget
{
    Q_OBJECT

public:
    enum class Tool
    {
        Select,
        Pan,
        Connector,
        Text
    };

    enum class Align
    {
        Left,
        CenterHorizontal,
        Right,
        Top,
        CenterVertical,
        Bottom
    };

    enum class SizeAction
    {
        SameWidth,
        SameHeight,
        SameSize
    };

    enum class ZOrder
    {
        Front,
        Back,
        Forward,
        Backward
    };

    explicit DiagramView(QWidget *parent = nullptr);
    ~DiagramView() override;

    void setDocument(Document *document);
    Document *document() const { return m_document; }
    Page *page() const { return m_page; }
    void refreshFromDocument();

    // --- view state ------------------------------------------------------
    double zoom() const { return m_zoom; }
    void setZoom(double zoom);
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitToWindow();
    void zoomToSelection();

    QPointF pan() const { return m_pan; }
    void centerOn(const QPointF &scenePoint);

    void setGridVisible(bool visible);
    bool isGridVisible() const { return m_gridVisible; }
    void setPageVisible(bool visible);
    bool isPageVisible() const { return m_pageVisible; }
    void setGuidesVisible(bool visible);
    bool guidesVisible() const { return m_guidesVisible; }
    void setDarkTheme(bool dark);
    bool isDarkTheme() const { return m_dark; }

    void setTool(Tool tool);
    Tool tool() const { return m_tool; }

    // --- selection -------------------------------------------------------
    QSet<QString> selection() const { return m_selection; }
    void setSelection(const QSet<QString> &ids);
    void selectCell(const QString &id, bool additive = false);
    void selectAll();
    void clearSelection();
    QStringList selectedCells() const;
    QVector<Cell *> selectedCellObjects() const;
    QRectF selectionBounds() const;
    bool hasSelection() const { return !m_selection.isEmpty(); }

    // --- editing ---------------------------------------------------------
    void deleteSelection();
    void copySelection();
    void cutSelection();
    void paste();
    void duplicateSelection();

    void moveSelectionBy(const QPointF &delta, bool snap = false);
    void alignSelection(Align align);
    void distributeHorizontally();
    void distributeVertically();
    void resizeSelection(SizeAction action);
    void changeZOrder(ZOrder order);
    void groupSelection();
    void ungroupSelection();

    void startEditing(const QString &cellId, bool selectAll = true);
    void startEditingSelection();
    void commitEditing();
    void cancelEditing();
    bool isEditing() const;

    void setCellValue(const QString &cellId, const QString &value);
    void applyStyle(const QString &key, const QString &value);
    void clearStyle(const QString &key);
    void applyGeometry(double x, double y, double width, double height);
    void setRotation(double degrees);

    Cell *addShape(const ShapeDefinition &definition, const QPointF &scenePos);
    Cell *addText(const QPointF &scenePos);
    Cell *addEdgeFromSelection();

    // --- undo ------------------------------------------------------------
    QUndoStack *undoStack() const { return m_undoStack; }
    void beginSnapshot(const QString &text);
    void commitSnapshot();
    void cancelSnapshot();
    /// Used by undo commands.
    void restoreSnapshot(int pageIndex, const QString &xml, const QSet<QString> &selection);

    // --- geometry helpers ------------------------------------------------
    QVector<QPointF> connectionPoints(const Cell &cell) const;
    int nearestConnectionSide(const Cell &cell, const QPointF &scenePoint) const;

    QPointF toScene(const QPointF &viewPoint) const;
    QPointF toView(const QPointF &scenePoint) const;
    QRectF sceneRect(const QRectF &viewRect) const;
    Cell *cellAt(const QPointF &scenePoint, bool includeEdges = true) const;
    Cell *topLevelSelectable(Cell *cell) const;

signals:
    void selectionChanged();
    void zoomChanged(double zoom);
    void cursorPositionChanged(const QPointF &scenePoint);
    void modified();
    void editStarted();
    void editFinished();
    void statusMessage(const QString &message);
    void contextMenuRequested(const QPoint &globalPos, const QString &cellId);
    void filesDropped(const QStringList &paths);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private:
    enum class DragMode
    {
        None,
        Move,
        Resize,
        Marquee,
        Pan,
        Connect
    };

    void paintSelectionOverlay(QPainter &painter);
    void paintConnectionPreview(QPainter &painter);

    struct Guide
    {
        bool visible = false;
        double position = 0.0;
        double from = 0.0;
        double to = 0.0;
    };

    struct SnapResult
    {
        bool found = false;
        double offset = 0.0;
        double position = 0.0;
        double from = 0.0;
        double to = 0.0;
    };

    SnapResult findVerticalSnap(const QRectF &moving, double threshold) const;
    SnapResult findHorizontalSnap(const QRectF &moving, double threshold) const;
    SnapResult findEdgeSnap(double value, bool vertical, const QRectF &target,
                            double threshold) const;
    bool isCellMoving(const Cell *cell) const;
    void clearGuides();
    void paintGuides(QPainter &painter);
    QVector<QRectF> handleRects() const;
    int handleAt(const QPointF &viewPoint) const;
    QRectF selectionBoundsInView() const;

    void applySnapshot(int pageIndex, const QString &xml, const QSet<QString> &selection);
    void pushSnapshotCommand(const QString &before, const QString &after,
                             const QSet<QString> &beforeSelection,
                             const QSet<QString> &afterSelection,
                             const QString &text);

    void beginMove(const QPointF &scenePoint);
    void beginResize(int handle, const QPointF &scenePoint);
    void beginMarquee(const QPointF &scenePoint);
    void beginPan(const QPointF &viewPoint);
    void beginConnect(Cell *source, const QPointF &scenePoint);

    void updateMove(const QPointF &scenePoint, bool constrain);
    void updateResize(const QPointF &scenePoint, bool keepRatio, bool disableSnap);
    void updateConnect(const QPointF &scenePoint, bool snap);
    void finishDrag();

    void deleteSelectedInternal();
    void cloneSelectionTo(Page &target, const QVector<Cell *> &sourceCells,
                          QHash<QString, QString> *idMap, double offsetX, double offsetY) const;
    QString selectionAsPageXml() const;
    bool pastePageXml(const QString &xml, double offsetX, double offsetY, bool select);

    void notifyModified();
    void updateCursorForPosition(const QPointF &viewPoint);

    Document *m_document = nullptr;
    Page *m_page = nullptr;

    double m_zoom = 1.0;
    QPointF m_pan;
    bool m_gridVisible = true;
    bool m_pageVisible = true;
    bool m_guidesVisible = true;
    Guide m_verticalGuide;
    Guide m_horizontalGuide;
    bool m_dark = false;
    Tool m_tool = Tool::Select;

    QSet<QString> m_selection;
    QString m_hoverCellId;

    QUndoStack *m_undoStack = nullptr;
    QString m_pendingSnapshot;
    QString m_pendingSnapshotText;
    QSet<QString> m_pendingSnapshotSelection;
    bool m_snapshotOpen = false;

    // Drag state
    DragMode m_dragMode = DragMode::None;
    QPointF m_dragStartScene;
    QRectF m_dragStartBounds;
    QHash<QString, QRectF> m_dragStartGeometry;
    QHash<QString, QVector<QPointF>> m_dragStartWaypoints;
    QHash<QString, QPointF> m_dragStartPoints;
    QRectF m_marquee;
    int m_resizeHandle = -1;
    bool m_dragMoved = false;
    QString m_dragBeforeXml;
    QSet<QString> m_dragBeforeSelection;

    QString m_connectSourceId;
    QString m_connectTargetId;
    QPointF m_connectStart;
    QPointF m_connectCurrent;
    int m_connectSourceSide = -1;
    int m_connectTargetSide = -1;
    bool m_connectFromSource = true;

    QPointF m_panStart;
    QPointF m_panOrigin;
    bool m_spacePressed = false;

    OverlayEditor *m_editor = nullptr;
    QString m_editorCellId;
    QString m_editorBeforeXml;

    QString m_clipboardXml;
};

} // namespace dq
