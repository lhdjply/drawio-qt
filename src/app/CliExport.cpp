#include "app/CliExport.h"

#include "core/Document.h"
#include "io/Exporter.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace dq {
namespace CliExport {
namespace {

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err()
{
    static QTextStream stream(stderr);
    return stream;
}

QStringList exportableExtensions()
{
    return {QStringLiteral("drawio"), QStringLiteral("dio"), QStringLiteral("xml"),
            QStringLiteral("png"), QStringLiteral("svg"), QStringLiteral("pdf")};
}

bool isExportable(const QFileInfo &info)
{
    return exportableExtensions().contains(info.suffix().toLower());
}

QVector<QString> collectFiles(const QStringList &inputs, bool recursive, QString *error)
{
    QVector<QString> files;

    for (const QString &input : inputs)
    {
        const QFileInfo info(input);

        if (!info.exists())
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("输入文件/文件夹不存在：%1").arg(input);
            }
            return {};
        }

        if (info.isFile())
        {
            files.append(info.absoluteFilePath());
            continue;
        }

        if (!info.isDir())
        {
            continue;
        }

        QDirIterator::IteratorFlags flags = QDirIterator::NoIteratorFlags;

        if (recursive)
        {
            flags |= QDirIterator::Subdirectories;
        }

        QDirIterator iterator(info.absoluteFilePath(),
                              QDir::Files | QDir::NoSymLinks, flags);

        while (iterator.hasNext())
        {
            const QString path = iterator.next();
            const QFileInfo candidate(path);

            if (candidate.isSymLink())
            {
                out() << QStringLiteral("Skipping %1 (symbolic link)\n").arg(path);
                continue;
            }

            if (isExportable(candidate))
            {
                files.append(path);
            }
        }
    }

    return files;
}

QString uniqueOutputPath(const QString &path)
{
    if (!QFileInfo::exists(path))
    {
        return path;
    }

    const QFileInfo info(path);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix();
    const QString dir = info.absolutePath();

    for (int counter = 1;; ++counter)
    {
        const QString candidate = QDir(dir).filePath(
            QStringLiteral("%1-%2.%3").arg(base).arg(counter).arg(suffix));

        if (!QFileInfo::exists(candidate))
        {
            return candidate;
        }
    }
}

ExportOptions toExportOptions(const CliOptions &cli, const QString &format)
{
    ExportOptions options;
    options.format = format;
    options.scale = cli.scale > 0 ? cli.scale : 1.0;
    options.border = cli.border > 0 ? cli.border : 0;
    options.transparent = cli.transparent;
    options.embedXml = cli.embedDiagram;
    options.uncompressed = cli.uncompressed;
    options.allPages = cli.allPages;
    options.pageView = cli.size == QLatin1String("page");
    options.quality = cli.quality > 0 ? cli.quality : 90;
    options.width = cli.width;
    options.height = cli.height;
    options.theme = cli.theme.isEmpty() ? cli.svgTheme : cli.theme;
    options.htmlTheme = cli.htmlTheme;
    options.htmlZoom = cli.htmlZoom;
    options.htmlLightbox = cli.htmlLightbox;
    options.htmlLayers = cli.htmlLayers;
    options.htmlTags = cli.htmlTags;
    options.htmlFit = cli.htmlFit;
    options.htmlLinkTarget = cli.htmlLinkTarget;
    options.htmlLinkColor = cli.htmlLinkColor;
    options.htmlEditLink = cli.htmlEditLink;

    return options;
}

bool isUnsupportedInput(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QLatin1String("vsdx") || suffix == QLatin1String("csv")
        || suffix == QLatin1String("mmd") || suffix == QLatin1String("mermaid");
}

} // namespace

