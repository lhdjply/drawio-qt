#include "ui/DiagramView.h"

#include "core/MxFileIO.h"
#include "render/DiagramRenderer.h"
#include "render/EdgeRenderer.h"
#include "render/EdgeRouter.h"
#include "render/ShapeRenderer.h"
#include "render/ShapeCatalog.h"
#include "render/ShapeRegistry.h"
#include "ui/OverlayEditor.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QUrl>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QTextDocument>
#include <QUndoCommand>
#include <QUndoStack>
#include <QWheelEvent>
#include <QtMath>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>

namespace dq {
namespace {

constexpr const char *kShapeMimeType = "application/x-drawio-shape";
constexpr double kMinZoom = 0.05;
constexpr double kMaxZoom = 8.0;
constexpr double kHandleSize = 8.0;

QString htmlEscape(const QString &text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    return escaped;
}

class PageSnapshotCommand : public QUndoCommand
{
public:
    PageSnapshotCommand(DiagramView *view, int pageIndex, const QString &before,
                        const QString &after, const QSet<QString> &beforeSelection,
                        const QSet<QString> &afterSelection, const QString &text)
        : QUndoCommand(text)
        , m_view(view)
        , m_pageIndex(pageIndex)
        , m_before(before)
        , m_after(after)
        , m_beforeSelection(beforeSelection)
        , m_afterSelection(afterSelection)
    {
    }

    void undo() override
    {
        m_view->restoreSnapshot(m_pageIndex, m_before, m_beforeSelection);
    }

