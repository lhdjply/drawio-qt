#pragma once

#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
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
    void applyTheme(bool dark);
    void compactGroups();

private:
    void buildUi();
    void updateEnabled(bool hasSelection, bool hasVertex, bool hasEdge);

    DiagramView *m_view = nullptr;
    bool m_updating = false;
    bool m_dark = false;
    QVector<QGroupBox *> m_groups;

    QDoubleSpinBox *m_x = nullptr;
    QDoubleSpinBox *m_y = nullptr;
    QDoubleSpinBox *m_w = nullptr;
    QDoubleSpinBox *m_h = nullptr;
    QDoubleSpinBox *m_rotation = nullptr;

    QSpinBox *m_strokeWidth = nullptr;
    QSpinBox *m_fontSize = nullptr;
    QComboBox *m_fontFamily = nullptr;
    QSpinBox *m_opacity = nullptr;

    QCheckBox *m_fillEnabled = nullptr;
    QCheckBox *m_gradientEnabled = nullptr;
    QCheckBox *m_strokeEnabled = nullptr;
    QCheckBox *m_rounded = nullptr;
    QCheckBox *m_sketch = nullptr;
    QCheckBox *m_glass = nullptr;
    QCheckBox *m_shadow = nullptr;
    QCheckBox *m_multiple = nullptr;
    QComboBox *m_lineStyle = nullptr;
    QSpinBox *m_perimeter = nullptr;

    QComboBox *m_gradientDirection = nullptr;
    QComboBox *m_edgeStyle = nullptr;
    QComboBox *m_startArrow = nullptr;
    QComboBox *m_endArrow = nullptr;
    QSpinBox *m_startSize = nullptr;
    QSpinBox *m_endSize = nullptr;
    QComboBox *m_align = nullptr;
    QComboBox *m_verticalAlign = nullptr;

    ColorButton *m_fill = nullptr;
    ColorButton *m_gradient = nullptr;
    ColorButton *m_stroke = nullptr;
    ColorButton *m_fontColor = nullptr;

    QLabel *m_hint = nullptr;
    QGroupBox *m_edgeGroup = nullptr;

    QToolButton *m_bold = nullptr;
    QToolButton *m_italic = nullptr;
    QToolButton *m_underline = nullptr;

    QString m_selectedId;
};

} // namespace dq
