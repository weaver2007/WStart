#include "../src/HotkeyConflictDetector.h"
#include "../src/HotkeyTypes.h"
#include "../src/RuleStore.h"

#include <QTemporaryDir>
#include <QtTest>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class CoreTests : public QObject {
    Q_OBJECT

private slots:
    void hotkeyDisplayText();
    void jsonRoundTrip();
    void sectionJsonRoundTrip();
    void ruleCanBeValidWithoutHotkey();
    void legacyFileRuleMigratesToProgramCategory();
    void appSettingsRoundTrip();
    void appSettingsDefaultsAndLanguageFallback();
    void itemAppearanceRoundTrip();
    void itemAppearanceDefaultsAndFallback();
    void sectionAndCategoryAppearanceRoundTrip();
    void sectionAndCategoryAppearanceDefaultsAndFallback();
    void conflictWarnings();
};

void CoreTests::hotkeyDisplayText() {
    HotkeyCombination hotkey;
    hotkey.modifiers = ModifierCtrl | ModifierAlt;
#ifdef Q_OS_WIN
    hotkey.key = 'K';
#else
    hotkey.key = 75;
#endif
    QCOMPARE(hotkey.displayText(), QString("Ctrl+Alt+K"));
    QVERIFY(hotkey.isValid());
}

void CoreTests::jsonRoundTrip() {
    HotkeyRule rule;
    rule.id = "rule-1";
    rule.enabled = true;
    rule.category = LauncherCategory::Website;
    rule.sectionId = "website-common";
    rule.hotkey.modifiers = ModifierWin;
#ifdef Q_OS_WIN
    rule.hotkey.key = 'E';
#else
    rule.hotkey.key = 69;
#endif
    rule.action.type = LaunchActionType::Url;
    rule.action.target = "https://example.com";
    rule.description = "Example";

    const HotkeyRule parsed = HotkeyRule::fromJson(rule.toJson());
    QCOMPARE(parsed.id, rule.id);
    QCOMPARE(parsed.enabled, rule.enabled);
    QCOMPARE(static_cast<int>(parsed.category), static_cast<int>(LauncherCategory::Website));
    QCOMPARE(parsed.sectionId, rule.sectionId);
    QCOMPARE(parsed.hotkey.displayText(), rule.hotkey.displayText());
    QCOMPARE(parsed.action.typeName(), QString("Url"));
    QCOMPARE(parsed.action.target, rule.action.target);
}

void CoreTests::sectionJsonRoundTrip() {
    LauncherSection section;
    section.id = "folder-system";
    section.category = LauncherCategory::Folder;
    section.name = QString::fromUtf8("系统文件夹");
    section.iconKey = "folder-system";
    section.sortOrder = 2;
    section.encrypted = true;
    section.passwordHash = "abc";
    section.collapsed = true;

    const LauncherSection parsed = LauncherSection::fromJson(section.toJson());
    QCOMPARE(parsed.id, section.id);
    QCOMPARE(static_cast<int>(parsed.category), static_cast<int>(LauncherCategory::Folder));
    QCOMPARE(parsed.name, section.name);
    QCOMPARE(parsed.iconKey, section.iconKey);
    QCOMPARE(parsed.sortOrder, section.sortOrder);
    QCOMPARE(parsed.encrypted, section.encrypted);
    QCOMPARE(parsed.passwordHash, section.passwordHash);
    QCOMPARE(parsed.collapsed, section.collapsed);

    QJsonObject legacy = section.toJson();
    legacy.remove("collapsed");
    QCOMPARE(LauncherSection::fromJson(legacy).collapsed, false);
}

void CoreTests::ruleCanBeValidWithoutHotkey() {
    HotkeyRule rule;
    rule.id = "rule-no-hotkey";
    rule.category = LauncherCategory::Program;
    rule.sectionId = "program-user";
    rule.action.type = LaunchActionType::Application;
    rule.action.target = "notepad.exe";

    QVERIFY(!rule.hotkey.isValid());
    QVERIFY(rule.isValid());
}