    void redo() override
    {
        // The first push happens after the edit was already applied.
        if (m_firstRedo)
        {
            m_firstRedo = false;
            return;
        }

        m_view->restoreSnapshot(m_pageIndex, m_after, m_afterSelection);
    }

private:
    bool m_firstRedo = true;
    DiagramView *m_view;
    int m_pageIndex;
    QString m_before;
    QString m_after;
    QSet<QString> m_beforeSelection;
    QSet<QString> m_afterSelection;
};

} // namespace

DiagramView::DiagramView(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(320, 240);

    m_undoStack = new QUndoStack(this);
    m_undoStack->setUndoLimit(200);

    m_editor = new OverlayEditor(this);

    connect(m_editor, &OverlayEditor::committed, this, [this]()
    {
        commitEditing();
    });

    connect(m_editor, &OverlayEditor::cancelled, this, [this]()
    {
        cancelEditing();
    });
}

DiagramView::~DiagramView() = default;

void DiagramView::setDocument(Document *document)
{
    if (m_document == document)
    {
        return;
    }

    if (m_document != nullptr)
    {
        disconnect(m_document, nullptr, this, nullptr);
    }

    m_document = document;

    if (m_document != nullptr)
    {
        connect(m_document, &Document::contentChanged, this, [this]()
        {
            update();
        });

        connect(m_document, &Document::currentPageChanged, this, [this](int)
        {
            m_page = m_document->currentPage();
            m_selection.clear();
            m_editor->hide();
            update();
            emit selectionChanged();
        });

        connect(m_document, &Document::pagesChanged, this, [this]()
        {
            m_page = m_document->currentPage();
            update();
        });
    }

    m_page = m_document != nullptr ? m_document->currentPage() : nullptr;
    m_undoStack->clear();
    m_selection.clear();
    update();
}

void DiagramView::refreshFromDocument()
{
    m_page = m_document != nullptr ? m_document->currentPage() : nullptr;

    // Drop selection entries that no longer exist.
    if (m_page != nullptr)
    {
        for (auto it = m_selection.begin(); it != m_selection.end();)
        {
            if (m_page->cell(*it) == nullptr)
            {
                it = m_selection.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    else
    {
        m_selection.clear();
    }

    update();
    emit selectionChanged();
}

// --- view state ----------------------------------------------------------

void DiagramView::setZoom(double zoom)
{
    const double clamped = qBound(kMinZoom, zoom, kMaxZoom);

    if (qFuzzyCompare(clamped, m_zoom))
    {
        return;
    }

    // Keep the view centre stable while zooming.
    const QPointF centerView(width() / 2.0, height() / 2.0);
    const QPointF centerScene = toScene(centerView);

    m_zoom = clamped;
    m_pan = centerScene - centerView / m_zoom;

    update();
    emit zoomChanged(m_zoom);
}

void DiagramView::zoomIn()
{
    setZoom(m_zoom * 1.2);
}

void DiagramView::zoomOut()
{
    setZoom(m_zoom / 1.2);
}

void DiagramView::resetZoom()
{
    setZoom(1.0);
}

void DiagramView::fitToWindow()
{
    if (m_page == nullptr)
    {
        return;
    }

    RenderOptions options;
    options.grid = false;
    options.page = m_pageVisible;
    options.dark = m_dark;

    QRectF bounds = DiagramRenderer::diagramBounds(*m_page, options);

    if (bounds.isEmpty())
    {
        bounds = QRectF(0, 0, 800, 600);
    }

    bounds.adjust(-30, -30, 30, 30);

    const double zx = width() / bounds.width();
    const double zy = height() / bounds.height();
    m_zoom = qBound(kMinZoom, qMin(zx, zy), kMaxZoom);
    m_pan = bounds.center() - QPointF(width(), height()) / (2.0 * m_zoom);

    update();
    emit zoomChanged(m_zoom);
}

void DiagramView::zoomToSelection()
{
    if (!hasSelection())
    {
        fitToWindow();
        return;
    }

    QRectF bounds = selectionBounds().adjusted(-20, -20, 20, 20);

    if (bounds.isEmpty())
    {
        return;
    }

    const double zx = width() / bounds.width();
    const double zy = height() / bounds.height();
    m_zoom = qBound(kMinZoom, qMin(zx, zy), kMaxZoom);
    m_pan = bounds.center() - QPointF(width(), height()) / (2.0 * m_zoom);

    update();
    emit zoomChanged(m_zoom);
}

void DiagramView::centerOn(const QPointF &scenePoint)
{
    m_pan = scenePoint - QPointF(width(), height()) / (2.0 * m_zoom);
    update();
}

void DiagramView::setGridVisible(bool visible)
{
    m_gridVisible = visible;
    update();
}

void DiagramView::setPageVisible(bool visible)
{
    m_pageVisible = visible;
    update();
}

void DiagramView::setGuidesVisible(bool visible)
{
    m_guidesVisible = visible;

    if (!visible)
    {
        clearGuides();
    }

    update();
}

bool DiagramView::isCellMoving(const Cell *cell) const
{
    if (cell == nullptr)
    {
        return false;
    }

    if (m_selection.contains(cell->id))
    {
        return true;
    }

    for (const Cell *parent = cell->parent; parent != nullptr; parent = parent->parent)
    {
        if (m_selection.contains(parent->id))
        {
            return true;
        }
    }

    return false;
}

DiagramView::SnapResult DiagramView::findVerticalSnap(const QRectF &moving,
                                                     double threshold) const
{
    SnapResult best;

    if (m_page == nullptr)
    {
        return best;
    }

    const double movingValues[3] = {moving.left(), moving.center().x(), moving.right()};

    for (Cell *cell : m_page->cells())
    {
        if (!cell->isVertex() || !cell->visible || isCellMoving(cell))
        {
            continue;
        }

        const QRectF other = cell->absoluteRect();

        if (other.isEmpty())
        {
            continue;
        }

        const double otherValues[3] = {other.left(), other.center().x(), other.right()};

        for (const double movingValue : movingValues)
        {
            for (const double otherValue : otherValues)
            {
                const double difference = otherValue - movingValue;

                if (qAbs(difference) <= threshold
                    && (!best.found || qAbs(difference) < qAbs(best.offset)))
                {
                    best.found = true;
                    best.offset = difference;
                    best.position = otherValue;
                    best.from = qMin(moving.top(), other.top()) - 12.0;
                    best.to = qMax(moving.bottom(), other.bottom()) + 12.0;
                }
            }
        }
    }

    return best;
}

DiagramView::SnapResult DiagramView::findEdgeSnap(double value, bool vertical,
                                                  const QRectF &target,
                                                  double threshold) const
{
    SnapResult best;

    if (m_page == nullptr)
    {
        return best;
    }

    for (Cell *cell : m_page->cells())
    {
        if (!cell->isVertex() || !cell->visible || isCellMoving(cell))
        {
            continue;
        }

        const QRectF other = cell->absoluteRect();

        if (other.isEmpty())
        {
            continue;
        }

        const double values[3] = {
            vertical ? other.left() : other.top(),
            vertical ? other.center().x() : other.center().y(),
            vertical ? other.right() : other.bottom(),
        };

        for (const double candidate : values)
        {
            const double difference = candidate - value;

            if (qAbs(difference) <= threshold
                && (!best.found || qAbs(difference) < qAbs(best.offset)))
            {
                best.found = true;
                best.offset = difference;
                best.position = candidate;

                if (vertical)
                {
                    best.from = qMin(target.top(), other.top()) - 12.0;
                    best.to = qMax(target.bottom(), other.bottom()) + 12.0;
                }
                else
                {
                    best.from = qMin(target.left(), other.left()) - 12.0;
                    best.to = qMax(target.right(), other.right()) + 12.0;
                }
            }
        }
    }

    return best;
}

DiagramView::SnapResult DiagramView::findHorizontalSnap(const QRectF &moving,
                                                       double threshold) const
{
    SnapResult best;

    if (m_page == nullptr)
    {
        return best;
    }

    const double movingValues[3] = {moving.top(), moving.center().y(), moving.bottom()};

    for (Cell *cell : m_page->cells())
    {
        if (!cell->isVertex() || !cell->visible || isCellMoving(cell))
        {
            continue;
        }

        const QRectF other = cell->absoluteRect();

        if (other.isEmpty())
        {
            continue;
        }

        const double otherValues[3] = {other.top(), other.center().y(), other.bottom()};

        for (const double movingValue : movingValues)
        {
            for (const double otherValue : otherValues)
            {
                const double difference = otherValue - movingValue;

                if (qAbs(difference) <= threshold
                    && (!best.found || qAbs(difference) < qAbs(best.offset)))
                {
                    best.found = true;
                    best.offset = difference;
                    best.position = otherValue;
                    best.from = qMin(moving.left(), other.left()) - 12.0;
                    best.to = qMax(moving.right(), other.right()) + 12.0;
                }
            }
        }
    }

    return best;
}

void DiagramView::clearGuides()
{
    m_verticalGuide = Guide{};
    m_horizontalGuide = Guide{};
}

void DiagramView::setDarkTheme(bool dark)
{
    m_dark = dark;
    update();
}

void DiagramView::setTool(Tool tool)
{
    m_tool = tool;

    switch (tool)
    {
    case Tool::Pan:
        setCursor(Qt::OpenHandCursor);
        break;
    case Tool::Connector:
        setCursor(Qt::CrossCursor);
        break;
    case Tool::Text:
        setCursor(Qt::IBeamCursor);
        break;
    case Tool::Select:
        setCursor(Qt::ArrowCursor);
        break;
    }
}

// --- coordinate helpers --------------------------------------------------

QPointF DiagramView::toScene(const QPointF &viewPoint) const
{
    return viewPoint / m_zoom + m_pan;
}

QPointF DiagramView::toView(const QPointF &scenePoint) const
{
    return (scenePoint - m_pan) * m_zoom;
}

QVector<QPointF> DiagramView::connectionPoints(const Cell &cell) const
{
    const QRectF rect = cell.absoluteRect();

    return {
        QPointF(rect.center().x(), rect.top()),
        QPointF(rect.right(), rect.center().y()),
        QPointF(rect.center().x(), rect.bottom()),
        QPointF(rect.left(), rect.center().y()),
    };
}

int DiagramView::nearestConnectionSide(const Cell &cell, const QPointF &scenePoint) const
{
    const QVector<QPointF> points = connectionPoints(cell);

    int best = -1;
    double bestDistance = std::numeric_limits<double>::max();

    for (int i = 0; i < points.size(); ++i)
    {
        const double distance = QLineF(points.at(i), scenePoint).length();

        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = i;
        }
    }

    return best;
}

QRectF DiagramView::sceneRect(const QRectF &viewRect) const
{
    return QRectF(toScene(viewRect.topLeft()), toScene(viewRect.bottomRight()));
}

Cell *DiagramView::topLevelSelectable(Cell *cell) const
{
    if (cell == nullptr)
    {
        return nullptr;
    }

    Cell *current = cell;

    while (current->parent != nullptr)
    {
        current = current->parent;
    }

    return current;
}

Cell *DiagramView::cellAt(const QPointF &scenePoint, bool includeEdges) const
{
    if (m_page == nullptr)
    {
        return nullptr;
    }

    const QVector<Cell *> cells = m_page->cells();

    if (includeEdges)
    {
        for (int i = cells.size() - 1; i >= 0; --i)
        {
            Cell *cell = cells.at(i);

            if (!cell->isEdge() || !cell->visible)
            {
                continue;
            }

            const RoutedEdge route = EdgeRouter::route(*m_page, *cell);

            for (int p = 1; p < route.points.size(); ++p)
            {
                const QLineF segment(route.points.at(p - 1), route.points.at(p));

                if (segment.length() <= 0.0)
                {
                    continue;
                }

                // Distance from the click to the segment.
                const QPointF a = segment.p1();
                const QPointF b = segment.p2();
                const QPointF ab = b - a;
                const double lengthSquared = ab.x() * ab.x() + ab.y() * ab.y();
                const double t = lengthSquared <= 0.0 ? 0.0
                    : qBound(0.0, ((scenePoint - a).x() * ab.x()
                                   + (scenePoint - a).y() * ab.y()) / lengthSquared, 1.0);
                const QPointF closest = a + ab * t;
                const double distance = QLineF(closest, scenePoint).length();

                if (distance <= qMax(4.0, 6.0 / m_zoom))
                {
                    return cell;
                }
            }
        }
    }

    for (int i = cells.size() - 1; i >= 0; --i)
    {
        Cell *cell = cells.at(i);

        if (!cell->isVertex() || !cell->visible)
        {
            continue;
        }

        if (cell->absoluteRect().contains(scenePoint))
        {
            return cell;
        }
    }

    return nullptr;
}

// --- selection -----------------------------------------------------------

void DiagramView::setSelection(const QSet<QString> &ids)
{
    m_selection = ids;
    update();
    emit selectionChanged();
}

void DiagramView::selectCell(const QString &id, bool additive)
{
    if (!additive)
    {
        m_selection.clear();
    }

    if (m_selection.contains(id))
    {
        if (additive)
        {
            m_selection.remove(id);
        }
    }
    else
    {
        m_selection.insert(id);
    }

    update();
    emit selectionChanged();
}

void DiagramView::selectAll()
{
    if (m_page == nullptr)
    {
        return;
    }

    m_selection.clear();

    for (Cell *cell : m_page->cells())
    {
        if (cell->visible)
        {
            m_selection.insert(cell->id);
        }
    }

    update();
    emit selectionChanged();
}

void DiagramView::clearSelection()
{
    if (m_selection.isEmpty())
    {
        return;
    }

    m_selection.clear();
    update();
    emit selectionChanged();
}

QStringList DiagramView::selectedCells() const
{
    QStringList result = m_selection.values();
    result.sort();
    return result;
}

QVector<Cell *> DiagramView::selectedCellObjects() const
{
    QVector<Cell *> result;

    if (m_page == nullptr)
    {
        return result;
    }

    for (const QString &id : m_selection)
    {
        if (Cell *cell = m_page->cell(id))
        {
            result.append(cell);
        }
    }

    return result;
}

QRectF DiagramView::selectionBounds() const
{
    if (m_page == nullptr)
    {
        return {};
    }

    QRectF bounds;
    bool first = true;

    for (const QString &id : m_selection)
    {
        Cell *cell = m_page->cell(id);

        if (cell == nullptr)
        {
            continue;
        }

        QRectF rect;

        if (cell->isEdge())
        {
            rect = EdgeRenderer::boundsFor(EdgeRouter::route(*m_page, *cell), *cell);
        }
        else
        {
            rect = cell->absoluteRect();
        }

        bounds = first ? rect : bounds.united(rect);
        first = false;
    }

    return bounds;
}

QRectF DiagramView::selectionBoundsInView() const
{
    const QRectF bounds = selectionBounds();
    return QRectF(toView(bounds.topLeft()), toView(bounds.bottomRight()));
}

QVector<QRectF> DiagramView::handleRects() const
{
    const QRectF bounds = selectionBoundsInView();
    const double half = kHandleSize / 2.0;

    const QVector<QPointF> centers = {
        bounds.topLeft(),
        QPointF(bounds.center().x(), bounds.top()),
        bounds.topRight(),
        QPointF(bounds.right(), bounds.center().y()),
        bounds.bottomRight(),
        QPointF(bounds.center().x(), bounds.bottom()),
        bounds.bottomLeft(),
        QPointF(bounds.left(), bounds.center().y()),
    };

    QVector<QRectF> rects;

    for (const QPointF &center : centers)
    {
        rects.append(QRectF(center.x() - half, center.y() - half, kHandleSize, kHandleSize));
    }

    return rects;
}

int DiagramView::handleAt(const QPointF &viewPoint) const
{
    const QVector<QRectF> rects = handleRects();

    for (int i = 0; i < rects.size(); ++i)
    {
        if (rects.at(i).adjusted(-2, -2, 2, 2).contains(viewPoint))
        {
            return i;
        }
    }

    return -1;
}

// --- painting ------------------------------------------------------------

void DiagramView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    painter.fillRect(rect(), DiagramRenderer::canvasColor(m_dark));

    if (m_page == nullptr)
    {
        return;
    }

    painter.save();
    painter.scale(m_zoom, m_zoom);
    painter.translate(-m_pan);

    RenderOptions options;
    options.grid = m_gridVisible;
    options.page = m_pageVisible;
    options.dark = m_dark;
    options.background = Qt::transparent;

    DiagramRenderer::paint(painter, *m_page, options);
    painter.restore();

    paintGuides(painter);
    paintSelectionOverlay(painter);
    paintConnectionPreview(painter);
}

void DiagramView::paintGuides(QPainter &painter)
{
    if (!m_guidesVisible
        || (!m_verticalGuide.visible && !m_horizontalGuide.visible))
    {
        return;
    }

    painter.save();
    QPen pen(QColor(0x1A, 0x73, 0xE8, 210));
    pen.setStyle(Qt::DashLine);
    pen.setWidthF(1.0);
    painter.setPen(pen);

    const auto drawGuide = [this, &painter, &pen](const Guide &guide, bool vertical)
    {
        if (!guide.visible)
        {
            return;
        }

        QPointF start;
        QPointF end;

        if (vertical)
        {
            start = toView(QPointF(guide.position, guide.from));
            end = toView(QPointF(guide.position, guide.to));
        }
        else
        {
            start = toView(QPointF(guide.from, guide.position));
            end = toView(QPointF(guide.to, guide.position));
        }

        painter.drawLine(start, end);

        // Small perpendicular ticks make the guide readable over dark shapes.
        constexpr double tick = 4.0;
        painter.setPen(QPen(QColor(0x1A, 0x73, 0xE8, 230), 1.4));

        if (vertical)
        {
            painter.drawLine(start + QPointF(-tick, 0), start + QPointF(tick, 0));
            painter.drawLine(end + QPointF(-tick, 0), end + QPointF(tick, 0));
        }
        else
        {
            painter.drawLine(start + QPointF(0, -tick), start + QPointF(0, tick));
            painter.drawLine(end + QPointF(0, -tick), end + QPointF(0, tick));
        }

        QPen dashed = pen;
        painter.setPen(dashed);
    };

    drawGuide(m_verticalGuide, true);
    drawGuide(m_horizontalGuide, false);
    painter.restore();
}

void DiagramView::paintSelectionOverlay(QPainter &painter)
{
    if (m_page == nullptr)
    {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor accent(0x1A, 0x73, 0xE8);

    // Outline every selected cell.
    for (const QString &id : m_selection)
    {
        Cell *cell = m_page->cell(id);

        if (cell == nullptr)
        {
            continue;
        }

        if (cell->isEdge())
        {
            const RoutedEdge route = EdgeRouter::route(*m_page, *cell);
            QPainterPath path;
            path.moveTo(toView(route.points.first()));

            for (int i = 1; i < route.points.size(); ++i)
            {
                path.lineTo(toView(route.points.at(i)));
            }

            QPen pen(accent, 3.0);
            pen.setCapStyle(Qt::RoundCap);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
            continue;
        }

        const QRectF rect = cell->absoluteRect();
        const QRectF viewRect(toView(rect.topLeft()), toView(rect.bottomRight()));

        QPen pen(accent, 1.5);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(viewRect);
    }

    // Handles only when vertices are selected.
    bool hasVertex = false;

    for (const QString &id : m_selection)
    {
        if (Cell *cell = m_page->cell(id))
        {
            hasVertex = hasVertex || cell->isVertex();
        }
    }

    if (hasVertex)
    {
        const QVector<QRectF> rects = handleRects();
        painter.setPen(QPen(accent, 1.0));
        painter.setBrush(QColor(Qt::white));

        for (const QRectF &rect : rects)
        {
            painter.drawRect(rect);
        }
    }

    if (!m_selection.isEmpty())
    {
        const QRectF bounds = selectionBoundsInView();

        if (m_selection.size() > 1)
        {
            QPen dashed(accent, 1.0, Qt::DashLine);
            painter.setPen(dashed);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(bounds.adjusted(-1, -1, 1, 1));
        }
    }

    // Hover affordance: small connection handles around the hovered shape.
    const bool showHoverPoints = !m_hoverCellId.isEmpty()
        && m_dragMode == DragMode::None
        && (m_tool == Tool::Connector
            || (m_tool == Tool::Select && !m_selection.contains(m_hoverCellId)));

    if (showHoverPoints)
    {
        if (Cell *cell = m_page->cell(m_hoverCellId))
        {
            if (cell->isVertex())
            {
                const QRectF rect = cell->absoluteRect();
                const QPointF centers[] = {
                    QPointF(rect.center().x(), rect.top()),
                    QPointF(rect.right(), rect.center().y()),
                    QPointF(rect.center().x(), rect.bottom()),
                    QPointF(rect.left(), rect.center().y()),
                };

                painter.setPen(Qt::NoPen);
                painter.setBrush(accent);

                for (const QPointF &center : centers)
                {
                    painter.drawEllipse(toView(center), 4.0, 4.0);
                }
            }
        }
    }

    // Marquee rectangle.
    if (m_dragMode == DragMode::Marquee && !m_marquee.isNull())
    {
        painter.setPen(QPen(accent, 1.0, Qt::DashLine));
        painter.setBrush(QColor(0x1A, 0x73, 0xE8, 30));
        painter.drawRect(m_marquee);
    }

    painter.restore();
}

void DiagramView::paintConnectionPreview(QPainter &painter)
{
    if (m_dragMode != DragMode::Connect || m_connectSourceId.isEmpty())
    {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(0x1A, 0x73, 0xE8), 1.6, Qt::DashLine));
    painter.drawLine(toView(m_connectStart), toView(m_connectCurrent));

    if (!m_connectTargetId.isEmpty() && m_page != nullptr)
    {
        if (Cell *target = m_page->cell(m_connectTargetId))
        {
            const QRectF rect = target->absoluteRect();
            painter.setPen(QPen(QColor(0x1A, 0x73, 0xE8), 1.6));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRectF(toView(rect.topLeft()), toView(rect.bottomRight())));

            const QVector<QPointF> anchors = connectionPoints(*target);

            for (int i = 0; i < anchors.size(); ++i)
            {
                const QPointF center = toView(anchors.at(i));
                const bool active = i == m_connectTargetSide;
                const double radius = active ? 6.0 : 4.5;
                painter.setPen(QPen(QColor(0x1A, 0x73, 0xE8), 1.5));
                painter.setBrush(active ? QColor(0x1A, 0x73, 0xE8) : QColor(Qt::white));
                painter.drawEllipse(center, radius, radius);
            }
        }
    }

    painter.restore();
}

// --- undo ----------------------------------------------------------------

void DiagramView::beginSnapshot(const QString &text)
{
    if (m_page == nullptr || m_snapshotOpen)
    {
        return;
    }

    m_pendingSnapshot = MxFileIO::pageToXml(*m_page, false);
    m_pendingSnapshotText = text;
    m_pendingSnapshotSelection = m_selection;
    m_snapshotOpen = true;
}

void DiagramView::commitSnapshot()
{
    if (!m_snapshotOpen || m_page == nullptr)
    {
        return;
    }

    m_snapshotOpen = false;

    const QString after = MxFileIO::pageToXml(*m_page, false);

    if (after == m_pendingSnapshot)
    {
        return;
    }

    pushSnapshotCommand(m_pendingSnapshot, after, m_pendingSnapshotSelection, m_selection,
                        m_pendingSnapshotText);
    notifyModified();
}

void DiagramView::cancelSnapshot()
{
    m_snapshotOpen = false;
    m_pendingSnapshot.clear();
}

void DiagramView::pushSnapshotCommand(const QString &before, const QString &after,
                                      const QSet<QString> &beforeSelection,
                                      const QSet<QString> &afterSelection,
                                      const QString &text)
{
    if (after == before)
    {
        return;
    }

    const int pageIndex = m_document != nullptr ? m_document->currentIndex() : 0;
    const bool signalsBlocked = m_undoStack->blockSignals(true);
    m_undoStack->push(new PageSnapshotCommand(this, pageIndex, before, after,
                                              beforeSelection, afterSelection, text));
    m_undoStack->blockSignals(signalsBlocked);
}

void DiagramView::restoreSnapshot(int pageIndex, const QString &xml,
                                  const QSet<QString> &selection)
{
    if (m_document == nullptr)
    {
        return;
    }

    m_document->setCurrentIndex(pageIndex);

    QString error;

    if (!m_document->restorePageXml(pageIndex, xml, &error))
    {
        return;
    }

    m_page = m_document->currentPage();
    m_selection = selection;
    m_editor->hide();
    update();
    emit selectionChanged();
    notifyModified();
}

void DiagramView::applySnapshot(int pageIndex, const QString &xml,
                                const QSet<QString> &selection)
{
    restoreSnapshot(pageIndex, xml, selection);
}

void DiagramView::notifyModified()
{
    if (m_document != nullptr)
    {
        m_document->setModified(true);
    }

    emit modified();
}

// --- mouse interaction ---------------------------------------------------

void DiagramView::mousePressEvent(QMouseEvent *event)
{
    setFocus();

    const QPointF viewPoint = event->position();
    const QPointF scenePoint = toScene(viewPoint);
    emit cursorPositionChanged(scenePoint);

    if (m_page == nullptr)
    {
        return;
    }

    if (event->button() == Qt::MiddleButton
        || (event->button() == Qt::LeftButton && (m_spacePressed || m_tool == Tool::Pan)))
    {
        beginPan(viewPoint);
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton)
    {
        return;
    }

    if (m_editor->isVisible())
    {
        commitEditing();
    }

    // 1. Resize handle of the current selection.
    if (hasSelection())
    {
        const int handle = handleAt(viewPoint);

        if (handle >= 0)
        {
            beginResize(handle, scenePoint);
            return;
        }
    }

    // 2. Connection handle of the hovered shape.
    if (m_tool == Tool::Select && !m_hoverCellId.isEmpty())
    {
        if (Cell *hover = m_page->cell(m_hoverCellId))
        {
            if (hover->isVertex())
            {
                const QRectF rect = hover->absoluteRect();
                const QPointF centers[] = {
                    QPointF(rect.center().x(), rect.top()),
                    QPointF(rect.right(), rect.center().y()),
                    QPointF(rect.center().x(), rect.bottom()),
                    QPointF(rect.left(), rect.center().y()),
                };

                for (const QPointF &center : centers)
                {
                    if (QLineF(toView(center), viewPoint).length() <= 8.0)
                    {
                        beginConnect(hover, center);
                        return;
                    }
                }
            }
        }
    }

    // 3. Shape hit testing.
    Cell *hit = cellAt(scenePoint);

    if (m_tool == Tool::Connector)
    {
        if (hit != nullptr)
        {
            beginConnect(hit, scenePoint);
        }
        else
        {
            beginMarquee(scenePoint);
        }
        return;
    }

    if (m_tool == Tool::Text)
    {
        addText(scenePoint);
        return;
    }

    if (hit != nullptr && (event->modifiers() & Qt::ControlModifier)
        && !event->modifiers().testFlag(Qt::ShiftModifier))
    {
        beginConnect(hit, scenePoint);
        return;
    }

    if (hit != nullptr)
    {
        Cell *selectable = topLevelSelectable(hit);

        if (event->modifiers() & Qt::ShiftModifier)
        {
            selectCell(selectable->id, true);
        }
        else if (!m_selection.contains(selectable->id))
        {
            selectCell(selectable->id, false);
        }

        beginMove(scenePoint);
        return;
    }

    if (!(event->modifiers() & Qt::ShiftModifier))
    {
        clearSelection();
    }

    beginMarquee(scenePoint);
}

void DiagramView::beginPan(const QPointF &viewPoint)
{
    m_dragMode = DragMode::Pan;
    m_panStart = viewPoint;
    m_panOrigin = m_pan;
    setCursor(Qt::ClosedHandCursor);
}

void DiagramView::beginMarquee(const QPointF &scenePoint)
{
    m_dragMode = DragMode::Marquee;
    m_dragStartScene = scenePoint;
    m_marquee = QRectF();
    update();
}

void DiagramView::beginMove(const QPointF &scenePoint)
{
    m_dragMode = DragMode::Move;
    clearGuides();
    m_dragStartScene = scenePoint;
    m_dragStartBounds = selectionBounds();
    m_dragBeforeXml = MxFileIO::pageToXml(*m_page, false);
    m_dragBeforeSelection = m_selection;
    m_dragMoved = false;

    m_dragStartGeometry.clear();
    m_dragStartWaypoints.clear();
    m_dragStartPoints.clear();

    for (Cell *cell : selectedCellObjects())
    {
        if (cell->isVertex())
        {
            m_dragStartGeometry.insert(cell->id, cell->geometry);
        }
        else
        {
            m_dragStartWaypoints.insert(cell->id, cell->waypoints);

            if (cell->sourceId.isEmpty())
            {
                m_dragStartPoints.insert(cell->id + QStringLiteral(":s"), cell->sourcePoint);
            }

            if (cell->targetId.isEmpty())
            {
                m_dragStartPoints.insert(cell->id + QStringLiteral(":t"), cell->targetPoint);
            }
        }
    }
}

void DiagramView::beginResize(int handle, const QPointF &scenePoint)
{
    m_dragMode = DragMode::Resize;
    m_resizeHandle = handle;
    m_dragStartScene = scenePoint;
    m_dragStartBounds = selectionBounds();
    m_dragBeforeXml = MxFileIO::pageToXml(*m_page, false);
    m_dragBeforeSelection = m_selection;
    m_dragMoved = false;

    m_dragStartGeometry.clear();

    for (Cell *cell : selectedCellObjects())
    {
        if (cell->isVertex())
        {
            m_dragStartGeometry.insert(cell->id, cell->absoluteRect());
        }
    }
}

void DiagramView::beginConnect(Cell *source, const QPointF &scenePoint)
{
    if (source == nullptr)
    {
        return;
    }

    Cell *selectable = topLevelSelectable(source);
    m_connectSourceId = selectable->id;
    m_connectSourceSide = nearestConnectionSide(*selectable, scenePoint);
    m_connectTargetId.clear();
    m_connectTargetSide = -1;
    m_connectStart = connectionPoints(*selectable).value(m_connectSourceSide, scenePoint);
    m_connectCurrent = scenePoint;
    m_dragMode = DragMode::Connect;
    m_dragMoved = false;
    update();
}

void DiagramView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF viewPoint = event->position();
    const QPointF scenePoint = toScene(viewPoint);
    emit cursorPositionChanged(scenePoint);

