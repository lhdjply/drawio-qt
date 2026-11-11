#include "ui/UpdateDialog.h"

#include "app/I18n.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace dq {

UpdateDialog::UpdateDialog(const ReleaseInfo &release, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(I18n::text(QStringLiteral("Update Available")));
    setModal(true);
    setMinimumSize(620, 520);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(12);

    auto *heading = new QLabel(I18n::text(I18n::text(QStringLiteral("Version %1 released"))).arg(release.tagName), this);
    heading->setStyleSheet(QStringLiteral("font-size: 17px; font-weight: 600;"));
    root->addWidget(heading);

    auto *subheading = new QLabel(
        I18n::text(I18n::text(QStringLiteral("Current version v%1. Release notes come from GitHub; download and installation are up to you, this app never replaces itself.")))
            .arg(QLatin1String(DRAWIO_QT_VERSION)),
        this);
    subheading->setWordWrap(true);
    subheading->setStyleSheet(QStringLiteral("color: palette(mid);"));
    root->addWidget(subheading);

    // 正文：Release 的 body 就是作者写的更新日志。标题常常就是版本号本身
    // （GitHub 发布页默认这么填），而版本号已经在对话框标题里了，重复一遍只会
    // 多出一行无意义的「## 0.1.0」。
    QString notesText = release.notes.trimmed();
    const QString title = release.title.trimmed();
    const bool titleIsVersion =
        QRegularExpression(QStringLiteral("^[vV]?\\d+(\\.\\d+)*([-+].*)?$"))
            .match(title)
            .hasMatch();
    if (!title.isEmpty() && !titleIsVersion && !notesText.startsWith(title)) {
        notesText = QStringLiteral("## %1\n\n%2").arg(title, notesText);
    }
    if (notesText.isEmpty()) {
        notesText = I18n::text(QStringLiteral("(No release notes for this version; open the release page.)"));
    }

    auto *notes = new QTextBrowser(this);
    notes->setOpenExternalLinks(false); // 日志里的链接不自动打开，避免误点
    notes->setMarkdown(notesText);
    root->addWidget(notes, 1);

    const QDateTime published =
        QDateTime::fromString(release.publishedAt, Qt::ISODate).toLocalTime();

    QString hint;
    if (published.isValid()) {
        hint = I18n::text(I18n::text(QStringLiteral("Published %1. Download the installer from the release page.")))
                   .arg(published.toString(QStringLiteral("yyyy-MM-dd")));
    } else {
        hint = I18n::text(QStringLiteral("Download the installer from the release page."));
    }
    auto *hintLabel = new QLabel(hint, this);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet(QStringLiteral("color: palette(mid);"));
    root->addWidget(hintLabel);

    auto *actions = new QHBoxLayout();
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(8);
    actions->addStretch(1);

    auto *later = new QPushButton(I18n::text(QStringLiteral("Later")), this);
    auto *open  = new QPushButton(I18n::text(QStringLiteral("Open Download Page")), this);
    open->setDefault(true);
    actions->addWidget(later);
    actions->addWidget(open);
    root->addLayout(actions);

    connect(later, &QPushButton::clicked, this, &QDialog::reject);
    connect(open, &QPushButton::clicked, this, [this, release]() {
        QDesktopServices::openUrl(release.pageUrl);
        accept();
    });
}

} // namespace dq
