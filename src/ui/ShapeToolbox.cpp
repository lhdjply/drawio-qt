#include "ui/ShapeToolbox.h"

#include "app/I18n.h"
#include "render/ShapeCatalog.h"
#include "ui/ShapeLibraryDialog.h"

#include <QApplication>
#include <QComboBox>
#include <QDrag>
#include <QMouseEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QApplication>
#include <QFontMetrics>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace dq {

namespace {

constexpr const char *kShapeMimeType = "application/x-drawio-shape";


QStringList wrapLabel(const QString &text, const QFontMetrics &metrics, int maxWidth,
                       int maxLines)
{
    QStringList lines;
    QString current;

    for (int i = 0; i < text.size(); ++i)
    {
        const QChar character = text.at(i);
        const QString candidate = current + character;

        if (!current.isEmpty() && metrics.horizontalAdvance(candidate) > maxWidth)
        {
            const int lastSpace = current.lastIndexOf(QLatin1Char(' '));

            if (lastSpace > 0 && !character.isSpace())
            {
                lines.append(current.left(lastSpace));
                current = current.mid(lastSpace + 1) + character;
            }
            else
            {
                lines.append(current);
                current = QString(character);
            }

            if (lines.size() == maxLines)
            {
                const int consumed = i - current.size() + 1;
                lines.last() = metrics.elidedText(lines.last() + text.mid(consumed),
                                                  Qt::ElideRight, maxWidth);
                return lines;
            }
        }
        else
        {
            current = candidate;
        }
    }

    if (!current.isEmpty())
    {
        lines.append(current);
    }

    if (lines.size() > maxLines)
    {
        lines = lines.mid(0, maxLines);
        lines.last() = metrics.elidedText(lines.last(), Qt::ElideRight, maxWidth);
    }

    return lines;
}

/// Draws the icon above a maximum of two centred text lines. Qt's icon mode
/// delegate otherwise elides labels to a single line, which made longer shape
/// names unreadable.
class ShapeItemDelegate : public QStyledItemDelegate
{
public:
    explicit ShapeItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(88, 100);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        const QWidget *widget = opt.widget;
        QStyle *style = widget != nullptr ? widget->style() : QApplication::style();

        // Background, selection and hover come from the current style.
        opt.text.clear();
        opt.icon = QIcon();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        const QRect rect = opt.rect;
        const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        const int iconSide = qMin(38, rect.width() - 12);
        const QRect iconRect(rect.center().x() - iconSide / 2, rect.top() + 6,
                             iconSide, iconSide);

        if (!icon.isNull())
        {
            icon.paint(painter, iconRect, Qt::AlignCenter,
                       (option.state & QStyle::State_Enabled) ? QIcon::Normal
                                                              : QIcon::Disabled);
        }

        QFont font = opt.font;
        font.setPointSizeF(9.0);
        const QFontMetrics metrics(font);
        const int maxWidth = qMax(20, rect.width() - 8);
        const QStringList lines = wrapLabel(index.data(Qt::DisplayRole).toString(),
                                            metrics, maxWidth, 2);

        const int lineHeight = metrics.height();
        const int textHeight = lineHeight * lines.size();
        const int blockHeight = iconSide + 4 + textHeight;
        int top = rect.top() + qMax(2, (rect.height() - blockHeight) / 2);

        painter->save();
        painter->setFont(font);
        painter->setPen(opt.palette.color((option.state & QStyle::State_Selected)
                                              ? QPalette::HighlightedText
                                              : QPalette::Text));

        int y = top + iconSide + 4;

        for (const QString &line : lines)
        {
            painter->drawText(QRect(rect.left(), y, rect.width(), lineHeight),
                              Qt::AlignHCenter | Qt::AlignTop, line);
            y += lineHeight;
        }

        painter->restore();
    }
};

/// Adds the shape key to the drag payload so DiagramView can rebuild the
/// definition on drop.
class ShapeList : public QListWidget
{
public:
    explicit ShapeList(QWidget *parent = nullptr)
        : QListWidget(parent)
    {
        // Drag is started manually from mouseMoveEvent: the default item-view
        // drag path is unreliable for icon grids inside docks, and this also
        // gives us a proper drag pixmap of the shape.
        setDragEnabled(false);
        setDragDropMode(QAbstractItemView::NoDragDrop);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setViewMode(QListView::IconMode);
        setResizeMode(QListView::Adjust);
        setMovement(QListView::Static);
        setIconSize(QSize(38, 38));
        setGridSize(QSize(88, 100));
        setSpacing(2);
        setWordWrap(true);
        setTextElideMode(Qt::ElideNone);
        setUniformItemSizes(true);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            m_pressPosition = event->position().toPoint();
            m_pressItem = itemAt(m_pressPosition);
        }

        QListWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_pressItem != nullptr && (event->buttons() & Qt::LeftButton)
            && (event->position().toPoint() - m_pressPosition).manhattanLength()
                   >= QApplication::startDragDistance())
        {
            QListWidgetItem *item = m_pressItem;
            m_pressItem = nullptr;
            startShapeDrag(item);
            return;
        }

        QListWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        m_pressItem = nullptr;
        QListWidget::mouseReleaseEvent(event);
    }

private:
    void startShapeDrag(QListWidgetItem *item)
    {
        if (item == nullptr)
        {
            return;
        }

        const QString key = item->data(Qt::UserRole).toString();

        auto *mime = new QMimeData();
        mime->setData(QLatin1String(kShapeMimeType), key.toUtf8());

        auto *drag = new QDrag(this);
        drag->setMimeData(mime);

        const QIcon icon = item->icon();

        if (!icon.isNull())
        {
            drag->setPixmap(icon.pixmap(48, 48));
        }

        drag->exec(Qt::CopyAction, Qt::CopyAction);
    }

    QPoint m_pressPosition;
    QListWidgetItem *m_pressItem = nullptr;
};

} // namespace

