#include "app/I18n.h"
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
#include <QFontMetrics>
#include <QCursor>
#include <QGuiApplication>
#include <QPainter>
#include <QtMath>
#include <QPainterPath>
#include <QUuid>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFragment>
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

    // Mirror the format at the cursor so the panel shows the selected run.
    connect(m_editor, &QTextEdit::currentCharFormatChanged, this,
            [this](const QTextCharFormat &format)
    {
        if (!m_editor->isRichText())
        {
            return;
        }

        int bits = 0;
        bits |= format.fontWeight() >= QFont::Bold ? 1 : 0;
        bits |= format.fontItalic() ? 2 : 0;
        bits |= format.fontUnderline() ? 4 : 0;

        const QStringList families = format.fontFamilies().toStringList();
        const QString family = families.isEmpty() ? QString() : families.first();

        emit editorCharFormatChanged(static_cast<int>(format.fontPointSize()),
                                     family, format.foreground().color(), bits);
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
    QVector<QPointF> points = {
        QPointF(rect.center().x(), rect.top()),
        QPointF(rect.right(), rect.center().y()),
        QPointF(rect.center().x(), rect.bottom()),
        QPointF(rect.left(), rect.center().y()),
    };

    const double angle = cell.effectiveRotation();

    if (!qFuzzyIsNull(angle))
    {
        QTransform transform;
        transform.translate(rect.center().x(), rect.center().y());
        transform.rotate(angle);
        transform.translate(-rect.center().x(), -rect.center().y());

        for (QPointF &point : points)
        {
            point = transform.map(point);
        }
    }

    return points;
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

            if (!cell->isEdge() || !cell->visible
                || isLayerLocked(layerIdForCell(cell)))
            {
                continue;
            }

            const RoutedEdge route = EdgeRouter::route(*m_page, *cell);

            // Edge labels are child cells with relative geometry; hit test
            // them first so the text can be selected and edited.
            for (Cell *child : m_page->cells())
            {
                if (child->parentId == cell->id && child->visible
                    && child->styleValue(QStringLiteral("shape")).isEmpty())
                {
                    const QRectF labelBounds = EdgeRenderer::childBounds(route, *child);

                    if (!labelBounds.isEmpty()
                        && labelBounds.adjusted(-2, -2, 2, 2).contains(scenePoint))
                    {
                        return child;
                    }
                }
            }

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

                double tolerance = qMax(4.0, 6.0 / m_zoom);

                if (!cell->styleValue(QStringLiteral("shape")).isEmpty())
                {
                    // Edge shapes such as the wedge arrows paint a wide band.
                    tolerance = qMax(tolerance,
                                     cell->styleNumber(QStringLiteral("startWidth"), 0.0));
                }

                if (distance <= tolerance)
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

        // transparentBounds containers are painted at the box derived from
        // their children, so hit testing uses that box too.
        const QRectF hitRect = ShapeRegistry::hasTransparentBounds(*cell)
            ? ShapeRegistry::visibleRect(*cell)
            : cell->visualRect();

        if (hitRect.contains(scenePoint))
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
        // Like draw.io, only top level cells are selected: edge labels and
        // other children keep their relative geometry and would drag the
        // selection box to the origin.
        if (cell->visible && m_page->layer(cell->parentId) != nullptr)
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
        else if (ShapeRegistry::hasTransparentBounds(*cell))
        {
            rect = ShapeRegistry::visibleRect(*cell);
        }
        else
        {
            rect = cell->visualRect();
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

Cell *DiagramView::singleRotatedVertex() const
{
    if (m_page == nullptr || m_selection.size() != 1)
    {
        return nullptr;
    }

    Cell *cell = m_page->cell(*m_selection.begin());

    if (cell == nullptr || !cell->isVertex() || qFuzzyIsNull(cell->effectiveRotation()))
    {
        return nullptr;
    }

    return cell;
}

QRectF DiagramView::selectionRectFor(const Cell &cell) const
{
    // transparentBounds containers are painted at the box derived from their
    // children; their stored geometry is empty.
    if (ShapeRegistry::hasTransparentBounds(cell))
    {
        const QRectF box = ShapeRegistry::visibleRect(cell);

        if (!box.isNull())
        {
            return box;
        }
    }

    return cell.absoluteRect();
}

QVector<QRectF> DiagramView::handleRects() const
{
    const double half = kHandleSize / 2.0;
    QVector<QPointF> centers;

    if (Cell *rotated = singleRotatedVertex())
    {
        const QRectF rect = selectionRectFor(*rotated);
        QTransform transform;
        transform.translate(rect.center().x(), rect.center().y());
        transform.rotate(rotated->effectiveRotation());
        transform.translate(-rect.center().x(), -rect.center().y());
        const QPointF local[] = {
            rect.topLeft(), QPointF(rect.center().x(), rect.top()), rect.topRight(),
            QPointF(rect.right(), rect.center().y()), rect.bottomRight(),
            QPointF(rect.center().x(), rect.bottom()), rect.bottomLeft(),
            QPointF(rect.left(), rect.center().y()),
        };

        for (const QPointF &point : local)
        {
            centers.append(toView(transform.map(point)));
        }
    }
    else
    {
        const QRectF bounds = selectionBoundsInView();
        centers = {
            bounds.topLeft(), QPointF(bounds.center().x(), bounds.top()), bounds.topRight(),
            QPointF(bounds.right(), bounds.center().y()), bounds.bottomRight(),
            QPointF(bounds.center().x(), bounds.bottom()), bounds.bottomLeft(),
            QPointF(bounds.left(), bounds.center().y()),
        };
    }

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

        const QRectF rect = selectionRectFor(*cell);
        const double angle = cell->effectiveRotation();

        QPen pen(accent, 1.5);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        if (!qFuzzyIsNull(angle))
        {
            QTransform transform;
            transform.translate(rect.center().x(), rect.center().y());
            transform.rotate(angle);
            transform.translate(-rect.center().x(), -rect.center().y());
            QPolygonF polygon;
            polygon << transform.map(rect.topLeft()) << transform.map(rect.topRight())
                    << transform.map(rect.bottomRight()) << transform.map(rect.bottomLeft());
            QPolygonF viewPolygon;

            for (const QPointF &point : polygon)
            {
                viewPolygon << toView(point);
            }

            painter.drawPolygon(viewPolygon);
        }
        else
        {
            const QRectF viewRect(toView(rect.topLeft()), toView(rect.bottomRight()));
            painter.drawRect(viewRect);
        }
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

    // Rotation handle above the selection.
    if (hasVertex)
    {
        const QPointF center = rotationHandleCenter();
        const QRectF handle(center.x() - 6, center.y() - 6, 12, 12);
        painter.setPen(QPen(accent, 1.4));
        painter.setBrush(QColor(Qt::white));
        painter.drawEllipse(handle);
        painter.setBrush(Qt::NoBrush);
        QPainterPath arrow;
        arrow.arcMoveTo(handle.adjusted(2, 2, -2, -2), 40);
        arrow.arcTo(handle.adjusted(2, 2, -2, -2), 40, 250);
        painter.drawPath(arrow);
        painter.drawLine(center + QPointF(4, -8), center + QPointF(4, -4));
        painter.drawLine(center + QPointF(4, -8), center + QPointF(0, -8));

        // Show the current angle while rotating.
        if (m_rotating && !m_rotateInitial.isEmpty())
        {
            if (const Cell *cell = m_page != nullptr ? m_page->cell(m_rotateInitial.first().first) : nullptr)
            {
                const double angle = std::fmod(cell->effectiveRotation(), 360.0);
                const QString text = tr("%1°").arg(QString::number(angle, 'f', 1));
                const QFontMetrics metrics(painter.fontMetrics());
                const int textWidth = metrics.horizontalAdvance(text);
                const QRectF badge(center.x() - textWidth / 2.0 - 8.0,
                                   center.y() - 34.0, textWidth + 16.0, 22.0);
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0, 0, 0, 180));
                painter.drawRoundedRect(badge, 4, 4);
                painter.setPen(QPen(QColor(Qt::white), 1.0));
                painter.drawText(badge, Qt::AlignCenter, text);
                painter.setBrush(Qt::NoBrush);
            }
        }
    }

    // Editable waypoints of selected edges. Hide them while dragging so the
    // inserted handle does not sit under the cursor.
    if (m_page != nullptr && m_dragMode == DragMode::None)
    {
        for (const QString &id : m_selection)
        {
            Cell *cell = m_page->cell(id);

            if (cell == nullptr || !cell->isEdge())
            {
                continue;
            }

            const RoutedEdge route = EdgeRouter::route(*m_page, *cell);

            // Segment midpoint handles, like draw.io: drag a dot to move the
            // whole segment. They are only affordances; hit testing accepts the
            // entire segment.
            painter.setPen(Qt::NoPen);
            painter.setBrush(accent);

            for (int i = 1; i < route.points.size(); ++i)
            {
                const QPointF a = route.points.at(i - 1);
                const QPointF b = route.points.at(i);

                if (QLineF(a, b).length() < 12.0)
                {
                    continue;
                }

                painter.drawEllipse(toView((a + b) / 2.0), 3.0, 3.0);
            }

            // Green endpoint crosses mark the attached connection points.
            const QColor endpoint(0x18, 0xA0, 0x5E);
            painter.setPen(QPen(endpoint, 1.8));
            painter.setBrush(Qt::NoBrush);

            for (const QPointF &endpointPoint : {route.points.first(), route.points.last()})
            {
                const QPointF center = toView(endpointPoint);
                painter.drawLine(center + QPointF(-4, -4), center + QPointF(4, 4));
                painter.drawLine(center + QPointF(-4, 4), center + QPointF(4, -4));
            }
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
                const QVector<QPointF> centers = connectionPoints(*cell);

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
    // Signals must stay connected: createUndoAction/createRedoAction rely on
    // canUndoChanged/canRedoChanged to enable the toolbar and menu entries.
    m_undoStack->push(new PageSnapshotCommand(this, pageIndex, before, after,
                                              beforeSelection, afterSelection, text));
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

    // 2b. Fold indicator of a container toggles its collapsed state.
    if (m_tool == Tool::Select && m_page != nullptr
        && !(event->modifiers() & Qt::ShiftModifier))
    {
        for (Cell *cell : m_page->cells())
        {
            if (!cell->visible || !cell->isVertex()
                || !cell->styleFlag(QStringLiteral("swimlane"), false)
                || !cell->styleFlag(QStringLiteral("collapsible"), true)
                || isLayerLocked(layerIdForCell(cell)))
            {
                continue;
            }

            const QRectF fold(cell->absolutePosition() + QPointF(4, 4), QSizeF(9, 9));

            if (fold.contains(scenePoint))
            {
                beginSnapshot(QStringLiteral("Toggle Container"));

                // mxGraph.cellsFolded(): the folded size is stored in the
                // geometry and the size to restore in alternateBounds.
                const double startSize = cell->styleNumber(QStringLiteral("startSize"), 23.0);
                const bool horizontal = cell->styleFlag(QStringLiteral("horizontal"), true);

                if (cell->collapsed)
                {
                    m_foldedBounds.insert(cell->id, cell->geometry);

                    if (!cell->alternateBounds.isNull())
                    {
                        cell->geometry = cell->alternateBounds;
                    }

                    cell->collapsed = false;
                }
                else
                {
                    cell->alternateBounds = cell->geometry;
                    cell->collapsed = true;

                    const QRectF folded = m_foldedBounds.value(cell->id);

                    if (!folded.isNull())
                    {
                        cell->geometry = folded;
                    }
                    else if (startSize <= 0.0)
                    {
                        // Nothing to fold to: keep the size, like draw.io.
                    }
                    else if (horizontal)
                    {
                        cell->geometry.setHeight(qMin(cell->geometry.height(), startSize));
                    }
                    else
                    {
                        cell->geometry.setWidth(qMin(cell->geometry.width(), startSize));
                    }
                }

                commitSnapshot();
                notifyModified();
                selectCell(cell->id, false);
                update();
                return;
            }
        }
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

    // 1a. Rotation handle.
    if (m_tool == Tool::Select && rotationHandleHit(viewPoint))
    {
        beginRotate(scenePoint);
        return;
    }

    // 1b. Waypoint handle of a selected edge.
    if (m_tool == Tool::Select && hasSelection())
    {
        QString edgeId;
        const int waypointIndex = waypointAt(viewPoint, &edgeId);

        if (waypointIndex >= 0)
        {
            Cell *edge = m_page != nullptr ? m_page->cell(edgeId) : nullptr;

            if (edge != nullptr && waypointIndex < edge->waypoints.size())
            {
                beginSnapshot(QStringLiteral("Move Control Point"));
                m_dragMode = DragMode::Waypoint;
                m_waypointEdgeId = edgeId;
                m_waypointIndex = waypointIndex;
                m_waypointAdded = false;
                m_dragMoved = false;
                update();
                return;
            }
        }
    }

    // 1c. Drag a selected connector segment as a whole. Also allow this
    // while the connector tool is active so dragging a just-created edge does
    // not start another connector.
    if ((m_tool == Tool::Select || m_tool == Tool::Connector) && hasSelection())
    {
        QPointF projected;

        if (Cell *edge = selectedEdgeAt(scenePoint, &projected))
        {
            const RoutedEdge route = EdgeRouter::route(*m_page, *edge);
            int segmentIndex = -1;
            double best = qMax(4.0, 6.0 / qMax(m_zoom, 0.01));

            for (int i = 1; i < route.points.size(); ++i)
            {
                const QPointF a = route.points.at(i - 1);
                const QPointF b = route.points.at(i);
                const QPointF ab = b - a;
                const double lengthSquared = ab.x() * ab.x() + ab.y() * ab.y();

                if (lengthSquared <= 0.0)
                {
                    continue;
                }

                const double t = qBound(0.0, QPointF::dotProduct(scenePoint - a, ab) / lengthSquared, 1.0);
                const double distance = QLineF(a + ab * t, scenePoint).length();

                if (distance < best)
                {
                    best = distance;
                    segmentIndex = i;
                }
            }

            if (segmentIndex > 0)
            {
                // Free floating edges have no endpoints to drag, so moving the
                // body should translate the whole shape instead.
                if (edge->sourceId.isEmpty() && edge->targetId.isEmpty())
                {
                    beginMove(scenePoint);
                    return;
                }

                beginSegmentDrag(edge, route, segmentIndex,
                                 route.points.at(segmentIndex - 1),
                                 route.points.at(segmentIndex), scenePoint);
                return;
            }
        }
    }

    // 2. Connection handle of the hovered shape.
    if (m_tool == Tool::Select && !m_hoverCellId.isEmpty())
    {
        if (Cell *hover = m_page->cell(m_hoverCellId))
        {
            if (hover->isVertex())
            {
                for (const QPointF &center : connectionPoints(*hover))
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

    // Clicking an edge label selects (and drags) the connector, like draw.io.
    if (hit != nullptr && !hit->parentId.isEmpty() && !hit->isEdge())
    {
        Cell *parent = m_page->cell(hit->parentId);

        if (parent != nullptr && parent->isEdge()
            && hit->styleValue(QStringLiteral("shape")).isEmpty())
        {
            hit = parent;
        }
    }

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

    // Ctrl+click on a hyperlinked cell opens the link, like draw.io; on any
    // other cell it starts a connector.
    if (hit != nullptr && (event->modifiers() & Qt::ControlModifier)
        && !event->modifiers().testFlag(Qt::ShiftModifier))
    {
        Cell *target = topLevelSelectable(hit);

        if (!target->link.isEmpty())
        {
            emit linkActivated(target->link);
            return;
        }

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

bool DiagramView::isTransparentBoundsContainer(const Cell *cell) const
{
    if (cell == nullptr || !cell->isVertex())
    {
        return false;
    }

    const QString value = Style::value(Style::withDefaults(cell->style),
                                       QStringLiteral("transparentBounds"));

    return value.compare(QLatin1String("1"), Qt::CaseInsensitive) == 0
        || value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
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

            if (isTransparentBoundsContainer(cell))
            {
                for (Cell *child : m_page->cells())
                {
                    if (child->parentId != cell->id)
                    {
                        continue;
                    }

                    if (child->isVertex())
                    {
                        m_dragStartGeometry.insert(child->id, child->geometry);
                    }
                    else
                    {
                        m_dragStartWaypoints.insert(child->id, child->waypoints);

                        if (child->sourceId.isEmpty())
                        {
                            m_dragStartPoints.insert(child->id + QStringLiteral(":s"),
                                                     child->sourcePoint);
                        }

                        if (child->targetId.isEmpty())
                        {
                            m_dragStartPoints.insert(child->id + QStringLiteral(":t"),
                                                     child->targetPoint);
                        }
                    }
                }
            }
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

Cell *DiagramView::selectedEdgeAt(const QPointF &scenePoint, QPointF *projected) const
{
    if (m_page == nullptr)
    {
        return nullptr;
    }

    Cell *best = nullptr;
    double bestDistance = qMax(4.0, 6.0 / qMax(m_zoom, 0.01));
    const double limit = bestDistance;

    for (const QString &id : m_selection)
    {
        Cell *cell = m_page->cell(id);

        if (cell == nullptr || !cell->isEdge())
        {
            continue;
        }

        const RoutedEdge route = EdgeRouter::route(*m_page, *cell);

        for (int i = 1; i < route.points.size(); ++i)
        {
            const QPointF a = route.points.at(i - 1);
            const QPointF b = route.points.at(i);
            const QPointF ab = b - a;
            const double lengthSquared = ab.x() * ab.x() + ab.y() * ab.y();

            if (lengthSquared <= 0.0)
            {
                continue;
            }

            const double t = qBound(0.0, QPointF::dotProduct(scenePoint - a, ab) / lengthSquared, 1.0);
            const QPointF point = a + ab * t;
            const double distance = QLineF(point, scenePoint).length();

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = cell;

                if (projected != nullptr)
                {
                    *projected = point;
                }
            }
        }
    }

    return bestDistance <= limit ? best : nullptr;
}

int DiagramView::waypointAt(const QPointF &viewPoint, QString *edgeId) const
{
    if (m_page == nullptr)
    {
        return -1;
    }

    for (const QString &id : m_selection)
    {
        Cell *cell = m_page->cell(id);

        if (cell == nullptr || !cell->isEdge())
        {
            continue;
        }

        for (int i = 0; i < cell->waypoints.size(); ++i)
        {
            if (QLineF(toView(cell->waypoints.at(i)), viewPoint).length() <= 8.0)
            {
                if (edgeId != nullptr)
                {
                    *edgeId = id;
                }

                return i;
            }
        }
    }

    return -1;
}

QPointF DiagramView::rotationHandleCenter() const
{
    if (Cell *rotated = singleRotatedVertex())
    {
        const QRectF rect = selectionRectFor(*rotated);
        QTransform transform;
        transform.translate(rect.center().x(), rect.center().y());
        transform.rotate(rotated->effectiveRotation());
        transform.translate(-rect.center().x(), -rect.center().y());
        return toView(transform.map(QPointF(rect.center().x(), rect.top() - 24.0)));
    }

    const QRectF bounds = selectionBoundsInView();
    return QPointF(bounds.center().x(), bounds.top() - 24.0);
}

bool DiagramView::rotationHandleHit(const QPointF &viewPoint) const
{
    if (!hasSelection())
    {
        return false;
    }

    const QPointF center = rotationHandleCenter();
    return QLineF(center, viewPoint).length() <= 10.0;
}

double DiagramView::snapRotationAngle(double angle, bool strict)
{
    // Strict mode (Shift) snaps to every 15° step.
    if (strict)
    {
        return std::round(angle / 15.0) * 15.0;
    }

    // Otherwise magnetically snap near multiples of 45° (0°, 45°, 90°, …).
    constexpr double kSnapStep = 45.0;
    constexpr double kSnapRange = 8.0;
    const double nearest = std::round(angle / kSnapStep) * kSnapStep;

    if (std::abs(angle - nearest) <= kSnapRange)
    {
        return nearest;
    }

    return angle;
}

void DiagramView::beginSegmentDrag(Cell *edge, const RoutedEdge &route, int index,
                                   const QPointF &a, const QPointF &b,
                                   const QPointF &startScene)
{
    m_segmentOriginalWaypoints = edge->waypoints;
    m_dragStartScene = startScene;
    m_segmentBaseA = a;
    m_segmentBaseB = b;
    m_segmentIndexA = -1;
    m_segmentIndexB = -1;

    QVector<QPointF> waypoints;

    for (int i = 1; i < route.points.size() - 1; ++i)
    {
        waypoints.append(route.points.at(i));
    }

    const bool startInserted = index - 1 == 0;

    if (startInserted)
    {
        waypoints.prepend(a);
        m_segmentIndexA = 0;
    }
    else
    {
        m_segmentIndexA = index - 2;
    }

    if (index == route.points.size() - 1)
    {
        waypoints.append(b);
        m_segmentIndexB = waypoints.size() - 1;
    }
    else
    {
        m_segmentIndexB = (index - 1) + (startInserted ? 1 : 0);
    }

    if (m_segmentIndexA < 0 || m_segmentIndexB < 0
        || m_segmentIndexA >= waypoints.size() || m_segmentIndexB >= waypoints.size())
    {
        return;
    }

    beginSnapshot(QStringLiteral("Adjust Connector"));
    edge->waypoints = waypoints;
    m_waypointEdgeId = edge->id;
    m_dragMode = DragMode::Segment;
    m_dragMoved = false;
    update();
}

void DiagramView::collectRotatedDescendants(Cell *cell)
{
    for (Cell *child : cell->children)
    {
        if (!child->isVertex() || child->relativeGeometry)
        {
            continue;
        }

        m_rotateInitialGeometry.insert(child->id, child->geometry);
        m_rotateInitialRotation.insert(child->id, child->effectiveRotation());
        collectRotatedDescendants(child);
    }
}

void DiagramView::rotateDescendants(Cell *cell, double angle)
{
    if (qFuzzyIsNull(angle))
    {
        return;
    }

    // The children of a transparentBounds container orbit the derived box:
    // the stored geometry is (0,0,0,0), so its own centre would be the page
    // origin.
    QPointF pivot(cell->geometry.width() / 2.0, cell->geometry.height() / 2.0);

    if (ShapeRegistry::hasTransparentBounds(*cell))
    {
        const QRectF box = ShapeRegistry::transparentBounds(*cell);

        if (!box.isNull())
        {
            pivot = box.center();
        }
    }
    const double radians = qDegreesToRadians(angle);
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);

    for (Cell *child : cell->children)
    {
        if (!child->isVertex() || child->relativeGeometry)
        {
            continue;
        }

        const QRectF original = m_rotateInitialGeometry.value(child->id, child->geometry);
        const QPointF offset = original.center() - pivot;
        const QPointF rotated(pivot.x() + offset.x() * cosine - offset.y() * sine,
                              pivot.y() + offset.x() * sine + offset.y() * cosine);

        QRectF moved = original;
        moved.moveCenter(rotated);
        child->geometry = moved;

        const double initial = m_rotateInitialRotation.value(child->id, 0.0);
        child->rotation = 0.0;
        child->setStyleValue(QStringLiteral("rotation"),
                             QString::number(initial + angle, 'g', 12));

        rotateDescendants(child, angle);
    }
}

void DiagramView::beginRotate(const QPointF &scenePoint)
{
    m_rotateCenter = selectionBounds().center();
    m_rotateStartAngle = qRadiansToDegrees(std::atan2(scenePoint.y() - m_rotateCenter.y(),
                                                     scenePoint.x() - m_rotateCenter.x()));
    m_rotateInitial.clear();

    m_rotateInitialGeometry.clear();
    m_rotateInitialRotation.clear();

    for (const QString &id : m_selection)
    {
        Cell *cell = m_page != nullptr ? m_page->cell(id) : nullptr;

        if (cell != nullptr && cell->isVertex())
        {
            m_rotateInitial.append({id, cell->effectiveRotation()});
            collectRotatedDescendants(cell);
        }
    }

    if (m_rotateInitial.isEmpty())
    {
        return;
    }

    beginSnapshot(QStringLiteral("Rotation"));
    m_dragMode = DragMode::Rotate;
    m_rotating = true;
    m_dragMoved = false;
    update();
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
    m_resizeRotatedCellId.clear();
    m_resizeRotatedAngle = 0.0;

    if (Cell *rotated = singleRotatedVertex())
    {
        m_resizeRotatedCellId = rotated->id;
        m_resizeRotatedAngle = rotated->effectiveRotation();
    }

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

        m_hoverModifiers = event->modifiers();

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

    if (m_dragMode == DragMode::Rotate)
    {
        const QPointF scenePoint = toScene(event->position());
        const double baseAngle = m_rotateInitial.isEmpty()
                                     ? 0.0
                                     : m_rotateInitial.first().second;
        const double angle = qRadiansToDegrees(std::atan2(scenePoint.y() - m_rotateCenter.y(),
                                                          scenePoint.x() - m_rotateCenter.x()));
        const double delta = snapRotationAngle(baseAngle + angle - m_rotateStartAngle,
                                               (event->modifiers() & Qt::ShiftModifier) != 0)
                             - baseAngle;

        // Restore the descendants first: the rotation is applied from the
        // starting geometry on every move so dragging back is exact.
        for (auto it = m_rotateInitialGeometry.constBegin();
             it != m_rotateInitialGeometry.constEnd(); ++it)
        {
            if (Cell *cell = m_page != nullptr ? m_page->cell(it.key()) : nullptr)
            {
                cell->geometry = it.value();
            }
        }

        for (const auto &entry : m_rotateInitial)
        {
            if (Cell *cell = m_page != nullptr ? m_page->cell(entry.first) : nullptr)
            {
                cell->rotation = 0.0;
                cell->setStyleValue(QStringLiteral("rotation"),
                                    QString::number(entry.second + delta, 'g', 12));
                rotateDescendants(cell, delta);
            }
        }

        m_dragMoved = true;
        update();
        return;
    }

    if (m_dragMode == DragMode::Segment)
    {
        Cell *edge = m_page != nullptr ? m_page->cell(m_waypointEdgeId) : nullptr;

        if (edge != nullptr)
        {
            QPointF delta = scenePoint - m_dragStartScene;
            const bool vertical = qAbs(m_segmentBaseB.y() - m_segmentBaseA.y())
                >= qAbs(m_segmentBaseB.x() - m_segmentBaseA.x());

            // Orthogonal segment drag only moves perpendicular to the segment,
            // exactly like draw.io; dragging along it should not add extra bends.
            if (vertical)
            {
                delta.setY(0.0);
            }
            else
            {
                delta.setX(0.0);
            }

            if (m_segmentIndexA >= 0 && m_segmentIndexA < edge->waypoints.size())
            {
                edge->waypoints[m_segmentIndexA] = m_segmentBaseA + delta;
            }

            if (m_segmentIndexB >= 0 && m_segmentIndexB < edge->waypoints.size())
            {
                edge->waypoints[m_segmentIndexB] = m_segmentBaseB + delta;
            }

            m_dragMoved = true;
            update();
        }

        return;
    }

    if (m_dragMode == DragMode::Waypoint)
    {
        Cell *edge = m_page != nullptr ? m_page->cell(m_waypointEdgeId) : nullptr;

        if (edge != nullptr && m_waypointIndex >= 0
            && m_waypointIndex < edge->waypoints.size())
        {
            edge->waypoints[m_waypointIndex] = scenePoint;
            m_dragMoved = true;
            update();
        }

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
                // draw.io keeps transparentBounds containers at (0,0,0,0) and
                // derives the box from their children, so dragging one moves
                // the children instead of the container geometry.
                if (isTransparentBoundsContainer(cell))
                {
                    for (Cell *child : m_page->cells())
                    {
                        if (child->parentId != cell->id)
                        {
                            continue;
                        }

                        const QRectF original = m_dragStartGeometry.value(child->id,
                                                                          child->geometry);

                        if (child->isVertex())
                        {
                            child->geometry.moveTopLeft(original.topLeft() + delta);
                        }
                        else
                        {
                            const QVector<QPointF> points =
                                m_dragStartWaypoints.value(child->id);

                            for (int i = 0; i < points.size() && i < child->waypoints.size(); ++i)
                            {
                                child->waypoints[i] = points.at(i) + delta;
                            }

                            if (child->sourceId.isEmpty())
                            {
                                child->sourcePoint = m_dragStartPoints.value(
                                                         child->id + QStringLiteral(":s"))
                                                     + delta;
                            }

                            if (child->targetId.isEmpty())
                            {
                                child->targetPoint = m_dragStartPoints.value(
                                                         child->id + QStringLiteral(":t"))
                                                     + delta;
                            }
                        }
                    }

                    continue;
                }

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
    if (!m_resizeRotatedCellId.isEmpty())
    {
        Cell *cell = m_page != nullptr ? m_page->cell(m_resizeRotatedCellId) : nullptr;
        const QRectF original = m_dragStartGeometry.value(m_resizeRotatedCellId);

        if (cell == nullptr || original.isEmpty())
        {
            return;
        }

        QTransform inverse;
        inverse.rotate(-m_resizeRotatedAngle);
        const QPointF delta = inverse.map(scenePoint - m_dragStartScene);
        double left = -original.width() / 2.0;
        double top = -original.height() / 2.0;
        double right = original.width() / 2.0;
        double bottom = original.height() / 2.0;

        switch (m_resizeHandle)
        {
        case 0: left += delta.x(); top += delta.y(); break;
        case 1: top += delta.y(); break;
        case 2: right += delta.x(); top += delta.y(); break;
        case 3: right += delta.x(); break;
        case 4: right += delta.x(); bottom += delta.y(); break;
        case 5: bottom += delta.y(); break;
        case 6: left += delta.x(); bottom += delta.y(); break;
        case 7: left += delta.x(); break;
        default: break;
        }

        if (right - left < 10.0) right = left + 10.0;
        if (bottom - top < 10.0) bottom = top + 10.0;

        if (keepRatio && original.width() > 0 && original.height() > 0)
        {
            const double ratio = original.width() / original.height();
            double width = right - left;
            double height = bottom - top;

            if (width / height > ratio) width = height * ratio;
            else height = width / ratio;

            if (m_resizeHandle == 0 || m_resizeHandle == 6 || m_resizeHandle == 7) left = right - width;
            else right = left + width;

            if (m_resizeHandle == 0 || m_resizeHandle == 1 || m_resizeHandle == 2) top = bottom - height;
            else bottom = top + height;
        }

        const QPointF localCenter((left + right) / 2.0, (top + bottom) / 2.0);
        QTransform rotate;
        rotate.rotate(m_resizeRotatedAngle);
        const QPointF center = original.center() + rotate.map(localCenter);
        const QSizeF size(right - left, bottom - top);
        QRectF mapped(center.x() - size.width() / 2.0, center.y() - size.height() / 2.0,
                      size.width(), size.height());

        if (cell->parent != nullptr && cell->parent->isVertex())
        {
            cell->geometry = QRectF(mapped.topLeft() - cell->parent->absolutePosition(), mapped.size());
        }
        else
        {
            cell->geometry = mapped;
        }

        m_dragMoved = true;
        update();
        return;
    }

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

        m_selection.unite(cellsInMarquee(sceneMarquee));

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

                beginSnapshot(QStringLiteral("Create Connector"));
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

    if (m_dragMode == DragMode::Rotate)
    {
        m_dragMode = DragMode::None;
        m_rotating = false;
        if (m_dragMoved)
        {
            commitSnapshot();
            notifyModified();
        }
        else
        {
            cancelSnapshot();
        }

        m_rotateInitial.clear();
        m_rotateInitialGeometry.clear();
        m_rotateInitialRotation.clear();
        m_dragMoved = false;
        update();
        return;
    }

    if (m_dragMode == DragMode::Segment)
    {
        m_dragMode = DragMode::None;
        Cell *edge = m_page != nullptr ? m_page->cell(m_waypointEdgeId) : nullptr;

        if (m_dragMoved)
        {
            commitSnapshot();
            notifyModified();
        }
        else
        {
            if (edge != nullptr)
            {
                edge->waypoints = m_segmentOriginalWaypoints;
            }

            cancelSnapshot();
        }

        m_segmentOriginalWaypoints.clear();
        m_segmentIndexA = -1;
        m_segmentIndexB = -1;
        m_waypointEdgeId.clear();
        m_dragMoved = false;
        update();
        return;
    }

    if (m_dragMode == DragMode::Waypoint)
    {
        m_dragMode = DragMode::None;

        if (m_dragMoved)
        {
            commitSnapshot();
            notifyModified();
        }
        else
        {
            if (m_waypointAdded)
            {
                if (Cell *edge = m_page != nullptr ? m_page->cell(m_waypointEdgeId) : nullptr)
                {
                    if (!edge->waypoints.isEmpty())
                    {
                        edge->waypoints.removeLast();
                    }
                }
            }

            cancelSnapshot();
        }

        m_waypointEdgeId.clear();
        m_waypointIndex = -1;
        m_waypointAdded = false;
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
                                    wasResize ? QStringLiteral("Resize")
                                              : QStringLiteral("Move"));
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

QSet<QString> DiagramView::cellsInMarquee(const QRectF &sceneMarquee) const
{
    QSet<QString> result;

    if (m_page == nullptr || sceneMarquee.width() <= 2 || sceneMarquee.height() <= 2)
    {
        return result;
    }

    for (Cell *cell : m_page->cells())
    {
        // Only top level cells: edge labels and symbols follow their connector
        // instead of being selected on their own.
        if (!cell->visible || m_page->layer(cell->parentId) == nullptr
            || isLayerLocked(layerIdForCell(cell)))
        {
            continue;
        }

        if (cell->isVertex())
        {
            // transparentBounds containers are painted at the box derived from
            // their children, so the marquee tests that box.
            const QRectF rect = ShapeRegistry::hasTransparentBounds(*cell)
                ? ShapeRegistry::visibleRect(*cell)
                : cell->absoluteRect();

            if (sceneMarquee.intersects(rect))
            {
                result.insert(cell->id);
            }

            continue;
        }

        // Connectors have no area, so test the routed path itself.
        const RoutedEdge route = EdgeRouter::route(*m_page, *cell);

        if (route.points.size() < 2)
        {
            continue;
        }

        QPainterPath path(route.points.first());

        for (int i = 1; i < route.points.size(); ++i)
        {
            path.lineTo(route.points.at(i));
        }

        if (path.intersects(sceneMarquee))
        {
            result.insert(cell->id);
        }
    }

    return result;
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
        // Double clicking a connector edits its label, which draw.io stores in
        // a child cell.
        if (hit->isEdge())
        {
            for (Cell *child : m_page->cells())
            {
                if (child->parentId == hit->id && !child->isEdge()
                    && child->styleValue(QStringLiteral("shape")).isEmpty())
                {
                    selectCell(hit->id, false);
                    startEditing(child->id, true);
                    return;
                }
            }
        }

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

    if (event->key() == Qt::Key_Control && !m_editor->isVisible())
    {
        m_hoverModifiers |= Qt::ControlModifier;
        updateCursorForPosition(mapFromGlobal(QCursor::pos()));
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

        beginSnapshot(QStringLiteral("Move"));
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

    if (event->key() == Qt::Key_Control && !m_editor->isVisible())
    {
        m_hoverModifiers &= ~Qt::ControlModifier;
        updateCursorForPosition(mapFromGlobal(QCursor::pos()));
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

    // Ctrl over a hyperlinked cell shows the "open link" hand, like draw.io.
    if ((m_hoverModifiers & Qt::ControlModifier)
        && m_page != nullptr && !m_hoverCellId.isEmpty())
    {
        Cell *hover = m_page->cell(m_hoverCellId);

        if (hover != nullptr && !hover->link.isEmpty())
        {
            setCursor(Qt::PointingHandCursor);
            return;
        }
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

    beginSnapshot(QStringLiteral("Delete"));
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
        emit statusMessage(I18n::text(QStringLiteral("Copied %1 cell(s)")).arg(m_selection.size()));
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
        emit statusMessage(QStringLiteral("Pasted"));
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
        copy->link = cell->link;
        copy->placeholders = cell->placeholders;
        copy->placeholderName = cell->placeholderName;
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
        emit statusMessage(I18n::text(QStringLiteral("Paste failed: %1")).arg(error));
        return false;
    }

    beginSnapshot(QStringLiteral("Paste"));

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

    beginSnapshot(QStringLiteral("Move"));

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

    beginSnapshot(QStringLiteral("Align"));

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

    beginSnapshot(QStringLiteral("Distribute Horizontally"));

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

    beginSnapshot(QStringLiteral("Distribute Vertically"));

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

    beginSnapshot(QStringLiteral("Resize"));

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

    beginSnapshot(QStringLiteral("Reorder"));

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

    beginSnapshot(QStringLiteral("Group"));

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

    beginSnapshot(QStringLiteral("Ungroup"));

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

    if (cell == nullptr || isLayerLocked(layerIdForCell(cell)))
    {
        return;
    }

    m_editorCellId = cellId;
    m_editorBeforeXml = MxFileIO::pageToXml(*m_page, false);

    QRect viewRect;

    // An edge label is a child cell whose geometry is relative to the edge.
    Cell *labelParent = cell->parentId.isEmpty() ? nullptr : m_page->cell(cell->parentId);

    if (labelParent != nullptr && labelParent->isEdge() && !cell->isEdge()
        && cell->styleValue(QStringLiteral("shape")).isEmpty())
    {
        const RoutedEdge route = EdgeRouter::route(*m_page, *labelParent);
        const QRectF bounds = EdgeRenderer::childBounds(route, *cell);
        const QPoint topLeft = toView(bounds.topLeft()).toPoint();
        const QPoint bottomRight = toView(bounds.bottomRight()).toPoint();
        viewRect = QRect(topLeft, bottomRight).normalized().adjusted(-4, -2, 4, 2);
    }
    else if (cell->isEdge())
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

    QFont font = LabelPainter::fontForLabel(*cell);
    font.setPixelSize(qMax(9, static_cast<int>(font.pixelSize() * m_zoom)));
    m_editor->setFont(font);

    // HTML labels are edited as rich text so that formatting such as the bold
    // Heading of the Textbox template stays visible while editing.
    if (cell->isHtmlLabel())
    {
        m_editor->beginHtml(viewRect, cell->value, selectAll, m_zoom);
    }
    else
    {
        m_editor->begin(viewRect, cell->plainLabel(), selectAll);
    }

    emit editStarted();
}

QString inlineFontStyle(const QString &html)
{
    static const QRegularExpression sizePattern(
        QStringLiteral("font-size\\s*:\\s*([0-9.]+\\s*(?:pt|px))"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression familyPattern(
        QStringLiteral("font-family\\s*:\\s*([^;\"']+)"),
        QRegularExpression::CaseInsensitiveOption);
    QString style;
    QRegularExpressionMatch size;

    for (auto it = sizePattern.globalMatch(html); it.hasNext();)
    {
        size = it.next();
    }

    if (size.hasMatch())
    {
        style += QStringLiteral("font-size: %1;").arg(size.captured(1).trimmed());
    }

    QRegularExpressionMatch family;

    for (auto it = familyPattern.globalMatch(html); it.hasNext();)
    {
        family = it.next();
    }

    if (family.hasMatch())
    {
        style += QStringLiteral("font-family: %1;").arg(family.captured(1).trimmed());
    }

    return style;
}

void DiagramView::commitEditing()
{
    if (!m_editor->isVisible() || m_editorCellId.isEmpty() || m_page == nullptr)
    {
        return;
    }

    Cell *cell = m_page->cell(m_editorCellId);
    const QString text = m_editor->toPlainText();
    const bool rich = m_editor->isRichText();
    const QTextDocument *document = m_editor->document();
    m_editor->hide();

    if (cell == nullptr)
    {
        m_editorCellId.clear();
        return;
    }

    QString value = text;

    if (cell->isHtmlLabel())
    {
        if (rich && !m_editor->contentChanged())
        {
            // Nothing changed: keep the original markup (and its formatting).
            value = cell->value;
        }
        else if (rich)
        {
            value = OverlayEditor::htmlFromDocument(*document);

            // Qt drops the character format when the whole label is replaced;
            // keep the label's original inline typography in that case.
            if (!value.contains(QLatin1String("font-size")))
            {
                const QString inlineStyle = inlineFontStyle(cell->value);

                if (!inlineStyle.isEmpty())
                {
                    value = QStringLiteral("<span style=\"%1\">%2</span>")
                                .arg(inlineStyle, value);
                }
            }
        }
        else
        {
            value = htmlEscape(text);
            value.replace(QStringLiteral("\n"), QStringLiteral("<br>"));

            // Keep Word/draw.io inline typography when the plain-text editor
            // replaces an HTML label; otherwise the cell's fontSize (often 28)
            // takes over and the text size jumps after a double click.
            const QString inlineStyle = inlineFontStyle(cell->value);

            if (!inlineStyle.isEmpty())
            {
                value = QStringLiteral("<span style=\"%1\">%2</span>")
                            .arg(inlineStyle, value);
            }
        }
    }

    const QString before = m_editorBeforeXml;
    const QSet<QString> beforeSelection = m_selection;

    if (value != cell->value)
    {
        cell->value = value;
        const QString after = MxFileIO::pageToXml(*m_page, false);
        pushSnapshotCommand(before, after, beforeSelection, m_selection,
                            QStringLiteral("Edit Text"));
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

bool DiagramView::applyEditorFontStyle(int bit, bool enabled)
{
    if (!isEditing() || m_editor == nullptr || !m_editor->isRichText())
    {
        return false;
    }

    m_editor->setFormat(static_cast<OverlayEditor::Format>(bit), enabled);
    return true;
}

bool DiagramView::applyEditorFontFamily(const QString &family)
{
    if (!isEditing() || m_editor == nullptr || !m_editor->isRichText())
    {
        return false;
    }

    m_editor->setFontFamily(family);
    m_editor->setFocus();
    return true;
}

bool DiagramView::applyEditorFontSize(int pointSize)
{
    if (!isEditing() || m_editor == nullptr || !m_editor->isRichText())
    {
        return false;
    }

    m_editor->setFontSize(pointSize);
    m_editor->setFocus();
    return true;
}

bool DiagramView::applyEditorFontColor(const QString &value)
{
    if (!isEditing() || m_editor == nullptr || !m_editor->isRichText())
    {
        return false;
    }

    if (value.isEmpty() || Style::isNoneColor(value))
    {
        m_editor->setTextColor(palette().color(QPalette::Text));
    }
    else
    {
        const QColor color = Style::parseColorValue(value);

        if (!color.isValid())
        {
            return false;
        }

        m_editor->setTextColor(color);
    }

    m_editor->setFocus();
    return true;
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

    beginSnapshot(QStringLiteral("Edit Text"));
    cell->value = value;
    commitSnapshot();
    update();
}

void DiagramView::setCellLink(const QString &cellId, const QString &link)
{
    if (m_page == nullptr)
    {
        return;
    }

    Cell *cell = m_page->cell(cellId);

    if (cell == nullptr || cell->link == link)
    {
        return;
    }

    beginSnapshot(QStringLiteral("Edit Link"));
    cell->link = link;
    commitSnapshot();
    update();
}

void DiagramView::applyStyle(const QString &key, const QString &value)
{
    if (m_page == nullptr || m_selection.isEmpty())
    {
        return;
    }

    beginSnapshot(QStringLiteral("Edit Style"));

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

    beginSnapshot(QStringLiteral("Edit Style"));

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

    beginSnapshot(QStringLiteral("Move / Resize"));
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

    const double delta = degrees - cell->effectiveRotation();

    beginSnapshot(QStringLiteral("Rotation"));
    collectRotatedDescendants(cell);
    cell->rotation = 0.0;
    cell->setStyleValue(QStringLiteral("rotation"),
                        QString::number(degrees, 'g', 12));
    rotateDescendants(cell, delta);
    m_rotateInitialGeometry.clear();
    m_rotateInitialRotation.clear();
    commitSnapshot();
    update();
}

QString DiagramView::layerIdForCell(const Cell *cell) const
{
    if (m_page == nullptr || cell == nullptr)
    {
        return {};
    }

    QString parentId = cell->parentId;

    while (!parentId.isEmpty())
    {
        if (m_page->layer(parentId) != nullptr)
        {
            return parentId;
        }

        const Cell *parent = m_page->cell(parentId);

        if (parent == nullptr)
        {
            break;
        }

        parentId = parent->parentId;
    }

    return {};
}

bool DiagramView::isLayerLocked(const QString &layerId) const
{
    const Layer *layer = m_page != nullptr ? m_page->layer(layerId) : nullptr;
    return layer != nullptr && layer->locked;
}

void DiagramView::setLayerLocked(const QString &layerId, bool locked)
{
    if (m_page == nullptr)
    {
        return;
    }

    Layer *layer = m_page->layer(layerId);

    if (layer == nullptr || layer->locked == locked)
    {
        return;
    }

    if (!locked && layer->locked)
    {
        // Unlocking is always allowed.
    }

    beginSnapshot(QStringLiteral("Lock Layer"));
    layer->locked = locked;

    if (locked)
    {
        for (auto it = m_selection.begin(); it != m_selection.end();)
        {
            Cell *cell = m_page->cell(*it);

            if (cell != nullptr && layerIdForCell(cell) == layerId)
            {
                it = m_selection.erase(it);
            }
            else
            {
                ++it;
            }
        }

        emit selectionChanged();
    }

    commitSnapshot();
    notifyModified();
    update();
}

void DiagramView::addLayer()
{
    if (m_page == nullptr)
    {
        return;
    }

    beginSnapshot(QStringLiteral("New Layer"));
    Layer layer;
    layer.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    layer.name = I18n::text(QStringLiteral("Layer %1")).arg(m_page->layers.size() + 1);
    layer.visible = true;
    m_page->layers.append(layer);
    m_page->activeLayerId = layer.id;
    commitSnapshot();
    notifyModified();
    update();
}

void DiagramView::removeLayer(const QString &layerId)
{
    if (m_page == nullptr || layerId.isEmpty() || m_page->layers.size() <= 1)
    {
        return;
    }

    const Layer *layerToRemove = m_page->layer(layerId);

    if (layerToRemove != nullptr && layerToRemove->locked)
    {
        return;
    }

    beginSnapshot(QStringLiteral("Delete Layer"));
    const QVector<Cell *> cells = m_page->cells();

    for (Cell *cell : cells)
    {
        if (cell->parentId == layerId)
        {
            m_page->removeCell(cell);
        }
    }

    for (int i = 0; i < m_page->layers.size(); ++i)
    {
        if (m_page->layers.at(i).id == layerId)
        {
            m_page->layers.removeAt(i);
            break;
        }
    }

    if (m_page->activeLayerId == layerId)
    {
        m_page->activeLayerId = m_page->layers.isEmpty()
            ? QStringLiteral("1") : m_page->layers.first().id;
    }

    commitSnapshot();
    notifyModified();
    update();
}

void DiagramView::renameLayer(const QString &layerId, const QString &name)
{
    if (m_page == nullptr)
    {
        return;
    }

    Layer *layer = m_page->layer(layerId);

    if (layer == nullptr || name.isEmpty())
    {
        return;
    }

    beginSnapshot(QStringLiteral("Rename Layer"));
    layer->name = name;
    commitSnapshot();
    notifyModified();
    update();
}

void DiagramView::setActiveLayer(const QString &layerId)
{
    if (m_page != nullptr && m_page->layer(layerId) != nullptr)
    {
        m_page->activeLayerId = layerId;
    }
}

void DiagramView::setLayerVisible(const QString &layerId, bool visible)
{
    if (m_page == nullptr)
    {
        return;
    }

    Layer *layer = m_page->layer(layerId);

    if (layer == nullptr || layer->visible == visible)
    {
        return;
    }

    beginSnapshot(QStringLiteral("Toggle Layer Visibility"));
    layer->visible = visible;
    commitSnapshot();
    notifyModified();
    update();
}

void DiagramView::moveLayer(const QString &layerId, int delta)
{
    if (m_page == nullptr || delta == 0)
    {
        return;
    }

    int index = -1;

    for (int i = 0; i < m_page->layers.size(); ++i)
    {
        if (m_page->layers.at(i).id == layerId)
        {
            index = i;
            break;
        }
    }

    const int target = index + delta;

    if (index < 0 || target < 0 || target >= m_page->layers.size())
    {
        return;
    }

    beginSnapshot(QStringLiteral("Reorder Layers"));
    m_page->layers.swapItemsAt(index, target);
    commitSnapshot();
    notifyModified();
    update();
}

void DiagramView::moveObject(const QString &cellId, int delta)
{
    if (m_page == nullptr || delta == 0)
    {
        return;
    }

    Cell *cell = m_page->cell(cellId);

    if (cell == nullptr)
    {
        return;
    }

    const auto layerOf = [this](const Cell *candidate)
    {
        QString parentId = candidate != nullptr ? candidate->parentId : QString();

        while (!parentId.isEmpty())
        {
            if (m_page->layer(parentId) != nullptr)
            {
                return parentId;
            }

            const Cell *parent = m_page->cell(parentId);

            if (parent == nullptr)
            {
                break;
            }

            parentId = parent->parentId;
        }

        return QString();
    };

    const QString layerId = layerOf(cell);
    const int current = m_page->indexOf(cell);
    int target = current + delta;

    while (target >= 0 && target < m_page->cells().size()
           && layerOf(m_page->cells().at(target)) != layerId)
    {
        target += delta;
    }

    if (current < 0 || target < 0 || target >= m_page->cells().size())
    {
        return;
    }

    beginSnapshot(QStringLiteral("Reorder Object"));
    m_page->moveCell(cell, target);
    commitSnapshot();
    notifyModified();
    update();
}

// --- creation ------------------------------------------------------------

Cell *DiagramView::addShape(const ShapeDefinition &definition, const QPointF &scenePos)
{
    if (m_page == nullptr)
    {
        return nullptr;
    }

    if (isLayerLocked(m_page->activeLayerId))
    {
        emit statusMessage(QStringLiteral("The current layer is locked; cannot add objects"));
        return nullptr;
    }

    beginSnapshot(QStringLiteral("Add Shape"));

    const QSizeF size = definition.size;
    QPointF topLeft = scenePos - QPointF(size.width() / 2.0, size.height() / 2.0);

    if (m_page->settings.grid)
    {
        const int grid = qMax(1, m_page->settings.gridSize);
        topLeft = QPointF(qRound(topLeft.x() / grid) * grid,
                          qRound(topLeft.y() / grid) * grid);
    }

    StyleMap style = Style::parse(definition.style);

    // New shapes default to a 2px outline, matching the shape palette previews.
    if (!style.contains(QStringLiteral("strokeWidth")))
    {
        style.insert(QStringLiteral("strokeWidth"), QStringLiteral("2"));
    }

    Cell *cell = nullptr;

    // draw.io's transparentBounds containers (live layout / tree containers)
    // keep their geometry pinned at (0,0,0,0); the visible box is derived from
    // the children plus padding, and the children carry the page coordinates.
    const bool transparentBounds = Style::value(
        Style::withDefaults(style), QStringLiteral("transparentBounds"))
        == QLatin1String("1");

    // draw.io's wedge arrows and the General palette's line/arrow templates
    // are edge shapes; inserting them as vertices would save a cell the
    // original editor cannot paint. Create a floating edge instead.
    if (definition.edge
        || Style::value(style, QStringLiteral("shape")).startsWith(
               QLatin1String("mxgraph.arrows2.wedgeArrow")))
    {
        cell = m_page->addEdge(QString(), QString(), style, definition.value);

        if (definition.edge)
        {
            // Edge templates run from the bottom left to the top right corner
            // of their bounds (Sidebar.createEdgeTemplate).
            cell->sourcePoint = topLeft + QPointF(0, size.height());
            cell->targetPoint = topLeft + QPointF(size.width(), 0);

            for (const QPointF &point : definition.waypoints)
            {
                cell->waypoints.append(topLeft + point);
            }
        }
        else
        {
            cell->sourcePoint = topLeft;
            cell->targetPoint = topLeft + QPointF(size.width(), 0);
        }
    }
    else
    {
        cell = m_page->addVertex(transparentBounds ? QRectF()
                                                   : QRectF(topLeft, size),
                                 style, definition.value);
    }

    // Palette entries such as the List container create their child cells too.
    if (cell != nullptr)
    {
        cell->link = definition.link;
        cell->placeholders = definition.placeholders;
        cell->placeholderName = definition.placeholderName;

        QVector<Cell *> vertices;
        QVector<QPair<const ShapeChildDefinition *, Cell *>> edges;

        for (const ShapeChildDefinition &child : definition.children)
        {
            if (child.edge)
            {
                edges.append({&child, nullptr});
                continue;
            }

            Cell *created = m_page->addVertex(
                transparentBounds ? child.geometry.translated(topLeft) : child.geometry,
                Style::parse(child.style), child.value, cell->id);

            if (created != nullptr)
            {
                created->relativeGeometry = child.relative;
                created->geometryOffset = child.offset;
                vertices.append(created);
            }
        }

        // Connectors of layout containers run between the entry's nodes.
        for (const auto &entry : edges)
        {
            const int source = entry.first->source;
            const int target = entry.first->target;

            if (source < 0 || target < 0 || source >= vertices.size()
                || target >= vertices.size())
            {
                continue;
            }

            Cell *edge = m_page->addEdge(vertices.at(source)->id, vertices.at(target)->id,
                                         Style::parse(entry.first->style),
                                         entry.first->value, cell->id);

            if (edge != nullptr)
            {
                edge->relativeGeometry = true;
            }
        }
    }

    m_selection = {cell->id};
    commitSnapshot();
    notifyModified();
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

    beginSnapshot(QStringLiteral("Create Connector"));
    Cell *edge = m_page->addEdge(vertices.at(0)->id, vertices.at(1)->id,
                                 Style::parse(ShapeRegistry::defaultEdgeStyle()));

    if (edge != nullptr)
    {
        m_selection = {edge->id};
    }

    commitSnapshot();
    notifyModified();
    update();
    emit selectionChanged();
    return edge;
}

} // namespace dq
