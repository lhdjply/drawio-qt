#include "render/ShapeRenderer.h"

#include "render/ShapeRegistry.h"

#include <QFontMetricsF>
#include <QRegularExpression>
#include <QImage>
#include <QLinearGradient>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSharedPointer>
#include <QSvgRenderer>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextOption>
#include <QtMath>

namespace dq {
namespace {

bool isClosingPunctuation(QChar character)
{
    static const QString closing = QStringLiteral("，。、；：！？）》】」』〕｝”’…");
    return closing.contains(character);
}

QString decodeHtmlEntities(QString text)
{
    text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return text;
}

Qt::Alignment alignmentFromTag(const QString &tag, Qt::Alignment fallback)
{
    static const QRegularExpression pattern(
        QStringLiteral("text-align\\s*:\\s*(left|right|center|justify)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = pattern.match(tag);

    if (!match.hasMatch())
    {
        return fallback & Qt::AlignHorizontal_Mask;
    }

    const QString value = match.captured(1).toLower();

    if (value == QLatin1String("left") || value == QLatin1String("justify"))
    {
        return Qt::AlignLeft;
    }

    if (value == QLatin1String("right"))
    {
        return Qt::AlignRight;
    }

    return Qt::AlignHCenter;
}



QString normalizedFontFamily(const QString &family)
{
    if (family.isEmpty())
    {
        return QStringLiteral("Helvetica");
    }

    return family;
}

QPen penFor(const Cell &cell)
{
    QPen pen;

    const QColor stroke = ShapeRenderer::strokeColor(cell);
    pen.setColor(stroke);
    pen.setWidthF(qMax(0.5, cell.styleNumber(QStringLiteral("strokeWidth"), 1.0)));

    if (cell.styleFlag(QStringLiteral("dashed")))
    {
        const QString dashPattern = cell.styleValue(QStringLiteral("dashPattern"));

        if (!dashPattern.isEmpty())
        {
            QVector<qreal> dashes;
            const QStringList parts = dashPattern.split(QLatin1Char(' '), Qt::SkipEmptyParts);

            for (const QString &part : parts)
            {
                bool ok = false;
                const double value = part.toDouble(&ok);

                if (ok && value > 0)
                {
                    dashes.append(value);
                }
            }

            if (!dashes.isEmpty())
            {
                pen.setDashPattern(dashes);
            }
            else
            {
                pen.setStyle(Qt::DashLine);
            }
        }
        else
        {
            // A slightly wider gap than Qt's default keeps dashed outlines
            // readable when the canvas is zoomed out.
            pen.setDashPattern({4.0, 3.0});
        }
    }

    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);
    return pen;
}


} // namespace

namespace LabelPainter {

QFont fontFor(const Cell &cell)
{
    QFont font(normalizedFontFamily(cell.styleValue(QStringLiteral("fontFamily"))));
    font.setPixelSize(qMax(1, static_cast<int>(cell.styleNumber(QStringLiteral("fontSize"), 11.0))));

    const int style = static_cast<int>(cell.styleNumber(QStringLiteral("fontStyle"), 0));
    font.setBold((style & 1) != 0);
    font.setItalic((style & 2) != 0);
    font.setUnderline((style & 4) != 0);
    return font;
}

QColor textColor(const Cell &cell)
{
    return cell.styleColor(QStringLiteral("fontColor"), QColor(Qt::black));
}

Qt::Alignment alignmentFor(const Cell &cell)
{
    const QString align = cell.styleValue(QStringLiteral("align"), QStringLiteral("center"));
    const QString valign = cell.styleValue(QStringLiteral("verticalAlign"), QStringLiteral("middle"));

    Qt::Alignment result;

    if (align == QLatin1String("left"))
    {
        result |= Qt::AlignLeft;
    }
    else if (align == QLatin1String("right"))
    {
        result |= Qt::AlignRight;
    }
    else
    {
        result |= Qt::AlignHCenter;
    }

    if (valign == QLatin1String("top"))
    {
        result |= Qt::AlignTop;
    }
    else if (valign == QLatin1String("bottom"))
    {
        result |= Qt::AlignBottom;
    }
    else
    {
        result |= Qt::AlignVCenter;
    }

    return result;
}

QRectF labelRect(const Cell &cell, const QRectF &bounds)
{
    // draw.io's default label box keeps only a hair of horizontal padding;
    // using 2 px on both sides made CJK text wrap one character earlier than
    // in the reference editor.
    const double spacing = cell.styleNumber(QStringLiteral("spacing"), 1.0);
    const double spacingLeft = cell.styleNumber(QStringLiteral("spacingLeft"), spacing);
    const double spacingRight = cell.styleNumber(QStringLiteral("spacingRight"), spacing);
    const double spacingTop = cell.styleNumber(QStringLiteral("spacingTop"), spacing);
    const double spacingBottom = cell.styleNumber(QStringLiteral("spacingBottom"), spacing);

    QRectF rect = bounds.adjusted(spacingLeft, spacingTop, -spacingRight, -spacingBottom);

    if (ShapeRegistry::isContainerShape(cell) && cell.styleFlag(QStringLiteral("swimlane")))
    {
        const double startSize = cell.styleNumber(QStringLiteral("startSize"), 23.0);
        rect.setHeight(qMax(0.0, startSize - spacingTop));
    }

    // Labels below / above the shape (used by umlActor and image shapes).
    const QString verticalLabelPosition = cell.styleValue(QStringLiteral("verticalLabelPosition"));

    if (verticalLabelPosition == QLatin1String("bottom"))
    {
        rect = QRectF(bounds.left(), bounds.bottom() + spacing,
                      bounds.width(), qMax(16.0, cell.styleNumber(QStringLiteral("fontSize"), 11.0) * 1.6));
    }
    else if (verticalLabelPosition == QLatin1String("top"))
    {
        rect = QRectF(bounds.left(), bounds.top() - spacing
                                        - qMax(16.0, cell.styleNumber(QStringLiteral("fontSize"), 11.0) * 1.6),
                      bounds.width(), qMax(16.0, cell.styleNumber(QStringLiteral("fontSize"), 11.0) * 1.6));
    }

    return rect;
}

void applyInlineFont(const QString &html, QFont &font)
{
    static const QRegularExpression sizePattern(
        QStringLiteral("font-size\\s*:\\s*([0-9.]+)\\s*(pt|px)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression familyPattern(
        QStringLiteral("font-family\\s*:\\s*([^;\"']+)"),
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch lastSize;

    for (auto it = sizePattern.globalMatch(html); it.hasNext();)
    {
        lastSize = it.next();
    }

    if (lastSize.hasMatch())
    {
        double size = lastSize.captured(1).toDouble();

        // Word writes font sizes in points. draw.io's HTML labels use a
        // slightly smaller pixel size than a strict 96 dpi conversion, which
        // lets an 11pt line of nine CJK characters fit in a 120px cell.
        if (lastSize.captured(2).compare(QLatin1String("pt"), Qt::CaseInsensitive) == 0)
        {
            size *= 1.3;
        }
        if (size > 0.5 && size < 500.0)
        {
            font.setPixelSize(qRound(size));
        }
    }

    QRegularExpressionMatch lastFamily;

    for (auto it = familyPattern.globalMatch(html); it.hasNext();)
    {
        lastFamily = it.next();
    }

    if (lastFamily.hasMatch())
    {
        QString family = lastFamily.captured(1).trimmed();
        family.remove(QLatin1Char('"'));
        family.remove(QLatin1Char('\''));

        if (!family.isEmpty())
        {
            font.setFamily(family);
        }
    }
}

QFont fontForLabel(const Cell &cell)
{
    QFont font = fontFor(cell);
    applyInlineFont(cell.value, font);
    return font;
}

void paint(QPainter &painter, const Cell &cell, const QRectF &bounds, bool drawBackground)
{
    if (cell.value.isEmpty())
    {
        return;
    }

    const QRectF rect = labelRect(cell, bounds);

    if (rect.width() <= 0 || rect.height() <= 0)
    {
        return;
    }

    painter.save();

    if (drawBackground)
    {
        const QString background = cell.styleValue(QStringLiteral("labelBackgroundColor"));

        if (!background.isEmpty() && !Style::isNoneColor(background))
        {
            painter.fillRect(rect, QColor(background));
        }
    }

    QFont font = fontForLabel(cell);
    painter.setFont(font);
    painter.setPen(textColor(cell));

    const QFontMetricsF metrics(font);
    const bool wrap = cell.styleValue(QStringLiteral("whiteSpace")) == QLatin1String("wrap")
        || cell.isHtmlLabel();
    const double maxWidth = wrap ? rect.width() : 100000.0;
    const Qt::Alignment baseAlignment = alignmentFor(cell) & Qt::AlignHorizontal_Mask;

    struct Paragraph
    {
        QString text;
        Qt::Alignment align;
    };

    QVector<Paragraph> paragraphs;

    if (cell.isHtmlLabel())
    {
        QString buffer;
        Qt::Alignment current = baseAlignment;

        const auto flush = [&](bool force)
        {
            const QString text = decodeHtmlEntities(buffer);
            buffer.clear();

            if (!text.isEmpty() || force)
            {
                paragraphs.append(Paragraph{text, current});
            }
        };

        const QString html = cell.value;

        for (int i = 0; i < html.size();)
        {
            if (html.at(i) == QLatin1Char('<'))
            {
                const int end = html.indexOf(QLatin1Char('>'), i);

                if (end < 0)
                {
                    buffer += html.mid(i);
                    break;
                }

                const QString tag = html.mid(i + 1, end - i - 1).trimmed();
                const QString lower = tag.toLower();

                if (lower.startsWith(QLatin1String("br")))
                {
                    flush(true);
                }
                else if (lower.startsWith(QLatin1String("p"))
                         && !lower.startsWith(QLatin1String("/p"))
                         && !lower.startsWith(QLatin1String("pre")))
                {
                    flush(false);
                    current = alignmentFromTag(tag, baseAlignment);
                }
                else if (lower.startsWith(QLatin1String("/p")))
                {
                    flush(false);
                    current = baseAlignment;
                }
                else if (lower.startsWith(QLatin1String("div"))
                         && !lower.startsWith(QLatin1String("/div")))
                {
                    flush(false);
                    current = alignmentFromTag(tag, baseAlignment);
                }
                else if (lower.startsWith(QLatin1String("/div")))
                {
                    flush(false);
                    current = baseAlignment;
                }

                i = end + 1;
            }
            else
            {
                buffer += html.at(i);
                ++i;
            }
        }

        flush(false);

        if (paragraphs.isEmpty())
        {
            paragraphs.append(Paragraph{cell.plainLabel(), baseAlignment});
        }
    }
    else
    {
        const QStringList parts = cell.value.split(QLatin1Char('\n'));

        for (const QString &part : parts)
        {
            paragraphs.append(Paragraph{part, baseAlignment});
        }
    }

    struct RenderLine
    {
        QString text;
        Qt::Alignment align;
    };

    QVector<RenderLine> lines;

    for (const Paragraph &paragraph : paragraphs)
    {
        if (paragraph.text.isEmpty())
        {
            lines.append(RenderLine{QString(), paragraph.align});
            continue;
        }

        QVector<RenderLine> paragraphLines;
        QString current;

        for (const QChar character : paragraph.text)
        {
            const QString candidate = current + character;

            if (!current.isEmpty() && metrics.horizontalAdvance(candidate) > maxWidth)
            {
                const int lastSpace = current.lastIndexOf(QLatin1Char(' '));

                if (isClosingPunctuation(character) && current.size() > 1)
                {
                    const QChar moved = current.at(current.size() - 1);
                    current.chop(1);
                    paragraphLines.append(RenderLine{current, paragraph.align});
                    current = QString(moved) + character;
                }
                else if (lastSpace > 0 && !character.isSpace())
                {
                    paragraphLines.append(RenderLine{current.left(lastSpace), paragraph.align});
                    current = current.mid(lastSpace + 1) + character;
                }
                else
                {
                    paragraphLines.append(RenderLine{current, paragraph.align});
                    current = QString(character);
                }
            }
            else
            {
                current = candidate;
            }
        }

        if (!current.isEmpty())
        {
            paragraphLines.append(RenderLine{current, paragraph.align});
        }

        // A single line behaves like a normal centred label; inline
        // text-align only matters when the paragraph actually wraps.
        if (paragraphLines.size() == 1)
        {
            paragraphLines.first().align = baseAlignment;
        }

        lines.append(paragraphLines);
    }

    if (lines.isEmpty())
    {
        painter.restore();
        return;
    }

    double lineHeight = qMax(1, font.pixelSize()) * 1.05;

    if (lineHeight <= 0.0)
    {
        lineHeight = metrics.height();
    }

    const double totalHeight = lineHeight * lines.size();
    double top = rect.top();

    if (alignmentFor(cell) & Qt::AlignVCenter)
    {
        top = rect.center().y() - totalHeight / 2.0;
    }
    else if (alignmentFor(cell) & Qt::AlignBottom)
    {
        top = rect.bottom() - totalHeight;
    }

    if (!ShapeRegistry::isTextShape(cell))
    {
        painter.setClipRect(rect.adjusted(-1, -1, 1, 1));
    }

    double y = top;

    const Qt::TextFlag clipFlag = ShapeRegistry::isTextShape(cell)
        ? Qt::TextDontClip : Qt::TextFlag(0);

    for (const RenderLine &line : lines)
    {
        painter.drawText(QRectF(rect.left(), y, rect.width(), lineHeight),
                         line.align | Qt::AlignVCenter | clipFlag, line.text);
        y += lineHeight;
    }

    painter.restore();
}

} // namespace LabelPainter

namespace ShapeRenderer {

QColor fillColor(const Cell &cell)
{
    const QString value = cell.styleValue(QStringLiteral("fillColor"));

    if (value.isEmpty())
    {
        return QColor(Qt::white);
    }

    if (Style::isNoneColor(value))
    {
        return Qt::transparent;
    }

    const QColor color = Style::parseColorValue(value);
    return color.isValid() ? color : QColor(Qt::white);
}

QColor strokeColor(const Cell &cell)
{
    const QString value = cell.styleValue(QStringLiteral("strokeColor"));

    if (value.isEmpty())
    {
        return QColor(Qt::black);
    }

    if (Style::isNoneColor(value))
    {
        return Qt::transparent;
    }

    const QColor color = Style::parseColorValue(value);
    return color.isValid() ? color : QColor(Qt::black);
}

void paintShadow(QPainter &painter, const Cell &cell)
{
    if (!cell.styleFlag(QStringLiteral("shadow")))
    {
        return;
    }

    painter.save();
    painter.translate(3, 3);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 50));
    painter.drawPath(ShapeRegistry::pathFor(cell));
    painter.restore();
}

void paint(QPainter &painter, const Cell &cell)
{
    // draw.io groups are invisible containers: their children carry the
    // visible geometry. Painting a group produced a stray black outline.
    if (cell.isGroup() || cell.styleFlag(QStringLiteral("group")))
    {
        return;
    }

    painter.save();

    const QRectF local(QPointF(0, 0), cell.geometry.size());
    const QPointF center = cell.absolutePosition() + local.center();

    double rotation = cell.rotation;

    if (qFuzzyIsNull(rotation))
    {
        rotation = cell.styleNumber(QStringLiteral("rotation"), 0.0);
    }

    if (!qFuzzyIsNull(rotation))
    {
        painter.translate(center);
        painter.rotate(rotation);
        painter.translate(-center);
    }

    painter.translate(cell.absolutePosition());


    const double opacity = qBound(0.0, cell.styleNumber(QStringLiteral("opacity"), 100.0), 100.0);
    painter.setOpacity(opacity / 100.0);

    // mxShapeElectricalBatteryStack from shapes/mxElectrical.js: a series of
    // filled short plates and long thin plates, with dashed links between cells.
    if (cell.styleValue(QStringLiteral("shape")).compare(
            QLatin1String("mxgraph.electrical.miscellaneous.batteryStack"),
            Qt::CaseInsensitive) == 0)
    {
        const double batteryWidth = local.height() * 0.3;
        const int cellCount = qBound(0, static_cast<int>(std::floor(
                                         (local.width() - 20.0) / batteryWidth)), 1000);
        const double startX = (local.width() - cellCount * batteryWidth) * 0.5;

        QPen batteryPen(strokeColor(cell),
                        qMax(0.5, cell.styleNumber(QStringLiteral("strokeWidth"), 1.0)),
                        Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
        painter.setPen(batteryPen);

        if (cellCount > 0)
        {
            painter.drawLine(QPointF(0, local.height() * 0.5),
                             QPointF(startX + batteryWidth * 0.2,
                                     local.height() * 0.5));
            painter.drawLine(QPointF(local.width() - startX - batteryWidth * 0.2,
                                     local.height() * 0.5),
                             QPointF(local.width(), local.height() * 0.5));

            for (int i = 0; i < cellCount; ++i)
            {
                const double currentX = startX + i * batteryWidth;
                const QRectF plate(currentX + batteryWidth * 0.2,
                                   local.height() * 0.25,
                                   batteryWidth * 0.2, local.height() * 0.5);

                painter.fillRect(plate, strokeColor(cell));
                painter.drawRect(plate);
                painter.drawLine(QPointF(currentX + batteryWidth * 0.8, 0),
                                 QPointF(currentX + batteryWidth * 0.8, local.height()));

                if (i > 0)
                {
                    QPen dashed = batteryPen;
                    dashed.setDashPattern({3.0, 3.0});
                    painter.setPen(dashed);
                    painter.drawLine(QPointF(currentX - batteryWidth * 0.2,
                                             local.height() * 0.5),
                                     QPointF(currentX + batteryWidth * 0.2,
                                             local.height() * 0.5));
                    painter.setPen(batteryPen);
                }
            }
        }

        LabelPainter::paint(painter, cell, local);
        painter.restore();
        return;
    }

    const QPainterPath path = ShapeRegistry::pathFor(cell);
    QColor fill = fillColor(cell);
    const QColor stroke = strokeColor(cell);
    const QString cellShape = cell.styleValue(QStringLiteral("shape")).toLower();

    if (cellShape == QLatin1String("waypoint"))
    {
        fill = stroke;
    }
    const bool textShape = ShapeRegistry::isTextShape(cell);
    const bool imageShape = ShapeRegistry::isImageShape(cell);
    const bool lineArt = ShapeRegistry::isLineArt(cell);

    QPen pen = penFor(cell);

    if (stroke.alpha() == 0)
    {
        pen = Qt::NoPen;
    }

    painter.setPen(pen);

    if (imageShape)
    {
        const QString source = cell.styleValue(QStringLiteral("image"));
        const bool keepAspect = cell.styleFlag(QStringLiteral("imageAspect"))
            || cell.styleFlag(QStringLiteral("aspect"));
        QRectF target = local;

        const auto fitTarget = [&local, keepAspect](const QSizeF &sourceSize)
        {
            if (!keepAspect || sourceSize.isEmpty())
            {
                return local;
            }

            QSizeF scaled = sourceSize;
            scaled.scale(local.size(), Qt::KeepAspectRatio);
            return QRectF(local.center() - QPointF(scaled.width() / 2.0,
                                                   scaled.height() / 2.0),
                          scaled);
        };

        QString path;

        if (source.startsWith(QLatin1String("data:")))
        {
            const int comma = source.indexOf(QLatin1Char(','));

            if (comma > 0)
            {
                const QByteArray data = QByteArray::fromBase64(source.mid(comma + 1).toLatin1());
                QImage image;
                image.loadFromData(data);

                if (!image.isNull())
                {
                    target = fitTarget(image.size());
                    painter.drawImage(target, image);
                }
            }
        }
        else
        {
            if (source.startsWith(QLatin1String("img/")))
            {
                path = QStringLiteral(":/drawio/") + source;
            }
            else if (source.startsWith(QLatin1String(":/")) || QFileInfo::exists(source))
            {
                path = source;
            }

            if (!path.isEmpty() && path.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive))
            {
                static QHash<QString, QSharedPointer<QSvgRenderer>> renderers;
                QSharedPointer<QSvgRenderer> renderer = renderers.value(path);

                if (renderer.isNull())
                {
                    renderer = QSharedPointer<QSvgRenderer>::create(path);
                    renderers.insert(path, renderer);
                }

                if (renderer->isValid())
                {
                    target = fitTarget(renderer->defaultSize());
                    renderer->render(&painter, target);
                }
            }
            else if (!path.isEmpty())
            {
                QImage image(path);

                if (!image.isNull())
                {
                    target = fitTarget(image.size());
                    painter.drawImage(target, image);
                }
            }
        }

        if (target.isEmpty() || (path.isEmpty() && !source.startsWith(QLatin1String("data:"))))
        {
            painter.setPen(QPen(QColor(160, 160, 160), 1, Qt::DashLine));
            painter.setBrush(QColor(245, 245, 245, 60));
            painter.drawRect(local);
        }

        LabelPainter::paint(painter, cell, local);
        painter.restore();
        return;
    }

    if (!textShape)
    {
        if (cell.styleFlag(QStringLiteral("shadow")))
        {
            painter.save();
            painter.translate(3, 3);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, 50));
            painter.drawPath(path);
            painter.restore();
        }

