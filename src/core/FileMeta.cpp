#include "core/FileMeta.h"

#include "core/Compression.h"

#include <QRegularExpression>

#include <algorithm>

#include <zlib.h>

namespace dq {
namespace {

quint32 readUInt32(const char *data)
{
    const auto *bytes = reinterpret_cast<const unsigned char *>(data);
    return (static_cast<quint32>(bytes[0]) << 24)
        | (static_cast<quint32>(bytes[1]) << 16)
        | (static_cast<quint32>(bytes[2]) << 8)
        | static_cast<quint32>(bytes[3]);
}

void appendUInt32(QByteArray &target, quint32 value)
{
    target.append(static_cast<char>((value >> 24) & 0xFF));
    target.append(static_cast<char>((value >> 16) & 0xFF));
    target.append(static_cast<char>((value >> 8) & 0xFF));
    target.append(static_cast<char>(value & 0xFF));
}

bool isPng(const QByteArray &data)
{
    static const unsigned char signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    return data.size() >= 8
        && QByteArray::fromRawData(reinterpret_cast<const char *>(signature), 8)
               == data.left(8);
}

bool isPdf(const QByteArray &data)
{
    return data.startsWith("%PDF-");
}

bool isSvg(const QByteArray &data)
{
    const QByteArray head = data.left(4096).trimmed();
    return head.startsWith("<?xml") ? head.contains("<svg") : head.startsWith("<svg");
}

QByteArray makeChunk(const QByteArray &type, const QByteArray &payload)
{
    QByteArray chunk;
    appendUInt32(chunk, static_cast<quint32>(payload.size()));
    chunk.append(type);
    chunk.append(payload);

    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const Bytef *>(type.constData()),
                static_cast<uInt>(type.size()));
    crc = crc32(crc, reinterpret_cast<const Bytef *>(payload.constData()),
                static_cast<uInt>(payload.size()));

    appendUInt32(chunk, static_cast<quint32>(crc));
    return chunk;
}

QString decodeEmbeddedValue(const QString &value)
{
    QString text = value;

    if (text.startsWith(QLatin1Char('%')))
    {
        return Compression::percentDecode(text);
    }

    if (!text.trimmed().startsWith(QLatin1Char('<')))
    {
        const QByteArray decoded = QByteArray::fromBase64(text.toLatin1());

        if (decoded.startsWith('<'))
        {
            return QString::fromUtf8(decoded);
        }
    }

    return text;
}

// --- minimal PDF string helpers -----------------------------------------

QString unescapePdfLiteral(const QByteArray &raw)
{
    QByteArray result;
    result.reserve(raw.size());

    for (int i = 0; i < raw.size(); ++i)
    {
        const char ch = raw.at(i);

        if (ch == '\\' && i + 1 < raw.size())
        {
            const char next = raw.at(++i);

            if (next == 'n')
            {
                result.append('\n');
            }
            else if (next == 'r')
            {
                result.append('\r');
            }
            else if (next == 't')
            {
                result.append('\t');
            }
            else
            {
                result.append(next);
            }
        }
        else
        {
            result.append(ch);
        }
    }

    return QString::fromLatin1(result);
}

QString decodePdfHex(const QByteArray &hex)
{
    QByteArray bytes;

    for (int i = 0; i + 1 < hex.size(); i += 2)
    {
        bool ok = false;
        const int value = hex.mid(i, 2).toInt(&ok, 16);

        if (ok)
        {
            bytes.append(static_cast<char>(value));
        }
    }

    // drawio's reader strips zero bytes from UTF-16BE encoded subjects.
    if (bytes.contains('\0'))
    {
        QByteArray filtered;
        filtered.reserve(bytes.size());

        for (const char ch : bytes)
        {
            if (ch != '\0')
            {
                filtered.append(ch);
            }
        }

        bytes = filtered;
    }

    return QString::fromLatin1(bytes);
}

QByteArray escapePdfLiteral(const QString &text)
{
    QByteArray result;
    const QByteArray utf8 = text.toLatin1();

    for (const char ch : utf8)
    {
        if (ch == '(' || ch == ')' || ch == '\\')
        {
            result.append('\\');
        }

        result.append(ch);
    }

    return result;
}

} // namespace