ShapeToolbox::ShapeToolbox(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("Search shapes…"));
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    auto *categoryRow = new QHBoxLayout();
    categoryRow->setSpacing(4);

    m_category = new QComboBox(this);
    m_category->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    categoryRow->addWidget(m_category, 1);

    auto *manage = new QToolButton(this);
    manage->setText(QStringLiteral("…"));
    manage->setToolTip(QStringLiteral("Choose shape libraries"));
    manage->setAutoRaise(true);
    categoryRow->addWidget(manage);

    layout->addLayout(categoryRow);

    m_list = new ShapeList(this);
    m_list->setItemDelegate(new ShapeItemDelegate(m_list));
    layout->addWidget(m_list, 1);

    m_count = new QLabel(this);
    m_count->setStyleSheet(QStringLiteral("color: #8a9099; font-size: 11px;"));
    layout->addWidget(m_count);

    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(180);

    populateCategories();
    populateList();

    connect(m_category, &QComboBox::currentIndexChanged, this, [this](int)
    {
        populateList();
    });
    connect(m_search, &QLineEdit::textChanged, this, [this]()
    {
        m_searchTimer->start();
    });
    connect(m_searchTimer, &QTimer::timeout, this, [this]()
    {
        populateList();
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item)
    {
        emitActivated(item);
    });
    connect(manage, &QToolButton::clicked, this, &ShapeToolbox::manageLibraries);
}

QStringList ShapeToolbox::enabledCategories() const
{
    return ShapeLibraryDialog::configuredCategories();
}

void ShapeToolbox::populateCategories()
{
    const QString previous = m_category->currentData().toString();
    m_category->blockSignals(true);
    m_category->clear();

    m_category->addItem(I18n::text(QStringLiteral("All Built-in Shapes")),
                        QStringLiteral("builtin:all"));

    const QStringList enabled = enabledCategories();

    for (const ShapeCategory &category : ShapeCatalog::instance().builtinCategories())
    {
        if (!enabled.isEmpty() && !enabled.contains(category.id))
        {
            continue;
        }

        m_category->addItem(I18n::categoryName(category.id, category.name), category.id);
    }

    bool separatorAdded = false;

    for (const ShapeCategory &category : ShapeCatalog::instance().imageCategories())
    {
        if (!enabled.isEmpty() && !enabled.contains(category.id))
        {
            continue;
        }

        if (!separatorAdded)
        {
            m_category->insertSeparator(m_category->count());
            separatorAdded = true;
        }

        m_category->addItem(I18n::categoryName(category.id, category.name), category.id);
    }

    const int index = m_category->findData(previous);

    if (index >= 0)
    {
        m_category->setCurrentIndex(index);
    }

    m_category->blockSignals(false);
}

void ShapeToolbox::populateList()
{
    if (m_list == nullptr)
    {
        return;
    }

    const QString query = m_search->text().trimmed();
    const QStringList enabled = enabledCategories();
    const QString categoryId = m_category->currentData().toString();

    QVector<ShapeDefinition> shapes;

    if (!query.isEmpty())
    {
        for (const ShapeDefinition &definition : ShapeCatalog::instance().search(query, 240))
        {
            if (enabled.isEmpty() || enabled.contains(definition.category))
            {
                shapes.append(definition);
            }
        }
    }
    else if (categoryId == QLatin1String("builtin:all"))
    {
        for (const ShapeCategory &category : ShapeCatalog::instance().builtinCategories())
        {
            if (enabled.isEmpty() || enabled.contains(category.id))
            {
                shapes.append(category.shapes);
            }
        }
    }
    else
    {
        for (const ShapeCategory &category : ShapeCatalog::instance().categories())
        {
            if (category.id == categoryId)
            {
                shapes = category.shapes;
                break;
            }
        }
    }

    m_list->clear();

    QFont listFont = m_list->font();
    listFont.setPointSizeF(9.0);
    m_list->setFont(listFont);

    for (const ShapeDefinition &definition : shapes)
    {
        auto *item = new QListWidgetItem(m_list);
        const QString name = I18n::shapeName(definition.key, definition.name);
        item->setText(name);
        item->setData(Qt::UserRole, definition.key);
        item->setToolTip(QStringLiteral("%1\n%2").arg(name, definition.style));
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        item->setIcon(ShapeCatalog::instance().iconFor(definition, 38));
    }

    m_count->setText(query.isEmpty()
                         ? I18n::text(I18n::text(QStringLiteral("%1 shapes"))).arg(shapes.size())
                         : I18n::text(I18n::text(QStringLiteral("%1 shapes found"))).arg(shapes.size()));
}

void ShapeToolbox::manageLibraries()
{
    ShapeLibraryDialog dialog(this);

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    ShapeLibraryDialog::setConfiguredCategories(dialog.enabledCategories());
    reload();
}

void ShapeToolbox::reload()
{
    populateCategories();
    populateList();
}

void ShapeToolbox::emitActivated(QListWidgetItem *item)
{
    if (item == nullptr)
    {
        return;
    }

    const ShapeDefinition definition =
        ShapeCatalog::instance().definition(item->data(Qt::UserRole).toString());

    if (!definition.key.isEmpty())
    {
        emit shapeActivated(definition);
    }
}

} // namespace dq
