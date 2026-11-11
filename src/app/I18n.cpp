#include "app/I18n.h"

#include "app/AppSettings.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QTabBar>
#include <QEvent>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGroupBox>
#include <QLabel>
#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
#include <QRegularExpression>
#include <QDebug>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QTabWidget>
#include <QWidget>

namespace dq {
namespace I18n {
namespace {

Language g_language = Language::Chinese;
QTranslator *g_qtTranslator = nullptr;

void applyQtTranslation()
{
    if (g_qtTranslator != nullptr)
    {
        QCoreApplication::removeTranslator(g_qtTranslator);
        delete g_qtTranslator;
        g_qtTranslator = nullptr;
    }

    if (g_language != Language::Chinese || qApp == nullptr)
    {
        return;
    }

    auto *translator = new QTranslator(qApp);
    const QString path = QLibraryInfo::path(QLibraryInfo::TranslationsPath);

    if (translator->load(QStringLiteral("qtbase_zh_CN"), path))
    {
        QCoreApplication::installTranslator(translator);
        g_qtTranslator = translator;
    }
    else
    {
        delete translator;
    }
}

QHash<QString, QString> loadTable(const QString &path)
{
    QHash<QString, QString> result;
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly))
    {
        return result;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());

    if (!document.isObject())
    {
        return result;
    }

    const QJsonObject object = document.object();

    for (auto it = object.constBegin(); it != object.constEnd(); ++it)
    {
        result.insert(it.key(), it.value().toString());
    }

    return result;
}

const QHash<QString, QString> &englishTable()
{
    static const QHash<QString, QString> table = loadTable(QStringLiteral(":/translations/en.json"));
    return table;
}

const QHash<QString, QString> &chineseTable()
{
    static const QHash<QString, QString> table = loadTable(QStringLiteral(":/translations/zh.json"));
    return table;
}

QString translateText(const QString &source)
{
    const bool modified = source.endsWith(QLatin1Char('*'));
    const QString base = modified ? source.left(source.size() - 1) : source;

    if (g_language == Language::English)
    {
        const auto it = englishTable().constFind(base);
        return (it != englishTable().constEnd() ? it.value() : base)
            + (modified ? QStringLiteral("*") : QString());
    }

    const auto it = chineseTable().constFind(base);

    if (it != chineseTable().constEnd())
    {
        return it.value() + (modified ? QStringLiteral("*") : QString());
    }

    static const QRegularExpression pagePattern(QStringLiteral("^Page (\\d+)$"));
    const QRegularExpressionMatch match = pagePattern.match(base);

    if (match.hasMatch())
    {
        QString templ = chineseTable().value(QStringLiteral("Page %1"));
        return templ.replace(QStringLiteral("%1"), match.captured(1))
            + (modified ? QStringLiteral("*") : QString());
    }

    return source;
}

void translateAction(QAction *action)
{
    if (action == nullptr || action->menu() != nullptr)
    {
        // Menu bar entries are handled together with their QMenu title.
        return;
    }

    QString source = action->property("i18n.source").toString();

    if (source.isEmpty())
    {
        source = action->text();
        action->setProperty("i18n.source", source);
    }

    action->setText(translateText(source));

    if (!action->toolTip().isEmpty() || !source.isEmpty())
    {
        // Tooltips usually mirror the action text.
        action->setToolTip(translateText(source));
    }
}

void translateWidget(QWidget *widget)
{
    if (widget == nullptr)
    {
        return;
    }

    if (auto *label = qobject_cast<QLabel *>(widget))
    {
        QString source = label->property("i18n.source").toString();

        if (source.isEmpty())
        {
            source = label->text();
            label->setProperty("i18n.source", source);
        }

        label->setText(translateText(source));
    }
    else if (auto *button = qobject_cast<QAbstractButton *>(widget))
    {
        QString source = button->property("i18n.source").toString();

        if (source.isEmpty())
        {
            source = button->text();
            button->setProperty("i18n.source", source);
        }

        button->setText(translateText(source));
    }
    else if (auto *group = qobject_cast<QGroupBox *>(widget))
    {
        QString source = group->property("i18n.source").toString();

        if (source.isEmpty())
        {
            source = group->title();
            group->setProperty("i18n.source", source);
        }

        group->setTitle(translateText(source));
    }
    else if (auto *edit = qobject_cast<QLineEdit *>(widget))
    {
        QString source = edit->property("i18n.placeholder").toString();

        if (source.isEmpty())
        {
            source = edit->placeholderText();
            edit->setProperty("i18n.placeholder", source);
        }

        edit->setPlaceholderText(translateText(source));
    }
    else if (auto *tabBar = qobject_cast<QTabBar *>(widget))
    {
        for (int i = 0; i < tabBar->count(); ++i)
        {
            QString source = tabBar->tabData(i).toString();

            if (source.isEmpty())
            {
                source = tabBar->tabText(i);
                tabBar->setTabData(i, source);
            }

            tabBar->setTabText(i, translateText(source));
        }
    }
    else if (auto *combo = qobject_cast<QComboBox *>(widget))
    {
        for (int i = 0; i < combo->count(); ++i)
        {
            QString source = combo->itemData(i, Qt::UserRole + 1000).toString();

            if (source.isEmpty())
            {
                source = combo->itemText(i);
                combo->setItemData(i, source, Qt::UserRole + 1000);
            }

            combo->setItemText(i, translateText(source));
        }
    }
    else if (auto *tabs = qobject_cast<QTabWidget *>(widget))
    {
        QStringList sources = tabs->property("i18n.tabs").toStringList();

        if (sources.size() != tabs->count())
        {
            sources.clear();

            for (int i = 0; i < tabs->count(); ++i)
            {
                sources.append(tabs->tabText(i));
            }

            tabs->setProperty("i18n.tabs", sources);
        }

        for (int i = 0; i < tabs->count() && i < sources.size(); ++i)
        {
            tabs->setTabText(i, translateText(sources.at(i)));
        }
    }
    else if (auto *menu = qobject_cast<QMenu *>(widget))
    {
        QString source = menu->property("i18n.source").toString();

        if (source.isEmpty())
        {
            source = menu->title();
            menu->setProperty("i18n.source", source);
        }

        const QString title = translateText(source);
        menu->setTitle(title);

        if (QAction *menuAction = menu->menuAction())
        {
            menuAction->setText(title);
            menuAction->setToolTip(title);
        }
    }

    QString titleSource = widget->property("i18n.windowTitle").toString();

    if (titleSource.isEmpty())
    {
        titleSource = widget->windowTitle();
        widget->setProperty("i18n.windowTitle", titleSource);
    }

    widget->setWindowTitle(translateText(titleSource));

    for (QAction *action : widget->actions())
    {
        translateAction(action);
    }
}

class LanguageFilter : public QObject
{
public:
    explicit LanguageFilter(QObject *parent) : QObject(parent) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Show)
        {
            if (auto *widget = qobject_cast<QWidget *>(watched))
            {
                retranslate(widget);
            }
        }

