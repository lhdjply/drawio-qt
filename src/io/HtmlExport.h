#pragma once

#include <QByteArray>
#include <QString>

namespace dq {

/// Options for the standalone HTML viewer (drawio's `.html` export).
struct HtmlOptions
{
    QString theme = QStringLiteral("auto"); // auto | light | dark
    QString linkTarget = QStringLiteral("auto"); // auto | blank | self
    QString linkColor;
    QString editLink;
    bool zoom = true;
    bool lightbox = true;
    bool layers = true;
    bool tags = true;
    bool fit = true;
};

namespace HtmlExport {

/// Builds a self contained HTML page that renders the diagram through the
/// diagrams.net viewer, using the same `<div data-mxgraph>` contract as the
/// Electron version so links, layers and pages keep working.
QByteArray build(const QString &xml, const QString &title, const HtmlOptions &options = {});

} // namespace HtmlExport
} // namespace dq