void CoreTests::legacyFileRuleMigratesToProgramCategory() {
    QJsonObject action;
    action["type"] = "File";
    action["target"] = "C:/Temp/readme.txt";

    QJsonObject object;
    object["id"] = "legacy-file";
    object["enabled"] = true;
    object["action"] = action;
    object["description"] = "Readme";

    const HotkeyRule parsed = HotkeyRule::fromJson(object);
    QCOMPARE(static_cast<int>(parsed.category), static_cast<int>(LauncherCategory::Program));
    QVERIFY(parsed.sectionId.isEmpty());
}

void CoreTests::appSettingsRoundTrip() {
    AppSettings settings;
    settings.language = "en-US";
    settings.hotkeysEnabled = false;
    settings.themeMode = "dark";
    settings.itemAppearance.itemWidth = 88;
    settings.itemAppearance.showEllipsis = true;
    settings.sectionAppearance.iconWidth = 22;
    settings.sectionAppearance.headerHeight = 40;
    settings.sectionAppearance.fontPointSize = 9;
    settings.sectionAppearance.textColor = "#123456";
    settings.categoryAppearance.iconWidth = 36;
    settings.categoryAppearance.buttonHeight = 42;
    settings.categoryAppearance.fontPointSize = 11;
    settings.categoryAppearance.textColor = "#abcdef";

    const AppSettings parsed = AppSettings::fromJson(settings.toJson());
    QCOMPARE(parsed.language, QString("en-US"));
    QCOMPARE(parsed.hotkeysEnabled, false);
    QCOMPARE(parsed.themeMode, QString("dark"));
    QCOMPARE(parsed.itemAppearance.itemWidth, 88);
    QCOMPARE(parsed.itemAppearance.showEllipsis, true);
    QCOMPARE(parsed.sectionAppearance.iconWidth, 22);
    QCOMPARE(parsed.sectionAppearance.headerHeight, 40);
    QCOMPARE(parsed.sectionAppearance.fontPointSize, 9);
    QCOMPARE(parsed.sectionAppearance.textColor, QString("#123456"));
    QCOMPARE(parsed.categoryAppearance.iconWidth, 36);
    QCOMPARE(parsed.categoryAppearance.buttonHeight, 42);
    QCOMPARE(parsed.categoryAppearance.fontPointSize, 11);
    QCOMPARE(parsed.categoryAppearance.textColor, QString("#abcdef"));
}

void CoreTests::appSettingsDefaultsAndLanguageFallback() {
    AppSettings defaults = AppSettings::fromJson({});
    QCOMPARE(defaults.language, QString("zh-CN"));
    QCOMPARE(defaults.hotkeysEnabled, true);
    QCOMPARE(defaults.themeMode, QString("system"));
    QCOMPARE(defaults.itemAppearance.iconWidth, 48);
    QCOMPARE(defaults.itemAppearance.itemWidth, 64);
    QCOMPARE(defaults.sectionAppearance.iconWidth, 18);
    QCOMPARE(defaults.sectionAppearance.headerHeight, 32);
    QCOMPARE(defaults.sectionAppearance.fontPointSize, 8);
    QCOMPARE(defaults.categoryAppearance.iconWidth, 32);
    QCOMPARE(defaults.categoryAppearance.buttonHeight, 30);
    QCOMPARE(defaults.categoryAppearance.fontPointSize, 10);

    QJsonObject invalid;
    invalid["language"] = "fr-FR";
    invalid["hotkeysEnabled"] = false;
    invalid["themeMode"] = "sepia";
    const AppSettings parsed = AppSettings::fromJson(invalid);
    QCOMPARE(parsed.language, QString("zh-CN"));
    QCOMPARE(parsed.hotkeysEnabled, false);
    QCOMPARE(parsed.themeMode, QString("system"));
}

