#include "core/Compression.h"
#include "core/Document.h"
#include "core/MxFileIO.h"
#include "core/Page.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace dq;

class TestMxFile : public QObject
{
    Q_OBJECT

private slots:
    void uncompressedRoundTrip()
    {
        Document document;
        Page *page = document.currentPage();
        page->name = QStringLiteral("流程");
        page->settings.gridSize = 20;
        page->settings.pageWidth = 1000;
        page->settings.pageHeight = 800;

        Cell *a = page->addVertex(QRectF(10, 20, 120, 60),
                                  Style::parse(QStringLiteral("rounded=1;fillColor=#dae8fc;")),
                                  QStringLiteral("开始"));
        Cell *b = page->addVertex(QRectF(300, 200, 120, 60), {}, QStringLiteral("结束"));
        Cell *edge = page->addEdge(a->id, b->id,
                                   Style::parse(QStringLiteral("edgeStyle=orthogonalEdgeStyle;")),
                                   QStringLiteral("标签"));
        edge->waypoints = {QPointF(200, 90)};

        const QString xml = MxFileIO::documentToXml(document, false);
        QVERIFY(xml.contains(QStringLiteral("<mxGraphModel")));
        QVERIFY(xml.contains(QStringLiteral("value=\"开始\"")));
        QVERIFY(xml.contains(QStringLiteral("as=\"geometry\"")));
        QVERIFY(!xml.contains(QStringLiteral("</mxGeometry> as=")));
        QVERIFY(xml.contains(QStringLiteral("<Array as=\"points\">")));

        Document loaded;
        QString error;
        QVERIFY2(MxFileIO::documentFromXml(loaded, xml, &error), qPrintable(error));
        QCOMPARE(loaded.pageCount(), 1);
        QCOMPARE(loaded.currentPage()->name, QStringLiteral("流程"));
        QCOMPARE(loaded.currentPage()->settings.gridSize, 20);
        QCOMPARE(loaded.currentPage()->settings.pageWidth, 1000.0);

        Page *loadedPage = loaded.currentPage();
        QCOMPARE(loadedPage->cells().size(), 3);
        QVERIFY(loadedPage->cell(a->id) != nullptr);
        QCOMPARE(loadedPage->cell(a->id)->value, QStringLiteral("开始"));
        QCOMPARE(loadedPage->cell(a->id)->geometry, QRectF(10, 20, 120, 60));
        QVERIFY(loadedPage->cell(edge->id)->waypoints == QVector<QPointF>({QPointF(200, 90)}));
    }

    void layersAreWrittenWithTheirCells()
    {
        Document document;
        Page *page = document.currentPage();
        page->layers = {Layer{QStringLiteral("L1"), QStringLiteral("图层1"), true, false},
                        Layer{QStringLiteral("L2"), QStringLiteral("图层2"), true, false}};
        page->activeLayerId = QStringLiteral("L1");
        page->addVertex(QRectF(10, 10, 50, 30), {}, QStringLiteral("A"));
        page->activeLayerId = QStringLiteral("L2");
        page->addVertex(QRectF(100, 10, 50, 30), {}, QStringLiteral("B"));

        const QString xml = MxFileIO::documentToXml(document, false);
        const int layer2 = xml.indexOf(QStringLiteral("id=\"L2\""));
        const int cellA = xml.indexOf(QStringLiteral("value=\"A\""));
        QVERIFY(layer2 > 0 && cellA > 0);
        QVERIFY(!xml.contains(QStringLiteral("</mxCell><mxCell id=\"L2\" vertex")));
    }

