#include "core/Style.h"

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

} // namespace dq