    if (m_page == nullptr)
    {
        return;
    }

    if (m_dragMode == DragMode::None)
    {
        Cell *hover = cellAt(scenePoint);
        const QString hoverId = hover != nullptr ? topLevelSelectable(hover)->id : QString();

        if (hoverId != m_hoverCellId)
        {
            m_hoverCellId = hoverId;
            update();
        }

        updateCursorForPosition(viewPoint);
        return;
    }

    if (m_dragMode == DragMode::Pan)
    {
        m_pan = m_panOrigin - (viewPoint - m_panStart) / m_zoom;
        update();
        return;
    }

    if (m_dragMode == DragMode::Move)
    {
        QPointF delta = scenePoint - m_dragStartScene;
        clearGuides();

        const bool disableSnap = (event->modifiers() & Qt::AltModifier) != 0;

        if (!disableSnap && m_guidesVisible)
        {
            const double threshold = 6.0 / qMax(m_zoom, 0.01);
            const QRectF moving = m_dragStartBounds.translated(delta);

            const SnapResult vertical = findVerticalSnap(moving, threshold);

            if (vertical.found)
            {
                delta.setX(delta.x() + vertical.offset);
                m_verticalGuide = Guide{true, vertical.position, vertical.from,
                                        vertical.to};
            }

            const SnapResult horizontal = findHorizontalSnap(moving, threshold);

            if (horizontal.found)
            {
                delta.setY(delta.y() + horizontal.offset);
                m_horizontalGuide = Guide{true, horizontal.position, horizontal.from,
                                          horizontal.to};
            }
        }

        if (!disableSnap && m_page->settings.grid)
        {
            const int grid = qMax(1, m_page->settings.gridSize);
            const QPointF target = m_dragStartBounds.topLeft() + delta;

            if (!m_verticalGuide.visible)
            {
                delta.setX(qRound(target.x() / grid) * grid - m_dragStartBounds.left());
            }

            if (!m_horizontalGuide.visible)
            {
                delta.setY(qRound(target.y() / grid) * grid - m_dragStartBounds.top());
            }
        }

        if (!delta.isNull())
        {
            m_dragMoved = true;
        }

        for (Cell *cell : selectedCellObjects())
        {
            if (cell->isVertex())
            {
                const QRectF original = m_dragStartGeometry.value(cell->id, cell->geometry);
                cell->geometry.moveTopLeft(original.topLeft() + delta);
            }
            else
            {
                const QVector<QPointF> points = m_dragStartWaypoints.value(cell->id);

                for (int i = 0; i < points.size() && i < cell->waypoints.size(); ++i)
                {
                    cell->waypoints[i] = points.at(i) + delta;
                }

                if (cell->sourceId.isEmpty())
                {
                    cell->sourcePoint = m_dragStartPoints.value(cell->id + QStringLiteral(":s"))
                        + delta;
                }

                if (cell->targetId.isEmpty())
                {
                    cell->targetPoint = m_dragStartPoints.value(cell->id + QStringLiteral(":t"))
                        + delta;
                }
            }
        }

        update();
        return;
    }

