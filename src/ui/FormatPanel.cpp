#include "ui/FormatPanel.h"

#include "core/Page.h"
#include "ui/DiagramView.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPainter>
#include <QPixmap>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace dq {
namespace {

const QString kNoneValue = QStringLiteral("none");

QToolButton *makeToggle(const QString &text, const QString &tooltip, QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setText(text);
    button->setToolTip(tooltip);
    button->setCheckable(true);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setMinimumWidth(30);
    return button;
}


} // namespace

/// A tool button that shows a colour swatch and reports the style value
/// (`#rrggbb`, `none`, or empty when unset).
class ColorButton : public QToolButton
{
    Q_OBJECT

public:
    explicit ColorButton(QWidget *parent = nullptr)
        : QToolButton(parent)
    {
        setMinimumSize(64, 24);
        setToolButtonStyle(Qt::ToolButtonTextOnly);
        setStyleSheet(QStringLiteral(
            "QToolButton { padding-left: 22px; text-align: left; }"));
        setText(QStringLiteral("…"));
        setToolTip(QStringLiteral("单击选择颜色，右键清除"));
        setContextMenuPolicy(Qt::CustomContextMenu);

        connect(this, &QToolButton::clicked, this, [this]()
        {
            const QColor initial = m_color.isValid() ? m_color : Qt::white;
            const QColor chosen = QColorDialog::getColor(initial, this,
                                                         QStringLiteral("选择颜色"),
                                                         QColorDialog::ShowAlphaChannel);

            if (chosen.isValid())
            {
                setStyleValue(chosen.alpha() == 255
                                  ? chosen.name()
                                  : chosen.name(QColor::HexArgb));
                emit valueChanged(m_value);
            }
        });

        connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos)
        {
            QMenu menu(this);
            QAction *none = menu.addAction(QStringLiteral("无颜色"));
            QAction *defaultColor = menu.addAction(QStringLiteral("恢复默认"));
            QAction *chosen = menu.exec(mapToGlobal(pos));

            if (chosen == none)
            {
                setStyleValue(kNoneValue);
                emit valueChanged(m_value);
            }
            else if (chosen == defaultColor)
            {
                setStyleValue(QString());
                emit valueChanged(m_value);
            }
        });
    }

    QString styleValue() const { return m_value; }

    void setStyleValue(const QString &value)
    {
        m_value = value;

        if (value.isEmpty())
        {
            m_color = QColor();
            setText(QStringLiteral("默认"));
        }
        else if (value == kNoneValue)
        {
            m_color = QColor();
            setText(QStringLiteral("无"));
        }
        else
        {
            m_color = QColor(value);
            setText(m_color.name().toUpper());
        }

        update();
    }

signals:
    void valueChanged(const QString &value);

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QToolButton::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);
        const QRect swatch(4, 4, 14, height() - 8);

        if (m_color.isValid())
        {
            painter.fillRect(swatch, m_color);
            painter.setPen(QColor(120, 120, 120));
            painter.drawRect(swatch);
        }
        else
        {
            painter.fillRect(swatch, Qt::white);
            painter.setPen(QColor(120, 120, 120));
            painter.drawRect(swatch);
            painter.drawLine(swatch.topLeft(), swatch.bottomRight());
        }
    }

private:
    QString m_value;
    QColor m_color;
};