void CoreTests::itemAppearanceRoundTrip() {
    LauncherItemAppearance appearance;
    appearance.iconWidth = 32;
    appearance.iconHeight = 28;
    appearance.itemWidth = 72;
    appearance.itemHeight = 96;
    appearance.fontFamily = "Segoe UI";
    appearance.fontPointSize = 9;
    appearance.horizontalSpacing = 4;
    appearance.verticalSpacing = 6;
    appearance.multilineText = false;
    appearance.showEllipsis = true;

    const LauncherItemAppearance parsed = LauncherItemAppearance::fromJson(appearance.toJson());
    QCOMPARE(parsed.iconWidth, 32);
    QCOMPARE(parsed.iconHeight, 28);
    QCOMPARE(parsed.itemWidth, 72);
    QCOMPARE(parsed.itemHeight, 96);
    QCOMPARE(parsed.fontFamily, QString("Segoe UI"));
    QCOMPARE(parsed.fontPointSize, 9);
    QCOMPARE(parsed.horizontalSpacing, 4);
    QCOMPARE(parsed.verticalSpacing, 6);
    QCOMPARE(parsed.multilineText, false);
    QCOMPARE(parsed.showEllipsis, true);
}

void CoreTests::itemAppearanceDefaultsAndFallback() {
    const LauncherItemAppearance defaults = LauncherItemAppearance::fromJson({});
    QCOMPARE(defaults.iconWidth, 48);
    QCOMPARE(defaults.iconHeight, 48);
    QCOMPARE(defaults.itemWidth, 64);
    QCOMPARE(defaults.itemHeight, 80);
    QCOMPARE(defaults.fontPointSize, 8);
    QCOMPARE(defaults.multilineText, true);
    QCOMPARE(defaults.showEllipsis, false);

    QJsonObject invalid;
    invalid["iconWidth"] = 1;
    invalid["iconHeight"] = 999;
    invalid["itemWidth"] = 10;
    invalid["itemHeight"] = 500;
    invalid["fontPointSize"] = 1;
    invalid["horizontalSpacing"] = -1;
    invalid["verticalSpacing"] = 999;
    invalid["multilineText"] = false;
    invalid["showEllipsis"] = true;
    const LauncherItemAppearance parsed = LauncherItemAppearance::fromJson(invalid);
    QVERIFY(parsed.iconWidth >= 16);
    QVERIFY(parsed.iconHeight <= 128);
    QVERIFY(parsed.itemWidth >= 40);
    QVERIFY(parsed.itemHeight <= 220);
    QVERIFY(parsed.fontPointSize >= 6);
    QCOMPARE(parsed.multilineText, false);
    QCOMPARE(parsed.showEllipsis, true);
}

void CoreTests::sectionAndCategoryAppearanceRoundTrip() {
    LauncherSectionAppearance section;
    section.iconWidth = 24;
    section.iconHeight = 20;
    section.headerHeight = 38;
    section.fontFamily = "Microsoft YaHei UI";
    section.fontPointSize = 9;
    section.textColor = "#336699";

    const LauncherSectionAppearance parsedSection = LauncherSectionAppearance::fromJson(section.toJson());
    QCOMPARE(parsedSection.iconWidth, 24);
    QCOMPARE(parsedSection.iconHeight, 20);
    QCOMPARE(parsedSection.headerHeight, 38);
    QCOMPARE(parsedSection.fontFamily, QString("Microsoft YaHei UI"));
    QCOMPARE(parsedSection.fontPointSize, 9);
    QCOMPARE(parsedSection.textColor, QString("#336699"));

    LauncherCategoryAppearance category;
    category.iconWidth = 40;
    category.iconHeight = 36;
    category.buttonHeight = 44;
    category.fontFamily = "Segoe UI";
    category.fontPointSize = 11;
    category.textColor = "#663399";

    const LauncherCategoryAppearance parsedCategory = LauncherCategoryAppearance::fromJson(category.toJson());
    QCOMPARE(parsedCategory.iconWidth, 40);
    QCOMPARE(parsedCategory.iconHeight, 36);
    QCOMPARE(parsedCategory.buttonHeight, 44);
    QCOMPARE(parsedCategory.fontFamily, QString("Segoe UI"));
    QCOMPARE(parsedCategory.fontPointSize, 11);
    QCOMPARE(parsedCategory.textColor, QString("#663399"));
}

