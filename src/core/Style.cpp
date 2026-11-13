#include "core/Style.h"

#include <QHash>
#include <QStringList>

namespace dq {

StyleMap Style::parse(const QString &text)
{
    StyleMap result;
    const QStringList parts = text.split(QLatin1Char(';'), Qt::SkipEmptyParts);

    for (const QString &part : parts)
    {
        const int eq = part.indexOf(QLatin1Char('='));

        if (eq <= 0)
        {
            // A bare keyword such as `text;` is a valid style fragment that
            // drawio renders as a flag; store it with an empty value so a
            // round-trip preserves it.
            if (eq < 0 && !part.trimmed().isEmpty())
            {
                result.insert(part.trimmed(), QString());
            }
            continue;
        }

        const QString key = part.left(eq).trimmed();
        const QString value = part.mid(eq + 1);
        result.insert(key, value);
    }

    return result;
}

QString Style::format(const StyleMap &style)
{
    QString result;
    result.reserve(style.size() * 16);

    for (auto it = style.constBegin(); it != style.constEnd(); ++it)
    {
        result += it.key();

        if (!it.value().isNull())
        {
            result += QLatin1Char('=');
            result += it.value();
        }

        result += QLatin1Char(';');
    }

    return result;
}

QString Style::value(const StyleMap &style, const QString &key, const QString &fallback)
{
    const auto it = style.constFind(key);
    return it == style.constEnd() ? fallback : it.value();
}

bool Style::flag(const StyleMap &style, const QString &key, bool fallback)
{
    const auto it = style.constFind(key);

    if (it == style.constEnd() || it.value().isEmpty())
    {
        // `shadow;` / `rounded;` style flags default to true.
        return it == style.constEnd() ? fallback : true;
    }

    const QString value = it.value().trimmed().toLower();
    return value == QStringLiteral("1") || value == QStringLiteral("true");
}

double Style::number(const StyleMap &style, const QString &key, double fallback)
{
    const auto it = style.constFind(key);

    if (it == style.constEnd() || it.value().isEmpty())
    {
        return fallback;
    }

    bool ok = false;
    const double parsed = it.value().toDouble(&ok);
    return ok ? parsed : fallback;
}

QColor Style::parseColorValue(const QString &value, bool dark)
{
    const QString text = value.trimmed();

    if (text.startsWith(QLatin1String("light-dark("), Qt::CaseInsensitive)
        && text.endsWith(QLatin1Char(')')))
    {
        const QString inner = text.mid(11, text.size() - 12);
        const int comma = inner.indexOf(QLatin1Char(','));

        if (comma >= 0)
        {
            const QString chosen = dark ? inner.mid(comma + 1) : inner.left(comma);
            return parseColorValue(chosen.trimmed(), dark);
        }
    }

    // draw.io's HTML labels use rgb()/rgba() colours.
    if (text.startsWith(QLatin1String("rgb"), Qt::CaseInsensitive)
        && text.endsWith(QLatin1Char(')')))
    {
        const int open = text.indexOf(QLatin1Char('('));
        const QStringList parts = text.mid(open + 1, text.size() - open - 2)
                                      .split(QLatin1Char(','), Qt::SkipEmptyParts);

        if (parts.size() >= 3)
        {
            const int r = qBound(0, qRound(parts.at(0).trimmed().toDouble()), 255);
            const int g = qBound(0, qRound(parts.at(1).trimmed().toDouble()), 255);
            const int b = qBound(0, qRound(parts.at(2).trimmed().toDouble()), 255);
            int a = 255;

            if (parts.size() >= 4)
            {
                a = qBound(0, qRound(parts.at(3).trimmed().toDouble() * 255.0), 255);
            }

            return QColor(r, g, b, a);
        }
    }

    return QColor(text);
}

QColor Style::color(const StyleMap &style, const QString &key, const QColor &fallback)
{
    const auto it = style.constFind(key);

    if (it == style.constEnd() || it.value().isEmpty() || isNoneColor(it.value()))
    {
        return it == style.constEnd() ? fallback : QColor(Qt::transparent);
    }

    const QColor parsed = parseColorValue(it.value());

    if (!parsed.isValid())
    {
        return fallback;
    }

    return parsed;
}

void Style::set(StyleMap &style, const QString &key, const QString &value)
{
    style.insert(key, value);
}

void Style::unset(StyleMap &style, const QString &key)
{
    style.remove(key);
}

bool Style::isNoneColor(const QString &value)
{
    return value.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0;
}

StyleMap Style::merged(const StyleMap &base, const StyleMap &overrides)
{
    StyleMap result = base;

    for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it)
    {
        result.insert(it.key(), it.value());
    }

    return result;
}