namespace {

QIcon arrowPreviewIcon(const QString &type)
{
    const int w = 48;
    const int h = 24;
    QPixmap pixmap(w, h);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor ink(0x5F, 0x63, 0x68);
    const QPointF tip(40, 12);
    const QPointF back(30, 12);
    QPen pen(ink, 1.6);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(ink);

    if (type == QStringLiteral("none") || type.isEmpty())
    {
        painter.drawLine(QPointF(3, 12), tip);
    }
    else if (type == QStringLiteral("classic") || type == QStringLiteral("classicThin"))
    {
        const double f = type == QStringLiteral("classicThin") ? 0.75 : 1.0;
        painter.drawLine(QPointF(3, 12), back);
        QPolygonF p;
        p << tip << back + QPointF(0, -5 * f) << QPointF(32.5, 12)
          << back + QPointF(0, 5 * f);
        painter.drawPolygon(p);
    }
    else if (type == QStringLiteral("block") || type == QStringLiteral("blockThin"))
    {
        const double f = type == QStringLiteral("blockThin") ? 0.7 : 1.0;
        painter.drawLine(QPointF(3, 12), back);
        QPolygonF p;
        p << tip << back + QPointF(0, -5 * f) << back + QPointF(0, 5 * f);
        painter.drawPolygon(p);
    }
    else if (type == QStringLiteral("open") || type == QStringLiteral("openThin"))
    {
        const double f = type == QStringLiteral("openThin") ? 0.65 : 1.0;
        painter.drawLine(QPointF(3, 12), QPointF(33, 12));
        painter.drawLine(tip, back + QPointF(0, -6 * f));
        painter.drawLine(tip, back + QPointF(0, 6 * f));
    }
    else if (type == QStringLiteral("oval"))
    {
        painter.drawLine(QPointF(3, 12), QPointF(30, 12));
        painter.drawEllipse(QPointF(34, 12), 4, 4);
    }
    else if (type == QStringLiteral("diamond") || type == QStringLiteral("diamondThin"))
    {
        const double f = type == QStringLiteral("diamondThin") ? 0.55 : 1.0;
        painter.drawLine(QPointF(3, 12), QPointF(28, 12));
        QPolygonF p;
        p << tip << QPointF(34, 12 - 6 * f) << QPointF(28, 12)
          << QPointF(34, 12 + 6 * f);
        painter.drawPolygon(p);
    }
    else if (type == QStringLiteral("baseDash"))
    {
        painter.drawLine(QPointF(3, 12), QPointF(33, 12));
        painter.drawLine(QPointF(33, 6), QPointF(33, 18));
    }
    else if (type == QStringLiteral("doubleBlock"))
    {
        painter.drawLine(QPointF(3, 12), QPointF(24, 12));
        for (int i = 0; i < 2; ++i)
        {
            const double x = 30 + i * 6;
            QPolygonF p;
            p << QPointF(x + 4, 12) << QPointF(x, 7) << QPointF(x, 17);
            painter.drawPolygon(p);
        }
    }
    else if (type == QStringLiteral("flexArrow"))
    {
        QPolygonF arrow;
        arrow << QPointF(4, 9) << QPointF(30, 9) << QPointF(30, 4)
              << QPointF(44, 12) << QPointF(30, 20) << QPointF(30, 15)
              << QPointF(4, 15);
        painter.drawPolygon(arrow);
    }
    else
    {
        painter.drawLine(QPointF(3, 12), QPointF(26, 12));
        painter.drawEllipse(QPointF(30, 12), 4, 4);
        painter.drawLine(QPointF(34, 12), QPointF(40, 12));
        painter.drawLine(QPointF(38, 12), QPointF(36, 9));
        painter.drawLine(QPointF(38, 12), QPointF(36, 15));
    }

    painter.end();
    return QIcon(pixmap);
}

} // namespace

