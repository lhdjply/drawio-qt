#include "ui/FormatPanel.h"

#include "app/I18n.h"

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
        setToolTip(QStringLiteral("Click to choose a color, right-click to clear"));
        setContextMenuPolicy(Qt::CustomContextMenu);

        connect(this, &QToolButton::clicked, this, [this]()
        {
            const QColor initial = m_color.isValid() ? m_color : Qt::white;
            const QColor chosen = QColorDialog::getColor(initial, this,
                                                         QStringLiteral("Choose Color"),
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
            QAction *none = menu.addAction(QStringLiteral("No Color"));
            QAction *defaultColor = menu.addAction(QStringLiteral("Restore Defaults"));
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
            setText(QStringLiteral("Default"));
        }
        else if (value == kNoneValue)
        {
            m_color = QColor();
            setText(QStringLiteral("None"));
        }
        else
        {
            m_color = Style::parseColorValue(value);

            if (m_color.isValid())
            {
                setText(m_color.name().toUpper());
            }
            else
            {
                setText(value);
            }
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

    m_hint = new QLabel(QStringLiteral("Select a shape on the canvas\nto edit its style and text"), content);
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setWordWrap(true);
    m_hint->setMinimumHeight(120);
    layout->addWidget(m_hint);

    // --- geometry ---------------------------------------------------------
    auto *geometryGroup = new QGroupBox(QStringLiteral("Position and Size"), content);
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
    geometryGrid->addWidget(new QLabel(QStringLiteral("W"), geometryGroup), 1, 0);
    geometryGrid->addWidget(m_w, 1, 1);
    geometryGrid->addWidget(new QLabel(QStringLiteral("H"), geometryGroup), 1, 2);
    geometryGrid->addWidget(m_h, 1, 3);
    geometryGrid->addWidget(new QLabel(QStringLiteral("Angle"), geometryGroup), 2, 0);
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
    auto *styleGroup = new QGroupBox(QStringLiteral("Style"), content);
    styleGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *styleForm = new QFormLayout(styleGroup);
    styleForm->setSpacing(4);

    m_fill = new ColorButton(styleGroup);
    m_fill->setObjectName(QStringLiteral("fillColor"));
    m_stroke = new ColorButton(styleGroup);
    m_stroke->setObjectName(QStringLiteral("strokeColor"));
    m_fontColor = new ColorButton(styleGroup);

    m_fillEnabled = new QCheckBox(styleGroup);
    m_fillEnabled->setObjectName(QStringLiteral("fillEnabled"));
    m_fillEnabled->setToolTip(I18n::text(QStringLiteral("Enable fill")));
    auto *fillRow = new QWidget(styleGroup);
    auto *fillLayout = new QHBoxLayout(fillRow);
    fillLayout->setContentsMargins(0, 0, 0, 0);
    fillLayout->addWidget(m_fillEnabled);
    fillLayout->addWidget(m_fill, 1);
    styleForm->addRow(QStringLiteral("Fill"), fillRow);

    m_gradient = new ColorButton(styleGroup);
    m_gradient->setObjectName(QStringLiteral("gradientColor"));
    m_gradientEnabled = new QCheckBox(styleGroup);
    m_gradientEnabled->setObjectName(QStringLiteral("gradientEnabled"));
    m_gradientEnabled->setToolTip(I18n::text(QStringLiteral("Enable gradient")));
    auto *gradientRow = new QWidget(styleGroup);
    auto *gradientLayout = new QHBoxLayout(gradientRow);
    gradientLayout->setContentsMargins(0, 0, 0, 0);
    gradientLayout->addWidget(m_gradientEnabled);
    gradientLayout->addWidget(m_gradient, 1);
    styleForm->addRow(QStringLiteral("Gradient"), gradientRow);

    m_gradientDirection = new QComboBox(styleGroup);
    m_gradientDirection->setObjectName(QStringLiteral("gradientDirection"));
    m_gradientDirection->addItem(QStringLiteral("South"), QStringLiteral("south"));
    m_gradientDirection->addItem(QStringLiteral("North"), QStringLiteral("north"));
    m_gradientDirection->addItem(QStringLiteral("East"), QStringLiteral("east"));
    m_gradientDirection->addItem(QStringLiteral("West"), QStringLiteral("west"));
    styleForm->addRow(QStringLiteral("Direction"), m_gradientDirection);

    m_strokeEnabled = new QCheckBox(styleGroup);
    m_strokeEnabled->setObjectName(QStringLiteral("strokeEnabled"));
    m_strokeEnabled->setToolTip(I18n::text(QStringLiteral("Enable border")));
    auto *strokeRow = new QWidget(styleGroup);
    auto *strokeLayout = new QHBoxLayout(strokeRow);
    strokeLayout->setContentsMargins(0, 0, 0, 0);
    strokeLayout->addWidget(m_strokeEnabled);
    strokeLayout->addWidget(m_stroke, 1);
    styleForm->addRow(QStringLiteral("Border"), strokeRow);

    m_strokeWidth = new QSpinBox(styleGroup);
    m_strokeWidth->setRange(0, 40);
    m_strokeWidth->setSuffix(QStringLiteral(" px"));
    m_strokeWidth->setMinimumWidth(64);
    styleForm->addRow(QStringLiteral("Line Width"), m_strokeWidth);

    m_opacity = new QSpinBox(styleGroup);
    m_opacity->setRange(0, 100);
    m_opacity->setSuffix(QStringLiteral(" %"));
    m_opacity->setMinimumWidth(64);
    styleForm->addRow(QStringLiteral("Opacity"), m_opacity);

    auto *flagRow = new QHBoxLayout();
    m_dashed = new QCheckBox(QStringLiteral("Dashed"), styleGroup);
    m_rounded = new QCheckBox(QStringLiteral("Rounded"), styleGroup);
    m_shadow = new QCheckBox(QStringLiteral("Shadow"), styleGroup);
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
    connect(m_fillEnabled, &QCheckBox::toggled, this, [this](bool enabled)
    {
        if (m_updating)
        {
            return;
        }

        if (!enabled)
        {
            m_view->applyStyle(QStringLiteral("fillColor"), QStringLiteral("none"));
        }
        else
        {
            QString value = m_fill->styleValue();

            if (value.isEmpty() || value == QLatin1String("none"))
            {
                value = QStringLiteral("#ffffff");
            }

            m_view->applyStyle(QStringLiteral("fillColor"), value);
        }

        m_fill->setEnabled(enabled);
        m_gradientEnabled->setEnabled(enabled);
        m_gradient->setEnabled(enabled && m_gradientEnabled->isChecked());
    });
    connect(m_gradient, &ColorButton::valueChanged, this, [this](const QString &value)
    {
        if (!m_updating && !value.isEmpty())
        {
            m_view->applyStyle(QStringLiteral("gradientColor"), value);
        }
    });
    connect(m_gradientDirection, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int)
    {
        if (!m_updating && m_gradientEnabled->isChecked())
        {
            m_view->applyStyle(QStringLiteral("gradientDirection"),
                               m_gradientDirection->currentData().toString());
        }
    });
    connect(m_gradientEnabled, &QCheckBox::toggled, this, [this](bool enabled)
    {
        if (m_updating)
        {
            return;
        }

        if (enabled)
        {
            QString value = m_gradient->styleValue();

            if (value.isEmpty() || value == QLatin1String("none"))
            {
                value = QStringLiteral("#ffffff");
            }

            m_view->applyStyle(QStringLiteral("gradientColor"), value);
        }
        else
        {
            m_view->clearStyle(QStringLiteral("gradientColor"));
        }

        m_gradient->setEnabled(enabled);
        m_gradientDirection->setEnabled(enabled);
    });
    connect(m_strokeEnabled, &QCheckBox::toggled, this, [this](bool enabled)
    {
        if (m_updating)
        {
            return;
        }

        if (!enabled)
        {
            m_view->applyStyle(QStringLiteral("strokeColor"), QStringLiteral("none"));
        }
        else
        {
            QString value = m_stroke->styleValue();

            if (value.isEmpty() || value == QLatin1String("none"))
            {
                value = QStringLiteral("#000000");
            }

            m_view->applyStyle(QStringLiteral("strokeColor"), value);
        }

        m_stroke->setEnabled(enabled);
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
    auto *textGroup = new QGroupBox(QStringLiteral("Text"), content);
    textGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *textForm = new QFormLayout(textGroup);
    textForm->setSpacing(4);

    m_fontSize = new QSpinBox(textGroup);
    m_fontSize->setRange(4, 200);
    m_fontSize->setSuffix(QStringLiteral(" px"));
    m_fontSize->setMinimumWidth(64);
    textForm->addRow(QStringLiteral("Font Size"), m_fontSize);
    textForm->addRow(QStringLiteral("Color"), m_fontColor);

    m_align = new QComboBox(textGroup);
    m_align->addItem(QStringLiteral("Align Left"), QStringLiteral("left"));
    m_align->addItem(QStringLiteral("Center"), QStringLiteral("center"));
    m_align->addItem(QStringLiteral("Align Right"), QStringLiteral("right"));
    textForm->addRow(QStringLiteral("Horizontal Align"), m_align);

    m_verticalAlign = new QComboBox(textGroup);
    m_verticalAlign->addItem(QStringLiteral("Top"), QStringLiteral("top"));
    m_verticalAlign->addItem(QStringLiteral("Center"), QStringLiteral("middle"));
    m_verticalAlign->addItem(QStringLiteral("Bottom"), QStringLiteral("bottom"));
    textForm->addRow(QStringLiteral("Vertical Align"), m_verticalAlign);

    auto *formatRow = new QHBoxLayout();
    m_bold = makeToggle(QStringLiteral("B"), QStringLiteral("Bold"), textGroup);
    m_italic = makeToggle(QStringLiteral("I"), QStringLiteral("Italic"), textGroup);
    m_underline = makeToggle(QStringLiteral("U"), QStringLiteral("Underline"), textGroup);
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
    auto *edgeGroup = new QGroupBox(QStringLiteral("Connector"), content);
    m_edgeGroup = edgeGroup;
    edgeGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *edgeForm = new QFormLayout(edgeGroup);
    edgeForm->setSpacing(4);

    m_edgeStyle = new QComboBox(edgeGroup);
    m_edgeStyle->addItem(QStringLiteral("Orthogonal"), QStringLiteral("orthogonalEdgeStyle"));
    m_edgeStyle->addItem(QStringLiteral("Straight Line"), QString());
    m_edgeStyle->addItem(QStringLiteral("Polyline"), QStringLiteral("segmentEdgeStyle"));
    m_edgeStyle->addItem(QStringLiteral("Entity Relation"), QStringLiteral("entityRelationEdgeStyle"));
    edgeForm->addRow(QStringLiteral("Route"), m_edgeStyle);

    m_startArrow = new QComboBox(edgeGroup);
    m_endArrow = new QComboBox(edgeGroup);

    const QVector<QPair<QString, QString>> arrows = {
        {QStringLiteral("None"), QStringLiteral("none")},
        {QStringLiteral("Filled Arrow"), QStringLiteral("classic")},
        {QStringLiteral("Thin Filled Arrow"), QStringLiteral("classicThin")},
        {QStringLiteral("Block Arrow"), QStringLiteral("block")},
        {QStringLiteral("Thin Block Arrow"), QStringLiteral("blockThin")},
        {QStringLiteral("Open Arrow"), QStringLiteral("open")},
        {QStringLiteral("Thin Open Arrow"), QStringLiteral("openThin")},
        {QStringLiteral("Ellipse"), QStringLiteral("oval")},
        {QStringLiteral("Diamond"), QStringLiteral("diamond")},
        {QStringLiteral("Thin Diamond"), QStringLiteral("diamondThin")},
        {QStringLiteral("Baseline"), QStringLiteral("baseDash")},
        {QStringLiteral("Double Block"), QStringLiteral("doubleBlock")},
        {QStringLiteral("Multi-select"), QStringLiteral("manyOptional")},
        {QStringLiteral("Flex Arrow (flexArrow)"), QStringLiteral("flexArrow")},
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
    m_startArrow->setToolTip(QStringLiteral("Start Arrow"));
    m_endArrow->setToolTip(QStringLiteral("End Arrow"));

    edgeForm->addRow(QStringLiteral("Start"), m_startArrow);
    edgeForm->addRow(QStringLiteral("End"), m_endArrow);

    m_startSize = new QSpinBox(edgeGroup);
    m_endSize = new QSpinBox(edgeGroup);

    for (QSpinBox *spin : {m_startSize, m_endSize})
    {
        spin->setRange(1, 60);
        spin->setValue(6);
        spin->setSuffix(QStringLiteral(" px"));
    }

    edgeForm->addRow(QStringLiteral("Start Size"), m_startSize);
    edgeForm->addRow(QStringLiteral("End Size"), m_endSize);
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
        const bool flexMode = m_startArrow->currentData().toString() == QLatin1String("flexArrow")
            || m_endArrow->currentData().toString() == QLatin1String("flexArrow");
        m_updating = true;

        if (value == QLatin1String("flexArrow"))
        {
            m_view->applyStyle(QStringLiteral("shape"), QStringLiteral("flexArrow"));
            m_view->applyStyle(styleKey, QStringLiteral("classic"));
        }
        else if (value == QLatin1String("none") && flexMode)
        {
            // Keep the flexArrow shaft and remove only this end's head.
            m_view->applyStyle(styleKey, QStringLiteral("none"));
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
    m_rotation->setValue(qRound(cell->effectiveRotation()));

    m_fill->setStyleValue(cell->styleValue(QStringLiteral("fillColor")));
    m_stroke->setStyleValue(cell->styleValue(QStringLiteral("strokeColor")));
    m_fontColor->setStyleValue(cell->styleValue(QStringLiteral("fontColor")));

    const QString gradientValue = cell->styleValue(QStringLiteral("gradientColor"));
    const bool gradientEnabled = !gradientValue.isEmpty()
        && gradientValue != QLatin1String("none");
    m_gradient->setStyleValue(gradientValue);
    m_gradientEnabled->setChecked(gradientEnabled);
    m_gradient->setEnabled(gradientEnabled);
    m_gradientDirection->setEnabled(gradientEnabled);
    const int directionIndex = m_gradientDirection->findData(
        cell->styleValue(QStringLiteral("gradientDirection"), QStringLiteral("south")));
    m_gradientDirection->setCurrentIndex(directionIndex >= 0 ? directionIndex : 0);

    const bool fillEnabled = cell->styleValue(QStringLiteral("fillColor")) != QLatin1String("none");
    const bool strokeEnabled = cell->styleValue(QStringLiteral("strokeColor")) != QLatin1String("none");
    m_fillEnabled->setChecked(fillEnabled);
    m_strokeEnabled->setChecked(strokeEnabled);
    m_fill->setEnabled(fillEnabled);
    m_stroke->setEnabled(strokeEnabled);

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
        const bool startHead = startArrow.compare(QLatin1String("none"), Qt::CaseInsensitive) != 0
            && !startArrow.isEmpty();
        const bool endHead = endArrow.compare(QLatin1String("none"), Qt::CaseInsensitive) != 0
            && !endArrow.isEmpty();
        m_startArrow->setCurrentIndex(qMax(0, m_startArrow->findData(
            startHead ? QStringLiteral("flexArrow") : QStringLiteral("none"))));
        m_endArrow->setCurrentIndex(qMax(0, m_endArrow->findData(
            endHead ? QStringLiteral("flexArrow") : QStringLiteral("none"))));
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

    m_fillEnabled->setEnabled(hasVertex);
    m_gradientEnabled->setEnabled(hasVertex && m_fillEnabled->isChecked());
    m_gradient->setEnabled(hasVertex && m_fillEnabled->isChecked()
                           && m_gradientEnabled->isChecked());
    m_gradientDirection->setEnabled(hasVertex && m_fillEnabled->isChecked()
                                    && m_gradientEnabled->isChecked());
    m_strokeEnabled->setEnabled(hasSelection);
    m_fill->setEnabled(hasVertex && m_fillEnabled->isChecked());
    m_stroke->setEnabled(hasSelection && m_strokeEnabled->isChecked());
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
