#include "core/MxFileIO.h"

#include "core/Compression.h"
#include "core/Document.h"
#include "core/Page.h"

#include <QDateTime>
#include <QDomDocument>
#include <QXmlStreamWriter>

#include <algorithm>

namespace dq {
namespace {

constexpr const char *kGraphModelTag = "mxGraphModel";

// --- writing -------------------------------------------------------------

void writePoint(QXmlStreamWriter &writer, const QPointF &point, const char *type)
{
    writer.writeStartElement(QStringLiteral("mxPoint"));
    writer.writeAttribute(QStringLiteral("x"), QString::number(point.x(), 'g', 12));
    writer.writeAttribute(QStringLiteral("y"), QString::number(point.y(), 'g', 12));

    if (type != nullptr)
    {
        writer.writeAttribute(QStringLiteral("as"), QString::fromLatin1(type));
    }

    writer.writeEndElement();
}

void writeCellGeometry(QXmlStreamWriter &writer, const Cell &cell)
{
    writer.writeStartElement(QStringLiteral("mxGeometry"));
    writer.writeAttribute(QStringLiteral("as"), QStringLiteral("geometry"));

    if (cell.isVertex())
    {
        writer.writeAttribute(QStringLiteral("x"), QString::number(cell.geometry.x(), 'g', 12));
        writer.writeAttribute(QStringLiteral("y"), QString::number(cell.geometry.y(), 'g', 12));
        writer.writeAttribute(QStringLiteral("width"), QString::number(cell.geometry.width(), 'g', 12));
        writer.writeAttribute(QStringLiteral("height"), QString::number(cell.geometry.height(), 'g', 12));
    }
    else
    {
        writer.writeAttribute(QStringLiteral("relative"), QStringLiteral("1"));

        if (cell.sourceId.isEmpty() && !cell.sourcePoint.isNull())
        {
            writePoint(writer, cell.sourcePoint, "sourcePoint");
        }

        if (cell.targetId.isEmpty() && !cell.targetPoint.isNull())
        {
            writePoint(writer, cell.targetPoint, "targetPoint");
        }

        if (cell.rotation != 0.0)
        {
            writer.writeAttribute(QStringLiteral("rotation"),
                                  QString::number(cell.rotation, 'g', 12));
        }

        if (!cell.waypoints.isEmpty())
        {
            writer.writeStartElement(QStringLiteral("Array"));
            writer.writeAttribute(QStringLiteral("as"), QStringLiteral("points"));

            for (const QPointF &point : cell.waypoints)
            {
                writePoint(writer, point, nullptr);
            }

            writer.writeEndElement();
        }
    }

    writer.writeEndElement();
}

void writeCell(QXmlStreamWriter &writer, const Cell &cell)
{
    writer.writeStartElement(QStringLiteral("mxCell"));

    if (!cell.id.isEmpty())
    {
        writer.writeAttribute(QStringLiteral("id"), cell.id);
    }

    if (!cell.value.isEmpty())
    {
        writer.writeAttribute(QStringLiteral("value"), cell.value);
    }

    if (!cell.style.isEmpty())
    {
        writer.writeAttribute(QStringLiteral("style"), Style::format(cell.style));
    }

    writer.writeAttribute(QStringLiteral("parent"),
                          cell.parentId.isEmpty() ? QStringLiteral("1") : cell.parentId);

    if (cell.isEdge())
    {
        writer.writeAttribute(QStringLiteral("edge"), QStringLiteral("1"));

        if (!cell.sourceId.isEmpty())
        {
            writer.writeAttribute(QStringLiteral("source"), cell.sourceId);
        }

        if (!cell.targetId.isEmpty())
        {
            writer.writeAttribute(QStringLiteral("target"), cell.targetId);
        }
    }
    else
    {
        writer.writeAttribute(QStringLiteral("vertex"), QStringLiteral("1"));
    }

    if (!cell.visible)
    {
        writer.writeAttribute(QStringLiteral("visible"), QStringLiteral("0"));
    }

    writeCellGeometry(writer, cell);
    writer.writeEndElement();
}

QString cellLayerId(const Page &page, const Cell *cell)
{
    QString parentId = cell != nullptr ? cell->parentId : QString();

    while (!parentId.isEmpty())
    {
        if (page.layer(parentId) != nullptr)
        {
            return parentId;
        }

        const Cell *parent = page.cell(parentId);

        if (parent == nullptr)
        {
            break;
        }

        parentId = parent->parentId;
    }

    return {};
}

QString pageModelXml(const Page &page)
{
    QString model;
    QXmlStreamWriter writer(&model);
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(2);

    const Page::Settings &s = page.settings;

    writer.writeStartElement(QString::fromLatin1(kGraphModelTag));
    writer.writeAttribute(QStringLiteral("dx"), QString::number(s.dx, 'g', 12));
    writer.writeAttribute(QStringLiteral("dy"), QString::number(s.dy, 'g', 12));
    writer.writeAttribute(QStringLiteral("grid"), s.grid ? QStringLiteral("1") : QStringLiteral("0"));
    writer.writeAttribute(QStringLiteral("gridSize"), QString::number(s.gridSize));
    writer.writeAttribute(QStringLiteral("guides"), s.guides ? QStringLiteral("1") : QStringLiteral("0"));
    writer.writeAttribute(QStringLiteral("tooltips"), QStringLiteral("1"));
    writer.writeAttribute(QStringLiteral("connect"), QStringLiteral("1"));
    writer.writeAttribute(QStringLiteral("arrows"), QStringLiteral("1"));
    writer.writeAttribute(QStringLiteral("fold"), QStringLiteral("1"));
    writer.writeAttribute(QStringLiteral("page"), s.page ? QStringLiteral("1") : QStringLiteral("0"));
    writer.writeAttribute(QStringLiteral("pageScale"), QString::number(s.pageScale, 'g', 12));
    writer.writeAttribute(QStringLiteral("pageWidth"), QString::number(s.pageWidth, 'g', 12));
    writer.writeAttribute(QStringLiteral("pageHeight"), QString::number(s.pageHeight, 'g', 12));
    writer.writeAttribute(QStringLiteral("math"), QStringLiteral("0"));
    writer.writeAttribute(QStringLiteral("shadow"), s.shadow ? QStringLiteral("1") : QStringLiteral("0"));

    writer.writeStartElement(QStringLiteral("root"));
    writer.writeStartElement(QStringLiteral("mxCell"));
    writer.writeAttribute(QStringLiteral("id"), QStringLiteral("0"));
    writer.writeEndElement();

    // draw.io writes each layer followed by its own cells, so keep that
    // order instead of putting every layer first.
    const QVector<Cell *> cells = page.cells();
    QSet<QString> written;

    for (const Layer &layer : page.layers)
    {
        writer.writeStartElement(QStringLiteral("mxCell"));
        writer.writeAttribute(QStringLiteral("id"), layer.id);

        if (layer.id != QLatin1String("1"))
        {
            writer.writeAttribute(QStringLiteral("value"), layer.name);
        }

        writer.writeAttribute(QStringLiteral("parent"), QStringLiteral("0"));

        if (!layer.visible || layer.locked)
        {
            QStringList flags;

            if (!layer.visible)
            {
                flags << QStringLiteral("visible=0");
            }

            if (layer.locked)
            {
                flags << QStringLiteral("locked=1");
            }

            writer.writeAttribute(QStringLiteral("style"),
                                  flags.join(QLatin1Char(';')) + QLatin1Char(';'));
        }

        writer.writeEndElement();

        for (Cell *cell : cells)
        {
            if (cellLayerId(page, cell) == layer.id)
            {
                writeCell(writer, *cell);
                written.insert(cell->id);
            }
        }
    }

    for (Cell *cell : cells)
    {
        if (!written.contains(cell->id))
        {
            writeCell(writer, *cell);
        }
    }

    writer.writeEndElement(); // root
    writer.writeEndElement(); // mxGraphModel

    return model.trimmed();
}

// --- reading -------------------------------------------------------------

/// Qt 6.8 deprecated the bool/error-pointer overload of QDomDocument::setContent;
/// use the ParseResult overload there and stay compatible with Qt 6.5.
bool parseXml(QDomDocument &document, const QString &xml, QString *error)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    const QDomDocument::ParseResult result = document.setContent(xml);

