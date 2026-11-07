#pragma once

#include "core/Cell.h"

#include <QFont>
#include <QPainter>
#include <QRectF>

namespace dq {

/// Paints cell labels. Handles the subset of draw.io's label styling that the
/// native editor supports: font family/size/style/colour, horizontal and
/// vertical alignment, word wrap, background and a small HTML subset.
namespace LabelPainter {

QFont fontFor(const Cell &cell);
QFont fontForLabel(const Cell &cell);
QColor textColor(const Cell &cell);
QRectF labelRect(const Cell &cell, const QRectF &bounds);
Qt::Alignment alignmentFor(const Cell &cell);

void paint(QPainter &painter, const Cell &cell, const QRectF &bounds,
           bool drawBackground = true);

} // namespace LabelPainter

/// Paints vertices (fill, stroke, gradient, shadow, label and the shape
/// specific decorations such as the cylinder top or the swimlane header).
namespace ShapeRenderer {

QColor fillColor(const Cell &cell);
QColor strokeColor(const Cell &cell);

void paint(QPainter &painter, const Cell &cell);
void paintShadow(QPainter &painter, const Cell &cell);

} // namespace ShapeRenderer

} // namespace dq
