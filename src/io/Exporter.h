#pragma once

#include "core/Document.h"

#include <QByteArray>
#include <QColor>
#include <QRectF>
#include <QString>

namespace dq {

struct ExportOptions
{
    QString format = QStringLiteral("png"); // png | jpg | jpeg | svg | pdf | xml | html
    double scale = 1.0;
    int border = 0;
    bool transparent = false;
    bool embedXml = false;
    bool uncompressed = false;
    bool allPages = false;
    bool pageView = false;         // --size page
    int fromPage = -1;             // 0 based, inclusive
    int toPage = -1;
    int width = 0;                 // fit to width (px)
    int height = 0;                // fit to height (px)
    int quality = 90;
    int dpi = 0;
    QString theme = QStringLiteral("auto");

    // HTML viewer options (mirrors drawio's --html-* flags)
    QString htmlTheme = QStringLiteral("auto");
    QString htmlLinkTarget = QStringLiteral("auto");
    QString htmlLinkColor;
    QString htmlEditLink;
    bool htmlZoom = true;
    bool htmlLightbox = true;
    bool htmlLayers = true;
    bool htmlTags = true;
    bool htmlFit = true;
};

struct ExportResult
{
    bool ok = false;
    QByteArray data;
    QString error;
    QString mimeType;
    double usedScale = 1.0;
    QSize imageSize;
};

/// Renders pages to files and byte arrays. The same code backs both the
/// interactive "Export" dialog and the command line export mode.
namespace Exporter {

/// Renders one page according to the options (format png/jpg/svg/pdf).
ExportResult exportPage(const Page &page, const ExportOptions &options,
                        const QString &title = QString());

/// Exports the document. For pdf/html/xml the whole document is emitted when
/// options.allPages (or when the format is inherently multi page).
ExportResult exportDocument(const Document &document, const ExportOptions &options);

/// True when the format only makes sense for the whole document.
bool isDocumentFormat(const QString &format);

QString normalizedFormat(const QString &format);

/// Bounds that will be rendered for a page, after the border was applied.
QRectF exportBounds(const Page &page, const ExportOptions &options);

} // namespace Exporter
} // namespace dq
