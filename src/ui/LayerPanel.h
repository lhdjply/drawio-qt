#pragma once

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QToolButton;

namespace dq {

class DiagramView;

/// Layer list for the current page. Each layer expands to show its objects;
/// supports add/remove/rename, reorder and visibility toggling.
class LayerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LayerPanel(QWidget *parent = nullptr);

    void setView(DiagramView *view);
    void refresh();
    void syncSelection();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QString currentLayerId() const;
    void addLayer();
    void removeLayer();
    void renameLayer();
    void moveLayer(int delta);
    void toggleLock();
    QString objectLabel(const QString &cellId) const;

    DiagramView *m_view = nullptr;
    QTreeWidget *m_tree = nullptr;
    QToolButton *m_add = nullptr;
    QToolButton *m_remove = nullptr;
    QToolButton *m_up = nullptr;
    QToolButton *m_down = nullptr;
    QToolButton *m_lock = nullptr;
    bool m_updating = false;
};

} // namespace dq