    if (!result)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("XML 解析失败（第 %1 行第 %2 列）：%3")
                         .arg(result.errorLine)
                         .arg(result.errorColumn)
                         .arg(result.errorMessage);
        }

        return false;
    }

    return true;
#else
    QString parseError;
    int line = 0;
    int column = 0;

    if (!document.setContent(xml, &parseError, &line, &column))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("XML 解析失败（第 %1 行第 %2 列）：%3")
                         .arg(line).arg(column).arg(parseError);
        }

        return false;
    }

    return true;
#endif
}

QPointF readPoint(const QDomElement &element)
{
    return QPointF(element.attribute(QStringLiteral("x")).toDouble(),
                   element.attribute(QStringLiteral("y")).toDouble());
}

bool readGeometry(Cell *cell, const QDomElement &geometry)
{
    if (cell->isVertex())
    {
        cell->geometry = QRectF(geometry.attribute(QStringLiteral("x")).toDouble(),
                                geometry.attribute(QStringLiteral("y")).toDouble(),
                                geometry.attribute(QStringLiteral("width")).toDouble(),
                                geometry.attribute(QStringLiteral("height")).toDouble());

        if (geometry.hasAttribute(QStringLiteral("rotation")))
        {
            cell->rotation = geometry.attribute(QStringLiteral("rotation")).toDouble();
        }

        return true;
    }

    for (QDomElement child = geometry.firstChildElement(); !child.isNull();
         child = child.nextSiblingElement())
    {
        if (child.tagName() == QLatin1String("mxPoint"))
        {
            const QString as = child.attribute(QStringLiteral("as"));

            if (as == QLatin1String("sourcePoint"))
            {
                cell->sourcePoint = readPoint(child);
            }
            else if (as == QLatin1String("targetPoint"))
            {
                cell->targetPoint = readPoint(child);
            }
            else if (as.isEmpty())
            {
                cell->waypoints.append(readPoint(child));
            }
        }
        else if (child.tagName() == QLatin1String("Array"))
        {
            for (QDomElement point = child.firstChildElement(); !point.isNull();
                 point = point.nextSiblingElement())
            {
                if (point.tagName() == QLatin1String("mxPoint"))
                {
                    cell->waypoints.append(readPoint(point));
                }
            }
        }
    }

    return true;
}

