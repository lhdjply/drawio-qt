#include "io/Exporter.h"

#include "core/FileMeta.h"
#include "core/MxFileIO.h"
#include "io/HtmlExport.h"
#include "render/DiagramRenderer.h"

#include <QBuffer>
#include <QImage>
#include <QPainter>
#include <QPdfWriter>
#include <QPageSize>
#include <QSvgGenerator>
#include <QtMath>

namespace dq {
namespace {

constexpr double kPixelsPerInch = 96.0;
constexpr int kMaxImageDimension = 20000;

QSizeF scaledSize(const QRectF &bounds, double scale)
{
    return QSizeF(bounds.width() * scale, bounds.height() * scale);
}

double fitScale(const QRectF &bounds, const ExportOptions &options)
{
    double scale = options.scale > 0 ? options.scale : 1.0;

    if (bounds.width() <= 0 || bounds.height() <= 0)
    {
        return scale;
    }

    if (options.height > 0)
    {
        scale = options.height / bounds.height();
    }
    else if (options.width > 0)
    {
        scale = options.width / bounds.width();
    }

    return qBound(0.01, scale, 100.0);
}

QRectF baseBounds(const Page &page, const ExportOptions &options)
{
    QRectF bounds = options.pageView && page.settings.page
        ? QRectF(0, 0, page.settings.pageWidth, page.settings.pageHeight)
        : DiagramRenderer::contentBounds(page);

    if (bounds.isEmpty())
    {
        bounds = QRectF(0, 0, 200, 120);
    }

    if (!options.pageView)
    {
        const double border = qMax(0, options.border);

        if (border > 0)
        {
            bounds = bounds.adjusted(-border, -border, border, border);
        }
    }

    return bounds;
}

QColor backgroundColorFor(const ExportOptions &options)
{
    if (options.transparent)
    {
        return QColor(Qt::transparent);
    }

    return QColor(Qt::white);
}

/// draw.io expects an mxfile container when a diagram is embedded in a
/// document (its PDF reader looks for `%3Cmxfile`, for instance). Build one
/// without an XML declaration so the first percent-encoded byte is `<`.
QString mxfileForPage(const Page &page)
{
    const QString model = MxFileIO::pageToXml(page, false);

    return QStringLiteral("<mxfile host=\"drawio-qt\" agent=\"drawio-qt\" version=\"32.6.10\">"
                          "<diagram id=\"%1\" name=\"%2\">%3</diagram></mxfile>")
        .arg(page.id.toHtmlEscaped(), page.name.toHtmlEscaped(), model);
}

QString mxfileForDocument(const Document &document)
{
    QString xml = MxFileIO::documentToXml(document, false).trimmed();

    if (xml.startsWith(QLatin1String("<?xml")))
    {
        const int end = xml.indexOf(QLatin1String("?>"));
        xml = end >= 0 ? xml.mid(end + 2).trimmed() : xml;
    }

    return xml;
}

RenderOptions renderOptionsFor(const ExportOptions &options)
{
    RenderOptions render;
    render.grid = false;
    render.page = options.pageView;
    render.dark = false;
    render.background = QColor(Qt::transparent);
    return render;
}

ExportResult exportImage(const Page &page, const ExportOptions &options)
{
    ExportResult result;
    const QRectF bounds = baseBounds(page, options);
    const double scale = fitScale(bounds, options);

    const int width = qBound(1, static_cast<int>(std::ceil(bounds.width() * scale)),
                             kMaxImageDimension);
    const int height = qBound(1, static_cast<int>(std::ceil(bounds.height() * scale)),
                              kMaxImageDimension);

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);

    if (image.isNull())
    {
        result.error = QStringLiteral("Cannot create %1x%2 image").arg(width).arg(height);
        return result;
    }

    const QColor background = backgroundColorFor(options);
    image.fill(background.alpha() == 0 ? Qt::transparent : background);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.scale(scale, scale);
    painter.translate(-bounds.topLeft());

    const RenderOptions render = renderOptionsFor(options);
    DiagramRenderer::paint(painter, page, render);
    painter.end();

    const QString format = Exporter::normalizedFormat(options.format);
    QByteArray data;

