#include "ui/OverlayEditor.h"

#include <QApplication>
#include <QKeyEvent>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextCursor>

namespace dq {

namespace {

/// True when \a widget is part of the format panel dock.
bool isFormatPanelWidget(const QWidget *widget)
{
    for (const QWidget *w = widget; w != nullptr; w = w->parentWidget())
    {
        if (w->objectName() == QLatin1String("formatPanel"))
        {
            return true;
        }
    }

    return false;
}

} // namespace

OverlayEditor::OverlayEditor(QWidget *parent)
    : QTextEdit(parent)
{
    setAcceptRichText(false);
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setStyleSheet(QStringLiteral(
        "QTextEdit { border: 1px solid #1a73e8; background: rgba(255,255,255,0.96);"
        " padding: 2px; selection-background-color: #1a73e8; }"));
    setTabChangesFocus(true);
    hide();
    // The editor may lose focus without a focusOutEvent (for example when the
    // format panel is used), so watch clicks globally to end the edit.
    qApp->installEventFilter(this);
}

void OverlayEditor::begin(const QRect &geometry, const QString &text, bool selectContent)
{
    m_rich = false;
    m_initialMarkup.clear();
    setAcceptRichText(false);
    setGeometry(geometry.adjusted(-2, -2, 2, 2));
    setPlainText(text);
    show();
    raise();
    setFocus(Qt::MouseFocusReason);

    if (selectContent)
    {
        QTextEdit::selectAll();
    }
    else
    {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::End);
        setTextCursor(cursor);
    }
}

void OverlayEditor::beginHtml(const QRect &geometry, const QString &html, bool selectContent,
                              double zoom)
{
    m_rich = true;
    setZoom(zoom);
    setAcceptRichText(true);
    setGeometry(geometry.adjusted(-2, -2, 2, 2));

    // Qt resolves the relative heading sizes against the default font, so the
    // zoomed label font has to become the document default.
    document()->setDefaultFont(font());
    document()->setHtml(html);
    m_initialMarkup = htmlFromDocument(*document(), m_zoom);
    show();
    raise();
    setFocus(Qt::MouseFocusReason);

    if (selectContent)
    {
        QTextEdit::selectAll();
    }
    else
    {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::End);
        setTextCursor(cursor);
    }
}

bool OverlayEditor::formatActive(Format format) const
{
    switch (format)
    {
    case Format::Bold:
        return fontWeight() >= QFont::Bold;
    case Format::Italic:
        return fontItalic();
    case Format::Underline:
        return fontUnderline();
    }

    return false;
}

void OverlayEditor::setFormat(Format format, bool enabled)
{
    QTextCharFormat charFormat;

    switch (format)
    {
    case Format::Bold:
        charFormat.setFontWeight(enabled ? QFont::Bold : QFont::Normal);
        break;
    case Format::Italic:
        charFormat.setFontItalic(enabled);
        break;
    case Format::Underline:
        charFormat.setFontUnderline(enabled);
        break;
    }

    QTextCursor cursor = textCursor();

    if (cursor.hasSelection())
    {
        cursor.mergeCharFormat(charFormat);
    }

    // Also updates the format used for text typed next.
    mergeCurrentCharFormat(charFormat);
}

void OverlayEditor::setFontFamily(const QString &family)
{
    if (family.isEmpty())
    {
        return;
    }

    QTextCharFormat charFormat;
    charFormat.setFontFamilies({family});

    QTextCursor cursor = textCursor();

    if (cursor.hasSelection())
    {
        cursor.mergeCharFormat(charFormat);
    }

    mergeCurrentCharFormat(charFormat);
}

void OverlayEditor::setFontSize(int pointSize)
{
    if (pointSize <= 0)
    {
        return;
    }

    // Qt keeps character sizes in points; the panel works in diagram pixels.
    QTextCharFormat charFormat;
    charFormat.setFontPointSize(qMax(1.0, pointSize * m_zoom / 1.3));

    QTextCursor cursor = textCursor();

    if (cursor.hasSelection())
    {
        cursor.mergeCharFormat(charFormat);
    }

    mergeCurrentCharFormat(charFormat);
}

