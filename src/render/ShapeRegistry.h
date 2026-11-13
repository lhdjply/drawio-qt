#pragma once

#include "core/Cell.h"

#include <QPainterPath>
#include <QSizeF>
#include <QString>
#include <QVector>

namespace dq {

/// A child cell created together with a palette entry (the items of the List
/// container, the labels and symbol of a connector template).
struct ShapeChildDefinition
{
    QString style;
    QString value;
    QRectF geometry;
    QPointF offset;
    bool relative = false;
};

/// A shape offered by the left hand shape palette.
struct ShapeDefinition
{
    QString key;
    QString name;
    QString style;
    QString value;
    QSizeF size = QSizeF(120, 60);
    QString category = QStringLiteral("basic");
    QString imagePath; // set for shapes rendered from a bundled SVG/PNG

    /// draw.io's General palette also holds edge templates (lines, arrows and
    /// connectors); those are inserted as floating edges, not vertices.
    bool edge = false;
    /// Waypoints of an edge template, relative to the cell's top-left corner.
    QVector<QPointF> waypoints;
    QVector<ShapeChildDefinition> children;
};

/// Geometry and defaults for the shape library. The names follow draw.io's
/// style keys so files round-trip between the two implementations.
namespace ShapeRegistry {

QVector<ShapeDefinition> toolboxShapes();
ShapeDefinition definition(const QString &key);

/// Builds the outline for a vertex in local (cell) coordinates.
QPainterPath pathFor(const Cell &cell);
QPainterPath pathForStyle(const StyleMap &style, const QRectF &rect);

/// Canonical shape key for a style map (handles mxgraph.basic stencil names).
QString canonicalKey(const StyleMap &style);

/// Appends an SVG style elliptical arc to \a path (mxStencil <arc> semantics).
void appendSvgArc(QPainterPath &path, const QPointF &from, const QPointF &to,
                  double rx, double ry, bool largeArc, bool sweep);

bool isTextShape(const Cell &cell);
bool isImageShape(const Cell &cell);
bool isLineArt(const Cell &cell);
bool isContainerShape(const Cell &cell);

/// Applies a shape preset to an existing cell.
void applyDefinition(Cell &cell, const ShapeDefinition &definition);

QString defaultVertexStyle();
QString defaultEdgeStyle();

} // namespace ShapeRegistry
} // namespace dq