    {
        QBuffer buffer(&data);
        buffer.open(QIODevice::WriteOnly);

        if (format == QLatin1String("jpg") || format == QLatin1String("jpeg"))
        {
            image.save(&buffer, "JPEG", qBound(1, options.quality > 0 ? options.quality : 90, 100));
        }
        else
        {
            image.save(&buffer, "PNG");
        }
    }

    if (data.isEmpty())
    {
        result.error = QStringLiteral("Image encoding failed");
        return result;
    }

    if (options.dpi > 0 && format == QLatin1String("png"))
    {
        data = FileMeta::embedPngDpi(data, options.dpi);
    }

    if (options.embedXml && format == QLatin1String("png"))
    {
        data = FileMeta::embedXmlInPng(data, mxfileForPage(page));
    }

    result.ok = true;
    result.data = data;
    result.mimeType = format == QLatin1String("png") ? QStringLiteral("image/png")
                                                     : QStringLiteral("image/jpeg");
    result.usedScale = scale;
    result.imageSize = QSize(width, height);
    return result;
}

ExportResult exportSvg(const Page &page, const ExportOptions &options)
{
    ExportResult result;
    const QRectF bounds = baseBounds(page, options);
    const double scale = fitScale(bounds, options);
    const QSizeF size = scaledSize(bounds, scale);

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);

    QSvgGenerator generator;
    generator.setOutputDevice(&buffer);
    generator.setSize(QSize(static_cast<int>(std::ceil(size.width())),
                           static_cast<int>(std::ceil(size.height()))));
    generator.setViewBox(QRectF(0, 0, size.width(), size.height()));
    generator.setTitle(QStringLiteral("drawio-qt diagram"));
    generator.setDescription(QStringLiteral("Exported by drawio-qt"));
    generator.setResolution(96);

    QPainter painter;

    if (!painter.begin(&generator))
    {
        result.error = QStringLiteral("Cannot create SVG output");
        return result;
    }

    painter.scale(scale, scale);
    painter.translate(-bounds.topLeft());
    DiagramRenderer::paint(painter, page, renderOptionsFor(options));
    painter.end();
    buffer.close();

    if (data.isEmpty())
    {
        result.error = QStringLiteral("SVG generation failed");
        return result;
    }

    if (options.embedXml)
    {
        data = FileMeta::embedXmlInSvg(data, mxfileForPage(page));
    }

    result.ok = true;
    result.data = data;
    result.mimeType = QStringLiteral("image/svg+xml");
    result.usedScale = scale;
    return result;
}

ExportResult exportPdf(const Document &document, const ExportOptions &options)
{
    ExportResult result;
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);

    QPdfWriter writer(&buffer);
    writer.setResolution(static_cast<int>(kPixelsPerInch));
    writer.setTitle(QStringLiteral("drawio-qt diagram"));
    writer.setCreator(QStringLiteral("drawio-qt %1").arg(QStringLiteral(DRAWIO_QT_VERSION)));

    int first = options.fromPage >= 0 ? options.fromPage : document.currentIndex();
    int last = options.toPage >= 0 ? options.toPage : first;

    if (options.allPages)
    {
        first = 0;
        last = document.pageCount() - 1;
    }

    first = qBound(0, first, document.pageCount() - 1);
    last = qBound(first, last, document.pageCount() - 1);

    QPainter painter;
    bool started = false;
    double usedScale = 1.0;

    for (int index = first; index <= last; ++index)
    {
        const Page *page = document.page(index);

        if (page == nullptr)
        {
            continue;
        }

        const QRectF bounds = baseBounds(*page, options);
        const double scale = fitScale(bounds, options);
        usedScale = scale;

        // One extra device pixel keeps strokes on the outline from being
        // clipped by the page edge.
        const double widthPx = bounds.width() * scale + 1.0;
        const double heightPx = bounds.height() * scale + 1.0;
        const QSizeF pageSize(widthPx / kPixelsPerInch * 25.4,
                              heightPx / kPixelsPerInch * 25.4);

        writer.setPageSize(QPageSize(pageSize, QPageSize::Millimeter,
                                     QStringLiteral("drawio-qt"),
                                     QPageSize::ExactMatch));
        writer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);

        if (!started)
        {
            started = painter.begin(&writer);
        }
        else
        {
            writer.newPage();
        }

        if (!started)
        {
            result.error = QStringLiteral("Cannot create PDF output");
            return result;
        }

        painter.scale(scale, scale);
        painter.translate(-bounds.topLeft());
        DiagramRenderer::paint(painter, *page, renderOptionsFor(options));

        // Reset the painter transform before the next page.
        painter.resetTransform();
    }

    if (started)
    {
        painter.end();
    }

    buffer.close();

    if (data.isEmpty())
    {
        result.error = QStringLiteral("PDF generation failed");
        return result;
    }

    if (options.embedXml)
    {
        // A single diagram is stored as a bare mxGraphModel so readers (and
        // draw.io itself) can parse it without an extra mxfile wrapper.
        QString xml;

        xml = mxfileForDocument(document);

        data = FileMeta::embedXmlInPdf(data, xml);
    }

    result.ok = true;
    result.data = data;
    result.mimeType = QStringLiteral("application/pdf");
    result.usedScale = usedScale;
    return result;
}

