#pragma once

#include <QByteArray>
#include <QString>

namespace dq {

/// Reading and writing of draw.io's "diagram embedded in a document" formats:
/// PNG (zTXt/tEXt chunk named mxGraphModel), PDF (the /Subject entry) and SVG
/// (the content attribute of the root element).
namespace FileMeta {

enum class Container
{
    None,
    Png,
    Pdf,
    Svg
};

Container detect(const QByteArray &data, const QString &path = QString());

/// Returns the diagram XML embedded in the given document, or an empty string.
QString xmlFromPng(const QByteArray &png, bool *ok = nullptr);
QString xmlFromPdf(const QByteArray &pdf, bool *ok = nullptr);
QString xmlFromSvg(const QByteArray &svg, bool *ok = nullptr);
QString xmlFromDocument(const QByteArray &data, Container container, bool *ok = nullptr);

/// Injects XML into a PNG (zTXt before IDAT) or PDF (Info /Subject).
QByteArray embedXmlInPng(const QByteArray &png, const QString &xml);
QByteArray embedXmlInPdf(const QByteArray &pdf, const QString &xml);
QByteArray embedXmlInSvg(const QByteArray &svg, const QString &xml);

/// Adds a pHYs chunk describing the resolution of an exported PNG.
QByteArray embedPngDpi(const QByteArray &png, int dpi);

} // namespace FileMeta

} // namespace dq
