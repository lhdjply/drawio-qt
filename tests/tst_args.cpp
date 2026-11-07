#include "app/Args.h"

#include <QtTest>

using namespace dq;

class TestArgs : public QObject
{
    Q_OBJECT

private slots:
    void defaults()
    {
        const ParseResult result = Args::parse({QStringLiteral("drawio-qt")});

        QCOMPARE(result.options.format, QStringLiteral("pdf"));
        QCOMPARE(result.options.size, QStringLiteral("diagram"));
        QCOMPARE(result.options.pageIndex, -1);
        QVERIFY(result.options.htmlZoom);
        QVERIFY(result.options.htmlFit);
        QVERIFY(result.options.embedSvgFonts);
        QVERIFY(!result.options.exportMode);
        QVERIFY(result.options.inputs.isEmpty());
    }

    void flagsAndValues()
    {
        const ParseResult result = Args::parse({QStringLiteral("drawio-qt"),
                                                QStringLiteral("-x"),
                                                QStringLiteral("-f"),
                                                QStringLiteral("png"),
                                                QStringLiteral("-o"),
                                                QStringLiteral("out.png"),
                                                QStringLiteral("-s"),
                                                QStringLiteral("2.5"),
                                                QStringLiteral("-b"),
                                                QStringLiteral("12"),
                                                QStringLiteral("-a"),
                                                QStringLiteral("-e"),
                                                QStringLiteral("-t"),
                                                QStringLiteral("input.drawio")});

        QVERIFY(result.options.exportMode);
        QCOMPARE(result.options.format, QStringLiteral("png"));
        QCOMPARE(result.options.output, QStringLiteral("out.png"));
        QCOMPARE(result.options.scale, 2.5);
        QCOMPARE(result.options.border, 12);
        QVERIFY(result.options.allPages);
        QVERIFY(result.options.embedDiagram);
        QVERIFY(result.options.transparent);
        QCOMPARE(result.options.inputs, QStringList{QStringLiteral("input.drawio")});
    }

    void combinedShortOptions()
    {
        const ParseResult result = Args::parse({QStringLiteral("drawio-qt"),
                                                QStringLiteral("-xe"),
                                                QStringLiteral("-fpng"),
                                                QStringLiteral("a.drawio")});

        QVERIFY(result.options.exportMode);
        QVERIFY(result.options.embedDiagram);
        QCOMPARE(result.options.format, QStringLiteral("png"));
        QCOMPARE(result.options.inputs, QStringList{QStringLiteral("a.drawio")});
    }

    void inlineLongValue()
    {
        const ParseResult result = Args::parse({QStringLiteral("drawio-qt"),
                                                QStringLiteral("--format=svg"),
                                                QStringLiteral("--width=800"),
                                                QStringLiteral("--html-theme=dark")});

        QCOMPARE(result.options.format, QStringLiteral("svg"));
        QCOMPARE(result.options.width, 800);
        QCOMPARE(result.options.htmlTheme, QStringLiteral("dark"));
    }

    void pageIndexIsZeroBased()
    {
        const ParseResult result = Args::parse({QStringLiteral("drawio-qt"),
                                                QStringLiteral("-p"),
                                                QStringLiteral("3")});
        QCOMPARE(result.options.pageIndex, 2);
    }

    void pageIndexZeroIsTrackedAsGiven()
    {
        const ParseResult result = Args::parse({QStringLiteral("drawio-qt"),
                                                QStringLiteral("-p"),
                                                QStringLiteral("0")});
        QVERIFY(result.options.hasPageIndex);
        QCOMPARE(result.options.pageIndex, -1);
    }

    void pageRange()
    {
        const ParseResult result = Args::parse({QStringLiteral("drawio-qt"),
                                                QStringLiteral("-g"),
                                                QStringLiteral("2..5")});
        QVERIFY(result.options.hasPageRange);
        QCOMPARE(result.options.pageRange, QVector<int>({1, 4}));
    }

    void invalidFormatFallsBackToDefault()
    {
        const ParseResult result = Args::parse({QStringLiteral("drawio-qt"),
                                                QStringLiteral("--format=bmp")});
        QCOMPARE(result.options.format, QStringLiteral("pdf"));
    }

    void helpAndVersion()
    {
        QVERIFY(Args::parse({QStringLiteral("drawio-qt"), QStringLiteral("--help")}).help);
        QVERIFY(Args::parse({QStringLiteral("drawio-qt"), QStringLiteral("-h")}).help);
        QVERIFY(Args::parse({QStringLiteral("drawio-qt"), QStringLiteral("--version")}).version);
        QVERIFY(Args::parse({QStringLiteral("drawio-qt"), QStringLiteral("-V")}).version);
    }

    void unknownOptionIsIgnored()
    {
        const ParseResult result = Args::parse({QStringLiteral("drawio-qt"),
                                                QStringLiteral("--unknown"),
                                                QStringLiteral("file.drawio")});
        QCOMPARE(result.options.inputs, QStringList{QStringLiteral("file.drawio")});
    }

    void doubleDashStopsParsing()
    {
        const ParseResult result = Args::parse({QStringLiteral("drawio-qt"),
                                                QStringLiteral("--"),
                                                QStringLiteral("-x"),
                                                QStringLiteral("--format"),
                                                QStringLiteral("png")});
        QCOMPARE(result.options.inputs,
                 QStringList({QStringLiteral("png")}));
        QVERIFY(!result.options.exportMode);
    }

    void helpTextListsOptions()
    {
        const QString text = Args::helpText(QStringLiteral("1.0.0"));
        QVERIFY(text.contains(QStringLiteral("--export")));
        QVERIFY(text.contains(QStringLiteral("--page-index")));
        QVERIFY(text.contains(QStringLiteral("--html-theme")));
        QVERIFY(text.contains(QStringLiteral("drawio-qt 1.0.0")));
    }
};

QTEST_MAIN(TestArgs)
#include "tst_args.moc"