bool readGraphModel(Page &page, const QDomElement &model, QString *error)
{
    Page::Settings &s = page.settings;
    s.grid = MxFileIO::xmlBool(model.attribute(QStringLiteral("grid")), true);
    s.gridSize = model.attribute(QStringLiteral("gridSize")).toInt() > 0
        ? model.attribute(QStringLiteral("gridSize")).toInt() : 10;
    s.guides = MxFileIO::xmlBool(model.attribute(QStringLiteral("guides")), true);
    s.page = MxFileIO::xmlBool(model.attribute(QStringLiteral("page")), true);
    s.pageScale = model.attribute(QStringLiteral("pageScale")).toDouble() > 0
        ? model.attribute(QStringLiteral("pageScale")).toDouble() : 1.0;
    s.pageWidth = model.attribute(QStringLiteral("pageWidth")).toDouble() > 0
        ? model.attribute(QStringLiteral("pageWidth")).toDouble() : 827.0;
    s.pageHeight = model.attribute(QStringLiteral("pageHeight")).toDouble() > 0
        ? model.attribute(QStringLiteral("pageHeight")).toDouble() : 1169.0;
    s.shadow = MxFileIO::xmlBool(model.attribute(QStringLiteral("shadow")), false);
    s.dx = model.attribute(QStringLiteral("dx")).toDouble();
    s.dy = model.attribute(QStringLiteral("dy")).toDouble();

    const QDomElement root = model.firstChildElement(QStringLiteral("root"));

    if (root.isNull())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("mxGraphModel 缺少 root 元素");
        }
        return false;
    }

    QVector<Layer> parsedLayers;

    for (QDomElement element = root.firstChildElement(); !element.isNull();
         element = element.nextSiblingElement())
    {
        if (element.tagName() != QLatin1String("mxCell"))
        {
            continue;
        }

        const QString id = element.attribute(QStringLiteral("id"));

        if (id.isEmpty())
        {
            continue;
        }

        const QString parentId = element.attribute(QStringLiteral("parent"));
        const bool isVertex = element.attribute(QStringLiteral("vertex")) == QLatin1String("1");
        const bool isEdge = element.attribute(QStringLiteral("edge")) == QLatin1String("1");

        if (id == QLatin1String("0"))
        {
            continue;
        }

        if (!isVertex && !isEdge && parentId == QLatin1String("0"))
        {
            if (id != QLatin1String("0"))
            {
                Layer layer;
                layer.id = id;
                layer.name = element.attribute(QStringLiteral("value"));

                if (layer.name.isEmpty())
                {
                    layer.name = QStringLiteral("图层 %1").arg(parsedLayers.size() + 1);
                }

                const StyleMap layerStyle =
                    Style::parse(element.attribute(QStringLiteral("style")));
                layer.visible = Style::flag(layerStyle, QStringLiteral("visible"), true);
                layer.locked = Style::flag(layerStyle, QStringLiteral("locked"), false);
                parsedLayers.append(layer);
            }

            continue;
        }

        auto cell = std::make_unique<Cell>(isEdge ? Cell::Kind::Edge : Cell::Kind::Vertex, id);
        cell->value = element.attribute(QStringLiteral("value"));
        cell->style = Style::parse(element.attribute(QStringLiteral("style")));
        cell->parentId = parentId.isEmpty() ? QStringLiteral("1") : parentId;
        cell->visible = MxFileIO::xmlBool(element.attribute(QStringLiteral("visible")), true);
        cell->sourceId = element.attribute(QStringLiteral("source"));
        cell->targetId = element.attribute(QStringLiteral("target"));

        const QDomElement geometry =
            element.firstChildElement(QStringLiteral("mxGeometry"));

        if (!geometry.isNull())
        {
            readGeometry(cell.get(), geometry);
        }

        page.addCell(std::move(cell));
    }

    if (!parsedLayers.isEmpty())
    {
        page.layers = parsedLayers;
    }

    if (page.layers.isEmpty())
    {
        page.layers.append(Layer{});
    }

    page.activeLayerId = page.layers.first().id;
    page.reindex();
    return true;
}