int run(const CliOptions &cli)
{
    // --- resolve output format ------------------------------------------
    QString format = cli.format;
    bool outputIsFile = false;
    bool outputIsDir = false;

    if (!cli.output.isEmpty())
    {
        const QFileInfo outputInfo(cli.output);

        if (outputInfo.exists())
        {
            if (outputInfo.isDir())
            {
                outputIsDir = true;
            }
            else
            {
                err() << QStringLiteral("Error: Output file already exists: %1\n").arg(cli.output);
                return 1;
            }
        }
        else if (outputInfo.suffix().isEmpty())
        {
            // A path without an extension is treated as an output folder, the
            // behaviour users expect from `-o out/` in batch exports.
            outputIsDir = true;
            QDir().mkpath(cli.output);
        }
        else
        {
            outputIsFile = true;
            const QString extension = outputInfo.suffix().toLower();

            if (Args::supportedFormats().contains(extension))
            {
                format = Args::normalizedFormat(extension);
            }
        }
    }

    format = Args::normalizedFormat(format);

    if (!Args::supportedFormats().contains(format))
    {
        err() << QStringLiteral("Error: Unsupported format: %1\n").arg(format);
        return 1;
    }

    if (cli.inputs.isEmpty())
    {
        err() << QStringLiteral("Error: An input file must be specified\n");
        return 1;
    }

    // --- page selection --------------------------------------------------
    int from = -1;
    int to = -1;

    if (cli.hasPageIndex && cli.pageIndex < 0)
    {
        err() << QStringLiteral("Error: Invalid page index: pages are numbered from 1 "
                                "(0-based before v27.0.2)\n");
        return 1;
    }

    if (cli.hasPageIndex && cli.pageIndex >= 0)
    {
        from = cli.pageIndex;
        to = cli.pageIndex;
    }
    else if (cli.hasPageRange)
    {
        if (cli.pageRange.size() == 2 && cli.pageRange.at(0) >= 0
            && cli.pageRange.at(1) >= cli.pageRange.at(0))
        {
            from = cli.pageRange.at(0);
            to = cli.pageRange.at(1);
        }
        else
        {
            err() << QStringLiteral("Error: Invalid page range, expected <from>..<to> "
                                    "with pages numbered from 1 and from <= to\n");
            return 1;
        }
    }

    // --- input files ------------------------------------------------------
    QString collectError;
    const QVector<QString> files = collectFiles(cli.inputs, cli.recursive, &collectError);

    if (!collectError.isEmpty())
    {
        err() << QStringLiteral("Error: %1\n").arg(collectError);
        return 1;
    }

    if (files.isEmpty())
    {
        err() << QStringLiteral("Error: no exportable files found in the input file/directory\n");
        return 1;
    }

    if (files.size() > 1 && outputIsFile && !cli.check)
    {
        err() << QStringLiteral("Error: output must be a folder when exporting multiple files\n");
        return 1;
    }

    bool failed = false;

    for (const QString &file : files)
    {
        if (isUnsupportedInput(file))
        {
            err() << QStringLiteral("Error: 原生渲染器暂不支持导入 %1：%2\n")
                         .arg(QFileInfo(file).suffix(), file);
            failed = true;
            continue;
        }

        Document document;
        QString loadError;

        if (!document.load(file, &loadError))
        {
            err() << QStringLiteral("Error: %1: %2\n").arg(loadError, file);
            failed = true;
            continue;
        }

        const int pageIndex = from >= 0 ? qBound(0, from, document.pageCount() - 1) : 0;
        document.setCurrentIndex(pageIndex);

        ExportOptions options = toExportOptions(cli, format);
        options.fromPage = from;
        options.toPage = to;

        const ExportResult result = Exporter::exportDocument(document, options);

        if (!result.ok)
        {
            err() << QStringLiteral("Error: %1: %2\n").arg(result.error, file);
            failed = true;
            continue;
        }

        QString outputPath;

        if (outputIsDir)
        {
            outputPath = QDir(cli.output).filePath(
                QFileInfo(file).completeBaseName() + QLatin1Char('.') + format);
        }
        else if (outputIsFile)
        {
            outputPath = cli.output;
        }
        else
        {
            outputPath = QFileInfo(file).absolutePath() + QLatin1Char('/')
                + QFileInfo(file).completeBaseName() + QLatin1Char('.') + format;
        }

        if (cli.check)
        {
            outputPath = uniqueOutputPath(outputPath);
        }

        QFile output(outputPath);

        if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            err() << QStringLiteral("Error: cannot write %1: %2\n")
                         .arg(outputPath, output.errorString());
            failed = true;
            continue;
        }

        const qint64 written = output.write(result.data);
        output.close();

        if (written != result.data.size())
        {
            err() << QStringLiteral("Error: writing to file failed: %1\n").arg(outputPath);
            failed = true;
            continue;
        }

        out() << file << QStringLiteral(" -> ") << outputPath << QLatin1Char('\n');
        out().flush();
    }

    return failed ? 1 : 0;
}

} // namespace CliExport
} // namespace dq