FormatPanel::FormatPanel(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

void FormatPanel::buildUi()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    outer->addWidget(scroll);

    auto *content = new QWidget(scroll);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    scroll->setWidget(content);

    m_hint = new QLabel(QStringLiteral("在画布中选择图形\n以编辑其样式与文本"), content);
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setWordWrap(true);
    m_hint->setMinimumHeight(120);
    layout->addWidget(m_hint);

    // --- geometry ---------------------------------------------------------
    auto *geometryGroup = new QGroupBox(QStringLiteral("位置与大小"), content);
    geometryGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *geometryGrid = new QGridLayout(geometryGroup);
    geometryGrid->setHorizontalSpacing(6);
    geometryGrid->setVerticalSpacing(2);

    auto makeSpin = [&](double min, double max, const QString &suffix)
    {
        auto *spin = new QDoubleSpinBox(geometryGroup);
        spin->setRange(min, max);
        spin->setDecimals(0);
        spin->setSingleStep(1);
        spin->setSuffix(suffix);
        spin->setKeyboardTracking(false);
        spin->setMinimumWidth(64);
        spin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        return spin;
    };

    m_x = makeSpin(-100000, 100000, QStringLiteral(" px"));
    m_y = makeSpin(-100000, 100000, QStringLiteral(" px"));
    m_w = makeSpin(1, 100000, QStringLiteral(" px"));
    m_h = makeSpin(1, 100000, QStringLiteral(" px"));
    m_rotation = makeSpin(-360, 360, QStringLiteral(" °"));

    geometryGrid->addWidget(new QLabel(QStringLiteral("X"), geometryGroup), 0, 0);
    geometryGrid->addWidget(m_x, 0, 1);
    geometryGrid->addWidget(new QLabel(QStringLiteral("Y"), geometryGroup), 0, 2);
    geometryGrid->addWidget(m_y, 0, 3);
    geometryGrid->addWidget(new QLabel(QStringLiteral("宽"), geometryGroup), 1, 0);
    geometryGrid->addWidget(m_w, 1, 1);
    geometryGrid->addWidget(new QLabel(QStringLiteral("高"), geometryGroup), 1, 2);
    geometryGrid->addWidget(m_h, 1, 3);
    geometryGrid->addWidget(new QLabel(QStringLiteral("角度"), geometryGroup), 2, 0);
    geometryGrid->addWidget(m_rotation, 2, 1, 1, 3);

    layout->addWidget(geometryGroup);

    connect(m_x, &QDoubleSpinBox::editingFinished, this, [this]()
    {
        if (!m_updating)
        {
            m_view->applyGeometry(m_x->value(), m_y->value(), m_w->value(), m_h->value());
        }
    });
    connect(m_y, &QDoubleSpinBox::editingFinished, this, [this]()
    {
        if (!m_updating)
        {
            m_view->applyGeometry(m_x->value(), m_y->value(), m_w->value(), m_h->value());
        }
    });
    connect(m_w, &QDoubleSpinBox::editingFinished, this, [this]()
    {
        if (!m_updating)
        {
            m_view->applyGeometry(m_x->value(), m_y->value(), m_w->value(), m_h->value());
        }
    });
    connect(m_h, &QDoubleSpinBox::editingFinished, this, [this]()
    {
        if (!m_updating)
        {
            m_view->applyGeometry(m_x->value(), m_y->value(), m_w->value(), m_h->value());
        }
    });
    connect(m_rotation, &QDoubleSpinBox::editingFinished, this, [this]()
    {
        if (!m_updating)
        {
            m_view->setRotation(m_rotation->value());
        }
    });

    // --- style ------------------------------------------------------------
    auto *styleGroup = new QGroupBox(QStringLiteral("样式"), content);
    styleGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *styleForm = new QFormLayout(styleGroup);
    styleForm->setSpacing(4);

    m_fill = new ColorButton(styleGroup);
    m_stroke = new ColorButton(styleGroup);
    m_fontColor = new ColorButton(styleGroup);
    styleForm->addRow(QStringLiteral("填充"), m_fill);
    styleForm->addRow(QStringLiteral("边框"), m_stroke);

    m_strokeWidth = new QSpinBox(styleGroup);
    m_strokeWidth->setRange(0, 40);
    m_strokeWidth->setSuffix(QStringLiteral(" px"));
    m_strokeWidth->setMinimumWidth(64);
    styleForm->addRow(QStringLiteral("线宽"), m_strokeWidth);

    m_opacity = new QSpinBox(styleGroup);
    m_opacity->setRange(0, 100);
    m_opacity->setSuffix(QStringLiteral(" %"));
    m_opacity->setMinimumWidth(64);
    styleForm->addRow(QStringLiteral("不透明度"), m_opacity);

    auto *flagRow = new QHBoxLayout();
    m_dashed = new QCheckBox(QStringLiteral("虚线"), styleGroup);
    m_rounded = new QCheckBox(QStringLiteral("圆角"), styleGroup);
    m_shadow = new QCheckBox(QStringLiteral("阴影"), styleGroup);
    flagRow->addWidget(m_dashed);
    flagRow->addWidget(m_rounded);
    flagRow->addWidget(m_shadow);
    styleForm->addRow(QString(), flagRow);

    layout->addWidget(styleGroup);

    connect(m_fill, &ColorButton::valueChanged, this, [this](const QString &value)
    {
        value.isEmpty() ? m_view->clearStyle(QStringLiteral("fillColor"))
                        : m_view->applyStyle(QStringLiteral("fillColor"), value);
    });
    connect(m_stroke, &ColorButton::valueChanged, this, [this](const QString &value)
    {
        value.isEmpty() ? m_view->clearStyle(QStringLiteral("strokeColor"))
                        : m_view->applyStyle(QStringLiteral("strokeColor"), value);
    });
    connect(m_strokeWidth, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value)
    {
        if (!m_updating)
        {
            m_view->applyStyle(QStringLiteral("strokeWidth"), QString::number(value));
        }
    });
    connect(m_opacity, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value)
    {
        if (!m_updating)
        {
            m_view->applyStyle(QStringLiteral("opacity"), QString::number(value));
        }
    });
    connect(m_dashed, &QCheckBox::toggled, this, [this](bool checked)
    {
        if (!m_updating)
        {
            checked ? m_view->applyStyle(QStringLiteral("dashed"), QStringLiteral("1"))
                    : m_view->clearStyle(QStringLiteral("dashed"));
        }
    });
    connect(m_rounded, &QCheckBox::toggled, this, [this](bool checked)
    {
        if (!m_updating)
        {
            checked ? m_view->applyStyle(QStringLiteral("rounded"), QStringLiteral("1"))
                    : m_view->applyStyle(QStringLiteral("rounded"), QStringLiteral("0"));
        }
    });
    connect(m_shadow, &QCheckBox::toggled, this, [this](bool checked)
    {
        if (!m_updating)
        {
            checked ? m_view->applyStyle(QStringLiteral("shadow"), QStringLiteral("1"))
                    : m_view->clearStyle(QStringLiteral("shadow"));
        }
    });

    // --- text -------------------------------------------------------------
    auto *textGroup = new QGroupBox(QStringLiteral("文本"), content);
    textGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *textForm = new QFormLayout(textGroup);
    textForm->setSpacing(4);

    m_fontSize = new QSpinBox(textGroup);
    m_fontSize->setRange(4, 200);
    m_fontSize->setSuffix(QStringLiteral(" px"));
    m_fontSize->setMinimumWidth(64);
    textForm->addRow(QStringLiteral("字号"), m_fontSize);
    textForm->addRow(QStringLiteral("颜色"), m_fontColor);

    m_align = new QComboBox(textGroup);
    m_align->addItem(QStringLiteral("左对齐"), QStringLiteral("left"));
    m_align->addItem(QStringLiteral("居中"), QStringLiteral("center"));
    m_align->addItem(QStringLiteral("右对齐"), QStringLiteral("right"));
    textForm->addRow(QStringLiteral("水平对齐"), m_align);

    m_verticalAlign = new QComboBox(textGroup);
    m_verticalAlign->addItem(QStringLiteral("顶部"), QStringLiteral("top"));
    m_verticalAlign->addItem(QStringLiteral("居中"), QStringLiteral("middle"));
    m_verticalAlign->addItem(QStringLiteral("底部"), QStringLiteral("bottom"));
    textForm->addRow(QStringLiteral("垂直对齐"), m_verticalAlign);

    auto *formatRow = new QHBoxLayout();
    m_bold = makeToggle(QStringLiteral("B"), QStringLiteral("粗体"), textGroup);
    m_italic = makeToggle(QStringLiteral("I"), QStringLiteral("斜体"), textGroup);
    m_underline = makeToggle(QStringLiteral("U"), QStringLiteral("下划线"), textGroup);
    m_bold->setStyleSheet(QStringLiteral("font-weight: bold;"));

    m_underline->setStyleSheet(QStringLiteral("text-decoration: underline;"));
    formatRow->addWidget(m_bold);
    formatRow->addWidget(m_italic);
    formatRow->addWidget(m_underline);
    formatRow->addStretch(1);
    textForm->addRow(QString(), formatRow);

    layout->addWidget(textGroup);

    connect(m_fontSize, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value)
    {
        if (!m_updating)
        {
            m_view->applyStyle(QStringLiteral("fontSize"), QString::number(value));
        }
    });
    connect(m_fontColor, &ColorButton::valueChanged, this, [this](const QString &value)
    {
        value.isEmpty() ? m_view->clearStyle(QStringLiteral("fontColor"))
                        : m_view->applyStyle(QStringLiteral("fontColor"), value);
    });
    connect(m_align, &QComboBox::currentIndexChanged, this, [this](int)
    {
        if (!m_updating)
        {
            m_view->applyStyle(QStringLiteral("align"), m_align->currentData().toString());
        }
    });
    connect(m_verticalAlign, &QComboBox::currentIndexChanged, this, [this](int)
    {
        if (!m_updating)
        {
            m_view->applyStyle(QStringLiteral("verticalAlign"),
                               m_verticalAlign->currentData().toString());
        }
    });

    auto updateFontStyle = [this]()
    {
        if (m_updating)
        {
            return;
        }

        int value = 0;
        value |= m_bold->isChecked() ? 1 : 0;
        value |= m_italic->isChecked() ? 2 : 0;
        value |= m_underline->isChecked() ? 4 : 0;
        m_view->applyStyle(QStringLiteral("fontStyle"), QString::number(value));
    };

    connect(m_bold, &QToolButton::toggled, this, updateFontStyle);
    connect(m_italic, &QToolButton::toggled, this, updateFontStyle);
    connect(m_underline, &QToolButton::toggled, this, updateFontStyle);

    // --- connectors -------------------------------------------------------
    auto *edgeGroup = new QGroupBox(QStringLiteral("连线"), content);
    m_edgeGroup = edgeGroup;
    edgeGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *edgeForm = new QFormLayout(edgeGroup);
    edgeForm->setSpacing(4);

    m_edgeStyle = new QComboBox(edgeGroup);
    m_edgeStyle->addItem(QStringLiteral("正交"), QStringLiteral("orthogonalEdgeStyle"));
    m_edgeStyle->addItem(QStringLiteral("直线"), QString());
    m_edgeStyle->addItem(QStringLiteral("折线"), QStringLiteral("segmentEdgeStyle"));
    m_edgeStyle->addItem(QStringLiteral("实体关系"), QStringLiteral("entityRelationEdgeStyle"));
    edgeForm->addRow(QStringLiteral("线路"), m_edgeStyle);

    m_startArrow = new QComboBox(edgeGroup);
    m_endArrow = new QComboBox(edgeGroup);

    const QVector<QPair<QString, QString>> arrows = {
        {QStringLiteral("无"), QStringLiteral("none")},
        {QStringLiteral("实心箭头"), QStringLiteral("classic")},
        {QStringLiteral("细实心箭头"), QStringLiteral("classicThin")},
        {QStringLiteral("方块箭头"), QStringLiteral("block")},
        {QStringLiteral("细方块箭头"), QStringLiteral("blockThin")},
        {QStringLiteral("空心箭头"), QStringLiteral("open")},
        {QStringLiteral("细空心箭头"), QStringLiteral("openThin")},
        {QStringLiteral("圆形"), QStringLiteral("oval")},
        {QStringLiteral("菱形"), QStringLiteral("diamond")},
        {QStringLiteral("细菱形"), QStringLiteral("diamondThin")},
        {QStringLiteral("基线"), QStringLiteral("baseDash")},
        {QStringLiteral("双方块"), QStringLiteral("doubleBlock")},
        {QStringLiteral("多选"), QStringLiteral("manyOptional")},
        {QStringLiteral("块状箭头（flexArrow）"), QStringLiteral("flexArrow")},
    };

    for (const auto &entry : arrows)
    {
        const QIcon icon = arrowPreviewIcon(entry.second);
        m_startArrow->addItem(icon, QString(), entry.second);
        m_endArrow->addItem(icon, QString(), entry.second);
        const int row = m_startArrow->count() - 1;
        m_startArrow->setItemData(row, entry.first, Qt::ToolTipRole);
        m_endArrow->setItemData(row, entry.first, Qt::ToolTipRole);
    }

    m_startArrow->setIconSize(QSize(48, 24));
    m_endArrow->setIconSize(QSize(48, 24));
    m_startArrow->setMinimumWidth(72);
    m_endArrow->setMinimumWidth(72);
    m_startArrow->setToolTip(QStringLiteral("起点箭头"));
    m_endArrow->setToolTip(QStringLiteral("终点箭头"));

    edgeForm->addRow(QStringLiteral("起点"), m_startArrow);
    edgeForm->addRow(QStringLiteral("终点"), m_endArrow);

    m_startSize = new QSpinBox(edgeGroup);
    m_endSize = new QSpinBox(edgeGroup);

    for (QSpinBox *spin : {m_startSize, m_endSize})
    {
        spin->setRange(1, 60);
        spin->setValue(6);
        spin->setSuffix(QStringLiteral(" px"));
    }

    edgeForm->addRow(QStringLiteral("起点大小"), m_startSize);
    edgeForm->addRow(QStringLiteral("终点大小"), m_endSize);
    layout->addWidget(edgeGroup);

    connect(m_edgeStyle, &QComboBox::currentIndexChanged, this, [this](int)
    {
        if (!m_updating)
        {
            const QString value = m_edgeStyle->currentData().toString();
            value.isEmpty() ? m_view->clearStyle(QStringLiteral("edgeStyle"))
                            : m_view->applyStyle(QStringLiteral("edgeStyle"), value);
        }
    });
    const auto applyArrowChoice = [this](QComboBox *combo, const QString &styleKey)
    {
        if (m_updating)
        {
            return;
        }

        const QString value = combo->currentData().toString();

        QComboBox *other = combo == m_startArrow ? m_endArrow : m_startArrow;
        m_updating = true;

        if (value == QLatin1String("flexArrow"))
        {
            m_view->applyStyle(QStringLiteral("shape"), QStringLiteral("flexArrow"));
            other->setCurrentIndex(combo->currentIndex());
        }
        else
        {
            m_view->clearStyle(QStringLiteral("shape"));
            m_view->applyStyle(styleKey, value);

            if (other->currentData().toString() == QLatin1String("flexArrow"))
            {
                other->setCurrentIndex(qMax(0, other->findData(
                    other == m_startArrow ? QStringLiteral("none")
                                          : QStringLiteral("classic"))));
            }
        }

        m_updating = false;
    };

    connect(m_startArrow, &QComboBox::currentIndexChanged, this,
            [this, applyArrowChoice](int)
    {
        applyArrowChoice(m_startArrow, QStringLiteral("startArrow"));
    });
    connect(m_endArrow, &QComboBox::currentIndexChanged, this,
            [this, applyArrowChoice](int)
    {
        applyArrowChoice(m_endArrow, QStringLiteral("endArrow"));
    });
    connect(m_startSize, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value)
    {
        if (!m_updating)
        {
            m_view->applyStyle(QStringLiteral("startSize"), QString::number(value));
        }
    });
    connect(m_endSize, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value)
    {
        if (!m_updating)
        {
            m_view->applyStyle(QStringLiteral("endSize"), QString::number(value));
        }
    });

    // --- arrange ----------------------------------------------------------
    layout->addStretch(1);

    for (QGroupBox *group : findChildren<QGroupBox *>())
    {
        m_groups.append(group);
    }

    if (m_hint != nullptr)
    {
        m_hint->setVisible(true);
    }

    for (QGroupBox *group : m_groups)
    {
        group->setVisible(false);
    }

    // Sensible values while nothing is selected.
    m_updating = true;
    m_x->setValue(0);
    m_y->setValue(0);
    m_w->setValue(120);
    m_h->setValue(60);
    m_rotation->setValue(0);
    m_strokeWidth->setValue(1);
    m_opacity->setValue(100);
    m_fontSize->setValue(11);
    m_align->setCurrentIndex(1);
    m_verticalAlign->setCurrentIndex(1);
    m_startArrow->setCurrentIndex(0);
    m_endArrow->setCurrentIndex(1);
    m_startSize->setValue(6);
    m_endSize->setValue(6);
    m_updating = false;
}

