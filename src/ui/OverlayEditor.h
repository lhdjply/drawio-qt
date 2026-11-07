#pragma once

#include <QTextEdit>

namespace dq {

/// A transparent text editor laid over a cell while its label is edited.
class OverlayEditor : public QTextEdit
{
    Q_OBJECT

public:
    explicit OverlayEditor(QWidget *parent = nullptr);

    void begin(const QRect &geometry, const QString &text, bool selectContent);

signals:
    void committed();
    void cancelled();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
};

} // namespace dq
