#pragma once

#include <QByteArray>
#include <QString>

namespace dq {

/// draw.io stores diagram pages as base64(deflateRaw(encodeURIComponent(xml))).
/// These helpers reproduce that format byte for byte so files written by this
/// application can be opened by the web and Electron editors and vice versa.
namespace Compression {

QByteArray deflateRaw(const QByteArray &data, bool *ok = nullptr);
QByteArray inflateRaw(const QByteArray &data, bool *ok = nullptr);

/// RFC 1950 zlib streams (used by PNG zTXt chunks and by Qt's qCompress).
QByteArray zlibCompress(const QByteArray &data, bool *ok = nullptr);
QByteArray zlibDecompress(const QByteArray &data, bool *ok = nullptr);

/// JavaScript `encodeURIComponent` compatible percent encoding. It keeps
/// A-Z a-z 0-9 - _ . ! ~ * ' ( ) and encodes every other UTF-8 byte.
QString percentEncode(const QString &text);
QString percentDecode(const QString &text);

QString compressDiagram(const QString &xml);
QString decompressDiagram(const QString &encoded, bool *ok = nullptr);

} // namespace Compression

} // namespace dq