        if (!lineArt && fill.alpha() > 0)
        {
            const QString gradientColor = cell.styleValue(QStringLiteral("gradientColor"));

            if (!gradientColor.isEmpty() && !Style::isNoneColor(gradientColor))
            {
                QLinearGradient gradient(local.topLeft(), local.bottomLeft());
                gradient.setColorAt(0.0, fill);
                gradient.setColorAt(1.0, QColor(gradientColor));
                painter.setBrush(gradient);
            }
            else
            {
                painter.setBrush(fill);
            }
        }
        else
        {
            painter.setBrush(Qt::NoBrush);
        }

        painter.drawPath(path);

        // Decorations that draw.io renders on top of the base outline.
        const QString shape = cell.styleValue(QStringLiteral("shape")).toLower();
        const QString key = shape.isEmpty() ? QStringLiteral("rect") : shape;

        if (key == QLatin1String("process"))
        {
            const double inset = qMin(local.width() * 0.08, 12.0);
            painter.drawLine(QPointF(inset, 0), QPointF(inset, local.height()));
            painter.drawLine(QPointF(local.width() - inset, 0),
                             QPointF(local.width() - inset, local.height()));
        }
        else if (key == QLatin1String("cylinder3") || key == QLatin1String("cylinder"))
        {
            const double cap = qMin(local.height() * 0.18, 26.0);
            QPainterPath top;
            top.moveTo(0, cap / 2);
            top.cubicTo(QPointF(0, 0), QPointF(local.width(), 0),
                        QPointF(local.width(), cap / 2));
            painter.drawPath(top);
        }
        else if (key == QLatin1String("note"))
        {
            const double fold = qMin(qMin(local.width(), local.height()) * 0.22, 24.0);
            painter.drawLine(QPointF(local.width() - fold, 0), QPointF(local.width() - fold, fold));
            painter.drawLine(QPointF(local.width() - fold, fold), QPointF(local.width(), fold));
        }
        else if (key == QLatin1String("cube"))
        {
            const double depth = qMin(qMin(local.width(), local.height()) * 0.18, 20.0);
            painter.drawLine(QPointF(0, depth), QPointF(local.width() - depth, depth));
            painter.drawLine(QPointF(local.width() - depth, depth),
                             QPointF(local.width(), 0));
            painter.drawLine(QPointF(local.width() - depth, depth),
                             QPointF(local.width() - depth, local.height()));
        }
        else if (key == QLatin1String("swimlane") || cell.styleFlag(QStringLiteral("swimlane")))
        {
            const double startSize = cell.styleNumber(QStringLiteral("startSize"), 23.0);
            painter.drawLine(QPointF(0, startSize), QPointF(local.width(), startSize));
        }
        else if (key == QLatin1String("mxgraph.flowchart.internal_storage"))
        {
            const double inset = local.width() * 0.14;
            painter.drawLine(QPointF(inset, local.height() * 0.2),
                             QPointF(local.width() - inset, local.height() * 0.2));
            painter.drawLine(QPointF(inset, local.height() * 0.24),
                             QPointF(local.width() - inset, local.height() * 0.24));
            painter.drawLine(QPointF(inset, local.height() * 0.2),
                             QPointF(inset, local.height() * 0.9));
        }
        else if (key == QLatin1String("mxgraph.flowchart.multi-document"))
        {
            painter.drawRect(QRectF(local.width() * 0.08, 0,
                                    local.width() * 0.84, local.height() * 0.7));
            painter.drawRect(QRectF(local.width() * 0.04, local.height() * 0.06,
                                    local.width() * 0.84, local.height() * 0.7));
        }
        else if (key == QLatin1String("mxgraph.flowchart.summing")
                 || cell.styleValue(QStringLiteral("fcType")) == QLatin1String("summing"))
        {
            painter.drawLine(QPointF(local.width() * 0.28, local.height() * 0.28),
                             QPointF(local.width() * 0.72, local.height() * 0.72));
            painter.drawLine(QPointF(local.width() * 0.72, local.height() * 0.28),
                             QPointF(local.width() * 0.28, local.height() * 0.72));
        }
        else if (cell.styleValue(QStringLiteral("fcType")) == QLatin1String("sort"))
        {
            painter.drawLine(QPointF(local.width() * 0.25, local.height() / 2.0),
                             QPointF(local.width() * 0.75, local.height() / 2.0));
        }
        else if (cell.styleValue(QStringLiteral("bpmnType")) == QLatin1String("exclusive"))
        {
            const double inset = qMin(local.width(), local.height()) * 0.25;
            painter.drawLine(QPointF(inset, inset),
                             QPointF(local.width() - inset, local.height() - inset));
            painter.drawLine(QPointF(local.width() - inset, inset),
                             QPointF(inset, local.height() - inset));
        }
        else if (cell.styleValue(QStringLiteral("bpmnType")) == QLatin1String("parallel"))
        {
            const double inset = qMin(local.width(), local.height()) * 0.24;
            painter.drawLine(QPointF(inset, local.height() / 2.0),
                             QPointF(local.width() - inset, local.height() / 2.0));
            painter.drawLine(QPointF(local.width() / 2.0, inset),
                             QPointF(local.width() / 2.0, local.height() - inset));
        }
        else if (cell.styleValue(QStringLiteral("bpmnType")) == QLatin1String("inclusive"))
        {
            const double radius = qMin(local.width(), local.height()) * 0.26;
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(local.center(), radius, radius);
        }
        else if (cell.styleValue(QStringLiteral("bpmnType")) == QLatin1String("subprocess"))
        {
            const double box = qMin(local.width(), local.height()) * 0.22;
            const QRectF marker(local.center().x() - box / 2.0,
                                local.bottom() - box - 6, box, box);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(marker);
            painter.drawLine(QPointF(marker.center().x() - box * 0.3, marker.center().y()),
                             QPointF(marker.center().x() + box * 0.3, marker.center().y()));
            painter.drawLine(QPointF(marker.center().x(), marker.center().y() - box * 0.3),
                             QPointF(marker.center().x(), marker.center().y() + box * 0.3));
        }
        else if (key == QLatin1String("mxgraph.bpmn.message"))
        {
            painter.drawLine(QPointF(0, 0), QPointF(local.width() / 2.0, local.height() * 0.55));
            painter.drawLine(QPointF(local.width(), 0),
                             QPointF(local.width() / 2.0, local.height() * 0.55));
        }
        else if (key == QLatin1String("mxgraph.bpmn.data_store"))
        {
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(QRectF(0, -local.height() * 0.12,
                                       local.width(), local.height() * 0.28));
        }
        else if (cell.styleValue(QStringLiteral("netType")) == QLatin1String("server"))
        {
            for (int i = 0; i < 3; ++i)
            {
                painter.drawLine(QPointF(local.width() * 0.18, local.height() * (0.28 + i * 0.14)),
                                 QPointF(local.width() * 0.82, local.height() * (0.28 + i * 0.14)));
            }
            painter.setBrush(QColor(0x82, 0xb3, 0x66));
            painter.drawEllipse(QPointF(local.width() * 0.5, local.height() * 0.82),
                                local.width() * 0.06, local.width() * 0.06);
        }
        else if (cell.styleValue(QStringLiteral("netType")) == QLatin1String("switch"))
        {
            for (int i = 0; i < 5; ++i)
            {
                painter.drawRect(QRectF(local.width() * (0.14 + i * 0.15),
                                        local.height() * 0.72,
                                        local.width() * 0.08, local.height() * 0.1));
            }
        }
        else if (cell.styleValue(QStringLiteral("netType")) == QLatin1String("workstation"))
        {
            painter.drawLine(QPointF(local.width() * 0.5, local.height() * 0.72),
                             QPointF(local.width() * 0.5, local.height() * 0.9));
            painter.drawLine(QPointF(local.width() * 0.3, local.height() * 0.9),
                             QPointF(local.width() * 0.7, local.height() * 0.9));
        }
        else if (cell.styleValue(QStringLiteral("netType")) == QLatin1String("wireless"))
        {
            painter.setBrush(Qt::NoBrush);
            for (int i = 1; i <= 3; ++i)
            {
                const double radius = local.width() * 0.18 * i;
                painter.drawArc(QRectF(local.center().x() - radius, local.bottom() - radius * 1.4,
                                       radius * 2, radius * 1.4),
                                30 * 16, 120 * 16);
            }
        }
        else if (cell.styleValue(QStringLiteral("netType")) == QLatin1String("loadbalancer"))
        {
            painter.drawLine(QPointF(local.width() * 0.25, local.height() / 2.0),
                             QPointF(local.width() * 0.75, local.height() / 2.0));
            painter.drawLine(QPointF(local.width() * 0.25, local.height() / 2.0),
                             QPointF(local.width() * 0.4, local.height() * 0.32));
            painter.drawLine(QPointF(local.width() * 0.25, local.height() / 2.0),
                             QPointF(local.width() * 0.4, local.height() * 0.68));
        }
        else if (key == QLatin1String("mxgraph.networks.firewall"))
        {
            for (int row = 1; row < 4; ++row)
            {
                painter.drawLine(QPointF(0, local.height() * row / 4.0),
                                 QPointF(local.width(), local.height() * row / 4.0));
            }
            painter.drawLine(QPointF(local.width() / 2.0, 0),
                             QPointF(local.width() / 2.0, local.height() / 4.0));
            painter.drawLine(QPointF(local.width() / 4.0, local.height() / 4.0),
                             QPointF(local.width() / 4.0, local.height() / 2.0));
            painter.drawLine(QPointF(local.width() * 0.75, local.height() / 4.0),
                             QPointF(local.width() * 0.75, local.height() / 2.0));
            painter.drawLine(QPointF(local.width() / 2.0, local.height() / 2.0),
                             QPointF(local.width() / 2.0, local.height() * 0.75));
        }
        else if (key == QLatin1String("mxgraph.networks.storage"))
        {
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(QRectF(0, 0, local.width(), qMin(local.height() * 0.24, 18.0)));
            painter.drawEllipse(QRectF(0, local.height() * 0.32, local.width(),
                                       qMin(local.height() * 0.24, 18.0)));
        }
        else if (key == QLatin1String("mxgraph.dfd.data_store"))
        {
            painter.drawLine(QPointF(0, 0), QPointF(0, local.height() * 0.2));
            painter.drawLine(QPointF(local.width(), 0),
                             QPointF(local.width(), local.height() * 0.2));
            painter.drawLine(QPointF(0, local.height()), QPointF(0, local.height() * 0.8));
            painter.drawLine(QPointF(local.width(), local.height()),
                             QPointF(local.width(), local.height() * 0.8));
        }
        else if (key == QLatin1String("elec.motor"))
        {
            QFont glyph = painter.font();
            glyph.setPixelSize(qMax(10, static_cast<int>(local.height() * 0.34)));
            glyph.setBold(true);
            painter.setFont(glyph);
            painter.setPen(stroke);
            painter.drawText(local, Qt::AlignCenter, QStringLiteral("M"));
        }
        else if (key == QLatin1String("elec.ammeter")
                 || key == QLatin1String("elec.voltmeter"))
        {
            QFont glyph = painter.font();
            glyph.setPixelSize(qMax(10, static_cast<int>(local.height() * 0.4)));
            glyph.setBold(true);
            painter.setFont(glyph);
            painter.setPen(stroke);
            painter.drawText(local, Qt::AlignCenter,
                             key == QLatin1String("elec.ammeter")
                                 ? QStringLiteral("A") : QStringLiteral("V"));
        }
        else if (key == QLatin1String("elec.opamp"))
        {
            const double mark = qMin(local.width(), local.height()) * 0.12;
            painter.drawLine(QPointF(local.width() * 0.08, local.height() * 0.28),
                             QPointF(local.width() * 0.08 + mark, local.height() * 0.28));
            painter.drawLine(QPointF(local.width() * 0.08 + mark / 2.0,
                                     local.height() * 0.28 - mark / 2.0),
                             QPointF(local.width() * 0.08 + mark / 2.0,
                                     local.height() * 0.28 + mark / 2.0));
            painter.drawLine(QPointF(local.width() * 0.08, local.height() * 0.72),
                             QPointF(local.width() * 0.08 + mark, local.height() * 0.72));
        }
        else if (!cell.styleValue(QStringLiteral("androidType")).isEmpty())
        {
            painter.setBrush(QColor(0x1B, 0x1D, 0x1E));
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(QRectF(local.width() * 0.1, local.height() * 0.1,
                                           local.width() * 0.8, local.height() * 0.8),
                                    3, 3);
            painter.setBrush(QColor(0xA4, 0xC6, 0x39));
            painter.drawEllipse(QPointF(local.width() / 2.0, local.height() * 0.94),
                                local.width() * 0.05, local.width() * 0.05);
        }
    }

    LabelPainter::paint(painter, cell, local);
    painter.restore();
}

} // namespace ShapeRenderer
} // namespace dq