namespace {

/// Named styles from draw.io's `styles/default.xml`. Only the entries that
/// appear as bare keywords in diagram styles are listed; `extend` is resolved
/// while building the table so that `edgeLabel` inherits from `text`.
const QHash<QString, StyleMap> &namedStyles()
{
    static const QHash<QString, StyleMap> table = []()
    {
        QHash<QString, StyleMap> styles;

        const StyleMap text = {
            {QStringLiteral("fillColor"), QStringLiteral("none")},
            {QStringLiteral("gradientColor"), QStringLiteral("none")},
            {QStringLiteral("strokeColor"), QStringLiteral("none")},
            {QStringLiteral("align"), QStringLiteral("left")},
            {QStringLiteral("verticalAlign"), QStringLiteral("top")},
        };
        styles.insert(QStringLiteral("text"), text);
        styles.insert(QStringLiteral("edgeLabel"),
                      Style::merged(text, {{QStringLiteral("labelBackgroundColor"), QStringLiteral("default")},
                                    {QStringLiteral("fontSize"), QStringLiteral("11")}}));
        // styles/default.xml: label is the default vertex shape with an
        // image and a text column; icon extends it with a centred image and
        // the label below the shape.
        styles.insert(QStringLiteral("label"),
                      {{QStringLiteral("shape"), QStringLiteral("label")},
                       {QStringLiteral("fontStyle"), QStringLiteral("1")},
                       {QStringLiteral("align"), QStringLiteral("left")},
                       {QStringLiteral("verticalAlign"), QStringLiteral("middle")},
                       {QStringLiteral("spacing"), QStringLiteral("2")},
                       {QStringLiteral("spacingLeft"), QStringLiteral("52")},
                       {QStringLiteral("imageWidth"), QStringLiteral("42")},
                       {QStringLiteral("imageHeight"), QStringLiteral("42")},
                       {QStringLiteral("rounded"), QStringLiteral("1")}});
        styles.insert(QStringLiteral("icon"),
                      {{QStringLiteral("shape"), QStringLiteral("label")},
                       {QStringLiteral("fontStyle"), QStringLiteral("0")},
                       {QStringLiteral("align"), QStringLiteral("center")},
                       {QStringLiteral("verticalAlign"), QStringLiteral("top")},
                       {QStringLiteral("verticalLabelPosition"), QStringLiteral("bottom")},
                       {QStringLiteral("imageAlign"), QStringLiteral("center")},
                       {QStringLiteral("spacing"), QStringLiteral("0")},
                       {QStringLiteral("spacingLeft"), QStringLiteral("0")},
                       {QStringLiteral("spacingTop"), QStringLiteral("6")},
                       {QStringLiteral("labelBackgroundColor"), QStringLiteral("default")},
                       {QStringLiteral("imageWidth"), QStringLiteral("48")},
                       {QStringLiteral("imageHeight"), QStringLiteral("48")},
                       {QStringLiteral("rounded"), QStringLiteral("1")}});
        styles.insert(QStringLiteral("swimlane"),
                      {{QStringLiteral("shape"), QStringLiteral("swimlane")},
                       {QStringLiteral("fontSize"), QStringLiteral("12")},
                       {QStringLiteral("fontStyle"), QStringLiteral("1")},
                       {QStringLiteral("startSize"), QStringLiteral("23")}});
        styles.insert(QStringLiteral("group"),
                      {{QStringLiteral("verticalAlign"), QStringLiteral("top")},
                       {QStringLiteral("fillColor"), QStringLiteral("none")},
                       {QStringLiteral("strokeColor"), QStringLiteral("none")},
                       {QStringLiteral("gradientColor"), QStringLiteral("none")},
                       {QStringLiteral("pointerEvents"), QStringLiteral("0")}});
        styles.insert(QStringLiteral("ellipse"),
                      {{QStringLiteral("shape"), QStringLiteral("ellipse")},
                       {QStringLiteral("perimeter"), QStringLiteral("ellipsePerimeter")}});
        styles.insert(QStringLiteral("rhombus"),
                      {{QStringLiteral("shape"), QStringLiteral("rhombus")},
                       {QStringLiteral("perimeter"), QStringLiteral("rhombusPerimeter")}});
        styles.insert(QStringLiteral("triangle"),
                      {{QStringLiteral("shape"), QStringLiteral("triangle")},
                       {QStringLiteral("perimeter"), QStringLiteral("trianglePerimeter")}});
        styles.insert(QStringLiteral("line"),
                      {{QStringLiteral("shape"), QStringLiteral("line")},
                       {QStringLiteral("strokeWidth"), QStringLiteral("4")},
                       {QStringLiteral("labelBackgroundColor"), QStringLiteral("default")},
                       {QStringLiteral("verticalAlign"), QStringLiteral("top")},
                       {QStringLiteral("spacingTop"), QStringLiteral("8")}});
        styles.insert(QStringLiteral("image"),
                      {{QStringLiteral("shape"), QStringLiteral("image")},
                       {QStringLiteral("labelBackgroundColor"), QStringLiteral("default")},
                       {QStringLiteral("verticalAlign"), QStringLiteral("top")},
                       {QStringLiteral("verticalLabelPosition"), QStringLiteral("bottom")}});
        styles.insert(QStringLiteral("arrow"),
                      {{QStringLiteral("shape"), QStringLiteral("arrow")},
                       {QStringLiteral("edgeStyle"), QStringLiteral("none")},
                       {QStringLiteral("fillColor"), QStringLiteral("default")}});

        return styles;
    }();

    return table;
}

} // namespace

StyleMap Style::withDefaults(const StyleMap &style)
{
    bool hasKeyword = false;

    for (auto it = style.constBegin(); it != style.constEnd(); ++it)
    {
        if (it.value().isNull())
        {
            hasKeyword = true;
            break;
        }
    }

    // Most styles only contain explicit pairs; the map is implicitly shared,
    // so returning it avoids building a copy for every painted label.
    if (!hasKeyword)
    {
        return style;
    }

    StyleMap result;

    // A bare keyword such as `text;` was stored with a null value by parse();
    // it pulls in the matching named style from draw.io's stylesheet.
    for (auto it = style.constBegin(); it != style.constEnd(); ++it)
    {
        if (!it.value().isNull())
        {
            continue;
        }

        const auto named = namedStyles().constFind(it.key());

        if (named != namedStyles().constEnd())
        {
            for (auto entry = named.value().constBegin();
                 entry != named.value().constEnd(); ++entry)
            {
                result.insert(entry.key(), entry.value());
            }
        }
    }

    // Explicit pairs win over the named style defaults.
    for (auto it = style.constBegin(); it != style.constEnd(); ++it)
    {
        if (!it.value().isNull())
        {
            result.insert(it.key(), it.value());
        }
    }

    return result;
}

} // namespace dq