void FormatPanel::setView(DiagramView *view)
{
    if (m_view != nullptr)
    {
        disconnect(m_view, nullptr, this, nullptr);
    }

    m_view = view;

    if (m_view != nullptr)
    {
        connect(m_view, &DiagramView::selectionChanged, this, &FormatPanel::updateFromSelection);
    }

    updateFromSelection();
}

void FormatPanel::updateFromSelection()
{
    if (m_view == nullptr)
    {
        if (m_hint != nullptr)
        {
            m_hint->setVisible(true);
        }

        for (QGroupBox *group : m_groups)
        {
            group->setVisible(false);
        }

        return;
    }

    const QVector<Cell *> cells = m_view->selectedCellObjects();
    const bool hasSelection = !cells.isEmpty();

    if (m_hint != nullptr)
    {
        m_hint->setVisible(!hasSelection);
    }

    for (QGroupBox *group : m_groups)
    {
        group->setVisible(hasSelection);
    }

    if (!hasSelection)
    {
        m_selectedId.clear();
        return;
    }

    compactGroups();
    bool hasVertex = false;
    bool hasEdge = false;

    for (Cell *cell : cells)
    {
        hasVertex = hasVertex || cell->isVertex();
        hasEdge = hasEdge || cell->isEdge();
    }

    updateEnabled(hasSelection, hasVertex, hasEdge);

    if (m_edgeGroup != nullptr)
    {
        m_edgeGroup->setVisible(hasSelection && hasEdge);
    }

    Cell *cell = cells.first();
    m_selectedId = cell->id;

    m_updating = true;

    const QRectF rect = cell->absoluteRect();
    m_x->setValue(rect.x());
    m_y->setValue(rect.y());
    m_w->setValue(qMax(1.0, rect.width()));
    m_h->setValue(qMax(1.0, rect.height()));
    m_rotation->setValue(cell->rotation);

    m_fill->setStyleValue(cell->styleValue(QStringLiteral("fillColor")));
    m_stroke->setStyleValue(cell->styleValue(QStringLiteral("strokeColor")));
    m_fontColor->setStyleValue(cell->styleValue(QStringLiteral("fontColor")));

    m_strokeWidth->setValue(static_cast<int>(cell->styleNumber(QStringLiteral("strokeWidth"), 1.0)));
    m_opacity->setValue(static_cast<int>(cell->styleNumber(QStringLiteral("opacity"), 100.0)));
    m_fontSize->setValue(static_cast<int>(cell->styleNumber(QStringLiteral("fontSize"), 11.0)));

    m_dashed->setChecked(cell->styleFlag(QStringLiteral("dashed")));
    m_rounded->setChecked(cell->styleFlag(QStringLiteral("rounded")));
    m_shadow->setChecked(cell->styleFlag(QStringLiteral("shadow")));

    const int fontStyle = static_cast<int>(cell->styleNumber(QStringLiteral("fontStyle"), 0));
    m_bold->setChecked((fontStyle & 1) != 0);
    m_italic->setChecked((fontStyle & 2) != 0);
    m_underline->setChecked((fontStyle & 4) != 0);

    const QString align = cell->styleValue(QStringLiteral("align"), QStringLiteral("center"));
    const int alignIndex = m_align->findData(align);
    m_align->setCurrentIndex(alignIndex >= 0 ? alignIndex : 1);

    const QString verticalAlign = cell->styleValue(QStringLiteral("verticalAlign"),
                                                   QStringLiteral("middle"));
    const int verticalIndex = m_verticalAlign->findData(verticalAlign);
    m_verticalAlign->setCurrentIndex(verticalIndex >= 0 ? verticalIndex : 1);

    const QString edgeStyle = cell->styleValue(QStringLiteral("edgeStyle"));
    int edgeIndex = m_edgeStyle->findData(edgeStyle);

    if (edgeIndex < 0)
    {
        edgeIndex = edgeStyle.isEmpty() ? 1 : 0;
    }

    m_edgeStyle->setCurrentIndex(edgeIndex);

    const bool flexArrow = cell->styleValue(QStringLiteral("shape")).compare(
        QLatin1String("flexArrow"), Qt::CaseInsensitive) == 0;

    const QString startArrow = cell->styleValue(QStringLiteral("startArrow"), QStringLiteral("none"));
    const QString endArrow = cell->styleValue(QStringLiteral("endArrow"), QStringLiteral("classic"));
    if (flexArrow)
    {
        m_startArrow->setCurrentIndex(qMax(0, m_startArrow->findData(QStringLiteral("flexArrow"))));
        m_endArrow->setCurrentIndex(qMax(0, m_endArrow->findData(QStringLiteral("flexArrow"))));
    }
    else
    {
        m_startArrow->setCurrentIndex(qMax(0, m_startArrow->findData(startArrow)));
        m_endArrow->setCurrentIndex(qMax(0, m_endArrow->findData(endArrow)));
    }
    m_startSize->setValue(qRound(cell->styleNumber(QStringLiteral("startSize"), 6.0)));
    m_endSize->setValue(qRound(cell->styleNumber(QStringLiteral("endSize"), 6.0)));

    m_updating = false;
}