    if (m_dragMode == DragMode::Resize)
    {
        updateResize(scenePoint, event->modifiers() & Qt::ShiftModifier,
                     event->modifiers() & Qt::AltModifier);
        return;
    }

    if (m_dragMode == DragMode::Connect)
    {
        Cell *hit = cellAt(scenePoint);
        QString targetId;
        int targetSide = -1;
        QPointF preview = scenePoint;

        if (hit != nullptr)
        {
            Cell *selectable = topLevelSelectable(hit);

            if (selectable->id != m_connectSourceId)
            {
                targetId = selectable->id;
                targetSide = nearestConnectionSide(*selectable, scenePoint);
                const QPointF anchor = connectionPoints(*selectable).value(targetSide);

                if (QLineF(anchor, scenePoint).length() <= 18.0 / qMax(m_zoom, 0.01))
                {
                    preview = anchor;
                }
            }
        }

        m_connectTargetId = targetId;
        m_connectTargetSide = targetSide;
        m_connectCurrent = preview;

        if (QLineF(m_connectStart, m_connectCurrent).length() > 2.0)
        {
            m_dragMoved = true;
        }

        update();
        return;
    }

    if (m_dragMode == DragMode::Marquee)
    {
        const QRectF viewRect(QPointF(toView(m_dragStartScene)), QPointF(toView(scenePoint)));
        m_marquee = viewRect.normalized();
        update();
    }
}

