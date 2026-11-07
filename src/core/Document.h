#pragma once

#include "core/Page.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace dq {

/// One open diagram file: a list of pages plus the state that the main window
/// needs to manage it (path, modified flag, page compression on save).
class Document : public QObject
{
    Q_OBJECT

public:
    explicit Document(QObject *parent = nullptr);
    ~Document() override;

    // --- pages -----------------------------------------------------------
    int pageCount() const { return m_pages.size(); }
    Page *page(int index) const;
    const QVector<Page *> &pages() const { return m_pages; }
    Page *currentPage() const;
    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);

    Page *addPage(const QString &name = QString());
    void removePage(int index);
    bool movePage(int from, int to);
    void clear();

    // --- file state ------------------------------------------------------
    QString filePath() const { return m_filePath; }
    void setFilePath(const QString &path);
    QString displayName() const;
    QString title() const;

    /// Where "Save As" should suggest storing a document opened from a PNG,
    /// PDF or SVG container (those files cannot be rewritten in place).
    QString suggestedSavePath() const { return m_suggestedSavePath; }

    bool isModified() const { return m_modified; }
    void setModified(bool modified);

    bool isCompressed() const { return m_compressed; }
    void setCompressed(bool compressed) { m_compressed = compressed; }

    bool isReadOnly() const { return m_readOnly; }
    void setReadOnly(bool readOnly) { m_readOnly = readOnly; }

    // --- persistence -----------------------------------------------------
    /// Loads .drawio/.xml, or a PNG/PDF/SVG that carries an embedded diagram.
    bool load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr);
    bool save(QString *error = nullptr);

    QString toXmlString() const;
    bool loadFromXmlString(const QString &xml, QString *error = nullptr);

    /// Page XML used for undo snapshots and clipboard fragments.
    QString pageXml(int index, bool compressed = false) const;
    bool restorePageXml(int index, const QString &xml, QString *error = nullptr);

    void newDocument();

    /// Serialised diagram for the clipboard (all pages).
    QString clipboardXml() const;

signals:
    void contentChanged();
    void modifiedChanged(bool modified);
    void pagesChanged();
    void currentPageChanged(int index);
    void filePathChanged(const QString &path);

private:
    QVector<Page *> m_pages;
    int m_currentIndex = 0;
    QString m_filePath;
    QString m_suggestedSavePath;
    bool m_modified = false;
    bool m_compressed = true;
    bool m_readOnly = false;
};

} // namespace dq