bool parseModelText(Page &page, const QString &content, QString *error)
{
    QString xml = content.trimmed();

    if (!xml.startsWith(QLatin1Char('<')))
    {
        bool ok = false;
        xml = Compression::decompressDiagram(xml, &ok);

        if (!ok)
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("无法解压缩页面内容");
            }
            return false;
        }
    }

    QDomDocument document;
    QString parseError;

    if (!parseXml(document, xml, &parseError))
    {
        if (error != nullptr)
        {
            *error = parseError;
        }

        return false;
    }

    const QDomElement root = document.documentElement();

    if (root.tagName() != QLatin1String(kGraphModelTag))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("缺少 mxGraphModel 元素");
        }
        return false;
    }

    page.clear();
    return readGraphModel(page, root, error);
}

} // namespace

bool MxFileIO::xmlBool(const QString &value, bool fallback)
{
    if (value.isEmpty())
    {
        return fallback;
    }

    const QString normalized = value.trimmed().toLower();

    if (normalized == QLatin1String("1") || normalized == QLatin1String("true"))
    {
        return true;
    }

    if (normalized == QLatin1String("0") || normalized == QLatin1String("false"))
    {
        return false;
    }

    return fallback;
}

QString MxFileIO::cellToXml(const Cell &cell)
{
    QString output;
    QXmlStreamWriter writer(&output);
    writer.setAutoFormatting(false);
    writeCell(writer, cell);
    return output;
}

