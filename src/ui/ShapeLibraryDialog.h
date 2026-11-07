#pragma once

#include <QDialog>

class QListWidget;

namespace dq {

/// Category chooser for the shape palette, mirroring draw.io's "Shape Library"
/// dialog: standard libraries plus the bundled software/artwork libraries.
class ShapeLibraryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShapeLibraryDialog(QWidget *parent = nullptr);

    QStringList enabledCategories() const;

    /// Categories enabled in the settings; empty means "all".
    static QStringList configuredCategories();
    static void setConfiguredCategories(const QStringList &ids);

private:
    void selectAll(bool selected);

    QListWidget *m_list = nullptr;
    QStringList m_allIds;
};

} // namespace dq
