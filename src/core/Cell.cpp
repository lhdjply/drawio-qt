#include "core/Cell.h"

#include "core/Page.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QTransform>

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

double Cell::effectiveRotation() const
{
    if (!qFuzzyIsNull(rotation))
    {
        return rotation;
    }

    return styleNumber(QStringLiteral("rotation"), 0.0);
}

QRectF Cell::visualRect() const
{
    const QRectF rect = absoluteRect();
    const double angle = effectiveRotation();

    if (qFuzzyIsNull(angle))
    {
        return rect;
    }

    QTransform transform;
    transform.translate(rect.center().x(), rect.center().y());
    transform.rotate(angle);
    transform.translate(-rect.center().x(), -rect.center().y());

    return transform.mapRect(rect);
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

QString Cell::displayLabel() const
{
    if (!placeholders)
    {
        return value;
    }

    QString text = value;

    if (!placeholderName.isEmpty())
    {
        text.replace(QStringLiteral("%name%"), placeholderName);
    }

    static const QRegularExpression datePattern(QStringLiteral("%date\\{([^}]*)\\}%"));

    for (auto it = datePattern.globalMatch(text); it.hasNext();)
    {
        const QRegularExpressionMatch match = it.next();
        QString format = match.captured(1);
        // JavaScript date tokens (draw.io labels) to Qt's equivalent. Minutes
        // are converted before the month token so that "MMM" stays intact.
        format.replace(QStringLiteral("MM"), QStringLiteral("mm"));
        format.replace(QStringLiteral("mmm"), QStringLiteral("MMM"));
        text.replace(match.captured(0), QDateTime::currentDateTime().toString(format));
    }

    return text;
}

QString Cell::plainLabel() const
{
    const QString label = displayLabel();

    if (label.isEmpty())
    {
        return QString();
    }

    if (!isHtmlLabel())
    {
        return label;
    }

    // A very small subset of the HTML used by draw.io labels. Tags often carry
    // Word-generated style attributes, so block tags are matched including
    // their attributes; otherwise the text runs together on one line.
    QString text = label;
    text.replace(QRegularExpression(QStringLiteral("<br[^>]*>"),
                                    QRegularExpression::CaseInsensitiveOption),
                 QStringLiteral("\n"));
    text.replace(QRegularExpression(QStringLiteral("<div[^>]*>"),
                                    QRegularExpression::CaseInsensitiveOption),
                 QStringLiteral("\n"));
    text.replace(QRegularExpression(QStringLiteral("</div\\s*>"),
                                    QRegularExpression::CaseInsensitiveOption),
                 QStringLiteral("\n"));
    text.replace(QRegularExpression(QStringLiteral("<p[^>]*>"),
                                    QRegularExpression::CaseInsensitiveOption),
                 QStringLiteral("\n"));
    text.replace(QRegularExpression(QStringLiteral("</p\\s*>"),
                                    QRegularExpression::CaseInsensitiveOption),
                 QStringLiteral("\n"));
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
    return isVertex()
        && (styleFlag(QStringLiteral("group"))
            || styleValue(QStringLiteral("shape")).compare(QLatin1String("group"),
                                                           Qt::CaseInsensitive) == 0);
}

bool Cell::isEdgeChild() const
{
    return parent != nullptr && parent->isEdge();
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
