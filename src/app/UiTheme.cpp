#include "app/UiTheme.h"

#include <QApplication>
#include <QGuiApplication>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>

namespace dq {
namespace UiTheme {

void applyFusionStyle()
{
    QApplication *app = qobject_cast<QApplication *>(QCoreApplication::instance());

    if (app == nullptr)
    {
        return;
    }

    QStyle *current = app->style();

    if (current != nullptr
        && current->objectName().compare(QLatin1String("fusion"), Qt::CaseInsensitive) == 0)
    {
        return;
    }

    if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion")))
    {
        QApplication::setStyle(fusion);
    }
}

QPalette lightPalette()
{
    QStyle *style = QApplication::style();

    if (style != nullptr)
    {
        return style->standardPalette();
    }

    return QPalette();
}

QPalette darkPalette()
{
    QPalette palette;
    const QColor window(0x1B, 0x1D, 0x1E);
    const QColor base(0x24, 0x27, 0x2B);
    const QColor alternate(0x2C, 0x30, 0x35);
    const QColor text(0xE8, 0xEA, 0xED);
    const QColor disabledText(0x7A, 0x7F, 0x86);
    const QColor button(0x2C, 0x30, 0x35);
    const QColor highlight(0x1A, 0x73, 0xE8);
    const QColor link(0x64, 0xA5, 0xFF);
    const QColor tooltip(0x2F, 0x33, 0x38);

    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, alternate);
    palette.setColor(QPalette::ToolTipBase, tooltip);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Light, QColor(0x3D, 0x42, 0x48));
    palette.setColor(QPalette::Midlight, QColor(0x34, 0x38, 0x3E));
    palette.setColor(QPalette::Mid, QColor(0x2A, 0x2E, 0x33));
    palette.setColor(QPalette::Dark, QColor(0x14, 0x16, 0x18));
    palette.setColor(QPalette::Shadow, QColor(0x0E, 0x10, 0x11));
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Link, link);
    palette.setColor(QPalette::LinkVisited, QColor(0x9A, 0x7A, 0xD8));
    palette.setColor(QPalette::PlaceholderText, QColor(0x9A, 0x9F, 0xA6));

    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x3A, 0x40, 0x48));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(0xB0, 0xB4, 0xB9));

    return palette;
}

bool isDarkScheme()
{
    const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    return scheme == Qt::ColorScheme::Dark;
}

void applyColorScheme(const QString &theme)
{
    QApplication *app = qobject_cast<QApplication *>(QCoreApplication::instance());

    if (app == nullptr)
    {
        return;
    }

    const bool dark = theme == QLatin1String("dark")
        || (theme != QLatin1String("light") && isDarkScheme());

    QApplication::setPalette(dark ? darkPalette() : lightPalette());
}

} // namespace UiTheme
} // namespace dq
