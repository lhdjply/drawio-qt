#include "core/Document.h"

#include "core/Compression.h"
#include "core/FileMeta.h"
#include "core/MxFileIO.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace dq {

Document::Document(QObject *parent)
    : QObject(parent)
{
    newDocument();
}

Document::~Document()
{
    clear();
}

Page *Document::page(int index) const
{
    if (index < 0 || index >= m_pages.size())
    {
        return nullptr;
    }

    return m_pages.at(index);
}

Page *Document::currentPage() const
{
    return page(m_currentIndex);
}

void Document::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_pages.size() || index == m_currentIndex)
    {
        return;
    }

    m_currentIndex = index;
    emit currentPageChanged(m_currentIndex);
}

Page *Document::addPage(const QString &name)
{
    auto *page = new Page(name.isEmpty()
                              ? QStringLiteral("Page %1").arg(m_pages.size() + 1)
                              : name);
    m_pages.append(page);
    emit pagesChanged();
    return page;
}

void Document::removePage(int index)
{
    if (index < 0 || index >= m_pages.size() || m_pages.size() <= 1)
    {
        return;
    }

    delete m_pages.takeAt(index);

    if (m_currentIndex >= m_pages.size())
    {
        m_currentIndex = m_pages.size() - 1;
    }

    emit pagesChanged();
    emit currentPageChanged(m_currentIndex);
    setModified(true);
}

bool Document::movePage(int from, int to)
{
    if (from < 0 || from >= m_pages.size() || to < 0 || to >= m_pages.size() || from == to)
    {
        return false;
    }

    m_pages.move(from, to);
    m_currentIndex = to;
    emit pagesChanged();
    emit currentPageChanged(m_currentIndex);
    setModified(true);
    return true;
}

void Document::clear()
{
    qDeleteAll(m_pages);
    m_pages.clear();
    m_currentIndex = 0;
}

void Document::newDocument()
{
    clear();
    m_filePath.clear();
    m_suggestedSavePath.clear();
    addPage(QStringLiteral("Page 1"));
    m_currentIndex = 0;
    m_modified = false;
    m_readOnly = false;
    emit filePathChanged(m_filePath);
    emit pagesChanged();
    emit currentPageChanged(0);
    emit modifiedChanged(false);
}

void Document::setFilePath(const QString &path)
{
    if (m_filePath == path)
    {
        return;
    }

    m_filePath = path;
    m_readOnly = !path.isEmpty() && !QFileInfo(path).isWritable();
    emit filePathChanged(m_filePath);
}

QString Document::displayName() const
{
    if (m_filePath.isEmpty())
    {
        if (!m_suggestedSavePath.isEmpty())
        {
            // Opened from an embedded PNG/PDF/SVG: show the name the document
            // would get when saved as a .drawio file.
            return QFileInfo(m_suggestedSavePath).fileName();
        }

        return QStringLiteral("Untitled Drawing");
    }

    return QFileInfo(m_filePath).fileName();
}

QString Document::title() const
{
    QString result = displayName();

    if (m_modified)
    {
        result.prepend(QLatin1Char('*'));
    }

    return QStringLiteral("%1 - drawio-qt").arg(result);
}

void Document::setModified(bool modified)
{
    if (m_modified == modified)
    {
        return;
    }

    m_modified = modified;
    emit modifiedChanged(m_modified);
}

bool Document::load(const QString &path, QString *error)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("Cannot open file: %1").arg(file.errorString());
        }
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    QString xml;

    if (FileMeta::detect(data, path) != FileMeta::Container::None)
    {
        bool ok = false;
        xml = FileMeta::xmlFromDocument(data, FileMeta::detect(data, path), &ok);

        if (!ok || xml.trimmed().isEmpty())
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("The file contains no embedded diagram data");
            }
            return false;
        }

        // Some producers embed the compressed page payload; accept both.
        if (!xml.trimmed().startsWith(QLatin1Char('<')))
        {
            bool inflated = false;
            const QString decoded = Compression::decompressDiagram(xml.trimmed(), &inflated);

            if (inflated && decoded.trimmed().startsWith(QLatin1Char('<')))
            {
                xml = decoded;
            }
        }
    }
    else
    {
        xml = QString::fromUtf8(data);
    }

    const bool container = FileMeta::detect(data, path) != FileMeta::Container::None;

    if (!loadFromXmlString(xml, error))
    {
        return false;
    }

    if (container)
    {
        // The diagram came from an image/document container. Keep it as an
        // untitled document so Save offers a .drawio destination instead of
        // overwriting the PNG/PDF/SVG with XML.
        m_suggestedSavePath = QFileInfo(path).absolutePath() + QLatin1Char('/')
            + QFileInfo(path).completeBaseName() + QStringLiteral(".drawio");
        setFilePath(QString());
    }
    else
    {
        m_suggestedSavePath.clear();
        setFilePath(path);
    }

    m_compressed = QFileInfo(path).suffix().compare(QLatin1String("xml"),
                                                   Qt::CaseInsensitive) != 0;
    setModified(false);
    return true;
}

bool Document::save(const QString &path, QString *error)
{
    if (path.isEmpty())
    {
        return save(error);
    }

    QSaveFile file(path);

    // Always write plain mxGraphModel XML: draw.io reads both encodings, and
    // uncompressed files stay human readable and diff friendly.
    m_compressed = false;

    if (!file.open(QIODevice::WriteOnly))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("Cannot write file: %1").arg(file.errorString());
        }
        return false;
    }

    const QByteArray payload = toXmlString().toUtf8();

    if (file.write(payload) != payload.size() || !file.commit())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("Failed to write file: %1").arg(file.errorString());
        }
        return false;
    }

    setFilePath(path);
    setModified(false);
    return true;
}

bool Document::save(QString *error)
{
    if (m_filePath.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("No file name specified");
        }
        return false;
    }

    return save(m_filePath, error);
}

QString Document::toXmlString() const
{
    return MxFileIO::documentToXml(*this, m_compressed);
}

bool Document::loadFromXmlString(const QString &xml, QString *error)
{
    if (!MxFileIO::documentFromXml(*this, xml, error))
    {
        return false;
    }

    m_currentIndex = 0;
    emit pagesChanged();
    emit currentPageChanged(0);
    return true;
}

QString Document::pageXml(int index, bool compressed) const
{
    const Page *target = page(index);

    if (target == nullptr)
    {
        return {};
    }

    return MxFileIO::pageToXml(*target, compressed);
}

bool Document::restorePageXml(int index, const QString &xml, QString *error)
{
    Page *target = page(index);

    if (target == nullptr)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("Invalid page index");
        }
        return false;
    }

    if (!MxFileIO::pageFromXml(*target, xml, error))
    {
        return false;
    }

    emit contentChanged();
    return true;
}

QString Document::clipboardXml() const
{
    return toXmlString();
}

} // namespace dq