void DiagramView::updateResize(const QPointF &scenePoint, bool keepRatio,
                               bool disableSnap)
{
    QRectF rect = m_dragStartBounds;
    const QPointF delta = scenePoint - m_dragStartScene;

    double left = rect.left();
    double top = rect.top();
    double right = rect.right();
    double bottom = rect.bottom();

    switch (m_resizeHandle)
    {
    case 0:
        left += delta.x();
        top += delta.y();
        break;
    case 1:
        top += delta.y();
        break;
    case 2:
        right += delta.x();
        top += delta.y();
        break;
    case 3:
        right += delta.x();
        break;
    case 4:
        right += delta.x();
        bottom += delta.y();
        break;
    case 5:
        bottom += delta.y();
        break;
    case 6:
        left += delta.x();
        bottom += delta.y();
        break;
    case 7:
        left += delta.x();
        break;
    default:
        break;
    }

    if (right - left < 10)
    {
        if (m_resizeHandle == 0 || m_resizeHandle == 6 || m_resizeHandle == 7)
        {
            left = right - 10;
        }
        else
        {
            right = left + 10;
        }
    }

    if (bottom - top < 10)
    {
        if (m_resizeHandle == 0 || m_resizeHandle == 1 || m_resizeHandle == 2)
        {
            top = bottom - 10;
        }
        else
        {
            bottom = top + 10;
        }
    }

    QRectF target(left, top, right - left, bottom - top);

    if (keepRatio && rect.width() > 0 && rect.height() > 0)
    {
        const double ratio = rect.width() / rect.height();
        double width = target.width();
        double height = target.height();

        if (width / height > ratio)
        {
            width = height * ratio;
        }
        else
        {
            height = width / ratio;
        }

        if (m_resizeHandle == 0 || m_resizeHandle == 6 || m_resizeHandle == 7)
        {
            target.setLeft(target.right() - width);
        }
        else
        {
            target.setWidth(width);
        }

        if (m_resizeHandle == 0 || m_resizeHandle == 1 || m_resizeHandle == 2)
        {
            target.setTop(target.bottom() - height);
        }
        else
        {
            target.setHeight(height);
        }
    }

    clearGuides();

    if (!disableSnap && m_guidesVisible)
    {
        const double threshold = 6.0 / qMax(m_zoom, 0.01);
        const bool dragsLeft = m_resizeHandle == 0 || m_resizeHandle == 6
            || m_resizeHandle == 7;
        const bool dragsRight = m_resizeHandle == 2 || m_resizeHandle == 3
            || m_resizeHandle == 4;
        const bool dragsTop = m_resizeHandle == 0 || m_resizeHandle == 1
            || m_resizeHandle == 2;
        const bool dragsBottom = m_resizeHandle == 4 || m_resizeHandle == 5
            || m_resizeHandle == 6;

        if (dragsLeft || dragsRight)
        {
            const SnapResult snap = findEdgeSnap(dragsLeft ? target.left() : target.right(),
                                                 true, target, threshold);

            if (snap.found)
            {
                if (dragsLeft)
                {
                    target.setLeft(qMin(target.left() + snap.offset, target.right() - 10.0));
                }
                else
                {
                    target.setRight(qMax(target.right() + snap.offset, target.left() + 10.0));
                }

                m_verticalGuide = Guide{true, snap.position, snap.from, snap.to};
            }
        }

        if (dragsTop || dragsBottom)
        {
            const SnapResult snap = findEdgeSnap(dragsTop ? target.top() : target.bottom(),
                                                 false, target, threshold);

            if (snap.found)
            {
                if (dragsTop)
                {
                    target.setTop(qMin(target.top() + snap.offset, target.bottom() - 10.0));
                }
                else
                {
                    target.setBottom(qMax(target.bottom() + snap.offset, target.top() + 10.0));
                }

                m_horizontalGuide = Guide{true, snap.position, snap.from, snap.to};
            }
        }
    }

    const double sx = rect.width() > 0 ? target.width() / rect.width() : 1.0;
    const double sy = rect.height() > 0 ? target.height() / rect.height() : 1.0;

    for (Cell *cell : selectedCellObjects())
    {
        if (!cell->isVertex())
        {
            continue;
        }

        const QRectF original = m_dragStartGeometry.value(cell->id, cell->absoluteRect());
        const QRectF mapped(target.left() + (original.left() - rect.left()) * sx,
                            target.top() + (original.top() - rect.top()) * sy,
                            original.width() * sx, original.height() * sy);

        if (cell->parent != nullptr && cell->parent->isVertex())
        {
            const QPointF parentPosition = cell->parent->absolutePosition();
            cell->geometry = QRectF(mapped.topLeft() - parentPosition, mapped.size());
        }
        else
        {
            cell->geometry = mapped;
        }
    }

    m_dragMoved = true;
    update();
}

void DiagramView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragMode == DragMode::Pan)
    {
        m_dragMode = DragMode::None;
        setCursor(m_tool == Tool::Pan ? Qt::OpenHandCursor : Qt::ArrowCursor);
        return;
    }

    if (m_dragMode == DragMode::Marquee)
    {
        const QRectF sceneMarquee = sceneRect(m_marquee).normalized();

        if (!(event->modifiers() & Qt::ShiftModifier))
        {
            m_selection.clear();
        }

        if (sceneMarquee.width() > 2 && sceneMarquee.height() > 2 && m_page != nullptr)
        {
            for (Cell *cell : m_page->cells())
            {
                if (!cell->visible)
                {
                    continue;
                }

                QRectF bounds;

                if (cell->isVertex())
                {
                    bounds = cell->absoluteRect();
                }
                else
                {
                    continue;
                }

                if (sceneMarquee.intersects(bounds))
                {
                    m_selection.insert(cell->id);
                }
            }
        }

        m_dragMode = DragMode::None;
        m_marquee = QRectF();
        update();
        emit selectionChanged();
        return;
    }

    if (m_dragMode == DragMode::Connect)
    {
        const QPointF scenePoint = toScene(event->position());
        Cell *target = cellAt(scenePoint);

        if (target != nullptr)
        {
            Cell *selectableTarget = topLevelSelectable(target);

            if (selectableTarget->id != m_connectSourceId && m_page != nullptr)
            {
                const int entrySide = nearestConnectionSide(*selectableTarget, scenePoint);
                static const double sidesX[4] = {0.5, 1.0, 0.5, 0.0};
                static const double sidesY[4] = {0.0, 0.5, 1.0, 0.5};

                QString style = QStringLiteral(
                    "edgeStyle=orthogonalEdgeStyle;rounded=0;orthogonalLoop=1;"
                    "jettySize=auto;html=1;endArrow=classic;endFill=1;");

                if (m_connectSourceSide >= 0 && m_connectSourceSide < 4)
                {
                    style += QStringLiteral("exitX=%1;exitY=%2;")
                                 .arg(sidesX[m_connectSourceSide])
                                 .arg(sidesY[m_connectSourceSide]);
                }

                if (entrySide >= 0 && entrySide < 4)
                {
                    style += QStringLiteral("entryX=%1;entryY=%2;")
                                 .arg(sidesX[entrySide])
                                 .arg(sidesY[entrySide]);
                }

                beginSnapshot(QStringLiteral("创建连线"));
                Cell *edge = m_page->addEdge(m_connectSourceId, selectableTarget->id,
                                             Style::parse(style));

                if (edge != nullptr)
                {
                    m_selection = {edge->id};
                }

                commitSnapshot();
                emit selectionChanged();
            }
        }

        m_dragMode = DragMode::None;
        m_connectSourceId.clear();
        m_connectTargetId.clear();
        m_connectSourceSide = -1;
        m_connectTargetSide = -1;
        m_dragMoved = false;
        update();
        return;
    }

    if (m_dragMode == DragMode::Move || m_dragMode == DragMode::Resize)
    {
        const bool wasResize = m_dragMode == DragMode::Resize;
        m_dragMode = DragMode::None;
        clearGuides();

        if (m_dragMoved && !m_dragBeforeXml.isEmpty() && m_page != nullptr)
        {
            const QString after = MxFileIO::pageToXml(*m_page, false);

            if (after != m_dragBeforeXml)
            {
                pushSnapshotCommand(m_dragBeforeXml, after, m_dragBeforeSelection, m_selection,
                                    wasResize ? QStringLiteral("调整大小")
                                              : QStringLiteral("移动"));
                notifyModified();
            }
        }

        m_dragBeforeXml.clear();
        m_dragMoved = false;
        update();
        return;
    }

    m_dragMode = DragMode::None;
}

void DiagramView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || m_page == nullptr)
    {
        return;
    }

    const QPointF scenePoint = toScene(event->position());
    Cell *hit = cellAt(scenePoint);

    if (hit != nullptr)
    {
        if (!m_selection.contains(topLevelSelectable(hit)->id))
        {
            selectCell(topLevelSelectable(hit)->id, false);
        }

        startEditing(hit->id);
        return;
    }

    // Double clicking empty canvas creates a shape with the default style.
    const ShapeDefinition definition = ShapeRegistry::definition(QStringLiteral("rectangle"));
    Cell *cell = addShape(definition, scenePoint);

    if (cell != nullptr)
    {
        startEditing(cell->id);
    }
}

void DiagramView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        const QPointF viewPoint = event->position();
        const QPointF scenePoint = toScene(viewPoint);
        const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        const double newZoom = qBound(kMinZoom, m_zoom * factor, kMaxZoom);

        if (!qFuzzyCompare(newZoom, m_zoom))
        {
            m_zoom = newZoom;
            m_pan = scenePoint - viewPoint / m_zoom;
            update();
            emit zoomChanged(m_zoom);
        }

        event->accept();
        return;
    }

    QPointF delta = event->pixelDelta().isNull()
        ? QPointF(event->angleDelta().x() / 8.0, event->angleDelta().y() / 8.0)
        : event->pixelDelta();

    if ((event->modifiers() & Qt::ShiftModifier) && qFuzzyIsNull(delta.x()))
    {
        delta = QPointF(delta.y(), 0);
    }

    if (!delta.isNull())
    {
        m_pan -= delta / m_zoom;
        update();
    }

    event->accept();
}

