#include "app/Args.h"

#include <QHash>
#include <QSet>

#include <cmath>

namespace dq {
namespace Args {
namespace {

struct OptionSpec
{
    QString shortName;
    QString longName;
    QString key;
    QString valueLabel;
    bool takesValue = false;
    bool helpOnly = false;
    QString description;
};

const QVector<OptionSpec> &specs()
{
    static const QVector<OptionSpec> table = {
        {QStringLiteral("-c"), QStringLiteral("--create"), QStringLiteral("create"), {}, false, false,
         QStringLiteral("creates a new empty file if no file is passed")},
        {QStringLiteral("-k"), QStringLiteral("--check"), QStringLiteral("check"), {}, false, false,
         QStringLiteral("does not overwrite existing files")},
        {QStringLiteral("-x"), QStringLiteral("--export"), QStringLiteral("export"), {}, false, false,
         QStringLiteral("export the input files/folders based on the given options")},
        {QStringLiteral("-r"), QStringLiteral("--recursive"), QStringLiteral("recursive"), {}, false, false,
         QStringLiteral("for a folder input, recursively convert all files in sub-folders also")},
        {QString(), QStringLiteral("--timeout"), QStringLiteral("timeout"),
         QStringLiteral("<seconds>"), true, false,
         QStringLiteral("fails the export of a file that takes longer than the given number of seconds (default: 0, no limit)")},
        {QStringLiteral("-o"), QStringLiteral("--output"), QStringLiteral("output"),
         QStringLiteral("<output file/folder>"), true, false,
         QStringLiteral("specify the output file/folder. If omitted, the input file name is used for output")},
        {QStringLiteral("-f"), QStringLiteral("--format"), QStringLiteral("format"),
         QStringLiteral("<format>"), true, false,
         QStringLiteral("output format: pdf, png, jpg, svg, xml or html (default: pdf)")},
        {QString(), QStringLiteral("--layout"), QStringLiteral("layout"), QStringLiteral("<name|json>"),
         true, false,
         QStringLiteral("run a layout on the diagram after opening it (not supported by the native renderer yet)")},
        {QString(), QStringLiteral("--normalize"), QStringLiteral("normalize"), {}, false, false,
         QStringLiteral("repair the model before anything else touches it (not supported yet)")},
        {QString(), QStringLiteral("--mermaid-image"), QStringLiteral("mermaidImage"),
         QStringLiteral("<true/false>"), true, false,
         QStringLiteral("open Mermaid files as a static image (not supported yet)")},
        {QStringLiteral("-q"), QStringLiteral("--quality"), QStringLiteral("quality"),
         QStringLiteral("<quality>"), true, false,
         QStringLiteral("output image quality for JPEG (default: 90)")},
        {QStringLiteral("-t"), QStringLiteral("--transparent"), QStringLiteral("transparent"), {}, false, false,
         QStringLiteral("set transparent background for PNG and SVG")},
        {QStringLiteral("-e"), QStringLiteral("--embed-diagram"), QStringLiteral("embedDiagram"), {}, false, false,
         QStringLiteral("includes a copy of the diagram (for PNG, SVG and PDF formats only)")},
        {QString(), QStringLiteral("--embed-svg-images"), QStringLiteral("embedSvgImages"), {}, false, false,
         QStringLiteral("embed images in SVG file (accepted, images are always embedded)")},
        {QString(), QStringLiteral("--embed-svg-fonts"), QStringLiteral("embedSvgFonts"),
         QStringLiteral("<true/false>"), true, false,
         QStringLiteral("embed fonts in SVG file (default: true)")},
        {QString(), QStringLiteral("--embed-svg-metadata"), QStringLiteral("embedSvgMetadata"), {}, false, false,
         QStringLiteral("embed cell metadata in SVG file")},
        {QStringLiteral("-b"), QStringLiteral("--border"), QStringLiteral("border"), QStringLiteral("<border>"),
         true, false, QStringLiteral("sets the border width around the diagram (default: 0)")},
        {QStringLiteral("-s"), QStringLiteral("--scale"), QStringLiteral("scale"), QStringLiteral("<scale>"),
         true, false, QStringLiteral("scales the diagram size")},
        {QString(), QStringLiteral("--width"), QStringLiteral("width"), QStringLiteral("<width>"), true, false,
         QStringLiteral("fits the generated image/pdf into the specified width, preserves aspect ratio")},
        {QString(), QStringLiteral("--height"), QStringLiteral("height"), QStringLiteral("<height>"), true, false,
         QStringLiteral("fits the generated image/pdf into the specified height, preserves aspect ratio")},
        {QString(), QStringLiteral("--crop"), QStringLiteral("crop"), {}, false, false,
         QStringLiteral("crops PDF to diagram size")},
        {QString(), QStringLiteral("--size"), QStringLiteral("size"), QStringLiteral("<size>"), true, false,
         QStringLiteral("export size mode: diagram [default] or page (png, jpg and svg)")},
        {QStringLiteral("-a"), QStringLiteral("--all-pages"), QStringLiteral("allPages"), {}, false, false,
         QStringLiteral("export all pages (for PDF, HTML and SVG formats)")},
        {QStringLiteral("-p"), QStringLiteral("--page-index"), QStringLiteral("pageIndex"),
         QStringLiteral("<pageIndex>"), true, false,
         QStringLiteral("selects a specific page (1-based)")},
        {QStringLiteral("-l"), QStringLiteral("--layers"), QStringLiteral("layers"),
         QStringLiteral("<comma separated layer indexes>"), true, false,
         QStringLiteral("selects which layers to export (applies to all pages)")},
        {QStringLiteral("-g"), QStringLiteral("--page-range"), QStringLiteral("pageRange"),
         QStringLiteral("<from>..<to>"), true, false,
         QStringLiteral("selects a page range (1-based, for PDF format only)")},
        {QStringLiteral("-u"), QStringLiteral("--uncompressed"), QStringLiteral("uncompressed"), {}, false, false,
         QStringLiteral("uncompressed XML output (for XML and SVG format only)")},
        {QStringLiteral("-z"), QStringLiteral("--zoom"), QStringLiteral("zoom"), QStringLiteral("<zoom>"),
         true, false, QStringLiteral("scales the application interface")},
        {QString(), QStringLiteral("--theme"), QStringLiteral("theme"), QStringLiteral("<theme>"), true, false,
         QStringLiteral("theme of the exported image: dark, light, auto [default]")},
        {QString(), QStringLiteral("--svg-theme"), QStringLiteral("svgTheme"), QStringLiteral("<theme>"),
         true, false, QStringLiteral("deprecated, use --theme")},
        {QString(), QStringLiteral("--svg-links-target"), QStringLiteral("svgLinksTarget"),
         QStringLiteral("<target>"), true, false,
         QStringLiteral("target of links in the exported SVG image (auto, new-win, same-win)")},
        {QString(), QStringLiteral("--html-theme"), QStringLiteral("htmlTheme"), QStringLiteral("<theme>"),
         true, false, QStringLiteral("theme of the HTML viewer (dark, light, auto [default])")},
        {QString(), QStringLiteral("--html-zoom"), QStringLiteral("htmlZoom"), QStringLiteral("<true/false>"),
         true, false, QStringLiteral("show zoom controls in HTML viewer (default: true)")},
        {QString(), QStringLiteral("--html-lightbox"), QStringLiteral("htmlLightbox"),
         QStringLiteral("<true/false>"), true, false,
         QStringLiteral("enable lightbox in HTML viewer (default: true)")},
        {QString(), QStringLiteral("--html-layers"), QStringLiteral("htmlLayers"),
         QStringLiteral("<true/false>"), true, false,
         QStringLiteral("show layers toolbar in HTML viewer (default: true)")},
        {QString(), QStringLiteral("--html-tags"), QStringLiteral("htmlTags"), QStringLiteral("<true/false>"),
         true, false, QStringLiteral("show tags toolbar in HTML viewer (default: true)")},
        {QString(), QStringLiteral("--html-fit"), QStringLiteral("htmlFit"), QStringLiteral("<true/false>"),
         true, false, QStringLiteral("responsive fit to container width in HTML viewer (default: true)")},
        {QString(), QStringLiteral("--html-link-target"), QStringLiteral("htmlLinkTarget"),
         QStringLiteral("<target>"), true, false,
         QStringLiteral("link target in HTML viewer (auto, blank, self)")},
        {QString(), QStringLiteral("--html-link-color"), QStringLiteral("htmlLinkColor"),
         QStringLiteral("<color>"), true, false,
         QStringLiteral("link highlight color in HTML viewer (default: #0000ff)")},
        {QString(), QStringLiteral("--html-edit-link"), QStringLiteral("htmlEditLink"),
         QStringLiteral("<url>"), true, false,
         QStringLiteral("URL for edit button in HTML viewer")},
        {QString(), QStringLiteral("--disable-update"), QStringLiteral("disableUpdate"), {}, false, true,
         QStringLiteral("disable auto-update")},
        {QString(), QStringLiteral("--no-silent-update"), QStringLiteral("noSilentUpdate"), {}, false, true,
         QStringLiteral("prompt before downloading updates instead of updating silently")},
    };

    return table;
}

const OptionSpec *findByShort(const QString &name)
{
    for (const OptionSpec &spec : specs())
    {
        if (!spec.helpOnly && spec.shortName == name)
        {
            return &spec;
        }
    }

    return nullptr;
}

const OptionSpec *findByLong(const QString &name)
{
    for (const OptionSpec &spec : specs())
    {
        if (!spec.helpOnly && spec.longName == name)
        {
            return &spec;
        }
    }

    return nullptr;
}

bool parseBool(const QString &value)
{
    return value == QLatin1String("true") || value == QLatin1String("1");
}

bool validFormat(const QString &value)
{
    static const QStringList formats = {QStringLiteral("pdf"), QStringLiteral("svg"),
                                        QStringLiteral("png"), QStringLiteral("jpeg"),
                                        QStringLiteral("jpg"), QStringLiteral("xml"),
                                        QStringLiteral("html")};
    return formats.contains(value.toLower());
}

void applyValue(CliOptions &options, const OptionSpec &spec, const QString &raw)
{
    const QString key = spec.key;
    const QString lower = raw.toLower();

    if (key == QLatin1String("timeout"))
    {
        bool ok = false;
        const int value = raw.toInt(&ok);

        if (ok && value >= 0)
        {
            options.timeout = value;
        }
    }
    else if (key == QLatin1String("output"))
    {
        options.output = raw;
    }
    else if (key == QLatin1String("format"))
    {
        options.format = validFormat(raw) ? lower : QStringLiteral("pdf");
    }
    else if (key == QLatin1String("layout"))
    {
        options.layout = raw;
    }
    else if (key == QLatin1String("mermaidImage"))
    {
        options.mermaidImage = parseBool(raw);
    }
    else if (key == QLatin1String("quality"))
    {
        bool ok = false;
        const int value = raw.toInt(&ok);

        if (ok)
        {
            options.quality = value;
        }
    }
    else if (key == QLatin1String("embedSvgFonts"))
    {
        options.embedSvgFonts = parseBool(raw);
    }
    else if (key == QLatin1String("border"))
    {
        bool ok = false;
        const int value = raw.toInt(&ok);

        if (ok)
        {
            options.border = value;
        }
    }
    else if (key == QLatin1String("scale"))
    {
        bool ok = false;
        const double value = raw.toDouble(&ok);

        if (ok)
        {
            options.scale = value;
        }
    }
    else if (key == QLatin1String("width") || key == QLatin1String("height"))
    {
        bool ok = false;
        const int value = raw.toInt(&ok);

        if (ok)
        {
            if (key == QLatin1String("width"))
            {
                options.width = value;
            }
            else
            {
                options.height = value;
            }
        }
    }
    else if (key == QLatin1String("size"))
    {
        options.size = (lower == QLatin1String("page")) ? QStringLiteral("page")
                                                        : QStringLiteral("diagram");
    }
    else if (key == QLatin1String("pageIndex"))
    {
        bool ok = false;
        const int value = raw.toInt(&ok);
        options.pageIndex = ok ? value - 1 : -2;
        options.hasPageIndex = true;
    }
    else if (key == QLatin1String("layers"))
    {
        options.layers = raw;
    }
    else if (key == QLatin1String("pageRange"))
    {
        const QStringList parts = raw.split(QStringLiteral(".."));

        if (parts.size() == 2)
        {
            bool okFrom = false;
            bool okTo = false;
            const int from = parts.at(0).toInt(&okFrom) - 1;
            const int to = parts.at(1).toInt(&okTo) - 1;

            if (okFrom && okTo)
            {
                options.pageRange = {from, to};
                options.hasPageRange = true;
            }
        }
    }
    else if (key == QLatin1String("zoom"))
    {
        bool ok = false;
        const double value = raw.toDouble(&ok);

        if (ok)
        {
            options.zoom = value;
        }
    }
    else if (key == QLatin1String("theme"))
    {
        options.theme = (lower == QLatin1String("dark") || lower == QLatin1String("light")
                         || lower == QLatin1String("auto"))
            ? lower : QString();
    }
    else if (key == QLatin1String("svgTheme"))
    {
        options.svgTheme = (lower == QLatin1String("dark") || lower == QLatin1String("light")
                            || lower == QLatin1String("auto"))
            ? lower : QStringLiteral("auto");
    }
    else if (key == QLatin1String("svgLinksTarget"))
    {
        options.svgLinksTarget = (lower == QLatin1String("auto")
                                  || lower == QLatin1String("new-win")
                                  || lower == QLatin1String("same-win"))
            ? lower : QStringLiteral("auto");
    }
    else if (key == QLatin1String("htmlTheme"))
    {
        options.htmlTheme = (lower == QLatin1String("dark") || lower == QLatin1String("light")
                             || lower == QLatin1String("auto"))
            ? lower : QStringLiteral("auto");
    }
    else if (key == QLatin1String("htmlLinkTarget"))
    {
        options.htmlLinkTarget = (lower == QLatin1String("auto")
                                  || lower == QLatin1String("blank")
                                  || lower == QLatin1String("self"))
            ? lower : QStringLiteral("auto");
    }
    else if (key == QLatin1String("htmlLinkColor"))
    {
        options.htmlLinkColor = raw;
    }
    else if (key == QLatin1String("htmlEditLink"))
    {
        options.htmlEditLink = raw;
    }
    else if (key == QLatin1String("htmlZoom"))
    {
        options.htmlZoom = parseBool(raw);
    }
    else if (key == QLatin1String("htmlLightbox"))
    {
        options.htmlLightbox = parseBool(raw);
    }
    else if (key == QLatin1String("htmlLayers"))
    {
        options.htmlLayers = parseBool(raw);
    }
    else if (key == QLatin1String("htmlTags"))
    {
        options.htmlTags = parseBool(raw);
    }
    else if (key == QLatin1String("htmlFit"))
    {
        options.htmlFit = parseBool(raw);
    }
}

void applyFlag(CliOptions &options, const QString &key)
{
    if (key == QLatin1String("create"))
    {
        options.create = true;
    }
    else if (key == QLatin1String("check"))
    {
        options.check = true;
    }
    else if (key == QLatin1String("export"))
    {
        options.exportMode = true;
    }
    else if (key == QLatin1String("recursive"))
    {
        options.recursive = true;
    }
    else if (key == QLatin1String("transparent"))
    {
        options.transparent = true;
    }
    else if (key == QLatin1String("embedDiagram"))
    {
        options.embedDiagram = true;
    }
    else if (key == QLatin1String("embedSvgImages"))
    {
        options.embedSvgImages = true;
    }
    else if (key == QLatin1String("embedSvgMetadata"))
    {
        options.embedSvgMetadata = true;
    }
    else if (key == QLatin1String("crop"))
    {
        options.crop = true;
    }
    else if (key == QLatin1String("allPages"))
    {
        options.allPages = true;
    }
    else if (key == QLatin1String("uncompressed"))
    {
        options.uncompressed = true;
    }
    else if (key == QLatin1String("normalize"))
    {
        options.normalize = true;
    }
}

} // namespace

ParseResult parse(const QStringList &arguments, bool hasProgramName)
{
    ParseResult result;
    CliOptions &options = result.options;

    QStringList tokens = arguments;

    if (hasProgramName && !tokens.isEmpty())
    {
        tokens.removeFirst();
    }

    int index = 0;

    while (index < tokens.size())
    {
        const QString token = tokens.at(index);

        if (token == QLatin1String("--"))
        {
            for (int i = index + 1; i < tokens.size(); ++i)
            {
                if (!tokens.at(i).startsWith(QLatin1Char('-')))
                {
                    options.inputs.append(tokens.at(i));
                }
            }
            break;
        }

        if (token == QLatin1String("--help") || token == QLatin1String("-h"))
        {
            result.help = true;
            ++index;
            continue;
        }

        if (token == QLatin1String("--version") || token == QLatin1String("-V"))
        {
            result.version = true;
            ++index;
            continue;
        }

        if (token == QLatin1String("--disable-update"))
        {
            result.disableUpdate = true;
            ++index;
            continue;
        }

        if (token == QLatin1String("--no-silent-update"))
        {
            result.noSilentUpdate = true;
            ++index;
            continue;
        }

        // --flag=value
        if (token.startsWith(QLatin1String("--")) && token.contains(QLatin1Char('=')))
        {
            const int eq = token.indexOf(QLatin1Char('='));
            const OptionSpec *spec = findByLong(token.left(eq));

            if (spec != nullptr && spec->takesValue)
            {
                applyValue(options, *spec, token.mid(eq + 1));
            }

            ++index;
            continue;
        }

        if (token.startsWith(QLatin1Char('-')))
        {
            const OptionSpec *spec = findByShort(token);

            if (spec == nullptr)
            {
                spec = findByLong(token);
            }

            // Combined short flags, e.g. -xa or -fpng.
            if (spec == nullptr && !token.startsWith(QLatin1String("--")) && token.size() > 2)
            {
                bool allRecognized = true;
                int position = 1;
                int consumedNext = 0;

                while (position < token.size())
                {
                    const OptionSpec *shortSpec =
                        findByShort(QStringLiteral("-") + token.at(position));

                    if (shortSpec == nullptr)
                    {
                        allRecognized = false;
                        break;
                    }

                    if (shortSpec->takesValue)
                    {
                        if (position + 1 < token.size())
                        {
                            applyValue(options, *shortSpec, token.mid(position + 1));
                        }
                        else if (index + 1 < tokens.size()
                                 && !tokens.at(index + 1).startsWith(QLatin1Char('-')))
                        {
                            applyValue(options, *shortSpec, tokens.at(index + 1));
                            consumedNext = 1;
                        }

                        position = token.size();
                        break;
                    }

                    applyFlag(options, shortSpec->key);
                    ++position;
                }

                if (allRecognized)
                {
                    index += 1 + consumedNext;
                    continue;
                }
            }

            if (spec == nullptr)
            {
                ++index;
                continue;
            }

            if (!spec->takesValue)
            {
                applyFlag(options, spec->key);
                ++index;
            }
            else
            {
                ++index;

                if (index < tokens.size() && !tokens.at(index).startsWith(QLatin1Char('-')))
                {
                    applyValue(options, *spec, tokens.at(index));
                    ++index;
                }
            }

            continue;
        }

        options.inputs.append(token);
        ++index;
    }

    return result;
}

QString helpText(const QString &version)
{
    QStringList rows;
    QStringList flagsList;
    QStringList descriptions;

    flagsList << QStringLiteral("-V, --version");
    descriptions << QStringLiteral("output the version number");

    for (const OptionSpec &spec : specs())
    {
        const QString label = spec.valueLabel.isEmpty()
            ? QString()
            : QStringLiteral(" ") + spec.valueLabel;
        const QString flags = spec.shortName.isEmpty()
            ? spec.longName + label
            : spec.shortName + QStringLiteral(", ") + spec.longName + label;

        flagsList << flags;
        descriptions << spec.description;
    }

    flagsList << QStringLiteral("-h, --help");
    descriptions << QStringLiteral("display help for command");

    int width = 0;

    for (const QString &flags : flagsList)
    {
        width = qMax(width, flags.size());
    }

    width += 4;

    QStringList lines;
    lines << QStringLiteral("drawio-qt %1 (native Qt6/C++ port of draw.io Desktop)").arg(version);
    lines << QString();
    lines << QStringLiteral("Usage: drawio-qt [options] [input file/folder]...");
    lines << QString();
    lines << QStringLiteral("Options:");

    for (int i = 0; i < flagsList.size(); ++i)
    {
        lines << QStringLiteral("  ") + flagsList.at(i).leftJustified(width)
                     + descriptions.at(i);
    }

    return lines.join(QLatin1Char('\n'));
}

QStringList supportedFormats()
{
    return {QStringLiteral("pdf"), QStringLiteral("png"), QStringLiteral("jpg"),
            QStringLiteral("jpeg"), QStringLiteral("svg"), QStringLiteral("xml"),
            QStringLiteral("html")};
}

QString normalizedFormat(const QString &format)
{
    const QString lower = format.trimmed().toLower();
    return lower == QLatin1String("jpeg") ? QStringLiteral("jpg") : lower;
}

} // namespace Args
} // namespace dq
