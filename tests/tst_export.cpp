#include "core/Document.h"
#include "core/FileMeta.h"
#include "core/MxFileIO.h"
#include "io/Exporter.h"
#include "io/HtmlExport.h"
#include "render/ShapeCatalog.h"
#include "render/ShapeRegistry.h"

#include <QFile>
#include <QFileInfo>
#include <QtTest>

using namespace dq;

namespace {

Document *makeDocument()
{
    auto *document = new Document();
    Page *page = document->currentPage();
    page->settings.page = true;
    page->settings.pageWidth = 400;
    page->settings.pageHeight = 300;

    Cell *a = page->addVertex(QRectF(40, 40, 120, 60),
                              Style::parse(QStringLiteral(
                                  "rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;"
                                  "strokeColor=#6c8ebf;")),
                              QStringLiteral("开始"));
    Cell *b = page->addVertex(QRectF(240, 160, 120, 60),
                              Style::parse(QStringLiteral(
                                  "rounded=0;whiteSpace=wrap;html=1;fillColor=#d5e8d4;"
                                  "strokeColor=#82b366;")),
                              QStringLiteral("结束"));
    page->addEdge(a->id, b->id,
                  Style::parse(QStringLiteral("edgeStyle=orthogonalEdgeStyle;endArrow=classic;")),
                  QStringLiteral("下一步"));

    return document;
}

ExportResult exportWith(Document *document, const QString &format, bool embed = false)
{
    ExportOptions options;
    options.format = format;
    options.embedXml = embed;
    options.border = 8;
    options.allPages = format == QLatin1String("pdf");
    return Exporter::exportDocument(*document, options);
}

} // namespace

class TestExport : public QObject
{
    Q_OBJECT

private slots:
    void pngExport()
    {
        QScopedPointer<Document> document(makeDocument());
        const ExportResult result = exportWith(document.data(), QStringLiteral("png"));

        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(result.data.startsWith("\x89PNG"));
        QVERIFY(result.imageSize.width() > 300);
        QVERIFY(result.imageSize.height() > 150);
    }

    void pngEmbeddedXmlRoundTrip()
    {
        QScopedPointer<Document> document(makeDocument());
        const ExportResult result = exportWith(document.data(), QStringLiteral("png"), true);
        QVERIFY(result.ok);

        bool ok = false;
        const QString xml = FileMeta::xmlFromPng(result.data, &ok);
        QVERIFY(ok);
        QVERIFY(xml.contains(QStringLiteral("mxGraphModel")));
        QVERIFY(xml.contains(QStringLiteral("开始")));

        Document reopened;
        QString error;
        QVERIFY2(MxFileIO::documentFromXml(reopened, xml, &error), qPrintable(error));
        QCOMPARE(reopened.currentPage()->vertices().size(), 2);
    }

    void svgExport()
    {
        QScopedPointer<Document> document(makeDocument());
        const ExportResult result = exportWith(document.data(), QStringLiteral("svg"), true);

        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(result.data.contains("<svg"));
        QVERIFY(result.data.contains("</svg>"));
        QVERIFY(result.data.contains("content="));

        bool ok = false;
        const QString xml = FileMeta::xmlFromSvg(result.data, &ok);
        QVERIFY(ok);
        QVERIFY(xml.contains(QStringLiteral("mxGraphModel")));
    }

    void pdfExport()
    {
        QScopedPointer<Document> document(makeDocument());
        const ExportResult result = exportWith(document.data(), QStringLiteral("pdf"), true);

        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(result.data.startsWith("%PDF"));
        QVERIFY(result.data.contains("%%EOF"));
        QVERIFY(result.data.contains("/Subject"));

        bool ok = false;
        const QString xml = FileMeta::xmlFromPdf(result.data, &ok);
        QVERIFY2(ok, "embedded PDF diagram missing");
        QVERIFY(xml.contains(QStringLiteral("mxGraphModel")));
    }

    void pdfAllPages()
    {
        QScopedPointer<Document> document(makeDocument());
        document->addPage(QStringLiteral("第二页"));
        document->page(1)->addVertex(QRectF(10, 10, 50, 50));

        ExportOptions options;
        options.format = QStringLiteral("pdf");
        options.allPages = true;

        const ExportResult result = Exporter::exportDocument(*document, options);
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(result.data.startsWith("%PDF"));

        // Two page objects are emitted for a two page document.
        QVERIFY(result.data.count("/Type /Page") >= 2 || result.data.count("/Type/Page") >= 2);
    }

    void xmlExport()
    {
        QScopedPointer<Document> document(makeDocument());
        ExportOptions options;
        options.format = QStringLiteral("xml");
        options.uncompressed = true;
        const ExportResult result = Exporter::exportDocument(*document, options);

        QVERIFY(result.ok);
        QVERIFY(result.data.contains("<mxfile"));
        QVERIFY(result.data.contains("mxGraphModel"));

        Document loaded;
        QString error;
        QVERIFY2(MxFileIO::documentFromXml(loaded, QString::fromUtf8(result.data), &error),
                 qPrintable(error));
    }

    void htmlExport()
    {
        QScopedPointer<Document> document(makeDocument());
        const ExportResult result = exportWith(document.data(), QStringLiteral("html"), false);

        QVERIFY(result.ok);
        QVERIFY(result.data.contains("data-mxgraph"));
        QVERIFY(result.data.contains("viewer-static.min.js"));
        QVERIFY(result.data.contains("<!DOCTYPE html>"));
    }

