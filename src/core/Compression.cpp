#include "core/Compression.h"

#include <QStringDecoder>

#include <zlib.h>

namespace dq {
namespace Compression {

QByteArray deflateRaw(const QByteArray &data, bool *ok)
{
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.constData()));
    stream.avail_in = static_cast<uInt>(data.size());

    if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -15, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK)
    {
        if (ok != nullptr)
        {
            *ok = false;
        }
        return {};
    }

    QByteArray output;
    output.resize(static_cast<int>(deflateBound(&stream, static_cast<uLong>(data.size()))));

    stream.next_out = reinterpret_cast<Bytef *>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());

    const int result = deflate(&stream, Z_FINISH);
    const int written = static_cast<int>(output.size() - stream.avail_out);
    deflateEnd(&stream);

    if (result != Z_STREAM_END)
    {
        if (ok != nullptr)
        {
            *ok = false;
        }
        return {};
    }

    output.resize(written);

    if (ok != nullptr)
    {
        *ok = true;
    }

    return output;
}

QByteArray inflateRaw(const QByteArray &data, bool *ok)
{
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.constData()));
    stream.avail_in = static_cast<uInt>(data.size());

    if (inflateInit2(&stream, -15) != Z_OK)
    {
        if (ok != nullptr)
        {
            *ok = false;
        }
        return {};
    }

    QByteArray output;
    QByteArray chunk(64 * 1024, Qt::Uninitialized);
    int result = Z_OK;

    do
    {
        stream.next_out = reinterpret_cast<Bytef *>(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());
        result = inflate(&stream, Z_NO_FLUSH);

        if (result != Z_OK && result != Z_STREAM_END)
        {
            inflateEnd(&stream);

            if (ok != nullptr)
            {
                *ok = false;
            }
            return {};
        }

        output.append(chunk.constData(), static_cast<int>(chunk.size() - stream.avail_out));
    } while (result != Z_STREAM_END);

    inflateEnd(&stream);

    if (ok != nullptr)
    {
        *ok = true;
    }

    return output;
}

static QByteArray runZlib(const QByteArray &data, int windowBits, bool compress, bool *ok)
{
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.constData()));
    stream.avail_in = static_cast<uInt>(data.size());

    if ((compress ? deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits, 8,
                                 Z_DEFAULT_STRATEGY)
                  : inflateInit2(&stream, windowBits)) != Z_OK)
    {
        if (ok != nullptr)
        {
            *ok = false;
        }
        return {};
    }

    QByteArray output;

    if (compress)
    {
        output.resize(static_cast<int>(deflateBound(&stream, static_cast<uLong>(data.size()))));
        stream.next_out = reinterpret_cast<Bytef *>(output.data());
        stream.avail_out = static_cast<uInt>(output.size());
        const int result = deflate(&stream, Z_FINISH);
        const int written = static_cast<int>(output.size() - stream.avail_out);
        deflateEnd(&stream);

        if (result != Z_STREAM_END)
        {
            if (ok != nullptr)
            {
                *ok = false;
            }
            return {};
        }

        output.resize(written);
    }
    else
    {
        QByteArray chunk(64 * 1024, Qt::Uninitialized);
        int result = Z_OK;

        do
        {
            stream.next_out = reinterpret_cast<Bytef *>(chunk.data());
            stream.avail_out = static_cast<uInt>(chunk.size());
            result = inflate(&stream, Z_NO_FLUSH);

            if (result != Z_OK && result != Z_STREAM_END)
            {
                inflateEnd(&stream);

                if (ok != nullptr)
                {
                    *ok = false;
                }
                return {};
            }

            output.append(chunk.constData(), static_cast<int>(chunk.size() - stream.avail_out));
        } while (result != Z_STREAM_END);

        inflateEnd(&stream);
    }

    if (ok != nullptr)
    {
        *ok = true;
    }

    return output;
}

QByteArray zlibCompress(const QByteArray &data, bool *ok)
{
    return runZlib(data, 15, true, ok);
}

QByteArray zlibDecompress(const QByteArray &data, bool *ok)
{
    return runZlib(data, 15, false, ok);
}

QString percentEncode(const QString &text)
{
    static const char hexDigits[] = "0123456789ABCDEF";
    const QByteArray utf8 = text.toUtf8();
    QByteArray output;
    output.reserve(utf8.size());

    for (const char ch : utf8)
    {
        const unsigned char byte = static_cast<unsigned char>(ch);
        const bool unreserved =
            (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')
            || (byte >= '0' && byte <= '9') || byte == '-' || byte == '_'
            || byte == '.' || byte == '!' || byte == '~' || byte == '*'
            || byte == '\'' || byte == '(' || byte == ')';

        if (unreserved)
        {
            output.append(ch);
        }
        else
        {
            output.append('%');
            output.append(hexDigits[byte >> 4]);
            output.append(hexDigits[byte & 0x0F]);
        }
    }

    return QString::fromLatin1(output);
}

namespace {

int hexDigit(char value)
{
    if (value >= '0' && value <= '9')
    {
        return value - '0';
    }

    if (value >= 'a' && value <= 'f')
    {
        return value - 'a' + 10;
    }

    if (value >= 'A' && value <= 'F')
    {
        return value - 'A' + 10;
    }

    return -1;
}

} // namespace

QString percentDecode(const QString &text)
{
    QByteArray input = text.toLatin1();
    QByteArray output;
    output.reserve(input.size());

    for (int i = 0; i < input.size(); ++i)
    {
        if (input.at(i) == '%' && i + 2 < input.size())
        {
            const int high = hexDigit(input.at(i + 1));
            const int low = hexDigit(input.at(i + 2));

            if (high >= 0 && low >= 0)
            {
                output.append(static_cast<char>((high << 4) | low));
                i += 2;
                continue;
            }
        }

        output.append(input.at(i));
    }

    return QString::fromUtf8(output);
}

QString compressDiagram(const QString &xml)
{
    bool ok = false;
    const QByteArray compressed = deflateRaw(percentEncode(xml).toUtf8(), &ok);

    if (!ok)
    {
        return QString();
    }

    return QString::fromLatin1(compressed.toBase64());
}

QString decompressDiagram(const QString &encoded, bool *ok)
{
    const QByteArray compressed =
        QByteArray::fromBase64(encoded.trimmed().toLatin1());
    bool inflatedOk = false;
    const QByteArray raw = inflateRaw(compressed, &inflatedOk);

    if (!inflatedOk)
    {
        if (ok != nullptr)
        {
            *ok = false;
        }
        return {};
    }

    if (ok != nullptr)
    {
        *ok = true;
    }

    return percentDecode(QString::fromUtf8(raw));
}

} // namespace Compression
} // namespace dq