void FormatPanel::applyTheme(bool dark)
{
    m_dark = dark;

    if (dark)
    {
        setStyleSheet(QStringLiteral(
            "QGroupBox { border: 1px solid #3a3f45; border-radius: 8px;"
            " margin-top: 8px; padding: 8px 8px 4px 8px; font-weight: 600; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px;"
            " color: #b8bec6; }"
            "QLabel { color: #b8bec6; font-size: 12px; }"
            "QToolButton { border: 1px solid transparent; border-radius: 4px;"
            " padding: 3px 7px; }"
            "QToolButton:checked { background: #1a3a5c; color: #8ab4f8;"
            " border-color: #8ab4f8; }"));
    }
    else
    {
        setStyleSheet(QStringLiteral(
            "QGroupBox { border: 1px solid #e3e6e9; border-radius: 8px;"
            " margin-top: 8px; padding: 8px 8px 4px 8px; font-weight: 600; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px;"
            " color: #5f6368; }"
            "QLabel { color: #5f6368; font-size: 12px; }"
            "QToolButton { border: 1px solid transparent; border-radius: 4px;"
            " padding: 3px 7px; }"
            "QToolButton:checked { background: #e8f0fe; color: #1a73e8;"
            " border-color: #1a73e8; }"));
    }

    compactGroups();
}

