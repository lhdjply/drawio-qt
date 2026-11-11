#pragma once

#include <QString>

class QWidget;

namespace dq {

/// Lightweight runtime translation for the Qt Widgets UI. Source strings stay
/// Chinese in code; the English table is applied to widget/action texts so the
/// language can be switched without rebuilding the interface.
namespace I18n {

enum class Language
{
    Chinese,
    English
};

void initialize();
void setLanguage(Language language);
Language language();
bool isEnglish();

QString text(const QString &source);
QString shapeName(const QString &key, const QString &fallback);
QString categoryName(const QString &id, const QString &fallback);
QString imageText(const QString &section, const QString &key, const QString &fallback);

/// Translate an existing widget tree (labels, buttons, menus and actions).
void retranslate(QWidget *root);
void retranslateAll();

void install();

} // namespace I18n
} // namespace dq
