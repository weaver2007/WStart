#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

enum HotkeyModifier {
    ModifierNone = 0,
    ModifierCtrl = 1 << 0,
    ModifierAlt = 1 << 1,
    ModifierShift = 1 << 2,
    ModifierWin = 1 << 3
};

Q_DECLARE_FLAGS(HotkeyModifiers, HotkeyModifier)
Q_DECLARE_OPERATORS_FOR_FLAGS(HotkeyModifiers)

enum class LaunchActionType {
    Application,
    File,
    Folder,
    Url
};

enum class LauncherCategory {
    Program,
    Folder,
    Website
};

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
inline size_t qHash(LauncherCategory category, size_t seed = 0) noexcept
{
    return ::qHash(static_cast<int>(category), seed);
}
#else
inline uint qHash(LauncherCategory category, uint seed = 0) noexcept
{
    return ::qHash(static_cast<int>(category)) ^ seed;
}
#endif

struct LauncherSection {
    QString id;
    LauncherCategory category = LauncherCategory::Program;
    QString name;
    QString iconKey;
    int sortOrder = 0;
    bool encrypted = false;
    QString passwordHash;
    bool collapsed = false;

    bool isValid() const;
    QString categoryName() const;
    QJsonObject toJson() const;

    static LauncherSection fromJson(const QJsonObject &object);
    static LauncherCategory categoryFromName(const QString &name);
    static QString categoryName(LauncherCategory category);
};

struct HotkeyCombination {
    HotkeyModifiers modifiers = ModifierNone;
    int key = 0;

    bool isValid() const;
    QString displayText() const;
    QString stableId() const;
    QJsonObject toJson() const;

    static HotkeyCombination fromJson(const QJsonObject &object);
    static QString keyName(int virtualKey);
    static bool isModifierKey(int virtualKey);
};

struct LaunchAction {
    LaunchActionType type = LaunchActionType::Application;
    QString target;
    QString arguments;
    QString workingDirectory;

    bool isValid() const;
    QString typeName() const;
    QJsonObject toJson() const;

    static LaunchAction fromJson(const QJsonObject &object);
    static LaunchActionType typeFromName(const QString &name);
};

struct HotkeyRule {
    QString id;
    bool enabled = true;
    LauncherCategory category = LauncherCategory::Program;
    QString sectionId;
    HotkeyCombination hotkey;
    LaunchAction action;
    QString description;

    bool isValid() const;
    QJsonObject toJson() const;

    static HotkeyRule fromJson(const QJsonObject &object);
};

struct LauncherItemAppearance {
    int iconWidth = 48;
    int iconHeight = 48;
    int itemWidth = 64;
    int itemHeight = 80;
    QString fontFamily;
    int fontPointSize = 8;
    int horizontalSpacing = 0;
    int verticalSpacing = 0;
    bool multilineText = true;
    bool showEllipsis = false;

    QJsonObject toJson() const;
    static LauncherItemAppearance fromJson(const QJsonObject &object);
};

struct LauncherSectionAppearance {
    int iconWidth = 18;
    int iconHeight = 18;
    int headerHeight = 32;
    QString fontFamily;
    int fontPointSize = 8;
    QString textColor;

    QJsonObject toJson() const;
    static LauncherSectionAppearance fromJson(const QJsonObject &object);
};

struct LauncherCategoryAppearance {
    int iconWidth = 32;
    int iconHeight = 32;
    int buttonHeight = 30;
    QString fontFamily;
    int fontPointSize = 10;
    QString textColor;

    QJsonObject toJson() const;
    static LauncherCategoryAppearance fromJson(const QJsonObject &object);
};

struct AppSettings {
    QString language = "zh-CN";
    bool hotkeysEnabled = true;
    QString themeMode = "system";
    LauncherItemAppearance itemAppearance;
    LauncherSectionAppearance sectionAppearance;
    LauncherCategoryAppearance categoryAppearance;

    QJsonObject toJson() const;
    static AppSettings fromJson(const QJsonObject &object);
};

struct LauncherDocument {
    AppSettings settings;
    QVector<LauncherSection> sections;
    QVector<HotkeyRule> rules;
};