void DiagramView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !m_editor->isVisible())
    {
        m_spacePressed = true;
        setCursor(Qt::OpenHandCursor);
        return;
    }

    if (event->key() == Qt::Key_Escape)
    {
        if (m_editor->isVisible())
        {
            cancelEditing();
        }
        else
        {
            clearSelection();
        }
        return;
    }

    if (m_editor->isVisible())
    {
        QWidget::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
        deleteSelection();
        return;
    }

    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
         || event->key() == Qt::Key_F2) && hasSelection())
    {
        startEditingSelection();
        return;
    }

    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right
        || event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)
    {
        double step = (event->modifiers() & Qt::ShiftModifier) ? 10.0 : 1.0;

        if (m_page != nullptr && m_page->settings.grid && !(event->modifiers() & Qt::AltModifier))
        {
            step = (event->modifiers() & Qt::ShiftModifier)
                ? qMax(1, m_page->settings.gridSize) : 1.0;
        }

        QPointF delta;

        switch (event->key())
        {
        case Qt::Key_Left:
            delta = QPointF(-step, 0);
            break;
        case Qt::Key_Right:
            delta = QPointF(step, 0);
            break;
        case Qt::Key_Up:
            delta = QPointF(0, -step);
            break;
        default:
            delta = QPointF(0, step);
            break;
        }

        beginSnapshot(QStringLiteral("移动"));
        for (Cell *cell : selectedCellObjects())
        {
            if (cell->isVertex())
            {
                cell->geometry.translate(delta);
            }
            else
            {
                for (QPointF &point : cell->waypoints)
                {
                    point += delta;
                }

                if (cell->sourceId.isEmpty())
                {
                    cell->sourcePoint += delta;
                }

                if (cell->targetId.isEmpty())
                {
                    cell->targetPoint += delta;
                }
            }
        }
        commitSnapshot();
        update();
        return;
    }

    // Shortcuts in the view mirror the Edit menu for keyboard-centric use.
    if (event->matches(QKeySequence::Copy))
    {
        copySelection();
        return;
    }

    if (event->matches(QKeySequence::Cut))
    {
        cutSelection();
        return;
    }

    if (event->matches(QKeySequence::Paste))
    {
        paste();
        return;
    }

    if (event->matches(QKeySequence::SelectAll))
    {
        selectAll();
        return;
    }

    QWidget::keyPressEvent(event);
}

void DiagramView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space)
    {
        m_spacePressed = false;
        setCursor(m_tool == Tool::Pan ? Qt::OpenHandCursor : Qt::ArrowCursor);
        return;
    }

    QWidget::keyReleaseEvent(event);
}

void DiagramView::contextMenuEvent(QContextMenuEvent *event)
{
    const QPointF scenePoint = toScene(event->pos());
    Cell *hit = cellAt(scenePoint);

    if (hit != nullptr && !m_selection.contains(topLevelSelectable(hit)->id))
    {
        selectCell(topLevelSelectable(hit)->id, false);
    }

    emit contextMenuRequested(event->globalPos(), hit != nullptr ? hit->id : QString());
    event->accept();
}

void DiagramView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(QLatin1String(kShapeMimeType))
        || event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
}

void DiagramView::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat(QLatin1String(kShapeMimeType)))
    {
        event->acceptProposedAction();
    }
}

void DiagramView::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat(QLatin1String(kShapeMimeType)))
    {
        const QString key = QString::fromUtf8(
            event->mimeData()->data(QLatin1String(kShapeMimeType)));
        addShape(ShapeCatalog::instance().definition(key), toScene(event->position()));
        event->acceptProposedAction();
        return;
    }

    if (event->mimeData()->hasUrls() && !event->mimeData()->urls().isEmpty())
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
            emit filesDropped(paths);
        }

        event->acceptProposedAction();
    }
}

void DiagramView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (m_editor->isVisible() && !m_editorCellId.isEmpty())
    {
        // Keep the editor aligned with the cell after a resize.
        Cell *cell = m_page != nullptr ? m_page->cell(m_editorCellId) : nullptr;

        if (cell != nullptr)
        {
            const QRectF rect = cell->absoluteRect();
            const QRect viewRect(QPoint(toView(rect.topLeft()).toPoint()),
                                 QPoint(toView(rect.bottomRight()).toPoint()));
            m_editor->setGeometry(viewRect.normalized().adjusted(-2, -2, 2, 2));
        }
    }
}

void DiagramView::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    update();
}

void DiagramView::updateCursorForPosition(const QPointF &viewPoint)
{
    if (m_tool == Tool::Pan || m_spacePressed)
    {
        setCursor(Qt::OpenHandCursor);
        return;
    }

    if (m_tool == Tool::Connector)
    {
        setCursor(Qt::CrossCursor);
        return;
    }

    if (hasSelection())
    {
        const int handle = handleAt(viewPoint);

        switch (handle)
        {
        case 0:
        case 4:
            setCursor(Qt::SizeFDiagCursor);
            return;
        case 2:
        case 6:
            setCursor(Qt::SizeBDiagCursor);
            return;
        case 1:
        case 5:
            setCursor(Qt::SizeVerCursor);
            return;
        case 3:
        case 7:
            setCursor(Qt::SizeHorCursor);
            return;
        default:
            break;
        }
    }

    setCursor(Qt::ArrowCursor);
}

// --- editing operations --------------------------------------------------

void DiagramView::deleteSelection()
{
    if (m_page == nullptr || m_selection.isEmpty())
    {
        return;
    }

    beginSnapshot(QStringLiteral("删除"));
    deleteSelectedInternal();
    commitSnapshot();
    m_selection.clear();
    update();
    emit selectionChanged();
}

void DiagramView::deleteSelectedInternal()
{
    // Copy the ids: removeCell also drops attached edges and children, so the
    // selection set can reference cells that are already gone.
    const QStringList ids = m_selection.values();

    for (const QString &id : ids)
    {
        if (m_page->cell(id) != nullptr)
        {
            m_page->removeCell(m_page->cell(id));
        }
    }
}

void DiagramView::copySelection()
{
    if (m_page == nullptr || m_selection.isEmpty())
    {
        return;
    }

    m_clipboardXml = selectionAsPageXml();

    if (!m_clipboardXml.isEmpty())
    {
        QApplication::clipboard()->setText(m_clipboardXml);
        emit statusMessage(QStringLiteral("已复制 %1 个单元格").arg(m_selection.size()));
    }
}

void DiagramView::cutSelection()
{
    copySelection();
    deleteSelection();
}

void DiagramView::paste()
{
    QString xml = m_clipboardXml;
    const QString clipboardText = QApplication::clipboard()->text().trimmed();

    if (clipboardText.contains(QLatin1String("mxGraphModel"))
        || clipboardText.startsWith(QLatin1String("<mxfile")))
    {
        xml = clipboardText;
    }

    if (xml.isEmpty())
    {
        return;
    }

    if (pastePageXml(xml, 10, 10, true))
    {
        emit statusMessage(QStringLiteral("已粘贴"));
    }
}

void DiagramView::duplicateSelection()
{
    if (m_page == nullptr || m_selection.isEmpty())
    {
        return;
    }

    const QString xml = selectionAsPageXml();

    if (!xml.isEmpty())
    {
        pastePageXml(xml, 10, 10, true);
    }
}

QString DiagramView::selectionAsPageXml() const
{
    if (m_page == nullptr || m_selection.isEmpty())
    {
        return {};
    }

    Page temporary;
    QHash<QString, QString> idMap;

    QVector<Cell *> roots;

    for (const QString &id : m_selection)
    {
        Cell *cell = m_page->cell(id);

        if (cell != nullptr)
        {
            roots.append(cell);
        }
    }

    cloneSelectionTo(temporary, roots, &idMap, 0, 0);
    temporary.reindex();

    return MxFileIO::pageToXml(temporary, false);
}

void DiagramView::cloneSelectionTo(Page &target, const QVector<Cell *> &sourceCells,
                                   QHash<QString, QString> *idMap, double offsetX,
                                   double offsetY) const
{
    Q_UNUSED(offsetX);
    Q_UNUSED(offsetY);

    QSet<QString> selectedTop;
    QVector<Cell *> ordered;

    std::function<void(Cell *)> collect = [&](Cell *cell)
    {
        if (cell == nullptr)
        {
            return;
        }

        const QString mappedId = idMap->value(cell->id, cell->id);
        idMap->insert(cell->id, mappedId);
        ordered.append(cell);

        for (Cell *child : cell->children)
        {
            collect(child);
        }
    };

    for (Cell *cell : sourceCells)
    {
        selectedTop.insert(cell->id);
        collect(cell);
    }

    // Attached edges whose both ends are inside the selection travel with it.
    for (Cell *edge : m_page->edges())
    {
        if (edge->sourceId.isEmpty() || edge->targetId.isEmpty())
        {
            continue;
        }

        if (selectedTop.contains(edge->sourceId) && selectedTop.contains(edge->targetId))
        {
            collect(edge);
        }
    }

    QSet<QString> included;

    for (Cell *cell : ordered)
    {
        included.insert(cell->id);
    }

    for (Cell *cell : ordered)
    {
        auto copy = std::make_unique<Cell>(cell->kind(), cell->id);
        copy->value = cell->value;
        copy->style = cell->style;
        copy->visible = cell->visible;
        copy->geometry = cell->geometry;
        copy->rotation = cell->rotation;
        copy->sourceId = cell->sourceId;
        copy->targetId = cell->targetId;
        copy->sourcePoint = cell->sourcePoint;
        copy->targetPoint = cell->targetPoint;
        copy->waypoints = cell->waypoints;

        if (included.contains(cell->parentId))
        {
            copy->parentId = cell->parentId;
        }
        else
        {
            copy->parentId = QStringLiteral("1");
        }

        target.addCell(std::move(copy));
    }
}

