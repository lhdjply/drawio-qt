#pragma once

#include "render/ShapeRegistry.h"

#include <QWidget>

class QListWidget;
class QListWidgetItem;

namespace dq {

/// The left hand shape palette. Shapes can be dragged onto the canvas or
/// inserted with a double click (centred in the current view).
class ShapeToolbox : public QWidget
{
    Q_OBJECT

public:
    explicit ShapeToolbox(QWidget *parent = nullptr);

signals:
    void shapeActivated(const ShapeDefinition &definition);

private:
    void populate();
    void emitActivated(QListWidgetItem *item);

    QListWidget *m_list = nullptr;
};

} // namespace dq