QString MxFileIO::pageToXml(const Page &page, bool compressed)
{
    const QString model = pageModelXml(page);

    if (!compressed)
    {
        return model;
    }

    return Compression::compressDiagram(model);
}

bool MxFileIO::pageFromXml(Page &page, const QString &xml, QString *error)
{
    return parseModelText(page, xml, error);
}

QString MxFileIO::documentToXml(const Document &document, bool compressPages)
{
    if (!compressPages)
    {
        QString xml = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                     "<mxfile host=\"drawio-qt\" modified=\"")
            + QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
            + QStringLiteral("\" type=\"device\">\n");

        for (const Page *page : document.pages())
        {
            xml += QStringLiteral("  <diagram id=\"")
                + page->id.toHtmlEscaped()
                + QStringLiteral("\" name=\"")
                + page->name.toHtmlEscaped()
                + QStringLiteral("\">\n");
            xml += pageToXml(*page, false);
            xml += QStringLiteral("\n  </diagram>\n");
        }

        xml += QStringLiteral("</mxfile>\n");
        return xml;
    }

    QString output;
    QXmlStreamWriter writer(&output);
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(2);
    writer.writeStartDocument(QStringLiteral("1.0"));

    writer.writeStartElement(QStringLiteral("mxfile"));
    writer.writeAttribute(QStringLiteral("host"), QStringLiteral("drawio-qt"));
    writer.writeAttribute(QStringLiteral("modified"),
                          QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    writer.writeAttribute(QStringLiteral("type"), QStringLiteral("device"));

    for (const Page *page : document.pages())
    {
        writer.writeStartElement(QStringLiteral("diagram"));
        writer.writeAttribute(QStringLiteral("id"), page->id);
        writer.writeAttribute(QStringLiteral("name"), page->name);
        writer.writeCharacters(pageToXml(*page, true));
        writer.writeEndElement();
    }

    writer.writeEndElement(); // mxfile
    writer.writeEndDocument();

    return output;
}

bool MxFileIO::documentFromXml(Document &document, const QString &xml, QString *error)
{
    QDomDocument dom;
    QString parseError;

    if (!parseXml(dom, xml, &parseError))
    {
        if (error != nullptr)
        {
            *error = parseError;
        }

        return false;
    }

    const QDomElement root = dom.documentElement();

    document.clear();

    if (root.tagName() == QLatin1String(kGraphModelTag))
    {
        Page *page = document.addPage(QStringLiteral("第 1 页"));

        if (!parseModelText(*page, xml, error))
        {
            return false;
        }

        document.setCurrentIndex(0);
        document.setModified(false);
        return true;
    }

    if (root.tagName() != QLatin1String("mxfile"))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("不是有效的 draw.io 文件（根元素为 %1）").arg(root.tagName());
        }
        return false;
    }

    for (QDomElement diagram = root.firstChildElement(); !diagram.isNull();
         diagram = diagram.nextSiblingElement())
    {
        if (diagram.tagName() != QLatin1String("diagram"))
        {
            continue;
        }

        Page *page = document.addPage(diagram.attribute(QStringLiteral("name"),
                                                       QStringLiteral("第 %1 页")
                                                           .arg(document.pageCount() + 1)));

        const QString pageId = diagram.attribute(QStringLiteral("id"));

        if (!pageId.isEmpty())
        {
            page->id = pageId;
        }

        const QDomElement model = diagram.firstChildElement(QString::fromLatin1(kGraphModelTag));

        if (!model.isNull())
        {
            page->clear();

            if (!readGraphModel(*page, model, error))
            {
                return false;
            }
        }
        else
        {
            if (!parseModelText(*page, diagram.text(), error))
            {
                return false;
            }
        }
    }

    if (document.pageCount() == 0)
    {
        document.addPage(QStringLiteral("第 1 页"));
    }

    document.setCurrentIndex(0);
    document.setModified(false);
    return true;
}

} // namespace dq