namespace FileMeta {

Container detect(const QByteArray &data, const QString &path)
{
    if (isPng(data))
    {
        return Container::Png;
    }

    if (isPdf(data))
    {
        return Container::Pdf;
    }

    if (isSvg(data))
    {
        return Container::Svg;
    }

    const QString suffix = path.section(QLatin1Char('.'), -1).toLower();

    if (suffix == QLatin1String("png"))
    {
        return Container::Png;
    }

    if (suffix == QLatin1String("pdf"))
    {
        return Container::Pdf;
    }

    if (suffix == QLatin1String("svg"))
    {
        return Container::Svg;
    }

    return Container::None;
}

QString xmlFromPng(const QByteArray &png, bool *ok)
{
    if (!isPng(png))
    {
        if (ok != nullptr)
        {
            *ok = false;
        }
        return {};
    }

    int offset = 8;

    while (offset + 8 <= png.size())
    {
        const quint32 length = readUInt32(png.constData() + offset);
        const QByteArray type = png.mid(offset + 4, 4);
        const int dataStart = offset + 8;

        if (dataStart + static_cast<int>(length) > png.size())
        {
            break;
        }

        if (type == "tEXt" || type == "zTXt")
        {
            const int keyEnd = png.indexOf('\0', dataStart);

            if (keyEnd > 0 && keyEnd < dataStart + static_cast<int>(length))
            {
                const QByteArray key = png.mid(dataStart, keyEnd - dataStart);

                if (key == "mxGraphModel")
                {
                    if (type == "tEXt")
                    {
                        const QString value = QString::fromUtf8(
                            png.mid(keyEnd + 1, dataStart + static_cast<int>(length) - keyEnd - 1));

                        if (ok != nullptr)
                        {
                            *ok = true;
                        }

                        return decodeEmbeddedValue(value);
                    }

                    // zTXt: key\0 compressionMethod compressedData
                    const QByteArray compressed = png.mid(
                        keyEnd + 2, dataStart + static_cast<int>(length) - keyEnd - 2);
                    bool inflatedOk = false;
                    QByteArray inflated = Compression::zlibDecompress(compressed, &inflatedOk);

                    if (!inflatedOk)
                    {
                        // Pre-fix drawio builds wrote raw deflate here.
                        inflated = Compression::inflateRaw(compressed, &inflatedOk);
                    }

                    if (inflatedOk)
                    {
                        if (ok != nullptr)
                        {
                            *ok = true;
                        }

                        const QString text = QString::fromUtf8(inflated);
                        return text.startsWith(QLatin1Char('%'))
                            ? Compression::percentDecode(text) : text;
                    }
                }
            }
        }

        offset = dataStart + static_cast<int>(length) + 4; // skip CRC
    }

    if (ok != nullptr)
    {
        *ok = false;
    }

    return {};
}

QString xmlFromPdf(const QByteArray &pdf, bool *ok)
{
    if (!isPdf(pdf))
    {
        if (ok != nullptr)
        {
            *ok = false;
        }
        return {};
    }

    const QByteArray text = pdf;
    int searchFrom = 0;

    while (true)
    {
        const int subject = text.indexOf("/Subject", searchFrom);

        if (subject < 0)
        {
            break;
        }

        int cursor = subject + 8;

        while (cursor < text.size() && (text.at(cursor) == ' ' || text.at(cursor) == '\n'
                                        || text.at(cursor) == '\r' || text.at(cursor) == '\t'))
        {
            ++cursor;
        }

        if (cursor >= text.size())
        {
            break;
        }

        if (text.at(cursor) == '(')
        {
            int depth = 0;
            int end = cursor;

            for (; end < text.size(); ++end)
            {
                const char ch = text.at(end);

                if (ch == '\\')
                {
                    ++end;
                    continue;
                }

                if (ch == '(')
                {
                    ++depth;
                }
                else if (ch == ')')
                {
                    --depth;

                    if (depth == 0)
                    {
                        break;
                    }
                }
            }

            if (end < text.size())
            {
                const QString value = unescapePdfLiteral(text.mid(cursor + 1, end - cursor - 1));
                const QString decoded = decodeEmbeddedValue(value);

                if (decoded.trimmed().startsWith(QLatin1Char('<')))
                {
                    if (ok != nullptr)
                    {
                        *ok = true;
                    }
                    return decoded;
                }
            }
        }
        else if (text.at(cursor) == '<')
        {
            const int end = text.indexOf('>', cursor);

            if (end > cursor)
            {
                const QString value = decodePdfHex(text.mid(cursor + 1, end - cursor - 1));
                const QString decoded = decodeEmbeddedValue(value);

                if (decoded.trimmed().startsWith(QLatin1Char('<')))
                {
                    if (ok != nullptr)
                    {
                        *ok = true;
                    }
                    return decoded;
                }
            }
        }

        searchFrom = subject + 8;
    }

    if (ok != nullptr)
    {
        *ok = false;
    }

    return {};
}

QString xmlFromSvg(const QByteArray &svg, bool *ok)
{
    const QString content = QString::fromUtf8(svg);
    static const QRegularExpression contentAttribute(
        QStringLiteral("\\bcontent\\s*=\\s*\"([^\"]*)\""),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = contentAttribute.match(content);

    if (!match.hasMatch())
    {
        if (ok != nullptr)
        {
            *ok = false;
        }
        return {};
    }

    QString value = match.captured(1);
    value.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    value.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    value.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    value.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    value.replace(QStringLiteral("&#39;"), QStringLiteral("'"));

    const QString decoded = decodeEmbeddedValue(value);

    if (ok != nullptr)
    {
        *ok = !decoded.isEmpty();
    }

    return decoded;
}

QString xmlFromDocument(const QByteArray &data, Container container, bool *ok)
{
    switch (container)
    {
    case Container::Png:
        return xmlFromPng(data, ok);
    case Container::Pdf:
        return xmlFromPdf(data, ok);
    case Container::Svg:
        return xmlFromSvg(data, ok);
    case Container::None:
        break;
    }

    if (ok != nullptr)
    {
        *ok = false;
    }

    return {};
}

QByteArray embedXmlInPng(const QByteArray &png, const QString &xml)
{
    if (!isPng(png))
    {
        return png;
    }

    const QByteArray payload = QByteArray("mxGraphModel", 12) + QByteArray(2, '\0')
        + Compression::zlibCompress(Compression::percentEncode(xml).toUtf8());
    const QByteArray chunk = makeChunk(QByteArray("zTXt"), payload);

    // Insert right before IDAT so decoders see the metadata early.
    int offset = 8;

    while (offset + 8 <= png.size())
    {
        const quint32 length = readUInt32(png.constData() + offset);
        const QByteArray type = png.mid(offset + 4, 4);

        if (type == "IDAT")
        {
            QByteArray result = png.left(offset);
            result.append(chunk);
            result.append(png.mid(offset));
            return result;
        }

        offset += 12 + static_cast<int>(length);
    }

    // No IDAT: append the chunk before IEND if present.
    const int iend = png.lastIndexOf("IEND");

    if (iend > 4)
    {
        QByteArray result = png.left(iend - 4);
        result.append(chunk);
        result.append(png.mid(iend - 4));
        return result;
    }

    return png;
}

QByteArray embedPngDpi(const QByteArray &png, int dpi)
{
    if (!isPng(png))
    {
        return png;
    }

    double meters = dpi / 0.0254;
    quint32 perMeter = static_cast<quint32>(meters <= 0 ? 3937.0 : meters);

    QByteArray payload;
    appendUInt32(payload, perMeter);
    appendUInt32(payload, perMeter);
    payload.append(static_cast<char>(1));

    const QByteArray chunk = makeChunk(QByteArray("pHYs"), payload);
    int offset = 8;

    while (offset + 8 <= png.size())
    {
        const quint32 length = readUInt32(png.constData() + offset);
        const QByteArray type = png.mid(offset + 4, 4);

        if (type == "IDAT")
        {
            QByteArray result = png.left(offset);
            result.append(chunk);
            result.append(png.mid(offset));
            return result;
        }

        offset += 12 + static_cast<int>(length);
    }

    return png;
}

namespace {

struct PdfTrailerInfo
{
    bool valid = false;
    QByteArray rootRef;
    QByteArray infoRef;
    QByteArray infoDictionary;
    int size = 0;
    int startXref = 0;
};

PdfTrailerInfo parseTrailer(const QByteArray &pdf)
{
    PdfTrailerInfo info;
    const int startXrefPos = pdf.lastIndexOf("startxref");

    if (startXrefPos < 0)
    {
        return info;
    }

    const int trailerPos = pdf.lastIndexOf("trailer", startXrefPos);

    if (trailerPos < 0)
    {
        return info;
    }

    const QByteArray trailer = pdf.mid(trailerPos, startXrefPos - trailerPos);
    static const QRegularExpression rootPattern(
        QStringLiteral("/Root\\s+(\\d+)\\s+(\\d+)\\s+R"));
    static const QRegularExpression infoPattern(
        QStringLiteral("/Info\\s+(\\d+)\\s+(\\d+)\\s+R"));
    static const QRegularExpression sizePattern(QStringLiteral("/Size\\s+(\\d+)"));

    const QRegularExpressionMatch rootMatch = rootPattern.match(QString::fromLatin1(trailer));
    const QRegularExpressionMatch infoMatch = infoPattern.match(QString::fromLatin1(trailer));
    const QRegularExpressionMatch sizeMatch = sizePattern.match(QString::fromLatin1(trailer));

    if (!rootMatch.hasMatch())
    {
        return info;
    }

    info.rootRef = (rootMatch.captured(1) + QStringLiteral(" ") + rootMatch.captured(2)
                    + QStringLiteral(" R")).toLatin1();

    if (infoMatch.hasMatch())
    {
        info.infoRef = (infoMatch.captured(1) + QStringLiteral(" ") + infoMatch.captured(2)
                        + QStringLiteral(" R")).toLatin1();

        // Extract the existing Info dictionary so its entries are preserved.
        const QByteArray marker = (infoMatch.captured(1) + QStringLiteral(" ")
                                   + infoMatch.captured(2) + QStringLiteral(" obj")).toLatin1();
        const int objPos = pdf.indexOf(marker);

        if (objPos >= 0)
        {
            const int dictStart = pdf.indexOf("<<", objPos);

            if (dictStart >= 0)
            {
                int depth = 0;
                int dictEnd = dictStart;

                for (; dictEnd < pdf.size(); ++dictEnd)
                {
                    if (pdf.mid(dictEnd, 2) == "<<")
                    {
                        ++depth;
                        ++dictEnd;
                    }
                    else if (pdf.mid(dictEnd, 2) == ">>")
                    {
                        --depth;

                        if (depth == 0)
                        {
                            break;
                        }

                        ++dictEnd;
                    }
                }

                if (dictEnd < pdf.size())
                {
                    info.infoDictionary = pdf.mid(dictStart + 2, dictEnd - dictStart - 2);
                }
            }
        }
    }

    if (sizeMatch.hasMatch())
    {
        info.size = sizeMatch.captured(1).toInt();
    }

    info.startXref = pdf.mid(startXrefPos + 9).trimmed().split('\n').value(0).trimmed().toInt();
    info.valid = true;
    return info;
}

/// Removes an existing /Subject entry from a flattened Info dictionary.
QByteArray stripSubject(const QByteArray &dictionary)
{
    QByteArray result;
    int i = 0;

    while (i < dictionary.size())
    {
        if (dictionary.at(i) == '/' && dictionary.mid(i, 8) == "/Subject")
        {
            i += 8;

            // Skip whitespace then one value: literal string, hex string, name
            // or reference. drawio's Subject is always a string.
            while (i < dictionary.size() && (dictionary.at(i) == ' ' || dictionary.at(i) == '\n'
                                             || dictionary.at(i) == '\r' || dictionary.at(i) == '\t'))
            {
                ++i;
            }

            if (i < dictionary.size() && dictionary.at(i) == '(')
            {
                int depth = 0;

                for (; i < dictionary.size(); ++i)
                {
                    const char ch = dictionary.at(i);

                    if (ch == '\\')
                    {
                        ++i;
                        continue;
                    }

                    if (ch == '(')
                    {
                        ++depth;
                    }
                    else if (ch == ')')
                    {
                        --depth;

                        if (depth == 0)
                        {
                            ++i;
                            break;
                        }
                    }
                }
            }
            else if (i < dictionary.size() && dictionary.at(i) == '<')
            {
                const int end = dictionary.indexOf('>', i);
                i = end < 0 ? dictionary.size() : end + 1;
            }
            else
            {
                while (i < dictionary.size() && dictionary.at(i) != '/' && dictionary.at(i) != '\n')
                {
                    ++i;
                }
            }

            continue;
        }

        result.append(dictionary.at(i));
        ++i;
    }

    return result;
}

} // namespace

QByteArray embedXmlInPdf(const QByteArray &pdf, const QString &xml)
{
    if (!isPdf(pdf) || xml.isEmpty())
    {
        return pdf;
    }

    const PdfTrailerInfo info = parseTrailer(pdf);

    if (!info.valid)
    {
        return pdf;
    }

    const int newObject = std::max(info.size, 2);
    QByteArray updatedPdf = pdf;

    if (!updatedPdf.endsWith('\n'))
    {
        updatedPdf.append('\n');
    }

    const QByteArray subjectValue = escapePdfLiteral(Compression::percentEncode(xml));
    const QByteArray dictionary = "<<"
        + stripSubject(info.infoDictionary).trimmed()
        + " /Subject (" + subjectValue + ") >>";

    const int objectOffset = updatedPdf.size();
    updatedPdf.append(QByteArray::number(newObject) + " 0 obj\n" + dictionary
                      + "\nendobj\n");

    const int xrefOffset = updatedPdf.size();
    QByteArray xref = "xref\n0 1\n0000000000 65535 f \n";
    xref.append(QByteArray::number(newObject) + " 1\n");
    xref.append(QByteArray::number(objectOffset).rightJustified(10, '0') + " 00000 n \n");
    updatedPdf.append(xref);

    QByteArray trailer = "trailer\n<< /Size ";
    trailer.append(QByteArray::number(newObject + 1));
    trailer.append(" /Root " + info.rootRef);
    trailer.append(" /Info " + QByteArray::number(newObject) + " 0 R");

    if (info.startXref > 0)
    {
        trailer.append(" /Prev " + QByteArray::number(info.startXref));
    }

    trailer.append(" >>\nstartxref\n" + QByteArray::number(xrefOffset) + "\n%%EOF\n");
    updatedPdf.append(trailer);

    return updatedPdf;
}

QByteArray embedXmlInSvg(const QByteArray &svg, const QString &xml)
{
    QString text = QString::fromUtf8(svg);
    const int svgStart = text.indexOf(QStringLiteral("<svg"));

    if (svgStart < 0)
    {
        return svg;
    }

    int tagEnd = text.indexOf(QLatin1Char('>'), svgStart);

    if (tagEnd < 0)
    {
        return svg;
    }

    QString escaped = Compression::percentEncode(xml);
    escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));

    // Insert before the closing '>' (or the '/>' of a self closing root).
    if (text.mid(tagEnd - 1, 1) == QLatin1String("/"))
    {
        --tagEnd;
    }

    text.insert(tagEnd, QStringLiteral(" content=\"") + escaped + QStringLiteral("\""));
    return text.toUtf8();
}

} // namespace FileMeta
} // namespace dq
