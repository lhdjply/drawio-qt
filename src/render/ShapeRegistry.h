#pragma once

#include "core/Cell.h"

#include <QPainterPath>
#include <QSizeF>
#include <QString>
#include <QVector>

namespace dq {

/// A shape offered by the left hand shape palette.
struct ShapeDefinition
{
    QString key;
    QString name;
    QString style;
    QSizeF size = QSizeF(120, 60);
    QString category = QStringLiteral("basic");
    QString imagePath; // set for shapes rendered from a bundled SVG/PNG
};

/// Geometry and defaults for the shape library. The names follow draw.io's
/// style keys so files round-trip between the two implementations.
namespace ShapeRegistry {

QVector<ShapeDefinition> toolboxShapes();
ShapeDefinition definition(const QString &key);

/// Builds the outline for a vertex in local (cell) coordinates.
QPainterPath pathFor(const Cell &cell);
QPainterPath pathForStyle(const StyleMap &style, const QRectF &rect);

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