bool DiagramView::pastePageXml(const QString &xml, double offsetX, double offsetY,
                               bool select)
{
    if (m_page == nullptr)
    {
        return false;
    }

    Page source;

    QString error;

    if (!MxFileIO::pageFromXml(source, xml, &error))
    {
        emit statusMessage(QStringLiteral("粘贴失败：%1").arg(error));
        return false;
    }

    beginSnapshot(QStringLiteral("粘贴"));

    QHash<QString, QString> idMap;
    QVector<Cell *> created;

    for (Cell *cell : source.cells())
    {
        idMap.insert(cell->id, m_page->newId());
    }

    for (Cell *cell : source.cells())
    {
        auto copy = std::make_unique<Cell>(cell->kind(), idMap.value(cell->id));
        copy->value = cell->value;
        copy->style = cell->style;
        copy->visible = cell->visible;
        copy->rotation = cell->rotation;

        copy->parentId = idMap.value(cell->parentId, QStringLiteral("1"));

        if (cell->isVertex())
        {
            copy->geometry = cell->geometry.translated(offsetX, offsetY);
        }
        else
        {
            copy->sourceId = idMap.value(cell->sourceId);
            copy->targetId = idMap.value(cell->targetId);
            copy->sourcePoint = cell->sourcePoint + QPointF(offsetX, offsetY);
            copy->targetPoint = cell->targetPoint + QPointF(offsetX, offsetY);

            for (const QPointF &point : cell->waypoints)
            {
                copy->waypoints.append(point + QPointF(offsetX, offsetY));
            }
        }

        Cell *added = m_page->addCell(std::move(copy));

        if (added != nullptr)
        {
            created.append(added);
        }
    }

    m_page->reindex();

    if (select)
    {
        m_selection.clear();

        for (Cell *cell : created)
        {
            m_selection.insert(cell->id);
        }
    }

    commitSnapshot();
    update();
    emit selectionChanged();
    return true;
}

void DiagramView::moveSelectionBy(const QPointF &delta, bool snap)
{
    if (m_page == nullptr || m_selection.isEmpty())
    {
        return;
    }

    QPointF applied = delta;

    if (snap && m_page->settings.grid)
    {
        const int grid = qMax(1, m_page->settings.gridSize);
        const QPointF topLeft = selectionBounds().topLeft() + delta;
        applied = QPointF(qRound(topLeft.x() / grid) * grid, qRound(topLeft.y() / grid) * grid)
            - selectionBounds().topLeft();
    }

    beginSnapshot(QStringLiteral("移动"));

    for (Cell *cell : selectedCellObjects())
    {
        if (cell->isVertex())
        {
            cell->geometry.translate(applied);
        }
        else
        {
            for (QPointF &point : cell->waypoints)
            {
                point += applied;
            }

            if (cell->sourceId.isEmpty())
            {
                cell->sourcePoint += applied;
            }

            if (cell->targetId.isEmpty())
            {
                cell->targetPoint += applied;
            }
        }
    }

    commitSnapshot();
    update();
}

void DiagramView::alignSelection(Align align)
{
    if (m_page == nullptr || m_selection.size() < 2)
    {
        return;
    }

    const QRectF bounds = selectionBounds();

    beginSnapshot(QStringLiteral("对齐"));

    for (Cell *cell : selectedCellObjects())
    {
        if (!cell->isVertex())
        {
            continue;
        }

        QRectF rect = cell->absoluteRect();
        QPointF position = rect.topLeft();

        switch (align)
        {
        case Align::Left:
            position.setX(bounds.left());
            break;
        case Align::CenterHorizontal:
            position.setX(bounds.center().x() - rect.width() / 2.0);
            break;
        case Align::Right:
            position.setX(bounds.right() - rect.width());
            break;
        case Align::Top:
            position.setY(bounds.top());
            break;
        case Align::CenterVertical:
            position.setY(bounds.center().y() - rect.height() / 2.0);
            break;
        case Align::Bottom:
            position.setY(bounds.bottom() - rect.height());
            break;
        }

        if (cell->parent != nullptr && cell->parent->isVertex())
        {
            cell->geometry.moveTopLeft(position - cell->parent->absolutePosition());
        }
        else
        {
            cell->geometry.moveTopLeft(position);
        }
    }

    commitSnapshot();
    update();
}

void DiagramView::distributeHorizontally()
{
    QVector<Cell *> cells;

    for (Cell *cell : selectedCellObjects())
    {
        if (cell->isVertex())
        {
            cells.append(cell);
        }
    }

    if (cells.size() < 3)
    {
        return;
    }

    std::sort(cells.begin(), cells.end(), [](Cell *a, Cell *b)
    {
        return a->absoluteRect().center().x() < b->absoluteRect().center().x();
    });

    const double first = cells.first()->absoluteRect().center().x();
    const double last = cells.last()->absoluteRect().center().x();
    const double step = (last - first) / (cells.size() - 1);

    beginSnapshot(QStringLiteral("水平分布"));

    for (int i = 1; i < cells.size() - 1; ++i)
    {
        QRectF rect = cells.at(i)->absoluteRect();
        const QPointF target(rect.left() + (first + step * i) - rect.center().x(), rect.top());
        cells.at(i)->geometry.moveTopLeft(target);
    }

    commitSnapshot();
    update();
}

void DiagramView::distributeVertically()
{
    QVector<Cell *> cells;

    for (Cell *cell : selectedCellObjects())
    {
        if (cell->isVertex())
        {
            cells.append(cell);
        }
    }

    if (cells.size() < 3)
    {
        return;
    }

    std::sort(cells.begin(), cells.end(), [](Cell *a, Cell *b)
    {
        return a->absoluteRect().center().y() < b->absoluteRect().center().y();
    });

    const double first = cells.first()->absoluteRect().center().y();
    const double last = cells.last()->absoluteRect().center().y();
    const double step = (last - first) / (cells.size() - 1);

    beginSnapshot(QStringLiteral("垂直分布"));

    for (int i = 1; i < cells.size() - 1; ++i)
    {
        QRectF rect = cells.at(i)->absoluteRect();
        const QPointF target(rect.left(), rect.top() + (first + step * i) - rect.center().y());
        cells.at(i)->geometry.moveTopLeft(target);
    }

    commitSnapshot();
    update();
}

void DiagramView::resizeSelection(SizeAction action)
{
    QVector<Cell *> cells;

    for (Cell *cell : selectedCellObjects())
    {
        if (cell->isVertex())
        {
            cells.append(cell);
        }
    }

    if (cells.isEmpty())
    {
        return;
    }

    const QSizeF reference = cells.first()->absoluteRect().size();

    beginSnapshot(QStringLiteral("调整大小"));

    for (Cell *cell : cells)
    {
        QRectF rect = cell->absoluteRect();
        QSizeF size = rect.size();

        if (action == SizeAction::SameWidth || action == SizeAction::SameSize)
        {
            size.setWidth(reference.width());
        }

        if (action == SizeAction::SameHeight || action == SizeAction::SameSize)
        {
            size.setHeight(reference.height());
        }

        rect.setSize(size);

        if (cell->parent != nullptr && cell->parent->isVertex())
        {
            cell->geometry = QRectF(rect.topLeft() - cell->parent->absolutePosition(), size);
        }
        else
        {
            cell->geometry = rect;
        }
    }

    commitSnapshot();
    update();
}

void DiagramView::changeZOrder(ZOrder order)
{
    if (m_page == nullptr || m_selection.isEmpty())
    {
        return;
    }

    beginSnapshot(QStringLiteral("调整层级"));

    for (const QString &id : m_selection)
    {
        Cell *cell = m_page->cell(id);

        if (cell == nullptr)
        {
            continue;
        }

        const int index = m_page->indexOf(cell);

        switch (order)
        {
        case ZOrder::Front:
            m_page->moveCell(cell, m_page->cells().size() - 1);
            break;
        case ZOrder::Back:
            m_page->moveCell(cell, 0);
            break;
        case ZOrder::Forward:
            m_page->moveCell(cell, index + 1);
            break;
        case ZOrder::Backward:
            m_page->moveCell(cell, index - 1);
            break;
        }
    }

    commitSnapshot();
    update();
}

