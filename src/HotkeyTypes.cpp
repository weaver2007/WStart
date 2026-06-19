#include "HotkeyTypes.h"

#include "QtCompat.h"

#include <QColor>
#include <QKeySequence>
#include <QStringList>
#include <QUuid>

#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QString normalizedOptionalColor(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    const QColor color(trimmed);
    return color.isValid() ? color.name() : QString();
}

}

bool HotkeyCombination::isValid() const
{
    return key > 0 && !isModifierKey(key);
}

QString HotkeyCombination::displayText() const
{
    QStringList parts;
    if (modifiers.testFlag(ModifierCtrl)) {
        parts << "Ctrl";
    }
    if (modifiers.testFlag(ModifierAlt)) {
        parts << "Alt";
    }
    if (modifiers.testFlag(ModifierShift)) {
        parts << "Shift";
    }
    if (modifiers.testFlag(ModifierWin)) {
        parts << "Win";
    }
    if (key > 0) {
        parts << keyName(key);
    }
    return parts.join("+");
}

QString HotkeyCombination::stableId() const
{
    return QString("%1:%2").arg(static_cast<int>(modifiers)).arg(key);
}

QJsonObject HotkeyCombination::toJson() const
{
    return {
        {"modifiers", static_cast<int>(modifiers)},
        {"key", key},
        {"displayText", displayText()}
    };
}

HotkeyCombination HotkeyCombination::fromJson(const QJsonObject &object)
{
    HotkeyCombination hotkey;
    hotkey.modifiers = HotkeyModifiers(QFlag(object.value("modifiers").toInt()));
    hotkey.key = object.value("key").toInt();
    return hotkey;
}

QString HotkeyCombination::keyName(int virtualKey)
{
#ifdef Q_OS_WIN
    if (virtualKey >= VK_F1 && virtualKey <= VK_F24) {
        return QString("F%1").arg(virtualKey - VK_F1 + 1);
    }
    if (virtualKey >= 'A' && virtualKey <= 'Z') {
        return QString(QChar(static_cast<ushort>(virtualKey)));
    }
    if (virtualKey >= '0' && virtualKey <= '9') {
        return QString(QChar(static_cast<ushort>(virtualKey)));
    }
    switch (virtualKey) {
    case VK_SPACE: return "Space";
    case VK_TAB: return "Tab";
    case VK_ESCAPE: return "Esc";
    case VK_RETURN: return "Enter";
    case VK_BACK: return "Backspace";
    case VK_DELETE: return "Delete";
    case VK_INSERT: return "Insert";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "PageUp";
    case VK_NEXT: return "PageDown";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_OEM_MINUS: return "-";
    case VK_OEM_PLUS: return "=";
    case VK_OEM_4: return "[";
    case VK_OEM_6: return "]";
    case VK_OEM_5: return "\\";
    case VK_OEM_1: return ";";
    case VK_OEM_7: return "'";
    case VK_OEM_COMMA: return ",";
    case VK_OEM_PERIOD: return ".";
    case VK_OEM_2: return "/";
    case VK_OEM_3: return "`";
    default: break;
    }
#endif
    return QString("VK_%1").arg(virtualKey);
}

bool HotkeyCombination::isModifierKey(int virtualKey)
{
#ifdef Q_OS_WIN
    switch (virtualKey) {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_LWIN:
    case VK_RWIN:
        return true;
    default:
        return false;
    }
#else
    Q_UNUSED(virtualKey)
    return false;
#endif
}

bool LaunchAction::isValid() const
{
    return !target.trimmed().isEmpty();
}

QString LaunchAction::typeName() const
{
    switch (type) {
    case LaunchActionType::Application: return "Application";
    case LaunchActionType::File: return "File";
    case LaunchActionType::Folder: return "Folder";
    case LaunchActionType::Url: return "Url";
    }
    return "Application";
}

QJsonObject LaunchAction::toJson() const
{
    return {
        {"type", typeName()},
        {"target", target},
        {"arguments", arguments},
        {"workingDirectory", workingDirectory}
    };
}

