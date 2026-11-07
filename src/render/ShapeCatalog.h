#pragma once

#include "render/ShapeRegistry.h"

#include <QHash>
#include <QIcon>
#include <QVector>

namespace dq {

struct ShapeCategory
{
    QString id;
    QString name;
    bool imageLibrary = false;
    QVector<ShapeDefinition> shapes;
};

/// Every shape the editor knows about: the procedural built-in library and the
/// bundled draw.io image libraries (Azure, Active Directory, IBM, SAP, ...).
/// The catalogue is the single place the toolbox, the insert menu and the
/// canvas use to resolve a shape key.
class ShapeCatalog
{
public:
    static ShapeCatalog &instance();

    const QVector<ShapeCategory> &categories() const;
    QVector<ShapeCategory> builtinCategories() const;
    QVector<ShapeCategory> imageCategories() const;

    QVector<ShapeDefinition> allDefinitions() const;
    QVector<ShapeDefinition> search(const QString &query, int limit = 240) const;
    ShapeDefinition definition(const QString &key) const;

    /// An icon for the toolbox: bundled artwork for image shapes, a rendered
    /// preview of the actual geometry for procedural shapes.
    QIcon iconFor(const ShapeDefinition &definition, int size = 48) const;

    static QString builtinCategoryName(const QString &id);

    void reload();

private:
    ShapeCatalog();
    void load();
    void loadBuiltins();
    void loadImages();

    QVector<ShapeCategory> m_categories;
    QHash<QString, ShapeDefinition> m_index;
    mutable QHash<QString, QIcon> m_iconCache;
    bool m_loaded = false;
};

} // namespace dq
