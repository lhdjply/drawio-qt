#include "io/HtmlExport.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace dq {
namespace HtmlExport {

QByteArray build(const QString &xml, const QString &title, const HtmlOptions &options)
{
    QJsonObject data;
    data.insert(QStringLiteral("nav"), true);
    data.insert(QStringLiteral("resize"), true);
    data.insert(QStringLiteral("xml"), xml);
    data.insert(QStringLiteral("highlight"),
                options.linkColor.isEmpty() ? QStringLiteral("#0000ff")
                                            : options.linkColor);

    if (options.linkTarget == QLatin1String("blank"))
    {
        data.insert(QStringLiteral("target"), QStringLiteral("_blank"));
    }
    else if (options.linkTarget == QLatin1String("self"))
    {
        data.insert(QStringLiteral("target"), QStringLiteral("_self"));
    }

    if (!options.lightbox)
    {
        data.insert(QStringLiteral("lightbox"), false);
    }

    QStringList toolbar;

    if (options.zoom)
    {
        toolbar << QStringLiteral("zoom");
    }

    if (options.layers)
    {
        toolbar << QStringLiteral("layers");
    }

    if (options.tags)
    {
        toolbar << QStringLiteral("tags");
    }

    if (options.lightbox)
    {
        toolbar << QStringLiteral("lightbox");
    }

    if (!toolbar.isEmpty())
    {
        data.insert(QStringLiteral("toolbar"), toolbar.join(QLatin1Char(' ')));
    }

    if (options.theme == QLatin1String("dark") || options.theme == QLatin1String("light"))
    {
        data.insert(QStringLiteral("dark-mode"), options.theme);
    }

    if (!options.editLink.isEmpty())
    {
        data.insert(QStringLiteral("edit"), options.editLink);
    }

    QString json = QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact));
    json.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    json.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    json.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    json.replace(QLatin1Char('>'), QStringLiteral("&gt;"));

    QString html;
    html += QStringLiteral("<!--[if IE]><meta http-equiv=\"X-UA-Compatible\" content=\"IE=5,IE=9\" ><![endif]-->\n");
    html += QStringLiteral("<!DOCTYPE html>\n<html>\n<head>\n<title>")
        + title.toHtmlEscaped() + QStringLiteral("</title>\n<meta charset=\"utf-8\"/>\n</head>\n<body>\n");
    html += QStringLiteral("<div class=\"mxgraph\" style=\"")
        + (options.fit ? QStringLiteral("max-width:100%;") : QString())
        + QStringLiteral("border:1px solid transparent;\" data-mxgraph=\"")
        + json + QStringLiteral("\"></div>\n");
    html += QStringLiteral("<script type=\"text/javascript\" src=\"https://viewer.diagrams.net/js/viewer-static.min.js\"></script>\n");
    html += QStringLiteral("</body>\n</html>\n");

    return html.toUtf8();
}

} // namespace HtmlExport
} // namespace dq
