#pragma once

#include "core/Cell.h"

#include <QHash>
#include <QRectF>
#include <QString>
#include <QVector>

#include <memory>
#include <vector>

namespace dq {

/// A draw.io layer. Every page has at least the default layer with id "1".
struct Layer
{
    QString id = QStringLiteral("1");
    QString name = QStringLiteral("Default Layer");
    bool visible = true;
    bool locked = false;
};

/// One `<diagram>` of an mxfile, i.e. one page of the document.
class Page
{
public:
    struct Settings
    {
        bool grid = true;
        int gridSize = 10;
        bool guides = true;
        bool page = true;
        double pageWidth = 827.0;  // A4 at 100 dpi, draw.io's default
        double pageHeight = 1169.0;
        double pageScale = 1.0;
        bool shadow = false;
        double dx = 0.0;
        double dy = 0.0;
    };

    explicit Page(const QString &name = QStringLiteral("Page 1"));
    ~Page();

    Page(const Page &) = delete;
    Page &operator=(const Page &) = delete;

    QString id;
    QString name;
    Settings settings;
    QVector<Layer> layers;

    // --- cell access -----------------------------------------------------
    const QVector<Cell *> &cells() const { return m_order; }
    Cell *cell(const QString &id) const { return m_index.value(id, nullptr); }
    QVector<Cell *> vertices() const;
    QVector<Cell *> edges() const;
    QVector<Cell *> topLevelCells() const { return childrenOf(QStringLiteral("1")); }
    QVector<Cell *> childrenOf(const QString &parentId) const;

    // --- editing ---------------------------------------------------------
    Cell *addVertex(const QRectF &rect, const StyleMap &style = {},
                    const QString &value = {}, const QString &parentId = {});
    Cell *addEdge(const QString &sourceId, const QString &targetId,
                  const StyleMap &style = {}, const QString &value = {});
    Cell *addCell(std::unique_ptr<Cell> cell);
    /// Removes a cell, its descendants and every edge attached to a removed
    /// vertex. Returns the number of removed cells.
    int removeCell(Cell *cell);
    void clear();
    void reindex();

    QString newId();

    int indexOf(const Cell *cell) const { return m_order.indexOf(const_cast<Cell *>(cell)); }
    /// Moves a cell inside the z-order list. Returns false for unknown cells.
    bool moveCell(Cell *cell, int newIndex);

    // --- geometry --------------------------------------------------------
    QRectF pageRect() const;
    QRectF contentBounds() const;
    QRectF boundsForCells(const QVector<Cell *> &cells) const;

    // --- layers ----------------------------------------------------------
    Layer *layer(const QString &id);
    const Layer *layer(const QString &id) const;
    QString activeLayerId = QStringLiteral("1");

private:
    Cell *addVertexInternal(const QRectF &rect, const StyleMap &style,
                            const QString &value, const QString &parentId);
    void collectRemoval(Cell *cell, QSet<QString> &removedIds, QVector<Cell *> &ordered);

    std::vector<std::unique_ptr<Cell>> m_storage;
    QVector<Cell *> m_order;
    QHash<QString, Cell *> m_index;
    qint64 m_idCounter = 1;
};

} // namespace dq