LaunchAction LaunchAction::fromJson(const QJsonObject &object)
{
    LaunchAction action;
    action.type = typeFromName(object.value("type").toString());
    action.target = object.value("target").toString();
    action.arguments = object.value("arguments").toString();
    action.workingDirectory = object.value("workingDirectory").toString();
    return action;
}

LaunchActionType LaunchAction::typeFromName(const QString &name)
{
    if (name.compare("File", Qt::CaseInsensitive) == 0) {
        return LaunchActionType::File;
    }
    if (name.compare("Folder", Qt::CaseInsensitive) == 0) {
        return LaunchActionType::Folder;
    }
    if (name.compare("Url", Qt::CaseInsensitive) == 0) {
        return LaunchActionType::Url;
    }
    return LaunchActionType::Application;
}

bool LauncherSection::isValid() const
{
    return !id.isEmpty() && !name.trimmed().isEmpty();
}

QString LauncherSection::categoryName() const
{
    return categoryName(category);
}

QJsonObject LauncherSection::toJson() const
{
    return {
        {"id", id},
        {"category", categoryName()},
        {"name", name},
        {"iconKey", iconKey},
        {"sortOrder", sortOrder},
        {"encrypted", encrypted},
        {"passwordHash", passwordHash},
        {"collapsed", collapsed}
    };
}

LauncherSection LauncherSection::fromJson(const QJsonObject &object)
{
    LauncherSection section;
    section.id = object.value("id").toString();
    if (section.id.isEmpty()) {
        section.id = QtCompat::uuidWithoutBraces();
    }
    section.category = categoryFromName(object.value("category").toString());
    section.name = object.value("name").toString();
    section.iconKey = object.value("iconKey").toString();
    section.sortOrder = object.value("sortOrder").toInt();
    section.encrypted = object.value("encrypted").toBool(false);
    section.passwordHash = object.value("passwordHash").toString();
    section.collapsed = object.value("collapsed").toBool(false);
    return section;
}

LauncherCategory LauncherSection::categoryFromName(const QString &name)
{
    if (name.compare("Folder", Qt::CaseInsensitive) == 0) {
        return LauncherCategory::Folder;
    }
    if (name.compare("Website", Qt::CaseInsensitive) == 0) {
        return LauncherCategory::Website;
    }
    return LauncherCategory::Program;
}

QString LauncherSection::categoryName(LauncherCategory category)
{
    switch (category) {
    case LauncherCategory::Program: return "Program";
    case LauncherCategory::Folder: return "Folder";
    case LauncherCategory::Website: return "Website";
    }
    return "Program";
}

bool HotkeyRule::isValid() const
{
    return !id.isEmpty() && !sectionId.isEmpty() && action.isValid();
}

QJsonObject HotkeyRule::toJson() const
{
    return {
        {"id", id},
        {"enabled", enabled},
        {"category", LauncherSection::categoryName(category)},
        {"sectionId", sectionId},
        {"hotkey", hotkey.toJson()},
        {"action", action.toJson()},
        {"description", description}
    };
}

HotkeyRule HotkeyRule::fromJson(const QJsonObject &object)
{
    HotkeyRule rule;
    rule.id = object.value("id").toString();
    if (rule.id.isEmpty()) {
        rule.id = QtCompat::uuidWithoutBraces();
    }
    rule.enabled = object.value("enabled").toBool(true);
    if (object.contains("category")) {
        rule.category = LauncherSection::categoryFromName(object.value("category").toString());
    } else {
        switch (LaunchAction::fromJson(object.value("action").toObject()).type) {
        case LaunchActionType::Folder:
            rule.category = LauncherCategory::Folder;
            break;
        case LaunchActionType::Url:
            rule.category = LauncherCategory::Website;
            break;
        case LaunchActionType::File:
        case LaunchActionType::Application:
            rule.category = LauncherCategory::Program;
            break;
        }
    }
    rule.sectionId = object.value("sectionId").toString();
    rule.hotkey = HotkeyCombination::fromJson(object.value("hotkey").toObject());
    rule.action = LaunchAction::fromJson(object.value("action").toObject());
    rule.description = object.value("description").toString();
    return rule;
}