void FormatPanel::compactGroups()
{
    // QGroupBox stretches when the surrounding column is taller than its
    // content, which spread the form rows apart. A fixed vertical policy keeps
    // every section exactly as tall as its layout needs.
    for (QGroupBox *group : m_groups)
    {
        group->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        group->setMaximumHeight(QWIDGETSIZE_MAX);
        group->updateGeometry();
    }
}

void FormatPanel::updateEnabled(bool hasSelection, bool hasVertex, bool hasEdge)
{
    m_x->setEnabled(hasVertex);
    m_y->setEnabled(hasVertex);
    m_w->setEnabled(hasVertex);
    m_h->setEnabled(hasVertex);
    m_rotation->setEnabled(hasVertex);

    m_fill->setEnabled(hasVertex);
    m_stroke->setEnabled(hasSelection);
    m_fontColor->setEnabled(hasSelection);
    m_strokeWidth->setEnabled(hasSelection);
    m_opacity->setEnabled(hasSelection);
    m_dashed->setEnabled(hasSelection);
    m_rounded->setEnabled(hasVertex);
    m_shadow->setEnabled(hasVertex);

    m_fontSize->setEnabled(hasSelection);
    m_align->setEnabled(hasSelection);
    m_verticalAlign->setEnabled(hasSelection);
    m_bold->setEnabled(hasSelection);
    m_italic->setEnabled(hasSelection);
    m_underline->setEnabled(hasSelection);

    m_edgeStyle->setEnabled(hasEdge);
    m_startArrow->setEnabled(hasEdge);
    m_endArrow->setEnabled(hasEdge);
    m_startSize->setEnabled(hasEdge);
    m_endSize->setEnabled(hasEdge);

}

} // namespace dq

#include "FormatPanel.moc"
