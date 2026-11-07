#pragma once

#include "render/ShapeRegistry.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QTimer;

namespace dq {

/// The shape palette. Categories and search cover both the procedural built-in
/// library and the bundled draw.io image libraries. Shapes can be dragged onto
/// the canvas or inserted with a double click.
class ShapeToolbox : public QWidget
{
    Q_OBJECT

public:
    explicit ShapeToolbox(QWidget *parent = nullptr);

    void reload();

signals:
    void shapeActivated(const ShapeDefinition &definition);

private:
    void populateCategories();
    void populateList();
    void manageLibraries();
    void emitActivated(QListWidgetItem *item);
    QStringList enabledCategories() const;

    QComboBox *m_category = nullptr;
    QLineEdit *m_search = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_count = nullptr;
    QTimer *m_searchTimer = nullptr;
};

} // namespace dq