void OverlayEditor::setTextColor(const QColor &color)
{
    if (!color.isValid())
    {
        return;
    }

    QTextCharFormat charFormat;
    charFormat.setForeground(color);

    QTextCursor cursor = textCursor();

    if (cursor.hasSelection())
    {
        cursor.mergeCharFormat(charFormat);
    }

    mergeCurrentCharFormat(charFormat);
}

/// Serialises an edited rich-text label back to draw.io's HTML dialect.
/// Qt keeps the block tag (h1..h6) and character weight of the imported
/// markup, so headings and bold runs survive a double click.
QString OverlayEditor::htmlFromDocument(const QTextDocument &document, double zoom)
{
    QString result;
    int blockCount = 0;

    for (QTextBlock block = document.begin(); block.isValid(); block = block.next())
    {
        ++blockCount;
    }

    if (zoom <= 0.0)
    {
        zoom = 1.0;
    }

    const double basePx = document.defaultFont().pixelSize() > 0
        ? document.defaultFont().pixelSize()
        : document.defaultFont().pointSizeF() * 1.3333;

    const auto plainRun = [](const QTextFragment &fragment)
    {
        const QTextCharFormat format = fragment.charFormat();

        return !format.fontItalic() && !format.fontUnderline()
            && format.fontWeight() < QFont::Bold
            && !format.hasProperty(QTextFormat::FontPixelSize)
            && !format.hasProperty(QTextFormat::FontPointSize);
    };

    for (QTextBlock block = document.begin(); block.isValid(); block = block.next())
    {
        const QTextBlockFormat blockFormat = block.blockFormat();
        const int heading = qBound(0, blockFormat.headingLevel(), 6);
        const QString tag = heading > 0 ? QStringLiteral("h%1").arg(heading)
                                        : QStringLiteral("div");

        QString style;

        if (heading > 0 || blockFormat.topMargin() > 0.0)
        {
            style += QStringLiteral("margin-top:%1px;").arg(qRound(blockFormat.topMargin()));
        }

        if (blockFormat.bottomMargin() > 0.0)
        {
            style += QStringLiteral("margin-bottom:%1px;")
                         .arg(qRound(blockFormat.bottomMargin()));
        }

        const Qt::Alignment alignment = blockFormat.alignment() & Qt::AlignHorizontal_Mask;

        if (alignment & Qt::AlignHCenter)
        {
            style += QStringLiteral("text-align:center;");
        }
        else if (alignment & Qt::AlignRight)
        {
            style += QStringLiteral("text-align:right;");
        }
        else if (alignment & Qt::AlignJustify)
        {
            style += QStringLiteral("text-align:justify;");
        }

        QString text;
        bool formatted = false;

        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it)
        {
            const QTextFragment fragment = it.fragment();

            if (!fragment.isValid())
            {
                continue;
            }

            QString run = fragment.text().toHtmlEscaped();
            run.replace(QChar::LineSeparator, QStringLiteral("<br>"));
            run.replace(QLatin1Char('\n'), QStringLiteral("<br>"));

            const QTextCharFormat format = fragment.charFormat();

            if (!plainRun(fragment))
            {
                formatted = true;
            }

            if (format.fontItalic())
            {
                run = QStringLiteral("<i>%1</i>").arg(run);
            }

            if (format.fontUnderline())
            {
                run = QStringLiteral("<u>%1</u>").arg(run);
            }

            // Headings are bold by default; wrapping them again is redundant,
            // but a heading run the user unbolded has to say so explicitly.
            if (format.fontWeight() >= QFont::Bold)
            {
                if (heading == 0)
                {
                    run = QStringLiteral("<b>%1</b>").arg(run);
                }
            }
            else if (heading > 0)
            {
                run = QStringLiteral("<span style=\"font-weight: normal;\">%1</span>")
                          .arg(run);
            }

            // Per-run font size is stored as a pixel size in the editor and as
            // diagram pixels in the markup.
            double pixelSize = 0.0;

            if (format.hasProperty(QTextFormat::FontPixelSize))
            {
                pixelSize = format.doubleProperty(QTextFormat::FontPixelSize);
            }
            else if (format.hasProperty(QTextFormat::FontPointSize))
            {
                pixelSize = format.fontPointSize() * 1.3;
            }

            const QColor color = format.foreground().color();

            if (format.hasProperty(QTextFormat::ForegroundBrush) && color.isValid()
                && color != QColor(Qt::black))
            {
                run = QStringLiteral("<span style=\"color: %1;\">%2</span>")
                          .arg(color.name(), run);
            }

            if (pixelSize > 0.0)
            {
                const int diagramSize = qMax(1, qRound(pixelSize / zoom));

                if (qAbs(diagramSize - basePx / zoom) > 0.5)
                {
                    run = QStringLiteral("<span style=\"font-size: %1px;\">%2</span>")
                              .arg(diagramSize).arg(run);
                }
            }

            text += run;
        }

        // A single plain paragraph stays plain text, exactly what draw.io's
        // contenteditable produces when a label is typed from scratch.
        if (blockCount == 1 && heading == 0 && style.isEmpty() && !formatted)
        {
            return text.isEmpty() ? QStringLiteral("<br>") : text;
        }

        if (text.isEmpty())
        {
            text = QStringLiteral("<br>");
        }

        result += QLatin1Char('<') + tag;

        if (!style.isEmpty())
        {
            result += QStringLiteral(" style=\"%1\"").arg(style);
        }

        result += QLatin1Char('>') + text + QStringLiteral("</") + tag + QLatin1Char('>');
    }

    return result;
}