ExportResult exportXml(const Document &document, const ExportOptions &options)
{
    ExportResult result;

    // draw.io's XML export is an mxfile container (one diagram per page), so
    // the result can be opened again with either editor.
    result.data = MxFileIO::documentToXml(document, !options.uncompressed).toUtf8();
    result.ok = true;
    result.mimeType = QStringLiteral("application/xml");
    return result;
}

ExportResult exportHtml(const Document &document, const ExportOptions &options)
{
    ExportResult result;
    const Page *page = document.currentPage();

    if (page == nullptr)
    {
        result.error = QStringLiteral("No pages to export");
        return result;
    }

    const QString xml = options.allPages
        ? MxFileIO::documentToXml(document, false)
        : MxFileIO::pageToXml(*page, false);

    HtmlOptions html;
    html.theme = options.htmlTheme;
    html.linkTarget = options.htmlLinkTarget;
    html.linkColor = options.htmlLinkColor;
    html.editLink = options.htmlEditLink;
    html.zoom = options.htmlZoom;
    html.lightbox = options.htmlLightbox;
    html.layers = options.htmlLayers;
    html.tags = options.htmlTags;
    html.fit = options.htmlFit;

    result.data = HtmlExport::build(xml, document.displayName(), html);
    result.ok = !result.data.isEmpty();
    result.mimeType = QStringLiteral("text/html");

    if (!result.ok)
    {
        result.error = QStringLiteral("HTML generation failed");
    }

    return result;
}

} // namespace

namespace Exporter {

bool isDocumentFormat(const QString &format)
{
    const QString normalized = normalizedFormat(format);
    return normalized == QLatin1String("pdf") || normalized == QLatin1String("html")
        || normalized == QLatin1String("xml");
}

QString normalizedFormat(const QString &format)
{
    const QString lower = format.trimmed().toLower();

    if (lower == QLatin1String("jpeg"))
    {
        return QStringLiteral("jpg");
    }

    return lower;
}

QRectF exportBounds(const Page &page, const ExportOptions &options)
{
    return baseBounds(page, options);
}

ExportResult exportPage(const Page &page, const ExportOptions &options, const QString &title)
{
    Q_UNUSED(title);

    const QString format = normalizedFormat(options.format);

    if (format == QLatin1String("svg"))
    {
        return exportSvg(page, options);
    }

    if (format == QLatin1String("png") || format == QLatin1String("jpg"))
    {
        return exportImage(page, options);
    }

    ExportResult result;
    result.error = QStringLiteral("Unsupported page export format: %1").arg(format);
    return result;
}

ExportResult exportDocument(const Document &document, const ExportOptions &options)
{
    const QString format = normalizedFormat(options.format);

    if (format == QLatin1String("pdf"))
    {
        return exportPdf(document, options);
    }

    if (format == QLatin1String("xml"))
    {
        return exportXml(document, options);
    }

    if (format == QLatin1String("html"))
    {
        return exportHtml(document, options);
    }

    const Page *page = document.currentPage();

    if (page == nullptr)
    {
        ExportResult result;
        result.error = QStringLiteral("No pages to export");
        return result;
    }

    return exportPage(*page, options);
}

} // namespace Exporter
} // namespace dq