void CoreTests::sectionAndCategoryAppearanceDefaultsAndFallback() {
    const LauncherSectionAppearance sectionDefaults = LauncherSectionAppearance::fromJson({});
    QCOMPARE(sectionDefaults.iconWidth, 18);
    QCOMPARE(sectionDefaults.iconHeight, 18);
    QCOMPARE(sectionDefaults.headerHeight, 32);
    QCOMPARE(sectionDefaults.fontPointSize, 8);
    QCOMPARE(sectionDefaults.textColor, QString());

    const LauncherCategoryAppearance categoryDefaults = LauncherCategoryAppearance::fromJson({});
    QCOMPARE(categoryDefaults.iconWidth, 32);
    QCOMPARE(categoryDefaults.iconHeight, 32);
    QCOMPARE(categoryDefaults.buttonHeight, 30);
    QCOMPARE(categoryDefaults.fontPointSize, 10);
    QCOMPARE(categoryDefaults.textColor, QString());

    QJsonObject invalid;
    invalid["iconWidth"] = 1;
    invalid["iconHeight"] = 999;
    invalid["headerHeight"] = 1;
    invalid["buttonHeight"] = 999;
    invalid["fontPointSize"] = 100;
    invalid["textColor"] = "not-a-color";
    const LauncherSectionAppearance parsedSection = LauncherSectionAppearance::fromJson(invalid);
    QVERIFY(parsedSection.iconWidth >= 12);
    QVERIFY(parsedSection.iconHeight <= 96);
    QVERIFY(parsedSection.headerHeight >= 24);
    QVERIFY(parsedSection.fontPointSize <= 18);
    QCOMPARE(parsedSection.textColor, QString());

    const LauncherCategoryAppearance parsedCategory = LauncherCategoryAppearance::fromJson(invalid);
    QVERIFY(parsedCategory.iconWidth >= 12);
    QVERIFY(parsedCategory.iconHeight <= 96);
    QVERIFY(parsedCategory.buttonHeight <= 96);
    QVERIFY(parsedCategory.fontPointSize <= 18);
    QCOMPARE(parsedCategory.textColor, QString());
}

void CoreTests::conflictWarnings() {
    RuleStore store;
    HotkeyRule first;
    first.id = "one";
    first.enabled = true;
    first.category = LauncherCategory::Program;
    first.sectionId = "program-user";
    first.hotkey.modifiers = ModifierWin;
#ifdef Q_OS_WIN
    first.hotkey.key = 'E';
#else
    first.hotkey.key = 69;
#endif
    first.action.target = "explorer.exe";

    HotkeyRule second = first;
    second.id = "two";

    QVector<HotkeyRule> existingRules;
    existingRules.push_back(first);
    const QStringList warnings = store.warningsForRule(second, existingRules);
    QVERIFY(!warnings.isEmpty());

    HotkeyRule shellShortcut = first;
    shellShortcut.id = "shell-search";
    shellShortcut.hotkey.modifiers = ModifierWin;
#ifdef Q_OS_WIN
    shellShortcut.hotkey.key = 'S';
#else
    shellShortcut.hotkey.key = 83;
#endif
    QVERIFY(HotkeyConflictDetector::isKnownSystemHotkey(shellShortcut.hotkey));
    QVERIFY(!store.warningsForRule(shellShortcut, QVector<HotkeyRule>()).isEmpty());

    HotkeyRule noHotkey = first;
    noHotkey.id = "three";
    noHotkey.hotkey = HotkeyCombination();
    QVERIFY(store.warningsForRule(noHotkey, existingRules).isEmpty());
}

QTEST_APPLESS_MAIN(CoreTests)
#include "test_core.moc"