void DiagramView::groupSelection()
{
    if (m_page == nullptr || m_selection.size() < 2)
    {
        return;
    }

    QVector<Cell *> cells;

    for (const QString &id : m_selection)
    {
        if (Cell *cell = m_page->cell(id))
        {
            if (cell->parent == nullptr || cell->parent->parent == nullptr)
            {
                cells.append(cell);
            }
        }
    }

    if (cells.isEmpty())
    {
        return;
    }

    const QRectF bounds = m_page->boundsForCells(cells);

    beginSnapshot(QStringLiteral("组合"));

    Cell *group = m_page->addVertex(bounds, Style::parse(QStringLiteral(
                                                     "group;fillColor=none;strokeColor=none;"
                                                     "html=1;pointerEvents=0;")),
                                    QString());
    group->parentId = m_page->activeLayerId;

    for (Cell *cell : cells)
    {
        const QPointF offset = bounds.topLeft();
        cell->parentId = group->id;

        if (cell->isVertex())
        {
            cell->geometry.moveTopLeft(cell->geometry.topLeft() - offset);
        }
        else
        {
            for (QPointF &point : cell->waypoints)
            {
                point -= offset;
            }

            if (cell->sourceId.isEmpty())
            {
                cell->sourcePoint -= offset;
            }

            if (cell->targetId.isEmpty())
            {
                cell->targetPoint -= offset;
            }
        }
    }

    m_page->reindex();
    m_selection = {group->id};
    commitSnapshot();
    update();
    emit selectionChanged();
}

void DiagramView::ungroupSelection()
{
    if (m_page == nullptr)
    {
        return;
    }

    QVector<Cell *> groups;

    for (Cell *cell : selectedCellObjects())
    {
        if (cell->isVertex() && !cell->children.isEmpty())
        {
            groups.append(cell);
        }
    }

    if (groups.isEmpty())
    {
        return;
    }

    beginSnapshot(QStringLiteral("取消组合"));

    for (Cell *group : groups)
    {
        const QPointF offset = group->absolutePosition();
        const QVector<Cell *> children = group->children;
        group->children.clear();

        for (Cell *child : children)
        {
            child->parentId = group->parentId;

            if (child->isVertex())
            {
                child->geometry.moveTopLeft(child->geometry.topLeft() + offset);
            }
            else
            {
                for (QPointF &point : child->waypoints)
                {
                    point += offset;
                }

                if (child->sourceId.isEmpty())
                {
                    child->sourcePoint += offset;
                }

                if (child->targetId.isEmpty())
                {
                    child->targetPoint += offset;
                }
            }
        }

        m_page->removeCell(group);
    }

    m_page->reindex();
    m_selection.clear();
    commitSnapshot();
    update();
    emit selectionChanged();
}

// --- inline editor -------------------------------------------------------

void DiagramView::startEditingSelection()
{
    if (m_selection.isEmpty())
    {
        return;
    }

    const QString id = *m_selection.begin();
    startEditing(id, false);
}

void DiagramView::startEditing(const QString &cellId, bool selectAll)
{
    if (m_page == nullptr)
    {
        return;
    }

    Cell *cell = m_page->cell(cellId);

    if (cell == nullptr)
    {
        return;
    }

    m_editorCellId = cellId;
    m_editorBeforeXml = MxFileIO::pageToXml(*m_page, false);

    QRect viewRect;

    if (cell->isEdge())
    {
        const RoutedEdge route = EdgeRouter::route(*m_page, *cell);
        const QPointF anchor = toView(route.pointAt(0.5));
        viewRect = QRect(static_cast<int>(anchor.x() - 80), static_cast<int>(anchor.y() - 12),
                         160, 24);
    }
    else
    {
        const QRectF rect = cell->absoluteRect();
        viewRect = QRect(toView(rect.topLeft()).toPoint(), toView(rect.bottomRight()).toPoint())
                       .normalized()
                       .intersected(this->rect().adjusted(0, 0, -1, -1));

        if (viewRect.width() < 40)
        {
            viewRect.setWidth(40);
        }

        if (viewRect.height() < 24)
        {
            viewRect.setHeight(24);
        }
    }

    QFont font = LabelPainter::fontFor(*cell);
    font.setPixelSize(qMax(9, static_cast<int>(font.pixelSize() * m_zoom)));
    m_editor->setFont(font);
    m_editor->begin(viewRect, cell->plainLabel(), selectAll);

    emit editStarted();
}

void DiagramView::commitEditing()
{
    if (!m_editor->isVisible() || m_editorCellId.isEmpty() || m_page == nullptr)
    {
        return;
    }

    Cell *cell = m_page->cell(m_editorCellId);
    const QString text = m_editor->toPlainText();
    m_editor->hide();

    if (cell == nullptr)
    {
        m_editorCellId.clear();
        return;
    }

    QString value = text;

    if (cell->isHtmlLabel())
    {
        value = htmlEscape(text);
        value.replace(QStringLiteral("\n"), QStringLiteral("<br>"));
    }

    const QString before = m_editorBeforeXml;
    const QSet<QString> beforeSelection = m_selection;

    if (value != cell->value)
    {
        cell->value = value;
        const QString after = MxFileIO::pageToXml(*m_page, false);
        pushSnapshotCommand(before, after, beforeSelection, m_selection,
                            QStringLiteral("编辑文本"));
        notifyModified();
    }

    m_editorCellId.clear();
    m_editorBeforeXml.clear();
    update();
    emit editFinished();
}

void DiagramView::cancelEditing()
{
    if (!m_editor->isVisible())
    {
        return;
    }

    m_editor->hide();
    m_editorCellId.clear();
    m_editorBeforeXml.clear();
    update();
    emit editFinished();
}

bool DiagramView::isEditing() const
{
    return m_editor != nullptr && m_editor->isVisible();
}

void DiagramView::setCellValue(const QString &cellId, const QString &value)
{
    if (m_page == nullptr)
    {
        return;
    }

    Cell *cell = m_page->cell(cellId);

    if (cell == nullptr || cell->value == value)
    {
        return;
    }

    beginSnapshot(QStringLiteral("编辑文本"));
    cell->value = value;
    commitSnapshot();
    update();
}

void DiagramView::applyStyle(const QString &key, const QString &value)
{
    if (m_page == nullptr || m_selection.isEmpty())
    {
        return;
    }

    beginSnapshot(QStringLiteral("修改样式"));

    for (Cell *cell : selectedCellObjects())
    {
        if (value.isEmpty())
        {
            cell->style.remove(key);
        }
        else
        {
            cell->style.insert(key, value);
        }
    }

    commitSnapshot();
    update();
}

void DiagramView::clearStyle(const QString &key)
{
    if (m_page == nullptr || m_selection.isEmpty())
    {
        return;
    }

    beginSnapshot(QStringLiteral("修改样式"));

    for (Cell *cell : selectedCellObjects())
    {
        cell->style.remove(key);
    }

    commitSnapshot();
    update();
}

void DiagramView::applyGeometry(double x, double y, double width, double height)
{
    Cell *cell = nullptr;

    if (m_page != nullptr && m_selection.size() == 1)
    {
        cell = m_page->cell(*m_selection.begin());
    }

    if (cell == nullptr || !cell->isVertex())
    {
        return;
    }

    beginSnapshot(QStringLiteral("调整位置/大小"));
    QRectF rect(x, y, qMax(1.0, width), qMax(1.0, height));

    if (cell->parent != nullptr && cell->parent->isVertex())
    {
        rect.moveTopLeft(QPointF(x, y) - cell->parent->absolutePosition());
        cell->geometry = rect;
    }
    else
    {
        cell->geometry = rect;
    }

    commitSnapshot();
    update();
}

void DiagramView::setRotation(double degrees)
{
    Cell *cell = nullptr;

    if (m_page != nullptr && m_selection.size() == 1)
    {
        cell = m_page->cell(*m_selection.begin());
    }

    if (cell == nullptr)
    {
        return;
    }

    beginSnapshot(QStringLiteral("旋转"));
    cell->rotation = degrees;
    commitSnapshot();
    update();
}

// --- creation ------------------------------------------------------------

Cell *DiagramView::addShape(const ShapeDefinition &definition, const QPointF &scenePos)
{
    if (m_page == nullptr)
    {
        return nullptr;
    }

    beginSnapshot(QStringLiteral("添加形状"));

    const QSizeF size = definition.size;
    QPointF topLeft = scenePos - QPointF(size.width() / 2.0, size.height() / 2.0);

    if (m_page->settings.grid)
    {
        const int grid = qMax(1, m_page->settings.gridSize);
        topLeft = QPointF(qRound(topLeft.x() / grid) * grid,
                          qRound(topLeft.y() / grid) * grid);
    }

    Cell *cell = m_page->addVertex(QRectF(topLeft, size), Style::parse(definition.style),
                                   QStringLiteral(""));
    m_selection = {cell->id};
    commitSnapshot();
    update();
    emit selectionChanged();
    return cell;
}

Cell *DiagramView::addText(const QPointF &scenePos)
{
    const ShapeDefinition definition = ShapeRegistry::definition(QStringLiteral("text"));
    Cell *cell = addShape(definition, scenePos);

    if (cell != nullptr)
    {
        startEditing(cell->id);
    }

    return cell;
}

Cell *DiagramView::addEdgeFromSelection()
{
    if (m_page == nullptr)
    {
        return nullptr;
    }

    QVector<Cell *> vertices;

    for (Cell *cell : selectedCellObjects())
    {
        if (cell->isVertex())
        {
            vertices.append(cell);
        }
    }

    if (vertices.size() < 2)
    {
        return nullptr;
    }

    beginSnapshot(QStringLiteral("创建连线"));
    Cell *edge = m_page->addEdge(vertices.at(0)->id, vertices.at(1)->id,
                                 Style::parse(ShapeRegistry::defaultEdgeStyle()));

    if (edge != nullptr)
    {
        m_selection = {edge->id};
    }

    commitSnapshot();
    update();
    emit selectionChanged();
    return edge;
}

} // namespace dq