QJsonObject AppSettings::toJson() const
{
    const QString normalizedLanguage = language.compare("en-US", Qt::CaseInsensitive) == 0 ? "en-US" : "zh-CN";
    const QString normalizedTheme = themeMode.compare("light", Qt::CaseInsensitive) == 0 ? "light" :
        themeMode.compare("dark", Qt::CaseInsensitive) == 0 ? "dark" : "system";
    return {
        {"language", normalizedLanguage},
        {"hotkeysEnabled", hotkeysEnabled},
        {"themeMode", normalizedTheme},
        {"itemAppearance", itemAppearance.toJson()},
        {"sectionAppearance", sectionAppearance.toJson()},
        {"categoryAppearance", categoryAppearance.toJson()}
    };
}

QJsonObject LauncherItemAppearance::toJson() const
{
    LauncherItemAppearance normalized = LauncherItemAppearance::fromJson({
        {"iconWidth", iconWidth},
        {"iconHeight", iconHeight},
        {"itemWidth", itemWidth},
        {"itemHeight", itemHeight},
        {"fontFamily", fontFamily},
        {"fontPointSize", fontPointSize},
        {"horizontalSpacing", horizontalSpacing},
        {"verticalSpacing", verticalSpacing},
        {"multilineText", multilineText},
        {"showEllipsis", showEllipsis}
    });
    return {
        {"iconWidth", normalized.iconWidth},
        {"iconHeight", normalized.iconHeight},
        {"itemWidth", normalized.itemWidth},
        {"itemHeight", normalized.itemHeight},
        {"fontFamily", normalized.fontFamily},
        {"fontPointSize", normalized.fontPointSize},
        {"horizontalSpacing", normalized.horizontalSpacing},
        {"verticalSpacing", normalized.verticalSpacing},
        {"multilineText", normalized.multilineText},
        {"showEllipsis", normalized.showEllipsis}
    };
}

LauncherItemAppearance LauncherItemAppearance::fromJson(const QJsonObject &object)
{
    LauncherItemAppearance appearance;
    appearance.iconWidth = QtCompat::boundedInt(object.value("iconWidth").toInt(appearance.iconWidth), 16, 128);
    appearance.iconHeight = QtCompat::boundedInt(object.value("iconHeight").toInt(appearance.iconHeight), 16, 128);
    appearance.itemWidth = QtCompat::boundedInt(object.value("itemWidth").toInt(appearance.itemWidth), 40, 180);
    appearance.itemHeight = QtCompat::boundedInt(object.value("itemHeight").toInt(appearance.itemHeight), 44, 220);
    appearance.fontFamily = object.value("fontFamily").toString(appearance.fontFamily).trimmed();
    appearance.fontPointSize = QtCompat::boundedInt(object.value("fontPointSize").toInt(appearance.fontPointSize), 6, 18);
    appearance.horizontalSpacing = QtCompat::boundedInt(object.value("horizontalSpacing").toInt(appearance.horizontalSpacing), 0, 40);
    appearance.verticalSpacing = QtCompat::boundedInt(object.value("verticalSpacing").toInt(appearance.verticalSpacing), 0, 40);
    appearance.multilineText = object.value("multilineText").toBool(appearance.multilineText);
    appearance.showEllipsis = object.value("showEllipsis").toBool(appearance.showEllipsis);
    return appearance;
}

QJsonObject LauncherSectionAppearance::toJson() const
{
    LauncherSectionAppearance normalized = LauncherSectionAppearance::fromJson({
        {"iconWidth", iconWidth},
        {"iconHeight", iconHeight},
        {"headerHeight", headerHeight},
        {"fontFamily", fontFamily},
        {"fontPointSize", fontPointSize},
        {"textColor", textColor}
    });
    return {
        {"iconWidth", normalized.iconWidth},
        {"iconHeight", normalized.iconHeight},
        {"headerHeight", normalized.headerHeight},
        {"fontFamily", normalized.fontFamily},
        {"fontPointSize", normalized.fontPointSize},
        {"textColor", normalized.textColor}
    };
}

