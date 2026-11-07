#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace dq {

/// Parsed command line options, mirroring drawio-desktop's `args.js`.
struct CliOptions
{
    QStringList inputs;

    // General
    bool create = false;
    bool check = false;
    bool recursive = false;
    bool exportMode = false;
    int timeout = 0;
    double zoom = 0.0;

    // Output
    QString output;
    QString format = QStringLiteral("pdf");
    QString size = QStringLiteral("diagram");
    int quality = 0;
    int border = 0;
    double scale = 0.0;
    int width = 0;
    int height = 0;
    bool crop = false;

    // Content
    bool transparent = false;
    bool embedDiagram = false;
    bool embedSvgImages = false;
    bool embedSvgFonts = true;
    bool embedSvgMetadata = false;
    bool uncompressed = false;
    bool allPages = false;
    int pageIndex = -1; // 0 based; -1 when not given
    bool hasPageIndex = false;
    bool hasPageRange = false;
    QVector<int> pageRange;
    QString layers;
    QString layout;
    bool normalize = false;
    bool mermaidImage = false;

    // Themes
    QString theme;
    QString svgTheme = QStringLiteral("auto");
    QString svgLinksTarget = QStringLiteral("auto");

    // HTML viewer
    QString htmlTheme = QStringLiteral("auto");
    bool htmlZoom = true;
    bool htmlLightbox = true;
    bool htmlLayers = true;
    bool htmlTags = true;
    bool htmlFit = true;
    QString htmlLinkTarget = QStringLiteral("auto");
    QString htmlLinkColor;
    QString htmlEditLink;
};

struct ParseResult
{
    CliOptions options;
    QString error;
    bool help = false;
    bool version = false;
    bool disableUpdate = false;
    bool noSilentUpdate = false;
};

namespace Args {

/// Parses argv (with or without a leading program name).
ParseResult parse(const QStringList &arguments, bool hasProgramName = true);

/// The `--help` text, aligned like commander.js output.
QString helpText(const QString &version);

QStringList supportedFormats();

QString normalizedFormat(const QString &format);

} // namespace Args
} // namespace dq
