#pragma once

#include <QColor>
#include <QMap>
#include <QString>

namespace dq {

/// draw.io style strings are semicolon separated `key=value` pairs, e.g.
/// `rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;`
using StyleMap = QMap<QString, QString>;

class Style
{
public:
    /// Parses a draw.io style string into a key/value map. Empty keys and
    /// fragments without '=' are ignored, matching mxGraph's behaviour.
    static StyleMap parse(const QString &text);

    /// Serialises a style map back into the canonical `k=v;` form.
    static QString format(const StyleMap &style);

    static QString value(const StyleMap &style, const QString &key,
                         const QString &fallback = QString());
    static bool flag(const StyleMap &style, const QString &key, bool fallback = false);
    static double number(const StyleMap &style, const QString &key, double fallback = 0.0);
    static QColor color(const StyleMap &style, const QString &key,
                        const QColor &fallback = QColor());

    /// Parses plain colours as well as draw.io's `light-dark(light,dark)`
    /// values. The dark variant is selected when  dark is true.
    static QColor parseColorValue(const QString &value, bool dark = false);

    static void set(StyleMap &style, const QString &key, const QString &value);
    static void unset(StyleMap &style, const QString &key);

    /// draw.io treats `none` as fully transparent for both fill and stroke.
    static bool isNoneColor(const QString &value);

    /// Joins two style fragments, keeping the keys of the first one first.
    static StyleMap merged(const StyleMap &base, const StyleMap &overrides);
};

} // namespace dq
