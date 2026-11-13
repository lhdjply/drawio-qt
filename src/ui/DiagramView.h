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

struct RoutedEdge;

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
    /// Cells a rubber band over \a sceneMarquee would select (top level cells
    /// only; transparentBounds containers use their derived box).
    QSet<QString> cellsInMarquee(const QRectF &sceneMarquee) const;
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

    /// While a label is being edited, applies bold/italic/underline to the
    /// selected text instead of the whole cell. Returns false when the editor
    /// is not open (the caller then falls back to the cell style).
    bool applyEditorFontStyle(int bit, bool enabled);
    bool applyEditorFontFamily(const QString &family);
    bool applyEditorFontSize(int pointSize);
    bool applyEditorFontColor(const QString &value);

    void setCellValue(const QString &cellId, const QString &value);
    void setCellLink(const QString &cellId, const QString &link);
    void applyStyle(const QString &key, const QString &value);
    void clearStyle(const QString &key);
    void applyGeometry(double x, double y, double width, double height);
    void setRotation(double degrees);

    // --- layers ----------------------------------------------------------
    void addLayer();
    void removeLayer(const QString &layerId);
    void renameLayer(const QString &layerId, const QString &name);
    void setActiveLayer(const QString &layerId);
    void setLayerVisible(const QString &layerId, bool visible);
    void moveLayer(const QString &layerId, int delta);
    void moveObject(const QString &cellId, int delta);
    void setLayerLocked(const QString &layerId, bool locked);
    bool isLayerLocked(const QString &layerId) const;

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
    /// Ctrl+clicked a cell that carries a hyperlink; the window opens it.
    void linkActivated(const QString &url);
    /// Emitted while a rich text label is edited, with the format at the cursor
    /// or selection so the format panel can mirror it.
    void editorCharFormatChanged(int pointSize, const QString &family,
                                 const QColor &color, int styleBits);
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
        Connect,
        Waypoint,
        Segment,
        Rotate
    };

    void paintSelectionOverlay(QPainter &painter);
    void paintConnectionPreview(QPainter &painter);

    QString layerIdForCell(const Cell *cell) const;
    int waypointAt(const QPointF &viewPoint, QString *edgeId) const;
    Cell *selectedEdgeAt(const QPointF &scenePoint, QPointF *projected) const;
    Cell *singleRotatedVertex() const;
    /// The rectangle the selection outline and handles use: the derived box
    /// for transparentBounds containers, the stored geometry otherwise.
    QRectF selectionRectFor(const Cell &cell) const;
    QPointF rotationHandleCenter() const;
    bool rotationHandleHit(const QPointF &viewPoint) const;
    void beginRotate(const QPointF &scenePoint);
    /// Remembers the starting geometry of a rotated cell's descendants so the
    /// rotation can be re-applied from scratch on every drag move.
    void collectRotatedDescendants(Cell *cell);
    /// Rotates the children of \a cell around its centre by \a angle, like
    /// mxVertexHandler.rotateCell().
    void rotateDescendants(Cell *cell, double angle);
    void beginSegmentDrag(Cell *edge, const RoutedEdge &route, int index,
                          const QPointF &a, const QPointF &b,
                          const QPointF &startScene);
    static double snapRotationAngle(double angle, bool strict);

    QString m_waypointEdgeId;
    int m_waypointIndex = -1;
    bool m_waypointAdded = false;
    QPointF m_segmentBaseA;
    QPointF m_segmentBaseB;
    int m_segmentIndexA = -1;
    int m_segmentIndexB = -1;
    QVector<QPointF> m_segmentOriginalWaypoints;
    QPointF m_rotateCenter;
    double m_rotateStartAngle = 0.0;
    QVector<QPair<QString, double>> m_rotateInitial;
    /// Starting geometry/rotation of every descendant of a rotated cell: a
    /// rotated container carries its children around its centre.
    QHash<QString, QRectF> m_rotateInitialGeometry;
    QHash<QString, double> m_rotateInitialRotation;
    bool m_rotating = false;
    QString m_resizeRotatedCellId;
    double m_resizeRotatedAngle = 0.0;

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

    /// draw.io transparentBounds containers (live layout / tree) derive their
    /// box from the children, so dragging them moves the children.
    bool isTransparentBoundsContainer(const Cell *cell) const;

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

    /// Size a container had while folded, so collapsing again reproduces it.
    QHash<QString, QRectF> m_foldedBounds;

    QString m_hoverCellId;
    /// Modifiers of the last hover event, so the cursor follows Ctrl presses
    /// that arrive without a mouse move.
    Qt::KeyboardModifiers m_hoverModifiers = Qt::NoModifier;

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
