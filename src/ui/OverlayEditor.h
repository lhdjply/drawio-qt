#pragma once

#include <QTextDocument>
#include <QTextEdit>

namespace dq {

/// A transparent text editor laid over a cell while its label is edited.
class OverlayEditor : public QTextEdit
{
    Q_OBJECT

public:
    explicit OverlayEditor(QWidget *parent = nullptr);
    ~OverlayEditor() override;

    void begin(const QRect &geometry, const QString &text, bool selectContent);

    /// Starts editing an HTML label: the markup is shown with its formatting
    /// (bold headings, paragraphs) like draw.io's contenteditable label.
    void beginHtml(const QRect &geometry, const QString &html, bool selectContent,
                   double zoom = 1.0);

    bool isRichText() const { return m_rich; }

    /// Editor font size divided by the canvas zoom, so serialised sizes are in
    /// diagram pixels rather than zoomed screen pixels.
    void setZoom(double zoom) { m_zoom = zoom > 0.0 ? zoom : 1.0; }

    /// Serialises the editor's document back to draw.io's HTML dialect.
    static QString htmlFromDocument(const QTextDocument &document, double zoom = 1.0);

    /// True when the user changed text or formatting since beginHtml().
    bool contentChanged() const;

    /// Commits the edit when the user presses the mouse outside the label and
    /// outside the format panel (the panel keeps the label editable).
    bool eventFilter(QObject *watched, QEvent *event) override;

    /// Character formats that can be applied to the selected label text.
    enum class Format
    {
        Bold = 1,
        Italic = 2,
        Underline = 4
    };

    /// Applies (or removes) bold/italic/underline on the selection, or on the
    /// format used for the next typed characters when nothing is selected.
    void setFormat(Format format, bool enabled);

    /// Applies a font family / size / colour to the selection, or to the text
    /// typed next when there is no selection.
    void setFontFamily(const QString &family);
    void setFontSize(int pointSize);
    void setTextColor(const QColor &color);

    /// True when the selection (or typing format) currently uses the format.
    bool formatActive(Format format) const;

signals:
    void committed();
    void cancelled();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    bool m_rich = false;
    double m_zoom = 1.0;
    QString m_initialMarkup;
};

} // namespace dq
