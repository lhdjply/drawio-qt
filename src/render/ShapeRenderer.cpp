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

    const StyleMap style = Style::withDefaults(cell.style);
    const QColor stroke = ShapeRenderer::strokeColor(cell);
    pen.setColor(stroke);
    pen.setWidthF(qMax(0.5, Style::number(style, QStringLiteral("strokeWidth"), 1.0)));

    if (Style::flag(style, QStringLiteral("dashed")))
    {
        const QString dashPattern = Style::value(style, QStringLiteral("dashPattern"));

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
            // mxAbstractCanvas2D.setDashed() uses 3 3 for plain dashed
            // strokes; Qt scales dash patterns by the pen width, exactly like
            // draw.io scales them by the stroke width.
            pen.setDashPattern({3.0, 3.0});
        }
    }

    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);
    return pen;
}


} // namespace

namespace LabelPainter {

namespace {

/// draw.io resolves bare style keywords such as `text;` through its
/// stylesheet (styles/default.xml). The renderer reads labels and colours
/// through this resolved view so that those defaults apply without changing
/// the cell's own style string.
StyleMap resolvedStyle(const Cell &cell)
{
    return Style::withDefaults(cell.style);
}

QString blockTagName(const QString &tag)
{
    int end = 0;

    while (end < tag.size() && (tag.at(end).isLetterOrNumber()))
    {
        ++end;
    }

    return tag.left(end).toLower();
}

double cssNumber(const QString &value, double emBase, bool *ok)
{
    static const QRegularExpression pattern(
        QStringLiteral("^\\s*(-?[0-9.]+)\\s*(px|pt|em|%)?\\s*$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = pattern.match(value);

    if (!match.hasMatch())
    {
        *ok = false;
        return 0.0;
    }

    *ok = true;
    double number = match.captured(1).toDouble();
    const QString unit = match.captured(2).toLower();

    if (unit == QLatin1String("pt"))
    {
        number *= 1.3;
    }
    else if (unit == QLatin1String("em"))
    {
        number *= emBase;
    }
    else if (unit == QLatin1String("%"))
    {
        number = number * emBase / 100.0;
    }

    return number;
}

double cssLength(const QString &tag, const QString &property, double fallback, double emBase)
{
    const QRegularExpression pattern(
        QStringLiteral("(?:^|[;\"'\\s])") + QRegularExpression::escape(property)
            + QStringLiteral("\\s*:\\s*([^;\"']+)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = pattern.match(tag);

    if (!match.hasMatch())
    {
        return fallback;
    }

    bool ok = false;
    const double value = cssNumber(match.captured(1), emBase, &ok);
    return ok ? value : fallback;
}

struct BlockStyle
{
    QFont font;
    Qt::Alignment align = Qt::AlignLeft;
    double marginTop = 0.0;
    double marginBottom = 0.0;
};

BlockStyle blockStyleFor(const QString &tag, const QFont &baseFont, Qt::Alignment baseAlign,
                         bool isHtml)
{
    BlockStyle block;
    block.font = baseFont;
    block.align = baseAlign;

    if (!isHtml)
    {
        return block;
    }

    const QString name = blockTagName(tag);
    static const QHash<QString, double> headings = {
        {QStringLiteral("h1"), 2.0}, {QStringLiteral("h2"), 1.5},
        {QStringLiteral("h3"), 1.17}, {QStringLiteral("h4"), 1.0},
        {QStringLiteral("h5"), 0.83}, {QStringLiteral("h6"), 0.67},
    };

    const auto heading = headings.constFind(name);

    if (heading != headings.constEnd())
    {
        block.font.setPixelSize(qMax(1, qRound(baseFont.pixelSize() * heading.value())));
        block.font.setBold(true);
        const double em = block.font.pixelSize();
        block.marginTop = 0.67 * em;
        block.marginBottom = 0.67 * em;
    }
    else if (name == QLatin1String("p"))
    {
        const double em = baseFont.pixelSize();
        block.marginTop = em;
        block.marginBottom = em;
    }

    // Inline style attributes override the tag defaults.
    const double em = block.font.pixelSize();

    if (tag.contains(QLatin1String("font-size"), Qt::CaseInsensitive))
    {
        const double size = cssLength(tag, QStringLiteral("font-size"), em, baseFont.pixelSize());

        if (size > 0.5 && size < 500.0)
        {
            block.font.setPixelSize(qRound(size));
        }
    }

    block.marginTop = cssLength(tag, QStringLiteral("margin-top"), block.marginTop, em);
    block.marginBottom = cssLength(tag, QStringLiteral("margin-bottom"), block.marginBottom, em);

    static const QRegularExpression alignPattern(
        QStringLiteral("text-align\\s*:\\s*(left|right|center|justify)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch alignMatch = alignPattern.match(tag);

    if (alignMatch.hasMatch())
    {
        block.align = alignmentFromTag(tag, baseAlign);
    }

    return block;
}

} // namespace

QFont fontFor(const Cell &cell)
{
    const StyleMap style = resolvedStyle(cell);
    QFont font(normalizedFontFamily(Style::value(style, QStringLiteral("fontFamily"))));
    const double fallback = cell.isEdge() ? 11.0 : 12.0;
    font.setPixelSize(qMax(1, qRound(Style::number(style, QStringLiteral("fontSize"), fallback))));

    const int fontStyle = static_cast<int>(Style::number(style, QStringLiteral("fontStyle"), 0));
    font.setBold((fontStyle & 1) != 0);
    font.setItalic((fontStyle & 2) != 0);
    font.setUnderline((fontStyle & 4) != 0);
    return font;
}

QColor textColor(const Cell &cell)
{
    const StyleMap style = resolvedStyle(cell);
    return Style::color(style, QStringLiteral("fontColor"), QColor(Qt::black));
}

Qt::Alignment alignmentFor(const Cell &cell)
{
    const StyleMap style = resolvedStyle(cell);
    const QString align = Style::value(style, QStringLiteral("align"), QStringLiteral("center"));
    const QString valign = Style::value(style, QStringLiteral("verticalAlign"),
                                        QStringLiteral("middle"));

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
    const StyleMap style = resolvedStyle(cell);
    // mxText.apply() adds the base spacing to the side specific values, so a
    // cell with spacingLeft=4 uses 6 pixels (2 + 4).
    const double spacing = Style::number(style, QStringLiteral("spacing"), 2.0);
    const double spacingLeft = spacing + Style::number(style, QStringLiteral("spacingLeft"), 0.0);
    const double spacingRight = spacing + Style::number(style, QStringLiteral("spacingRight"), 0.0);
    const double spacingTop = spacing + Style::number(style, QStringLiteral("spacingTop"), 0.0);
    const double spacingBottom = spacing + Style::number(style, QStringLiteral("spacingBottom"), 0.0);

    QRectF rect = bounds.adjusted(spacingLeft, spacingTop, -spacingRight, -spacingBottom);

    if (ShapeRegistry::isContainerShape(cell) && cell.styleFlag(QStringLiteral("swimlane")))
    {
        const double startSize = Style::number(style, QStringLiteral("startSize"), 23.0);

        if (Style::flag(style, QStringLiteral("horizontal"), true))
        {
            rect.setHeight(qMax(0.0, startSize - spacingTop));
        }
        else
        {
            rect.setWidth(qMax(0.0, startSize - spacingLeft));
        }
    }

    // Labels below / above the shape (used by umlActor and image shapes).
    // mxGraphView.updateVertexLabelOffset moves the whole label box by the
    // height of the cell, it does not shrink it.
    const QString verticalLabelPosition =
        Style::value(style, QStringLiteral("verticalLabelPosition"));

    if (verticalLabelPosition == QLatin1String("bottom"))
    {
        rect = bounds.translated(0.0, bounds.height());
    }
    else if (verticalLabelPosition == QLatin1String("top"))
    {
        rect = bounds.translated(0.0, -bounds.height());
    }

    return rect;
}

void applyInlineFont(const QString &html, QFont &font)
{
    // Only a label that is entirely wrapped in one inline element takes that
    // element's font for the whole text. Mixed markup (headings, paragraphs,
    // partly coloured runs) is formatted per run by the HTML parser instead.
    static const QRegularExpression wrapper(
        QStringLiteral("^\\s*<(span|font)\\b[^>]*>.*</\\1>\\s*$"),
        QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression blockTag(
        QStringLiteral("<\\s*(h[1-6]|p|div|br|table|ul|ol)\\b"),
        QRegularExpression::CaseInsensitiveOption);

    if (!wrapper.match(html).hasMatch() || blockTag.match(html).hasMatch())
    {
        return;
    }

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

namespace {

struct TextRun
{
    QString text;
    QFont font;
    QColor color;
};

struct TextParagraph
{
    QVector<TextRun> runs;
    Qt::Alignment align = Qt::AlignLeft;
    QFont font;
    double marginTop = 0.0;
    double marginBottom = 0.0;
};

QColor inlineSpanColor(const QString &tag)
{
    static const QRegularExpression pattern(
        QStringLiteral("color\\s*:\\s*(#[0-9a-fA-F]{3,8}|rgba?\\([^)]*\\)|[a-zA-Z]+)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = pattern.match(tag);

    if (!match.hasMatch())
    {
        return QColor();
    }

    const QColor color = Style::parseColorValue(match.captured(1).trimmed());
    return color.isValid() ? color : QColor();
}

QFont inlineSpanFont(const QString &tag, const QFont &base)
{
    QFont font = base;

    static const QRegularExpression weightPattern(
        QStringLiteral("font-weight\\s*:\\s*(bold|[0-9]{3})"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch weight = weightPattern.match(tag);

    if (weight.hasMatch())
    {
        const QString value = weight.captured(1).toLower();
        font.setBold(value == QLatin1String("bold") || value.toInt() >= 600);
    }
    else if (QRegularExpression(QStringLiteral("font-weight\\s*:\\s*(normal|[1-5][0-9]{2})"),
                                QRegularExpression::CaseInsensitiveOption).match(tag).hasMatch())
    {
        font.setBold(false);
    }

    if (QRegularExpression(QStringLiteral("font-style\\s*:\\s*italic"),
                           QRegularExpression::CaseInsensitiveOption).match(tag).hasMatch())
    {
        font.setItalic(true);
    }

    if (QRegularExpression(QStringLiteral("text-decoration\\s*:\\s*[^;\"']*underline"),
                           QRegularExpression::CaseInsensitiveOption).match(tag).hasMatch())
    {
        font.setUnderline(true);
    }

    const double size = cssLength(tag, QStringLiteral("font-size"), 0.0, base.pixelSize());

    if (size > 0.5 && size < 500.0)
    {
        font.setPixelSize(qRound(size));
    }

    return font;
}

} // namespace

void paint(QPainter &painter, const Cell &cell, const QRectF &bounds, bool drawBackground)
{
    if (cell.value.isEmpty())
    {
        return;
    }

    const StyleMap style = resolvedStyle(cell);

    painter.save();

    // Vertical swimlanes rotate their title by -90 degrees so that it runs
    // along the header instead of wrapping inside its 23 pixel width.
    const bool verticalSwimlane = ShapeRegistry::isContainerShape(cell)
        && cell.styleFlag(QStringLiteral("swimlane"))
        && !Style::flag(style, QStringLiteral("horizontal"), true);
    QRectF rect = labelRect(cell, bounds);

    if (verticalSwimlane)
    {
        const double startSize = Style::number(style, QStringLiteral("startSize"), 23.0);
        const double spacing = Style::number(style, QStringLiteral("spacing"), 2.0);
        const double width = qMax(1.0, bounds.height() - 2.0 * spacing);
        const double height = qMax(1.0, startSize - 2.0 * spacing);

        painter.translate(startSize / 2.0, bounds.height() / 2.0);
        painter.rotate(-90.0);
        // The text block of a rotated label sits two pixels off the label
        // centre in draw.io (the label image is not centred on the line box).
        rect = QRectF(-width / 2.0 + 2.0, -height / 2.0, width, height);
    }

    if (rect.width() <= 0 || rect.height() <= 0)
    {
        painter.restore();
        return;
    }

    if (drawBackground)
    {
        const QString background = Style::value(style, QStringLiteral("labelBackgroundColor"));

        if (!background.isEmpty() && !Style::isNoneColor(background)
            && background.compare(QLatin1String("default"), Qt::CaseInsensitive) != 0)
        {
            painter.fillRect(rect, QColor(background));
        }
    }

    const QFont font = fontForLabel(cell);
    painter.setPen(textColor(cell));

    const bool wrap = Style::value(style, QStringLiteral("whiteSpace")) == QLatin1String("wrap")
        || cell.isHtmlLabel();
    const double maxWidth = wrap ? rect.width() : 100000.0;
    const Qt::Alignment baseAlignment = alignmentFor(cell) & Qt::AlignHorizontal_Mask;

    QVector<TextParagraph> paragraphs;

    if (cell.isHtmlLabel())
    {
        QString buffer;
        QFont runFont = font;
        QColor runColor;
        QVector<QPair<QFont, QColor>> inlineStack;
        BlockStyle block;
        block.font = font;
        block.align = baseAlignment;
        bool blockPending = true;
        bool paragraphStarted = false;

        const auto beginParagraph = [&]()
        {
            TextParagraph paragraph;
            paragraph.align = block.align;
            paragraph.font = block.font;
            paragraph.marginTop = blockPending ? block.marginTop : 0.0;
            paragraph.marginBottom = blockPending ? block.marginBottom : 0.0;
            blockPending = false;
            paragraphs.append(paragraph);
            paragraphStarted = true;
        };

        const auto flushRun = [&]()
        {
            if (buffer.isEmpty())
            {
                return;
            }

            if (!paragraphStarted)
            {
                beginParagraph();
            }

            TextRun run;
            run.text = decodeHtmlEntities(buffer);
            run.font = runFont;
            run.color = runColor;
            buffer.clear();
            paragraphs.last().runs.append(run);
        };

        // A block boundary closes the current paragraph; the next run starts a
        // new one with the block's font and margins.
        const auto flushParagraph = [&](bool force)
        {
            flushRun();

            if (force && !paragraphStarted)
            {
                beginParagraph();
            }

            paragraphStarted = false;
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
                const bool closing = lower.startsWith(QLatin1Char('/'));
                const QString name = blockTagName(closing ? lower.mid(1) : lower);

                if (lower.startsWith(QLatin1String("br")))
                {
                    flushRun();

                    if (!paragraphStarted)
                    {
                        beginParagraph();
                    }

                    // A line break ends the current line but stays in the
                    // same paragraph: store it as an explicit empty run.
                    paragraphs.last().runs.append(
                        TextRun{QStringLiteral("\n"), runFont, runColor});
                }
                else if (name == QLatin1String("p") || name == QLatin1String("div")
                         || (name.size() == 2 && name.startsWith(QLatin1Char('h'))
                             && name.at(1).isDigit()))
                {
                    if (closing)
                    {
                        flushParagraph(false);
                        block = BlockStyle();
                        block.font = font;
                        block.align = baseAlignment;
                        runFont = font;
                        blockPending = true;
                    }
                    else
                    {
                        flushParagraph(false);
                        block = blockStyleFor(tag, font, baseAlignment, true);
                        runFont = block.font;
                        blockPending = true;
                    }
                }
                else if (name == QLatin1String("b") || name == QLatin1String("strong")
                         || name == QLatin1String("i") || name == QLatin1String("em")
                         || name == QLatin1String("u"))
                {
                    flushRun();

                    // draw.io writes colours on the weight tags too
                    // (<b style="color: ...">), so these nest like spans.
                    if (closing)
                    {
                        if (!inlineStack.isEmpty())
                        {
                            const auto state = inlineStack.takeLast();
                            runFont = state.first;
                            runColor = state.second;
                        }
                    }
                    else
                    {
                        inlineStack.append({runFont, runColor});

                        if (name == QLatin1String("b") || name == QLatin1String("strong"))
                        {
                            runFont.setBold(true);
                        }
                        else if (name == QLatin1String("i") || name == QLatin1String("em"))
                        {
                            runFont.setItalic(true);
                        }
                        else
                        {
                            runFont.setUnderline(true);
                        }

                        const QColor color = inlineSpanColor(tag);

                        if (color.isValid())
                        {
                            runColor = color;
                        }
                    }
                }
                else if (name == QLatin1String("span") || name == QLatin1String("font"))
                {
                    flushRun();

                    // Inline elements nest (a coloured span can contain a
                    // normal-weight span with its own size), so the outer
                    // format has to be restored when the inner one closes.
                    if (closing)
                    {
                        if (!inlineStack.isEmpty())
                        {
                            const auto state = inlineStack.takeLast();
                            runFont = state.first;
                            runColor = state.second;
                        }
                        else
                        {
                            runFont = block.font;
                            runColor = QColor();
                        }
                    }
                    else
                    {
                        inlineStack.append({runFont, runColor});

                        QFont nextFont = inlineSpanFont(tag, runFont);

                        // A span without its own colour inherits the outer one.
                        const QColor color = inlineSpanColor(tag);

                        runFont = nextFont;

                        if (color.isValid())
                        {
                            runColor = color;
                        }
                    }
                }

                i = end + 1;
            }
            else
            {
                buffer += html.at(i);
                ++i;
            }
        }

        flushParagraph(false);

        if (paragraphs.isEmpty())
        {
            TextParagraph paragraph;
            paragraph.align = baseAlignment;
            paragraph.font = font;
            paragraph.runs.append(TextRun{cell.plainLabel(), font, QColor()});
            paragraphs.append(paragraph);
        }
    }
    else
    {
        const QStringList parts = cell.value.split(QLatin1Char('\n'));

        for (const QString &part : parts)
        {
            TextParagraph paragraph;
            paragraph.align = baseAlignment;
            paragraph.font = font;
            paragraph.runs.append(TextRun{part, font, QColor()});
            paragraphs.append(paragraph);
        }
    }

    struct Glyph
    {
        QChar character;
        QFont font;
        QColor color;
    };

    struct RenderLine
    {
        QVector<Glyph> glyphs;
        QFont font;
        double lineHeight = 0.0;
        Qt::Alignment align = Qt::AlignLeft;
    };

    struct LaidOutParagraph
    {
        QVector<RenderLine> lines;
        double marginTop = 0.0;
        double marginBottom = 0.0;
    };

    QVector<LaidOutParagraph> layout;

    for (const TextParagraph &paragraph : paragraphs)
    {
        LaidOutParagraph laid;
        laid.marginTop = paragraph.marginTop;
        laid.marginBottom = paragraph.marginBottom;

        const double lineHeight = qMax(1.0, paragraph.font.pixelSize() * 1.2);
        QVector<Glyph> current;
        double currentWidth = 0.0;

        const auto emitLine = [&](QVector<Glyph> glyphs)
        {
            RenderLine line;
            line.glyphs = glyphs;
            line.font = paragraph.font;
            line.lineHeight = lineHeight;
            line.align = paragraph.align;

            for (const Glyph &glyph : glyphs)
            {
                line.lineHeight = qMax(line.lineHeight, glyph.font.pixelSize() * 1.2);
            }

            laid.lines.append(line);
        };

        // Measures glyphs run by run so that kerning matches the painted text.
        const auto measureGlyphs = [](const QVector<Glyph> &glyphs)
        {
            double width = 0.0;

            for (int i = 0; i < glyphs.size();)
            {
                const QFont runFont = glyphs.at(i).font;
                const QColor runColor = glyphs.at(i).color;
                QString text;

                while (i < glyphs.size() && glyphs.at(i).font == runFont
                       && glyphs.at(i).color == runColor)
                {
                    text += glyphs.at(i).character;
                    ++i;
                }

                width += QFontMetricsF(runFont).horizontalAdvance(text);
            }

            return width;
        };

        for (const TextRun &run : paragraph.runs)
        {
            if (run.text == QLatin1String("\n"))
            {
                emitLine(current);
                current.clear();
                currentWidth = 0.0;
                continue;
            }

            const QFontMetricsF metrics(run.font);
            QString runOnLine;

            for (const QChar character : run.text)
            {
                const QString candidate = runOnLine + character;
                const double advance = metrics.horizontalAdvance(candidate)
                    - metrics.horizontalAdvance(runOnLine);
                // Half a pixel of tolerance absorbs the rounding difference
                // between per-character advances and the browser's text width.
                const bool exceeds = !current.isEmpty()
                    && currentWidth + advance > maxWidth + 0.5;

                if (exceeds)
                {
                    int lastSpace = -1;

                    for (int i = 0; i < current.size(); ++i)
                    {
                        if (current.at(i).character == QLatin1Char(' '))
                        {
                            lastSpace = i;
                        }
                    }

                    if (isClosingPunctuation(character) && current.size() > 1)
                    {
                        const Glyph moved = current.last();
                        current.removeLast();
                        emitLine(current);
                        current.clear();
                        current.append(moved);
                    }
                    else if (lastSpace > 0 && !character.isSpace())
                    {
                        QVector<Glyph> next = current.mid(lastSpace + 1);
                        current.resize(lastSpace);
                        emitLine(current);
                        current = next;
                    }
                    else
                    {
                        emitLine(current);
                        current.clear();
                    }

                    runOnLine.clear();
                    currentWidth = measureGlyphs(current);
                }

                current.append(Glyph{character, run.font, run.color});
                runOnLine = candidate;
                currentWidth += advance;
            }
        }

        emitLine(current);

        if (laid.lines.size() == 1 && !cell.isHtmlLabel())
        {
            laid.lines.first().align = baseAlignment;
        }

        layout.append(laid);
    }

    if (layout.isEmpty())
    {
        painter.restore();
        return;
    }

    // Vertical layout: block margins collapse like in CSS.
    double totalHeight = 0.0;
    double previousBottom = 0.0;

    for (const LaidOutParagraph &paragraph : layout)
    {
        totalHeight += qMax(previousBottom, paragraph.marginTop);

        for (const RenderLine &line : paragraph.lines)
        {
            totalHeight += line.lineHeight;
        }

        previousBottom = paragraph.marginBottom;
    }

    const Qt::Alignment vertical = alignmentFor(cell) & Qt::AlignVertical_Mask;
    double y = rect.top() + 5.0; // mxText.baseSpacingTop

    if (vertical & Qt::AlignVCenter)
    {
        y = rect.center().y() - totalHeight / 2.0;
    }
    else if (vertical & Qt::AlignBottom)
    {
        y = rect.bottom() - 1.0 - totalHeight; // mxText.baseSpacingBottom
    }

    if (!ShapeRegistry::isTextShape(cell))
    {
        painter.setClipRect(rect.adjusted(-1, -1, 1, 1));
    }

    const Qt::TextFlag clipFlag = ShapeRegistry::isTextShape(cell)
        ? Qt::TextDontClip : Qt::TextFlag(0);
    Q_UNUSED(clipFlag);

    previousBottom = 0.0;

    for (const LaidOutParagraph &paragraph : layout)
    {
        y += qMax(previousBottom, paragraph.marginTop);
        previousBottom = paragraph.marginBottom;

        for (const RenderLine &line : paragraph.lines)
        {
            // Measure the line so that alignment can position the runs. Runs
            // are measured as whole strings so that kerning matches draw.io.
            double lineWidth = 0.0;

            for (int i = 0; i < line.glyphs.size();)
            {
                const QFont runFont = line.glyphs.at(i).font;
                QString text;

                while (i < line.glyphs.size() && line.glyphs.at(i).font == runFont)
                {
                    text += line.glyphs.at(i).character;
                    ++i;
                }

                lineWidth += QFontMetricsF(runFont).horizontalAdvance(text);
            }

            double x = rect.left();

            if (line.align & Qt::AlignHCenter)
            {
                x = rect.center().x() - lineWidth / 2.0;
            }
            else if (line.align & Qt::AlignRight)
            {
                x = rect.right() - lineWidth;
            }

            int index = 0;

            while (index < line.glyphs.size())
            {
                const QFont runFont = line.glyphs.at(index).font;
                const QColor runColor = line.glyphs.at(index).color;
                QString text;

                while (index < line.glyphs.size()
                       && line.glyphs.at(index).font == runFont
                       && line.glyphs.at(index).color == runColor)
                {
                    text += line.glyphs.at(index).character;
                    ++index;
                }

                const QFontMetricsF metrics(runFont);
                // Qt's font metrics carry more leading than the browser
                // metrics draw.io lays labels out with; shift the glyphs up by
                // one pixel per 12 pixels of font size so baselines match.
                const double baselineShift = runFont.pixelSize() / 12.0;
                const double baseline = y + (line.lineHeight - metrics.height()) / 2.0
                    + metrics.ascent() - baselineShift;

                painter.setFont(runFont);

                if (runColor.isValid())
                {
                    painter.setPen(runColor);
                }

                painter.drawText(QPointF(x, baseline), text);

                if (runColor.isValid())
                {
                    painter.setPen(textColor(cell));
                }
                x += metrics.horizontalAdvance(text);
            }

            y += line.lineHeight;
        }
    }

    painter.restore();
}

} // namespace LabelPainter

namespace {

QPainterPath roughOutline(const QPainterPath &path, double amount, uint seed)
{
    QPainterPath result;

    for (const QPolygonF &polygon : path.toFillPolygons())
    {
        if (polygon.size() < 2)
        {
            continue;
        }

        for (int i = 0; i < polygon.size(); ++i)
        {
            const QPointF point = polygon.at(i);
            const double nx = (static_cast<int>((seed + i * 2654435761u) % 2001) / 1000.0) - 1.0;
            const double ny = (static_cast<int>((seed * 3 + i * 40503u) % 2001) / 1000.0) - 1.0;
            const QPointF jittered = point + QPointF(nx * amount, ny * amount);

            if (i == 0)
            {
                result.moveTo(jittered);
            }
            else
            {
                result.lineTo(jittered);
            }
        }

        result.closeSubpath();
    }

    return result;
}

} // namespace

namespace ShapeRenderer {

QColor fillColor(const Cell &cell)
{
    const StyleMap style = Style::withDefaults(cell.style);
    const QString value = Style::value(style, QStringLiteral("fillColor"));

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
    const StyleMap style = Style::withDefaults(cell.style);
    const QString value = Style::value(style, QStringLiteral("strokeColor"));

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

namespace {

/// draw.io snaps shapes with an odd effective stroke width to half pixels
/// (mxImageExport.getSvgShapeOffset). The offset is applied to the shape and
/// its decorations, not to the label.
double crispOffsetFor(QPainter &painter, const Cell &cell)
{
    const StyleMap style = Style::withDefaults(cell.style);
    const double strokeWidth = Style::number(style, QStringLiteral("strokeWidth"), 1.0);

    if (strokeWidth <= 0.0 || !cell.hasStroke())
    {
        return 0.0;
    }

    const QTransform transform = painter.transform();
    const double scale = std::hypot(transform.m11(), transform.m12());
    const int deviceWidth = qMax(1, qRound(strokeWidth * scale));

    return (deviceWidth % 2 == 1) ? 0.5 : 0.0;
}

void paintLabel(QPainter &painter, const Cell &cell, const QRectF &local, double offset)
{
    painter.save();

    if (offset > 0.0)
    {
        painter.translate(-offset, -offset);
    }

    LabelPainter::paint(painter, cell, local);
    painter.restore();
}

} // namespace

void paint(QPainter &painter, const Cell &cell, const QPointF *absolutePosition)
{
    // draw.io groups are invisible containers: their children carry the
    // visible geometry. Painting a group produced a stray black outline.
    if (cell.isGroup() || cell.styleFlag(QStringLiteral("group")))
    {
        return;
    }

    painter.save();

    const QPointF position = absolutePosition != nullptr ? *absolutePosition
                                                         : cell.absolutePosition();
    const QRectF local(QPointF(0, 0), cell.geometry.size());
    const QPointF center = position + local.center();
    const QString direction = cell.styleValue(QStringLiteral("direction")).toLower();

    // mxShape.isPaintBoundsInverted(): north/south swap the paint box so the
    // rotated shape fills the cell instead of sticking out sideways.
    QRectF paintRect = local;

    if (direction == QLatin1String("north") || direction == QLatin1String("south"))
    {
        const double t = (local.width() - local.height()) / 2.0;
        paintRect = QRectF(t, -t, local.height(), local.width());
    }

    double rotation = cell.rotation;

    if (qFuzzyIsNull(rotation))
    {
        rotation = cell.styleNumber(QStringLiteral("rotation"), 0.0);
    }

    // mxShape.getShapeRotation(): the direction style adds a quarter turn.

    if (direction == QLatin1String("north"))
    {
        rotation += 270.0;
    }
    else if (direction == QLatin1String("west"))
    {
        rotation += 180.0;
    }
    else if (direction == QLatin1String("south"))
    {
        rotation += 90.0;
    }

    const bool flipH = cell.styleFlag(QStringLiteral("flipH"));
    const bool flipV = cell.styleFlag(QStringLiteral("flipV"));

    if (!qFuzzyIsNull(rotation) || flipH || flipV)
    {
        painter.translate(center);
        painter.rotate(rotation);

        if (flipH || flipV)
        {
            painter.scale(flipH ? -1.0 : 1.0, flipV ? -1.0 : 1.0);
        }

        painter.translate(-center);
    }

    painter.translate(position);

    const double crispOffset = crispOffsetFor(painter, cell);

    if (crispOffset > 0.0)
    {
        painter.translate(crispOffset, crispOffset);
    }

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

        paintLabel(painter, cell, local, crispOffset);
        painter.restore();
        return;
    }

    const QPainterPath path = ShapeRegistry::pathForStyle(cell.style, paintRect);
    QColor fill = fillColor(cell);
    const QColor stroke = strokeColor(cell);
    const QString cellShape = cell.styleValue(QStringLiteral("shape")).toLower();

    if (cellShape == QLatin1String("waypoint"))
    {
        fill = stroke;
    }

    if (ShapeRegistry::canonicalKey(cell.style) == QLatin1String("flowchart.parallel_mode"))
    {
        // flowchart.xml paints the diamonds with
        // <fillcolor color="accentColor" default="#ffff00"/>.
        fill = Style::color(cell.style, QStringLiteral("accentColor"),
                            QColor(0xff, 0xff, 0x00));
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

        paintLabel(painter, cell, local, crispOffset);
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
                const QString direction = cell.styleValue(QStringLiteral("gradientDirection"),
                                                          QStringLiteral("south"));
                QPointF from = local.topLeft();
                QPointF to = local.bottomLeft();

                if (direction == QLatin1String("north"))
                {
                    from = local.bottomLeft();
                    to = local.topLeft();
                }
                else if (direction == QLatin1String("east"))
                {
                    from = local.topLeft();
                    to = local.topRight();
                }
                else if (direction == QLatin1String("west"))
                {
                    from = local.topRight();
                    to = local.topLeft();
                }

                QLinearGradient gradient(from, to);
                gradient.setColorAt(0.0, fill);
                gradient.setColorAt(1.0, Style::parseColorValue(gradientColor));
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

        if (cell.styleFlag(QStringLiteral("multiple")))
        {
            const int count = qBound(1, static_cast<int>(
                cell.styleNumber(QStringLiteral("multipleCount"), 2.0)), 16);
            const double dx = cell.styleNumber(QStringLiteral("multipleDx"), 8.0);
            const double dy = cell.styleNumber(QStringLiteral("multipleDy"), -8.0);

            for (int copy = count; copy >= 1; --copy)
            {
                painter.save();
                painter.translate(copy * dx, copy * dy);
                painter.drawPath(path);
                painter.restore();
            }
        }

        const bool sketch = cell.styleFlag(QStringLiteral("sketch"));
        const bool comic = cell.styleFlag(QStringLiteral("comic"));
        const QPen outlinePen = painter.pen();

        if (sketch || comic)
        {
            painter.setPen(Qt::NoPen);
        }

        painter.drawPath(path);

        if (sketch || comic)
        {
            painter.setPen(outlinePen);
            const QPainterPath rough = roughOutline(
                path, comic ? 3.0 : 1.6,
                qHash(cell.id) ^ static_cast<uint>(local.width() * 13 + local.height()));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(rough);
        }

        if (cell.styleFlag(QStringLiteral("glass")) && fill.alpha() > 0)
        {
            painter.save();
            painter.setClipPath(path);
            QLinearGradient glass(local.topLeft(),
                                  QPointF(local.left(), local.top() + local.height() * 0.6));
            glass.setColorAt(0.0, QColor(255, 255, 255, 230));
            glass.setColorAt(1.0, QColor(255, 255, 255, 25));
            painter.setPen(Qt::NoPen);
            painter.setBrush(glass);
            painter.drawRect(local);
            painter.restore();
        }

        // Decorations that draw.io renders on top of the base outline.
        const QString key = ShapeRegistry::canonicalKey(cell.style);

        if (key == QLatin1String("process"))
        {
            // ProcessShape.paintForeground: the two inset lines are derived
            // from size (and pushed inwards by the corner radius when the
            // body is rounded), then snapped to whole pixels like draw.io.
            const bool fixed = cell.styleFlag(QStringLiteral("fixedSize"));
            double inset = cell.styleNumber(QStringLiteral("size"), 0.1);

            if (fixed)
            {
                inset = qBound(0.0, inset, local.width());
            }
            else
            {
                inset = local.width() * qBound(0.0, inset, 1.0);

                if (cell.styleFlag(QStringLiteral("rounded")))
                {
                    const double f = qBound(0.0,
                        cell.styleNumber(QStringLiteral("arcSize"), 15.0), 100.0) / 100.0;
                    inset = qMax(inset, qMin(local.width() * f, local.height() * f));
                }
            }

            inset = qRound(inset);
            painter.drawLine(QPointF(inset, 0), QPointF(inset, local.height()));
            painter.drawLine(QPointF(local.width() - inset, 0),
                             QPointF(local.width() - inset, local.height()));
        }
        else if (key == QLatin1String("cylinder3") || key == QLatin1String("cylinder"))
        {
            // draw.io strokes the front half of the top lid after the body so the
            // ellipse reads as a closed lid; filling it would punch a hole.
            if (cell.styleFlag(QStringLiteral("lid"), true))
            {
                const double size = qBound(0.0,
                    cell.styleNumber(QStringLiteral("size"), 15.0), local.height() * 0.5);

                if (size > 0.0)
                {
                    QPainterPath lid;
                    lid.arcMoveTo(QRectF(0, 0, local.width(), 2 * size), 0);
                    lid.arcTo(QRectF(0, 0, local.width(), 2 * size), 0, -180);
                    painter.setBrush(Qt::NoBrush);
                    painter.drawPath(lid);
                }
            }
        }
        else if (key == QLatin1String("note"))
        {
            // NoteShape.paintVertexShape: the folded corner is filled with
            // darkOpacity and then stroked.
            const double size = qBound(0.0,
                cell.styleNumber(QStringLiteral("size"), 30.0),
                qMin(local.width(), local.height()));
            const double op = qBound(-1.0,
                cell.styleNumber(QStringLiteral("darkOpacity"), 0.0), 1.0);

            if (op != 0.0)
            {
                QPolygonF corner;
                corner << QPointF(local.width() - size, 0)
                       << QPointF(local.width() - size, size)
                       << QPointF(local.width(), size);
                QColor color = op < 0 ? QColor(255, 255, 255) : QColor(0, 0, 0);
                color.setAlphaF(qAbs(op));
                painter.save();
                painter.setPen(Qt::NoPen);
                painter.setBrush(color);
                painter.drawPolygon(corner);
                painter.restore();
            }

            painter.drawLine(QPointF(local.width() - size, 0),
                             QPointF(local.width() - size, size));
            painter.drawLine(QPointF(local.width() - size, size),
                             QPointF(local.width(), size));
        }
        else if (key == QLatin1String("message"))
        {
            // MessageShape foreground: the envelope fold.
            painter.drawLine(QPointF(0, 0),
                             QPointF(local.width() / 2.0, local.height() / 2.0));
            painter.drawLine(QPointF(local.width(), 0),
                             QPointF(local.width() / 2.0, local.height() / 2.0));
        }
        else if (key == QLatin1String("smiley"))
        {
            // Mouth and eyes use their own stroke widths in the stencil.
            const double scale = local.height() / 98.0;
            const auto px = [&](double v) { return local.width() * (v / 98.0); };
            const auto py = [&](double v) { return local.height() * (v / 98.0); };

            painter.save();
            QPen pen = painter.pen();
            pen.setWidthF(qMax(1.0, 2.0 * scale));
            pen.setCapStyle(Qt::RoundCap);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);

            QPainterPath mouth;
            mouth.moveTo(px(11), py(54));
            ShapeRegistry::appendSvgArc(mouth, QPointF(px(11), py(54)), QPointF(px(87), py(54)),
                                 local.width() * 38.0 / 98.0, local.height() * 27.0 / 98.0,
                                 true, false);
            painter.drawPath(mouth);

            pen.setWidthF(qMax(1.0, 6.0 * scale));
            painter.setPen(pen);
            painter.drawEllipse(QRectF(px(24), py(27),
                                       local.width() * 6.0 / 98.0,
                                       local.height() * 16.0 / 98.0));
            painter.drawEllipse(QRectF(px(68), py(27),
                                       local.width() * 6.0 / 98.0,
                                       local.height() * 16.0 / 98.0));
            painter.restore();
        }
        else if (key == QLatin1String("isoCube"))
        {
            const double angle = qBound(0.01,
                cell.styleNumber(QStringLiteral("isoAngle"), 15.0), 94.0);
            const double isoH = qMin(local.width() * std::tan(angle * M_PI / 200.0),
                                     local.height() * 0.5);

            painter.drawLine(QPointF(0, isoH), QPointF(local.width() * 0.5, 2 * isoH));
            painter.drawLine(QPointF(local.width() * 0.5, 2 * isoH),
                             QPointF(local.width(), isoH));
            painter.drawLine(QPointF(local.width() * 0.5, 2 * isoH),
                             QPointF(local.width() * 0.5, local.height()));
        }
        else if (key == QLatin1String("cube"))
        {
            // Interior edges are stroked separately: keeping them inside the
            // filled path would close them into triangles and punch holes.
            const double depth = qBound(0.0, cell.styleNumber(QStringLiteral("size"), 20.0),
                                        qMin(local.width(), local.height()));
            const double opTop = qBound(-1.0, cell.styleNumber(QStringLiteral("darkOpacity"), 0.0), 1.0);
            const double opSide = qBound(-1.0, cell.styleNumber(QStringLiteral("darkOpacity2"), 0.0), 1.0);
            const QColor topColor = opTop < 0 ? QColor(255, 255, 255) : QColor(0, 0, 0);
            const QColor sideColor = opSide < 0 ? QColor(255, 255, 255) : QColor(0, 0, 0);

            painter.save();
            painter.setPen(Qt::NoPen);

            if (opTop != 0.0)
            {
                QPolygonF top;
                top << QPointF(0, 0) << QPointF(local.width() - depth, 0)
                    << QPointF(local.width(), depth) << QPointF(depth, depth);
                QColor color = topColor;
                color.setAlphaF(qAbs(opTop));
                painter.setBrush(color);
                painter.drawPolygon(top);
            }

            if (opSide != 0.0)
            {
                QPolygonF side;
                side << QPointF(0, 0) << QPointF(depth, depth)
                     << QPointF(depth, local.height()) << QPointF(0, local.height() - depth);
                QColor color = sideColor;
                color.setAlphaF(qAbs(opSide));
                painter.setBrush(color);
                painter.drawPolygon(side);
            }

            painter.restore();

            painter.drawLine(QPointF(depth, local.height()), QPointF(depth, depth));
            painter.drawLine(QPointF(depth, depth), QPointF(0, 0));
            painter.drawLine(QPointF(depth, depth), QPointF(local.width(), depth));
        }
        else if (key == QLatin1String("banner"))
        {
            // Fold lines from the banner stencil's foreground path.
            const double px = local.width() / 96.0;
            const double py = local.height() / 50.0;

            QPainterPath folds;
            folds.moveTo(40.5 * px, 47.5 * py);
            folds.quadTo(38 * px, 47.5 * py, 38 * px, 45 * py);
            folds.lineTo(28 * px, 45 * py);
            folds.quadTo(28 * px, 40 * py, 30.5 * px, 40 * py);
            folds.lineTo(65.5 * px, 40 * py);
            folds.quadTo(68 * px, 40 * py, 68 * px, 45 * py);
            folds.lineTo(58 * px, 45 * py);
            folds.quadTo(55.5 * px, 45 * py, 55.5 * px, 47.5 * py);
            folds.moveTo(25.5 * px, 42.5 * py);
            folds.lineTo(25.5 * px, 10 * py);
            folds.moveTo(70.5 * px, 42.5 * py);
            folds.lineTo(70.5 * px, 10 * py);

            painter.setBrush(Qt::NoBrush);
            painter.drawPath(folds);
        }
        else if (key == QLatin1String("swimlane") || cell.styleFlag(QStringLiteral("swimlane")))
        {
            const double startSize = cell.styleNumber(QStringLiteral("startSize"), 23.0);

            // Fold indicator: draw.io shows a small square with a minus (or a
            // plus once collapsed) at the top left of collapsible containers.
            if (cell.styleFlag(QStringLiteral("collapsible"), true)
                && local.width() > 20.0 && local.height() > 20.0)
            {
                const QRectF fold(4.0, 4.0, 9.0, 9.0);
                const QColor fill = fillColor(cell);
                painter.setPen(QPen(strokeColor(cell), 1.0));
                painter.setBrush(fill.alpha() == 0 ? QColor(Qt::white) : fill);
                painter.drawRect(fold);
                painter.setBrush(Qt::NoBrush);
                painter.drawLine(QPointF(fold.left() + 2.0, fold.center().y()),
                                 QPointF(fold.right() - 2.0, fold.center().y()));

                if (cell.collapsed)
                {
                    painter.drawLine(QPointF(fold.center().x(), fold.top() + 2.0),
                                     QPointF(fold.center().x(), fold.bottom() - 2.0));
                }
            }

            // swimlane;horizontal=0 puts the title bar on the left edge.
            if (cell.styleFlag(QStringLiteral("horizontal"), true))
            {
                painter.drawLine(QPointF(0, startSize), QPointF(local.width(), startSize));
            }
            else
            {
                painter.drawLine(QPointF(startSize, 0), QPointF(startSize, local.height()));
            }
        }
        else if (key == QLatin1String("flowchart.annotation_2"))
        {
            // Annotation 2: divider between the label and the note text.
            const double px = local.width() / 100.0;
            const double py = local.height() / 98.0;

            painter.drawLine(QPointF(0, 49 * py), QPointF(50 * px, 49 * py));
        }
        else if (key == QLatin1String("flowchart.database"))
        {
            // Database: front half of the top lid (sweep-flag 0).
            const double sx = local.width() / 60.0;
            const double sy = local.height() / 60.0;
            QPainterPath lid;
            lid.moveTo(0, 10 * sy);
            ShapeRegistry::appendSvgArc(lid, QPointF(0, 10 * sy), QPointF(60 * sx, 10 * sy),
                                        30 * sx, 10 * sy, false, false);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(lid);
        }
        else if (key == QLatin1String("flowchart.direct_data"))
        {
            // Direct Data: the right hand side is a full ellipse.
            const double sx = local.width() / 98.0;
            const double sy = local.height() / 60.0;
            QPainterPath side;
            side.moveTo(89 * sx, 0);
            ShapeRegistry::appendSvgArc(side, QPointF(89 * sx, 0), QPointF(89 * sx, 60 * sy),
                                        9 * sx, 30 * sy, true, false);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(side);
        }
        else if (key == QLatin1String("flowchart.internal_storage"))
        {
            // Internal Storage stencil: cross lines at 15 units.
            const double px = local.width() / 70.0;
            const double py = local.height() / 70.0;

            painter.drawLine(QPointF(0, 15 * py), QPointF(70 * px, 15 * py));
            painter.drawLine(QPointF(15 * px, 0), QPointF(15 * px, 70 * py));
        }
        else if (key == QLatin1String("flowchart.multi-document"))
        {
            // Multi-Document: two extra pages, filled like the front page.
            const double sx = local.width() / 88.0;
            const double sy = local.height() / 60.28;

            const auto page = [&](double dx, double dy)
            {
                QPainterPath pagePath;
                pagePath.moveTo(dx + 5 * sx, dy + 10 * sy);
                ShapeRegistry::appendSvgArc(pagePath, QPointF(dx + 5 * sx, dy + 10 * sy),
                                            QPointF(dx + 10 * sx, dy + 5 * sy),
                                            5 * sx, 5 * sy, false, true);
                pagePath.lineTo(dx + 78 * sx, dy + 5 * sy);
                ShapeRegistry::appendSvgArc(pagePath, QPointF(dx + 78 * sx, dy + 5 * sy),
                                            QPointF(dx + 83 * sx, dy + 10 * sy),
                                            5 * sx, 5 * sy, false, true);
                pagePath.lineTo(dx + 83 * sx, dy + 50 * sy);
                ShapeRegistry::appendSvgArc(pagePath, QPointF(dx + 83 * sx, dy + 50 * sy),
                                            QPointF(dx + 44 * sx, dy + 50 * sy),
                                            50 * sx, 50 * sy, false, false);
                ShapeRegistry::appendSvgArc(pagePath, QPointF(dx + 44 * sx, dy + 50 * sy),
                                            QPointF(dx + 5 * sx, dy + 50 * sy),
                                            50 * sx, 50 * sy, false, true);
                pagePath.closeSubpath();
                return pagePath;
            };

            painter.setBrush(fill);
            painter.drawPath(page(0, 0));
            painter.drawPath(page(-5 * sx, 5 * sy));
            painter.setBrush(Qt::NoBrush);
        }
        else if (key == QLatin1String("flowchart.or"))
        {
            // Or: the two diagonals of the ellipse.
            const double px = local.width() / 70.0;
            const double py = local.height() / 70.0;

            painter.drawLine(QPointF(10 * px, 60 * py), QPointF(60 * px, 10 * py));
            painter.drawLine(QPointF(10 * px, 10 * py), QPointF(60 * px, 60 * py));
        }
        else if (key == QLatin1String("flowchart.or_2"))
        {
            // Or 2: a plus inside the ellipse.
            const double px = local.width() / 70.0;
            const double py = local.height() / 70.0;

            painter.drawLine(QPointF(0, 35 * py), QPointF(70 * px, 35 * py));
            painter.drawLine(QPointF(35 * px, 0), QPointF(35 * px, 70 * py));
        }
        else if (key == QLatin1String("flowchart.parallel_mode"))
        {
            // Parallel Mode: the accent filled diamonds are the fill path,
            // the two horizontal bars are stroked on top.
            const double px = local.width() / 94.0;
            const double py = local.height() / 40.0;

            painter.drawLine(QPointF(0, 0), QPointF(94 * px, 0));
            painter.drawLine(QPointF(0, 40 * py), QPointF(94 * px, 40 * py));
        }
        else if (key == QLatin1String("flowchart.predefined_process"))
        {
            // Predefined Process: the two vertical bars.
            const double px = local.width() / 98.0;
            const double py = local.height() / 60.0;

            painter.drawLine(QPointF(14 * px, 0), QPointF(14 * px, 60 * py));
            painter.drawLine(QPointF(84 * px, 0), QPointF(84 * px, 60 * py));
        }
        else if (key == QLatin1String("flowchart.sequential_data"))
        {
            // Sequential Data: the flat bottom edge of the circle.
            const double px = local.width() / 99.0;
            const double py = local.height() / 99.0;

            painter.drawLine(QPointF(49.5 * px, 99 * py), QPointF(99 * px, 99 * py));
        }
        else if (key == QLatin1String("flowchart.sort"))
        {
            // Sort: the horizontal divider.
            const double px = local.width() / 98.0;
            const double py = local.height() / 98.0;

            painter.drawLine(QPointF(0, 49 * py), QPointF(98 * px, 49 * py));
        }
        else if (key == QLatin1String("flowchart.summing_function"))
        {
            // Summing Function: a plus inside the circle.
            const double px = local.width() / 70.0;
            const double py = local.height() / 70.0;

            painter.drawLine(QPointF(0, 35 * py), QPointF(70 * px, 35 * py));
            painter.drawLine(QPointF(35 * px, 0), QPointF(35 * px, 70 * py));
        }
        else if (key == QLatin1String("flowchart.summing_junction"))
        {
            // Summing Junction: the two diagonals of the circle.
            const double px = local.width() / 70.0;
            const double py = local.height() / 70.0;

            painter.drawLine(QPointF(10 * px, 60 * py), QPointF(60 * px, 10 * py));
            painter.drawLine(QPointF(10 * px, 10 * py), QPointF(60 * px, 60 * py));
        }
        else if (key == QLatin1String("internalStorage"))
        {
            // InternalStorageShape.paintForeground: cross lines at dx/dy,
            // pushed inwards by the corner radius when the body is rounded.
            double inset = 0.0;

            if (cell.styleFlag(QStringLiteral("rounded")))
            {
                const double f = qBound(0.0,
                    cell.styleNumber(QStringLiteral("arcSize"), 15.0), 100.0) / 100.0;
                inset = qMin(local.width() * f, local.height() * f);
            }

            const double dx = qMax(inset, qMin(local.width(),
                cell.styleNumber(QStringLiteral("dx"), 20.0)));
            const double dy = qMax(inset, qMin(local.height(),
                cell.styleNumber(QStringLiteral("dy"), 20.0)));

            painter.drawLine(QPointF(0, dy), QPointF(local.width(), dy));
            painter.drawLine(QPointF(dx, 0), QPointF(dx, local.height()));
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

    paintLabel(painter, cell, local, crispOffset);
    painter.restore();
}

} // namespace ShapeRenderer
} // namespace dq
