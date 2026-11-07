#include "core/Compression.h"
#include "core/FileMeta.h"

#include <QBuffer>
#include <QImage>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QtTest>

using namespace dq;

class TestFileMeta : public QObject
{
    Q_OBJECT

private slots:
    void pngEmbedAndRead()
    {
        QImage image(32, 24, QImage::Format_ARGB32);
        image.fill(Qt::white);

        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        buffer.close();

        const QString xml = QStringLiteral("<mxGraphModel><root><mxCell id=\"0\"/></root></mxGraphModel>");
        const QByteArray withXml = FileMeta::embedXmlInPng(png, xml);

        QVERIFY(withXml.startsWith("\x89PNG"));
        QVERIFY(withXml.size() > png.size());

        bool ok = false;
        const QString extracted = FileMeta::xmlFromPng(withXml, &ok);
        QVERIFY(ok);
        QCOMPARE(extracted, xml);
        QVERIFY(FileMeta::detect(withXml) == FileMeta::Container::Png);
    }

    void pngDpiChunk()
    {
        QImage image(8, 8, QImage::Format_ARGB32);
        image.fill(Qt::black);
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        buffer.close();

        const QByteArray withDpi = FileMeta::embedPngDpi(png, 300);
        QVERIFY(withDpi.contains("pHYs"));
    }

    void svgEmbedAndRead()
    {
        const QByteArray svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"10\" height=\"10\"></svg>";
        const QString xml = QStringLiteral("<mxGraphModel><root/></mxGraphModel>");
        const QByteArray embedded = FileMeta::embedXmlInSvg(svg, xml);

        QVERIFY(embedded.contains("content="));

        bool ok = false;
        const QString extracted = FileMeta::xmlFromSvg(embedded, &ok);
        QVERIFY(ok);
        QCOMPARE(extracted, xml);
    }

    void pdfEmbedAndRead()
    {
        QByteArray pdf;
        QBuffer buffer(&pdf);
        buffer.open(QIODevice::WriteOnly);

        QPdfWriter writer(&buffer);
        writer.setResolution(96);
        writer.setPageSize(QPageSize(QSizeF(100, 100), QPageSize::Millimeter));
        writer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
        writer.setTitle(QStringLiteral("test"));

        QPainter painter;
        QVERIFY(painter.begin(&writer));
        painter.drawRect(10, 10, 50, 50);
        painter.end();
        buffer.close();

        QVERIFY(pdf.startsWith("%PDF"));

        const QString xml = QStringLiteral("<mxGraphModel><root><mxCell id=\"0\"/></root></mxGraphModel>");
        const QByteArray embedded = FileMeta::embedXmlInPdf(pdf, xml);
        QVERIFY(embedded.startsWith("%PDF"));
        QVERIFY(embedded.contains("/Subject"));

        bool ok = false;
        const QString extracted = FileMeta::xmlFromPdf(embedded, &ok);
        QVERIFY2(ok, "PDF subject extraction failed");
        QCOMPARE(extracted, xml);

        // The original drawing must still be readable after the incremental update.
        QVERIFY(embedded.contains("startxref"));
        QVERIFY(embedded.contains("%%EOF"));
    }

    void detectBySignature()
    {
        QCOMPARE(FileMeta::detect(QByteArray("\x89PNG\r\n\x1a\n")), FileMeta::Container::Png);
        QCOMPARE(FileMeta::detect(QByteArray("%PDF-1.4")), FileMeta::Container::Pdf);
        QCOMPARE(FileMeta::detect(QByteArray("<svg xmlns=\"http://www.w3.org/2000/svg\"/>")),
                 FileMeta::Container::Svg);
        QCOMPARE(FileMeta::detect(QByteArray("hello")), FileMeta::Container::None);
    }

    void missingXmlIsReported()
    {
        QImage image(4, 4, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        buffer.close();

        bool ok = true;
        const QString xml = FileMeta::xmlFromPng(png, &ok);
        QVERIFY(!ok);
        QVERIFY(xml.isEmpty());
    }
};

QTEST_MAIN(TestFileMeta)
#include "tst_filemeta.moc"
