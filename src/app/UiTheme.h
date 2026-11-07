#pragma once

#include <QPalette>
#include <QString>

namespace dq {

/// Application-wide look and feel. drawio-qt uses Qt's Fusion style so the
/// editor looks the same on every platform (and independent of the desktop
/// environment's own Qt style).
namespace UiTheme {

/// Installs the Fusion style. Safe to call more than once.
void applyFusionStyle();

/// Applies the light or dark palette ("auto" follows the system colour
/// scheme). Must be called after the Fusion style is installed.
void applyColorScheme(const QString &theme);

QPalette lightPalette();
QPalette darkPalette();

bool isDarkScheme();

} // namespace UiTheme
} // namespace dq
