#pragma once

#include "core/Style.h"

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

namespace dq {

class Page;

/// A single diagram cell. draw.io has exactly two cell kinds at the model
/// level: vertices (`vertex="1"`) and edges (`edge="1"`). Groups, containers
/// and layers are vertices with children.
class Cell
{
public:
    enum class Kind
    {
        Vertex,
        Edge
    };

    Cell(Kind kind, const QString &id);
    ~Cell();

    Cell(const Cell &) = delete;
    Cell &operator=(const Cell &) = delete;

    Kind kind() const { return m_kind; }
    bool isVertex() const { return m_kind == Kind::Vertex; }
    bool isEdge() const { return m_kind == Kind::Edge; }

    QString id;
    QString value;
    StyleMap style;
    QString parentId = QStringLiteral("1");
    bool visible = true;

    /// draw.io stores hyperlinks and placeholders in a UserObject wrapper.
    /// The label template stays in `value` (so editing shows `%date{}%`),
    /// while displayLabel() expands it for painting and export.
    QString link;
    bool placeholders = false;
    QString placeholderName;

    /// draw.io stores the folded state as a cell attribute and the size to
    /// restore on expand in <alternateBounds>.
    bool collapsed = false;
    QRectF alternateBounds;

    // Vertex geometry, expressed in the coordinate space of the parent.
    QRectF geometry;
    double rotation = 0.0;
    /// draw.io's `relative="1"` geometry: the cell is placed on its parent
    /// edge (edge labels and symbols such as `shape=message`).
    bool relativeGeometry = false;
    /// `<mxPoint as="offset">` of the geometry, in page units.
    QPointF geometryOffset;

    // Edge terminals. A terminal is either a cell reference or a free point.
    QString sourceId;
    QString targetId;
    QPointF sourcePoint;
    QPointF targetPoint;
    QVector<QPointF> waypoints;

    Cell *parent = nullptr;
    QVector<Cell *> children;

    Page *page = nullptr;

    // --- helpers ---------------------------------------------------------
    QString styleValue(const QString &key, const QString &fallback = QString()) const;
    bool styleFlag(const QString &key, bool fallback = false) const;
    double styleNumber(const QString &key, double fallback = 0.0) const;
    QColor styleColor(const QString &key, const QColor &fallback = QColor()) const;
    void setStyleValue(const QString &key, const QString &value);

    bool isHtmlLabel() const;
    bool hasFill() const;
    bool hasStroke() const;

    /// Vertex rectangle in the page coordinate system (unrotated).
    QRectF absoluteRect() const;
    /// Axis aligned bounds after applying the cell's rotation (geometry or
    /// style). Connection routing and hit testing use these bounds.
    QRectF visualRect() const;
    double effectiveRotation() const;
    /// Top-left of the cell in the page coordinate system.
    QPointF absolutePosition() const;
    QPointF center() const;

    QPointF mapToAbsolute(const QPointF &local) const;
    QPointF mapFromAbsolute(const QPointF &absolute) const;

    /// The label with draw.io placeholders expanded (`%name%`,
    /// `%date{...}%`). HTML markup is preserved.
    QString displayLabel() const;

    /// Text shown in the label, with placeholders expanded and basic HTML
    /// entities/tags resolved.
    QString plainLabel() const;

    /// Container cells (groups, swimlanes) clip and move their children.
    bool isContainer() const;
    bool isGroup() const;
    /// True for labels and symbols placed on an edge (parent is an edge).
    bool isEdgeChild() const;

    /// Walks up the parent chain. Never returns the root layer.
    Cell *topLevelAncestor() const;

    bool contains(const QPointF &absolutePoint) const;

    int depth() const;

private:
    Kind m_kind;
};

} // namespace dq