void OverlayEditor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        emit cancelled();
        return;
    }

    if (m_rich && (event->modifiers() & Qt::ControlModifier) != 0)
    {
        if (event->matches(QKeySequence::Bold))
        {
            setFormat(Format::Bold, !formatActive(Format::Bold));
            event->accept();
            return;
        }

        if (event->matches(QKeySequence::Italic))
        {
            setFormat(Format::Italic, !formatActive(Format::Italic));
            event->accept();
            return;
        }

        if (event->matches(QKeySequence::Underline))
        {
            setFormat(Format::Underline, !formatActive(Format::Underline));
            event->accept();
            return;
        }
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        if (event->modifiers() & (Qt::ShiftModifier | Qt::AltModifier))
        {
            // Shift/Alt+Enter inserts a line break; plain Enter commits.
            QTextCursor cursor = textCursor();
            cursor.insertBlock();
            setTextCursor(cursor);
            return;
        }

        emit committed();
        return;
    }

    QTextEdit::keyPressEvent(event);
}

OverlayEditor::~OverlayEditor()
{
    qApp->removeEventFilter(this);
}

bool OverlayEditor::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress && isVisible())
    {
        QWidget *target = qobject_cast<QWidget *>(watched);

        if (target != nullptr && !isAncestorOf(target) && !isFormatPanelWidget(target))
        {
            emit committed();
        }
    }

    return QTextEdit::eventFilter(watched, event);
}

void OverlayEditor::focusOutEvent(QFocusEvent *event)
{
    QTextEdit::focusOutEvent(event);

    // Clicking the format panel must not end the edit: the user is formatting
    // the selected run, and committing would apply the change to the cell.
    if (isFormatPanelWidget(QApplication::focusWidget()))
    {
        return;
    }

    emit committed();
}

bool OverlayEditor::contentChanged() const
{
    return m_rich && htmlFromDocument(*document()) != m_initialMarkup;
}

} // namespace dq