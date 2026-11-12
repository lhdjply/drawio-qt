#include "render/ShapeCatalog.h"

#include "app/I18n.h"

#include "render/ShapeRenderer.h"

#include <QDirIterator>
#include <QFile>
#include <QImageReader>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>

namespace dq {
namespace {

constexpr const char *kLibraryRoot = ":/drawio/img/lib";

QString prettifyName(const QString &raw)
{
    QString text = raw;
    static const QRegularExpression sizeSuffix(
        QStringLiteral("_(?:16|24|32|48|64|96|128|256)x(?:16|24|32|48|64|96|128|256)$"),
        QRegularExpression::CaseInsensitiveOption);
    text.remove(sizeSuffix);
    text.replace(QLatin1Char('_'), QLatin1Char(' '));
    text.replace(QLatin1Char('-'), QLatin1Char(' '));
    text = text.simplified();

    const QStringList words = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList result;

    for (const QString &word : words)
    {
        // Keep short all-caps tokens such as AWS, SQL or VPN untouched.
        if (word.size() <= 4 && word == word.toUpper() && word.at(0).isLetter())
        {
            result.append(word);
            continue;
        }

        QString formatted = word.toLower();
        formatted[0] = formatted.at(0).toUpper();
        result.append(formatted);
    }

    return result.join(QLatin1Char(' '));
}

QString translatedName(const QString &raw)
{
    const QString english = prettifyName(raw);

    if (I18n::isEnglish())
    {
        return english;
    }

    const QString phrase = I18n::imageText(QStringLiteral("phrase"), english.toLower(), QString());

    if (!phrase.isEmpty())
    {
        return phrase;
    }

    QStringList translated;

    for (const QString &token : english.split(QLatin1Char(' '), Qt::SkipEmptyParts))
    {
        translated.append(I18n::imageText(QStringLiteral("word"), token.toLower(), token));
    }

    QString result = translated.join(QLatin1Char(' '));
    static const QRegularExpression cjkGap(
        QStringLiteral("(?<=[\\x{4e00}-\\x{9fff}])\\s+(?=[\\x{4e00}-\\x{9fff}])"));
    result.remove(cjkGap);
    return result.isEmpty() ? english : result;
}

QString topLevelName(const QString &dir)
{
    const QString english = prettifyName(dir);

    if (I18n::isEnglish())
    {
        return english;
    }

    return I18n::imageText(QStringLiteral("dir"), dir, english);
}

QString subLevelName(const QString &dir)
{
    const QString english = prettifyName(dir);

    if (I18n::isEnglish())
    {
        return english;
    }

    return I18n::imageText(QStringLiteral("dir"), dir, english);
}

QSizeF fitIconSize(const QSizeF &size)
{
    if (!size.isValid() || size.isEmpty())
    {
        return QSizeF(64, 64);
    }

    const double maxSide = qMax(size.width(), size.height());
    const double scale = 64.0 / maxSide;

    return QSizeF(qMax(16.0, size.width() * scale),
                  qMax(16.0, size.height() * scale));
}

QSizeF readImageSize(const QString &resourcePath)
{
    if (resourcePath.endsWith(QLatin1String(".png"), Qt::CaseInsensitive))
    {
        QImageReader reader(resourcePath);
        const QSize size = reader.size();
        return size.isValid() ? fitIconSize(QSizeF(size)) : QSizeF(64, 64);
    }

    QFile file(resourcePath);

    if (!file.open(QIODevice::ReadOnly))
    {
        return QSizeF(64, 64);
    }

    const QByteArray head = file.read(2048);

    static const QRegularExpression widthPattern(
        QStringLiteral("\\bwidth\\s*=\\s*[\"']([0-9.]+)"));
    static const QRegularExpression heightPattern(
        QStringLiteral("\\bheight\\s*=\\s*[\"']([0-9.]+)"));
    static const QRegularExpression viewBoxPattern(
        QStringLiteral("\\bviewBox\\s*=\\s*[\"']\\s*[-0-9.]+\\s+[-0-9.]+\\s+([0-9.]+)\\s+([0-9.]+)"));

    const QString text = QString::fromLatin1(head);
    const QRegularExpressionMatch widthMatch = widthPattern.match(text);
    const QRegularExpressionMatch heightMatch = heightPattern.match(text);

    if (widthMatch.hasMatch() && heightMatch.hasMatch())
    {
        return fitIconSize(QSizeF(widthMatch.captured(1).toDouble(),
                                 heightMatch.captured(1).toDouble()));
    }

    const QRegularExpressionMatch viewBoxMatch = viewBoxPattern.match(text);

    if (viewBoxMatch.hasMatch())
    {
        return fitIconSize(QSizeF(viewBoxMatch.captured(1).toDouble(),
                                 viewBoxMatch.captured(2).toDouble()));
    }

    return QSizeF(64, 64);
}

bool hasVisiblePixels(const QPixmap &pixmap)
{
    if (pixmap.isNull())
    {
        return false;
    }

    const QImage image = pixmap.toImage();

    for (int y = 0; y < image.height(); y += 2)
    {
        for (int x = 0; x < image.width(); x += 2)
        {
            if (image.pixelColor(x, y).alpha() > 8)
            {
                return true;
            }
        }
    }

    return false;
}

} // namespace

ShapeCatalog &ShapeCatalog::instance()
{
    static ShapeCatalog catalog;
    return catalog;
}

ShapeCatalog::ShapeCatalog()
{
    load();
}

void ShapeCatalog::reload()
{
    m_categories.clear();
    m_index.clear();
    m_iconCache.clear();
    load();
}

void ShapeCatalog::load()
{
    loadBuiltins();
    loadImages();
    m_loaded = true;
}

void ShapeCatalog::loadBuiltins()
{
    QHash<QString, ShapeCategory> byId;

    for (const ShapeDefinition &definition : ShapeRegistry::toolboxShapes())
    {
        ShapeCategory &category = byId[definition.category];
        category.id = definition.category;
        category.name = builtinCategoryName(definition.category);
        category.imageLibrary = false;
        category.shapes.append(definition);
        m_index.insert(definition.key, definition);
    }

    // Stable, user friendly ordering of the standard libraries.
    const QStringList order = {QStringLiteral("basic"), QStringLiteral("arrows"),
                               QStringLiteral("misc"), QStringLiteral("advanced"),
                               QStringLiteral("flowchart"), QStringLiteral("uml"),
                               QStringLiteral("er"), QStringLiteral("bpmn"),
                               QStringLiteral("cloud"), QStringLiteral("network"),
                               QStringLiteral("electrical"), QStringLiteral("floorplan"),
                               QStringLiteral("infographic"), QStringLiteral("sysml"),
                               QStringLiteral("archimate"), QStringLiteral("eip"),
                               QStringLiteral("bootstrap"), QStringLiteral("kubernetes"),
                               QStringLiteral("mockup"), QStringLiteral("c4"),
                               QStringLiteral("dfd"), QStringLiteral("software")};

    for (const QString &id : order)
    {
        if (byId.contains(id))
        {
            m_categories.append(byId.take(id));
        }
    }

    for (ShapeCategory &remaining : byId)
    {
        m_categories.append(remaining);
    }
}

void ShapeCatalog::loadImages()
{
    const QStringList topOrder = {QStringLiteral("clip_art"), QStringLiteral("active_directory"),
                                  QStringLiteral("atlassian"), QStringLiteral("azure2"),
                                  QStringLiteral("allied_telesis"), QStringLiteral("cumulus"),
                                  QStringLiteral("dynamics365"), QStringLiteral("ibm"),
                                  QStringLiteral("mscae"), QStringLiteral("sap")};

    QHash<QString, ShapeCategory> byId;

    QDirIterator iterator(QString::fromLatin1(kLibraryRoot), QDir::Files,
                          QDirIterator::Subdirectories);

    while (iterator.hasNext())
    {
        const QString resourcePath = iterator.next();
        const QString suffix = resourcePath.section(QLatin1Char('.'), -1).toLower();

        if (suffix != QLatin1String("svg") && suffix != QLatin1String("png"))
        {
            continue;
        }

        // :/drawio/img/lib/<relative path>
        const QString relative = resourcePath.mid(QString::fromLatin1(kLibraryRoot).size() + 1);
        const QString directory = relative.section(QLatin1Char('/'), 0, -2);
        const QString fileName = relative.section(QLatin1Char('/'), -1);

        ShapeDefinition definition;
        definition.key = QStringLiteral("img/lib/") + relative;
        definition.imagePath = definition.key;
        definition.name = translatedName(fileName.section(QLatin1Char('.'), 0, -2));
        definition.size = readImageSize(resourcePath);
        definition.style = QStringLiteral(
            "shape=image;verticalLabelPosition=bottom;verticalAlign=top;html=1;"
            "image=%1;imageAspect=1;strokeColor=none;fillColor=none;")
            .arg(definition.key);

        QString categoryId = QStringLiteral("img/") + directory;
        QString categoryName;

        if (directory.contains(QLatin1Char('/')))
        {
            const QString top = directory.section(QLatin1Char('/'), 0, 0);
            const QString sub = directory.section(QLatin1Char('/'), 1);
            categoryName = topLevelName(top) + QStringLiteral(" · ") + subLevelName(sub);
        }
        else
        {
            categoryName = topLevelName(directory);
        }

        definition.category = categoryId;
        byId[categoryId].id = categoryId;
        byId[categoryId].name = categoryName;
        byId[categoryId].imageLibrary = true;
        byId[categoryId].shapes.append(definition);
        m_index.insert(definition.key, definition);
    }

    for (ShapeCategory &category : byId)
    {
        std::sort(category.shapes.begin(), category.shapes.end(),
                  [](const ShapeDefinition &a, const ShapeDefinition &b)
        {
            return a.name.localeAwareCompare(b.name) < 0;
        });
    }

    // Order the library by the same sequence draw.io uses in its library
    // dialog, then alphabetically for anything unknown.
    for (const QString &top : topOrder)
    {
        QStringList groupIds;

        for (auto it = byId.constBegin(); it != byId.constEnd(); ++it)
        {
            const QString id = it.key();
            const QString withoutPrefix = id.mid(QStringLiteral("img/").size());

            if (withoutPrefix == top || withoutPrefix.startsWith(top + QLatin1Char('/')))
            {
                groupIds.append(id);
            }
        }

        std::sort(groupIds.begin(), groupIds.end());

        for (const QString &id : groupIds)
        {
            if (byId.contains(id))
            {
                m_categories.append(byId.take(id));
            }
        }
    }

    for (ShapeCategory &remaining : byId)
    {
        m_categories.append(remaining);
    }
}

const QVector<ShapeCategory> &ShapeCatalog::categories() const
{
    return m_categories;
}

QVector<ShapeCategory> ShapeCatalog::builtinCategories() const
{
    QVector<ShapeCategory> result;

    for (const ShapeCategory &category : m_categories)
    {
        if (!category.imageLibrary)
        {
            result.append(category);
        }
    }

    return result;
}

QVector<ShapeCategory> ShapeCatalog::imageCategories() const
{
    QVector<ShapeCategory> result;

    for (const ShapeCategory &category : m_categories)
    {
        if (category.imageLibrary)
        {
            result.append(category);
        }
    }

    return result;
}

QVector<ShapeDefinition> ShapeCatalog::allDefinitions() const
{
    QVector<ShapeDefinition> result;

    for (const ShapeCategory &category : m_categories)
    {
        result.append(category.shapes);
    }

    return result;
}

QVector<ShapeDefinition> ShapeCatalog::search(const QString &query, int limit) const
{
    const QString needle = query.trimmed().toLower();

    if (needle.isEmpty())
    {
        return {};
    }

    QVector<ShapeDefinition> result;

    for (const ShapeCategory &category : m_categories)
    {
        for (const ShapeDefinition &definition : category.shapes)
        {
            // Match English names, keys, categories and the translated names so
            // both "star" and "星" find the same shapes.
            const QString zhName = I18n::textFor(I18n::Language::Chinese, definition.key);
            const QString enName = I18n::textFor(I18n::Language::English, definition.key);
            const QString zhCategory = I18n::textFor(I18n::Language::Chinese, category.id);
            const QString enCategory = I18n::textFor(I18n::Language::English, category.id);

            if (definition.name.toLower().contains(needle)
                || definition.key.toLower().contains(needle)
                || category.name.toLower().contains(needle)
                || zhName.toLower().contains(needle)
                || enName.toLower().contains(needle)
                || zhCategory.toLower().contains(needle)
                || enCategory.toLower().contains(needle))
            {
                result.append(definition);

                if (result.size() >= limit)
                {
                    return result;
                }
            }
        }
    }

    return result;
}

ShapeDefinition ShapeCatalog::definition(const QString &key) const
{
    return m_index.value(key, ShapeDefinition{});
}

QString ShapeCatalog::builtinCategoryName(const QString &id)
{
    return I18n::categoryName(id, id);
}

QIcon ShapeCatalog::iconFor(const ShapeDefinition &definition, int size) const
{
    const QString cacheKey = definition.key + QLatin1Char('#') + QString::number(size);

    if (m_iconCache.contains(cacheKey))
    {
        return m_iconCache.value(cacheKey);
    }

    QIcon icon;

    if (!definition.imagePath.isEmpty())
    {
        icon = QIcon(QStringLiteral(":/drawio/") + definition.imagePath);
    }

    if (icon.isNull())
    {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        Cell cell(Cell::Kind::Vertex, QStringLiteral("preview"));
        cell.style = Style::parse(definition.style);

        // Palette previews use the same 2px outline as newly inserted shapes.
        if (!cell.style.contains(QStringLiteral("strokeWidth")))
        {
            cell.style.insert(QStringLiteral("strokeWidth"), QStringLiteral("2"));
        }

        const QSizeF target = definition.size.isEmpty() ? QSizeF(80, 50) : definition.size;
        const double scale = (size - 8.0) / qMax(target.width(), target.height());
        cell.geometry = QRectF(0, 0, target.width(), target.height());

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.translate((size - target.width() * scale) / 2.0,
                          (size - target.height() * scale) / 2.0);
        painter.scale(scale, scale);
        painter.translate(-cell.absolutePosition());
        ShapeRenderer::paint(painter, cell);
        painter.end();

        if (!hasVisiblePixels(pixmap))
        {
            // Text-only and image placeholders have no geometry of their own:
            // draw a neutral glyph so the palette remains usable.
            pixmap.fill(Qt::transparent);
            QPainter fallback(&pixmap);
            fallback.setRenderHint(QPainter::Antialiasing, true);
            fallback.setPen(QPen(QColor(0x9A, 0xA0, 0xA6), 1.4));
            fallback.setBrush(QColor(0xF5, 0xF6, 0xF7));
            fallback.drawRoundedRect(QRectF(3, 3, size - 6, size - 6), 4, 4);
            QFont font = fallback.font();
            font.setPixelSize(size / 2);
            fallback.setFont(font);
            fallback.setPen(QColor(0x5F, 0x63, 0x68));
            fallback.drawText(QRectF(0, 0, size, size), Qt::AlignCenter,
                              definition.name.left(1));
            fallback.end();
        }

        icon = QIcon(pixmap);
    }

    m_iconCache.insert(cacheKey, icon);
    return icon;
}

} // namespace dq
