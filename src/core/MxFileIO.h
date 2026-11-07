#pragma once

#include <QString>

namespace dq {

class Cell;
class Document;
class Page;

/// Reading and writing of draw.io's native XML container format:
///
///   <mxfile ...>
///     <diagram id="..." name="Page-1">
///       <mxGraphModel ...><root><mxCell .../></root></mxGraphModel>
///     </diagram>
///   </mxfile>
///
/// Pages are compressed by default, using the same
/// base64+deflate+percent-encoding pipeline as the JavaScript editor.
namespace MxFileIO {

QString cellToXml(const Cell &cell);
QString pageToXml(const Page &page, bool compressed = false);
bool pageFromXml(Page &page, const QString &xml, QString *error = nullptr);

QString documentToXml(const Document &document, bool compressPages = true);
bool documentFromXml(Document &document, const QString &xml, QString *error = nullptr);

/// Reads the value of a boolean/number XML attribute with a fallback.
bool xmlBool(const QString &value, bool fallback);

} // namespace MxFileIO

} // namespace dq
