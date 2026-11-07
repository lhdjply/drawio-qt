#include "core/Cell.h"

#include "core/Page.h"

#include <QRegularExpression>

namespace dq {

Cell::Cell(Kind kind, const QString &cellId)
    : id(cellId)
    , m_kind(kind)
{
}

Cell::~Cell() = default;

QString Cell::styleValue(const QString &key, const QString &fallback) const
{
    return Style::value(style, key, fallback);
}

bool Cell::styleFlag(const QString &key, bool fallback) const
{
    return Style::flag(style, key, fallback);
}

double Cell::styleNumber(const QString &key, double fallback) const
{
    return Style::number(style, key, fallback);
}

QColor Cell::styleColor(const QString &key, const QColor &fallback) const
{
    return Style::color(style, key, fallback);
}

void Cell::setStyleValue(const QString &key, const QString &value)
{
    style.insert(key, value);
}

bool Cell::isHtmlLabel() const
{
    return styleFlag(QStringLiteral("html"), false);
}

bool Cell::hasFill() const
{
    const QString fill = styleValue(QStringLiteral("fillColor"));
    return isVertex() && (fill.isEmpty() || !Style::isNoneColor(fill));
}

bool Cell::hasStroke() const
{
    const QString stroke = styleValue(QStringLiteral("strokeColor"));
    return stroke.isEmpty() || !Style::isNoneColor(stroke);
}

QPointF Cell::absolutePosition() const
{
    if (parent != nullptr && parent->isVertex())
    {
        return parent->absolutePosition() + geometry.topLeft();
    }

    return geometry.topLeft();
}

QRectF Cell::absoluteRect() const
{
    if (isEdge())
    {
        QRectF result = QRectF(sourcePoint, targetPoint).normalized();

        for (const QPointF &point : waypoints)
        {
            result |= QRectF(point, QSizeF(0, 0));
        }

        return result;
    }

    return QRectF(absolutePosition(), geometry.size());
}

QPointF Cell::center() const
{
    const QRectF rect = absoluteRect();
    return rect.center();
}

QPointF Cell::mapToAbsolute(const QPointF &local) const
{
    if (parent != nullptr && parent->isVertex())
    {
        return parent->absolutePosition() + local;
    }

    return local;
}

QPointF Cell::mapFromAbsolute(const QPointF &absolute) const
{
    return absolute - (mapToAbsolute(QPointF(0, 0)));
}

QString Cell::plainLabel() const
{
    if (value.isEmpty())
    {
        return QString();
    }

    if (!isHtmlLabel())
    {
        return value;
    }

    // A very small subset of the HTML used by draw.io labels: <br>, <b>, <i>,
    // <u>, <font color>, numeric entities and named entities.
    QString text = value;
    text.replace(QRegularExpression(QStringLiteral("<br\\s*/?>"), QRegularExpression::CaseInsensitiveOption),
                 QStringLiteral("\n"));
    text.replace(QRegularExpression(QStringLiteral("<div[^>]*>"), QRegularExpression::CaseInsensitiveOption),
                 QStringLiteral("\n"));
    text.replace(QRegularExpression(QStringLiteral("</div>"), QRegularExpression::CaseInsensitiveOption),
                 QString());
    text.replace(QRegularExpression(QStringLiteral("<[^>]+>")), QString());
    text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));

    return text.trimmed();
}

bool Cell::isContainer() const
{
    return isVertex() && (!children.isEmpty() || styleFlag(QStringLiteral("container")));
}

bool Cell::isGroup() const
{
    return isVertex() && !children.isEmpty()
        && (styleValue(QStringLiteral("group")) == QLatin1String("1")
            || styleValue(QStringLiteral("shape")) == QLatin1String("group"));
}

Cell *Cell::topLevelAncestor() const
{
    const Cell *current = this;

    while (current->parent != nullptr && current->parent->isVertex()
           && current->parent->parent != nullptr)
    {
        current = current->parent;
    }

    return const_cast<Cell *>(current);
}

bool Cell::contains(const QPointF &absolutePoint) const
{
    if (!isVertex())
    {
        return false;
    }

    return absoluteRect().contains(absolutePoint);
}

int Cell::depth() const
{
    int result = 0;
    const Cell *current = parent;

    while (current != nullptr)
    {
        ++result;
        current = current->parent;
    }

    return result;
}


} // namespace dq
