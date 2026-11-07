#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QToolButton;

namespace dq {

class ColorButton;
class DiagramView;

/// The right hand format panel (draw.io's "Format" panel). It edits the style
/// and geometry of the current selection through DiagramView, which keeps the
/// changes undoable.
class FormatPanel : public QWidget
{
    Q_OBJECT

public:
    explicit FormatPanel(QWidget *parent = nullptr);

    void setView(DiagramView *view);
    void updateFromSelection();

private:
    void buildUi();
    void updateEnabled(bool hasSelection, bool hasVertex, bool hasEdge);

    DiagramView *m_view = nullptr;
    bool m_updating = false;

    QDoubleSpinBox *m_x = nullptr;
    QDoubleSpinBox *m_y = nullptr;
    QDoubleSpinBox *m_w = nullptr;
    QDoubleSpinBox *m_h = nullptr;
    QDoubleSpinBox *m_rotation = nullptr;

    QSpinBox *m_strokeWidth = nullptr;
    QSpinBox *m_fontSize = nullptr;
    QSpinBox *m_opacity = nullptr;

    QCheckBox *m_dashed = nullptr;
    QCheckBox *m_rounded = nullptr;
    QCheckBox *m_shadow = nullptr;

    QComboBox *m_edgeStyle = nullptr;
    QComboBox *m_startArrow = nullptr;
    QComboBox *m_endArrow = nullptr;
    QComboBox *m_align = nullptr;
    QComboBox *m_verticalAlign = nullptr;

    ColorButton *m_fill = nullptr;
    ColorButton *m_stroke = nullptr;
    ColorButton *m_fontColor = nullptr;

    QToolButton *m_bold = nullptr;
    QToolButton *m_italic = nullptr;
    QToolButton *m_underline = nullptr;

    QString m_selectedId;
};

} // namespace dq