    void savedDrawioIsUncompressed()
    {
        Document document;
        Page *page = document.currentPage();
        page->addVertex(QRectF(10, 10, 50, 30), {}, QStringLiteral("A"));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("plain.drawio"));
        QVERIFY(document.save(path));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString xml = QString::fromUtf8(file.readAll());
        QVERIFY(xml.contains(QStringLiteral("<mxGraphModel")));
        QVERIFY(xml.contains(QStringLiteral("value=\"A\"")));
        QVERIFY(xml.contains(QStringLiteral("\n")));
        QVERIFY(!xml.contains(QStringLiteral("agent=\"drawio-qt")));
        QVERIFY(!xml.contains(QStringLiteral("version=\"32.6.10")));
    }

    void compressedRoundTrip()
    {
        Document document;
        Page *page = document.currentPage();
        page->addVertex(QRectF(0, 0, 100, 40), {}, QStringLiteral("压缩页面"));

        const QString xml = MxFileIO::documentToXml(document, true);
        Document loaded;
        QString error;
        QVERIFY2(MxFileIO::documentFromXml(loaded, xml, &error), qPrintable(error));
        QCOMPARE(loaded.pageCount(), 1);
        QCOMPARE(loaded.currentPage()->vertices().size(), 1);
        QCOMPARE(loaded.currentPage()->vertices().first()->value, QStringLiteral("压缩页面"));
    }

    void pageXmlRoundTrip()
    {
        Page page(QStringLiteral("页面"));
        page.settings.page = false;
        page.settings.grid = false;
        Cell *cell = page.addVertex(QRectF(5, 6, 70, 30),
                                    Style::parse(QStringLiteral("fillColor=#ffffff;")),
                                    QStringLiteral("文本"));
        Q_UNUSED(cell);

        const QString xml = MxFileIO::pageToXml(page, false);
        QVERIFY(xml.startsWith(QStringLiteral("<mxGraphModel")));

        Page restored;
        QString error;
        QVERIFY2(MxFileIO::pageFromXml(restored, xml, &error), qPrintable(error));
        QCOMPARE(restored.settings.page, false);
        QCOMPARE(restored.settings.grid, false);
        QCOMPARE(restored.vertices().size(), 1);
        QCOMPARE(restored.vertices().first()->geometry, QRectF(5, 6, 70, 30));
    }

    void compressedPageXml()
    {
        Page page;
        page.addVertex(QRectF(0, 0, 40, 40), {}, QStringLiteral("A"));

        const QString compressed = MxFileIO::pageToXml(page, true);
        QVERIFY(!compressed.contains(QLatin1Char('<')));

        bool ok = false;
        const QString decompressed = Compression::decompressDiagram(compressed, &ok);
        QVERIFY(ok);
        QVERIFY(decompressed.startsWith(QStringLiteral("<mxGraphModel")));
    }

    void percentEncoding()
    {
        const QString original = QStringLiteral("中文 <xml> %20 & = ; / ?");
        const QString encoded = Compression::percentEncode(original);
        QVERIFY(encoded.contains(QStringLiteral("%E4%B8%AD")));
        QCOMPARE(Compression::percentDecode(encoded), original);
    }

    void emptyFileRejected()
    {
        Document document;
        QString error;
        QVERIFY(!MxFileIO::documentFromXml(document, QString(), &error));
        QVERIFY(!error.isEmpty());
    }

    void layerRoundTrip()
    {
        Document document;
        Page *page = document.currentPage();
        Layer second;
        second.id = QStringLiteral("layer2");
        second.name = QStringLiteral("第二层");
        page->layers.append(second);
        page->activeLayerId = second.id;
        page->addVertex(QRectF(0, 0, 50, 50), {}, QStringLiteral("层级"));

        const QString xml = MxFileIO::documentToXml(document, false);
        Document loaded;
        QString error;
        QVERIFY2(MxFileIO::documentFromXml(loaded, xml, &error), qPrintable(error));
        QCOMPARE(loaded.currentPage()->layers.size(), 2);
        QCOMPARE(loaded.currentPage()->layers.at(1).name, QStringLiteral("第二层"));
        QCOMPARE(loaded.currentPage()->vertices().first()->parentId, QStringLiteral("layer2"));
    }
};

QTEST_MAIN(TestMxFile)
#include "tst_mxfile.moc"