        return QObject::eventFilter(watched, event);
    }
};

} // namespace

void initialize()
{
    const QString language = AppSettings::instance().language();

    if (language == QLatin1String("en"))
    {
        g_language = Language::English;
    }
    else if (language == QLatin1String("zh"))
    {
        g_language = Language::Chinese;
    }
    else
    {
        g_language = QLocale::system().language() == QLocale::English
            ? Language::English : Language::Chinese;
    }

    install();
    applyQtTranslation();
}

void setLanguage(Language language)
{
    g_language = language;
    applyQtTranslation();
    retranslateAll();
}

Language language()
{
    return g_language;
}

bool isEnglish()
{
    return g_language == Language::English;
}

QString text(const QString &source)
{
    return translateText(source);
}

QString shapeName(const QString &key, const QString &fallback)
{
    const QHash<QString, QString> &table = g_language == Language::English
        ? englishTable() : chineseTable();
    const auto it = table.constFind(key);
    return it != table.constEnd() ? it.value() : fallback;
}

QString imageText(const QString &section, const QString &key, const QString &fallback)
{
    if (g_language != Language::Chinese)
    {
        return fallback;
    }

    return chineseTable().value(QStringLiteral("image.") + section + QLatin1Char('.') + key,
                                fallback);
}

QString categoryName(const QString &id, const QString &fallback)
{
    const QHash<QString, QString> &table = g_language == Language::English
        ? englishTable() : chineseTable();
    const auto it = table.constFind(id);
    return it != table.constEnd() ? it.value() : fallback;
}

void retranslate(QWidget *root)
{
    if (root == nullptr)
    {
        return;
    }

    translateWidget(root);

    for (QAction *action : root->findChildren<QAction *>())
    {
        translateAction(action);
    }

    for (QWidget *child : root->findChildren<QWidget *>())
    {
        translateWidget(child);
    }
}

void retranslateAll()
{
    for (QWidget *widget : QApplication::topLevelWidgets())
    {
        retranslate(widget);

        if (auto *window = qobject_cast<QMainWindow *>(widget))
        {
            QMenuBar *bar = window->menuBar();

            if (bar != nullptr)
            {
                bar->adjustSize();
                bar->update();
                bar->repaint();
            }
        }
    }
}

void install()
{
    static LanguageFilter *filter = nullptr;

    if (filter == nullptr && qApp != nullptr)
    {
        filter = new LanguageFilter(qApp);
        qApp->installEventFilter(filter);
    }
}

} // namespace I18n
} // namespace dq
