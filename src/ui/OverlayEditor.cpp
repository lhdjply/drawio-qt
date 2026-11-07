#include "ui/OverlayEditor.h"

#include <QKeyEvent>
#include <QTextCursor>

namespace dq {

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
}

void OverlayEditor::begin(const QRect &geometry, const QString &text, bool selectContent)
{
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

void OverlayEditor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        emit cancelled();
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        if (event->modifiers() & Qt::ShiftModifier)
        {
            QTextEdit::keyPressEvent(event);
            return;
        }

        emit committed();
        return;
    }

    QTextEdit::keyPressEvent(event);
}

void OverlayEditor::focusOutEvent(QFocusEvent *event)
{
    QTextEdit::focusOutEvent(event);
    emit committed();
}

} // namespace dq