LauncherSectionAppearance LauncherSectionAppearance::fromJson(const QJsonObject &object)
{
    LauncherSectionAppearance appearance;
    appearance.iconWidth = QtCompat::boundedInt(object.value("iconWidth").toInt(appearance.iconWidth), 12, 96);
    appearance.iconHeight = QtCompat::boundedInt(object.value("iconHeight").toInt(appearance.iconHeight), 12, 96);
    appearance.headerHeight = QtCompat::boundedInt(object.value("headerHeight").toInt(appearance.headerHeight), 24, 96);
    appearance.fontFamily = object.value("fontFamily").toString(appearance.fontFamily).trimmed();
    appearance.fontPointSize = QtCompat::boundedInt(object.value("fontPointSize").toInt(appearance.fontPointSize), 6, 18);
    appearance.textColor = normalizedOptionalColor(object.value("textColor").toString(appearance.textColor));
    return appearance;
}

QJsonObject LauncherCategoryAppearance::toJson() const
{
    LauncherCategoryAppearance normalized = LauncherCategoryAppearance::fromJson({
        {"iconWidth", iconWidth},
        {"iconHeight", iconHeight},
        {"buttonHeight", buttonHeight},
        {"fontFamily", fontFamily},
        {"fontPointSize", fontPointSize},
        {"textColor", textColor}
    });
    return {
        {"iconWidth", normalized.iconWidth},
        {"iconHeight", normalized.iconHeight},
        {"buttonHeight", normalized.buttonHeight},
        {"fontFamily", normalized.fontFamily},
        {"fontPointSize", normalized.fontPointSize},
        {"textColor", normalized.textColor}
    };
}

LauncherCategoryAppearance LauncherCategoryAppearance::fromJson(const QJsonObject &object)
{
    LauncherCategoryAppearance appearance;
    appearance.iconWidth = QtCompat::boundedInt(object.value("iconWidth").toInt(appearance.iconWidth), 12, 96);
    appearance.iconHeight = QtCompat::boundedInt(object.value("iconHeight").toInt(appearance.iconHeight), 12, 96);
    appearance.buttonHeight = QtCompat::boundedInt(object.value("buttonHeight").toInt(appearance.buttonHeight), 24, 96);
    appearance.fontFamily = object.value("fontFamily").toString(appearance.fontFamily).trimmed();
    appearance.fontPointSize = QtCompat::boundedInt(object.value("fontPointSize").toInt(appearance.fontPointSize), 6, 18);
    appearance.textColor = normalizedOptionalColor(object.value("textColor").toString(appearance.textColor));
    return appearance;
}

AppSettings AppSettings::fromJson(const QJsonObject &object)
{
    AppSettings settings;
    const QString languageValue = object.value("language").toString(settings.language);
    settings.language = languageValue.compare("en-US", Qt::CaseInsensitive) == 0 ||
            languageValue.compare("en", Qt::CaseInsensitive) == 0 ? "en-US" : "zh-CN";
    settings.hotkeysEnabled = object.value("hotkeysEnabled").toBool(true);
    const QString themeValue = object.value("themeMode").toString(settings.themeMode);
    settings.themeMode = themeValue.compare("light", Qt::CaseInsensitive) == 0 ? "light" :
        themeValue.compare("dark", Qt::CaseInsensitive) == 0 ? "dark" : "system";
    settings.itemAppearance = LauncherItemAppearance::fromJson(object.value("itemAppearance").toObject());
    settings.sectionAppearance = LauncherSectionAppearance::fromJson(object.value("sectionAppearance").toObject());
    settings.categoryAppearance = LauncherCategoryAppearance::fromJson(object.value("categoryAppearance").toObject());
    return settings;
}