    void openingContainerSuggestsDrawioSave()
    {
        QScopedPointer<Document> document(makeDocument());
        const ExportResult result = exportWith(document.data(), QStringLiteral("png"), true);
        QVERIFY(result.ok);

        const QString path = QStringLiteral(DRAWIO_QT_BINARY_DIR)
            + QStringLiteral("/container-input.png");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(result.data);
        file.close();

        Document reopened;
        QString error;
        QVERIFY2(reopened.load(path, &error), qPrintable(error));
        QCOMPARE(reopened.pageCount(), 1);
        QCOMPARE(reopened.currentPage()->vertices().size(), 2);
        QVERIFY(reopened.filePath().isEmpty());
        QVERIFY(reopened.suggestedSavePath().endsWith(QStringLiteral(".drawio")));
    }

    void shapeGalleryRenders()
    {
        // One gallery per built-in category, so shapes can be inspected (and
        // regressions spotted) without opening the editor.
        const QVector<ShapeCategory> categories = ShapeCatalog::instance().builtinCategories();
        QVERIFY(!categories.isEmpty());

        int total = 0;

        for (const ShapeCategory &category : categories)
        {
            Document document;
            Page *page = document.currentPage();
            page->settings.page = false;
            page->settings.grid = false;

            int index = 0;

            for (const ShapeDefinition &shape : category.shapes)
            {
                const int column = index % 5;
                const int row = index / 5;
                const QPointF topLeft(40 + column * 230, 40 + row * 160);
                page->addVertex(QRectF(topLeft, shape.size), Style::parse(shape.style),
                                shape.name);
                ++index;
            }

            ExportOptions options;
            options.format = QStringLiteral("png");
            options.border = 20;
            options.scale = 1.0;

            const ExportResult result = Exporter::exportDocument(document, options);
            QVERIFY2(result.ok, qPrintable(result.error));

            const QString path = QStringLiteral(DRAWIO_QT_BINARY_DIR)
                + QStringLiteral("/gallery-%1.png").arg(category.id);
            QFile file(path);
            QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
            file.write(result.data);
            file.close();
            total += category.shapes.size();
        }

        QVERIFY(total >= 150);
    }


    void bundledImageLibraryRenders()
    {
        const ShapeDefinition definition = ShapeCatalog::instance()
            .definition(QStringLiteral("img/lib/azure2/compute/Azure_Linux.svg"));
        QVERIFY(!definition.key.isEmpty());
        QVERIFY(!definition.imagePath.isEmpty());

        Document document;
        Page *page = document.currentPage();
        page->addVertex(QRectF(40, 40, 120, 120), Style::parse(definition.style), QString());

        ExportOptions options;
        options.format = QStringLiteral("png");
        options.border = 12;
        const ExportResult result = Exporter::exportDocument(document, options);
        QVERIFY2(result.ok, qPrintable(result.error));

        const QString path = QStringLiteral(DRAWIO_QT_BINARY_DIR)
            + QStringLiteral("/image-shape.png");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(result.data);
        file.close();

        QImage image;
        QVERIFY(image.loadFromData(result.data, "PNG"));
        int colored = 0;

        for (int y = 0; y < image.height(); y += 3)
        {
            for (int x = 0; x < image.width(); x += 3)
            {
                const QColor color = image.pixelColor(x, y);

                if (color.alpha() > 10 && color.lightness() < 245)
                {
                    ++colored;
                }
            }
        }

        QVERIFY2(colored > 50, "the bundled SVG did not render into the export");
    }

    void catalogExposesStandardAndImageLibraries()
    {
        const ShapeCatalog &catalog = ShapeCatalog::instance();
        QVERIFY(!catalog.builtinCategories().isEmpty());
        QVERIFY(!catalog.imageCategories().isEmpty());
        QVERIFY(catalog.allDefinitions().size() > 1800);

        QStringList categoryIds;

        for (const ShapeCategory &category : catalog.builtinCategories())
        {
            categoryIds.append(category.id);
        }

        QVERIFY(categoryIds.contains(QStringLiteral("electrical")));
        QVERIFY(categoryIds.contains(QStringLiteral("flowchart")));
        QVERIFY(categoryIds.contains(QStringLiteral("uml")));
        QVERIFY(categoryIds.contains(QStringLiteral("bpmn")));
        QVERIFY(categoryIds.contains(QStringLiteral("network")));
        QVERIFY(categoryIds.contains(QStringLiteral("kubernetes")));

        // Every built-in definition must be resolvable by key.
        for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
        {
            QVERIFY(!catalog.definition(definition.key).key.isEmpty());
        }
    }

    void transparentPngHasAlpha()
    {
        QScopedPointer<Document> document(makeDocument());
        ExportOptions options;
        options.format = QStringLiteral("png");
        options.transparent = true;

        const ExportResult result = Exporter::exportDocument(*document, options);
        QVERIFY(result.ok);

        QImage image;
        QVERIFY(image.loadFromData(result.data, "PNG"));
        QVERIFY(image.hasAlphaChannel());

        // A corner pixel outside all shapes must be fully transparent.
        const QColor corner = image.pixelColor(0, 0);
        QCOMPARE(corner.alpha(), 0);
    }

    void fitToWidthPreservesAspectRatio()
    {
        QScopedPointer<Document> document(makeDocument());
        ExportOptions options;
        options.format = QStringLiteral("png");
        options.width = 800;

        const ExportResult result = Exporter::exportDocument(*document, options);
        QVERIFY(result.ok);
        QCOMPARE(result.imageSize.width(), 800);
        QVERIFY(result.imageSize.height() > 100);
    }

    void unsupportedFormatFails()
    {
        QScopedPointer<Document> document(makeDocument());
        ExportOptions options;
        options.format = QStringLiteral("bmp");
        const ExportResult result = Exporter::exportDocument(*document, options);
        QVERIFY(!result.ok);
        QVERIFY(!result.error.isEmpty());
    }
};

QTEST_MAIN(TestExport)
#include "tst_export.moc"
