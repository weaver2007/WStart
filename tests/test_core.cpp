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
    void conflictWarnings();
};

void CoreTests::hotkeyDisplayText()
{
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

void CoreTests::jsonRoundTrip()
{
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

void CoreTests::sectionJsonRoundTrip()
{
    LauncherSection section;
    section.id = "folder-system";
    section.category = LauncherCategory::Folder;
    section.name = QString::fromUtf8("系统文件夹");
    section.iconKey = "folder-system";
    section.sortOrder = 2;
    section.encrypted = true;
    section.passwordHash = "abc";
    section.viewMode = "details";
    section.collapsed = true;

    const LauncherSection parsed = LauncherSection::fromJson(section.toJson());
    QCOMPARE(parsed.id, section.id);
    QCOMPARE(static_cast<int>(parsed.category), static_cast<int>(LauncherCategory::Folder));
    QCOMPARE(parsed.name, section.name);
    QCOMPARE(parsed.iconKey, section.iconKey);
    QCOMPARE(parsed.sortOrder, section.sortOrder);
    QCOMPARE(parsed.encrypted, section.encrypted);
    QCOMPARE(parsed.passwordHash, section.passwordHash);
    QCOMPARE(parsed.viewMode, section.viewMode);
    QCOMPARE(parsed.collapsed, section.collapsed);

    QJsonObject invalid = section.toJson();
    invalid["viewMode"] = "cards";
    QCOMPARE(LauncherSection::fromJson(invalid).viewMode, QString("mediumIcons"));

    QJsonObject legacy = section.toJson();
    legacy.remove("collapsed");
    QCOMPARE(LauncherSection::fromJson(legacy).collapsed, false);
}

void CoreTests::ruleCanBeValidWithoutHotkey()
{
    HotkeyRule rule;
    rule.id = "rule-no-hotkey";
    rule.category = LauncherCategory::Program;
    rule.sectionId = "program-user";
    rule.action.type = LaunchActionType::Application;
    rule.action.target = "notepad.exe";

    QVERIFY(!rule.hotkey.isValid());
    QVERIFY(rule.isValid());
}

void CoreTests::legacyFileRuleMigratesToProgramCategory()
{
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

void CoreTests::appSettingsRoundTrip()
{
    AppSettings settings;
    settings.language = "en-US";
    settings.hotkeysEnabled = false;
    settings.themeMode = "dark";

    const AppSettings parsed = AppSettings::fromJson(settings.toJson());
    QCOMPARE(parsed.language, QString("en-US"));
    QCOMPARE(parsed.hotkeysEnabled, false);
    QCOMPARE(parsed.themeMode, QString("dark"));
}

void CoreTests::appSettingsDefaultsAndLanguageFallback()
{
    AppSettings defaults = AppSettings::fromJson({});
    QCOMPARE(defaults.language, QString("zh-CN"));
    QCOMPARE(defaults.hotkeysEnabled, true);
    QCOMPARE(defaults.themeMode, QString("system"));

    QJsonObject invalid;
    invalid["language"] = "fr-FR";
    invalid["hotkeysEnabled"] = false;
    invalid["themeMode"] = "sepia";
    const AppSettings parsed = AppSettings::fromJson(invalid);
    QCOMPARE(parsed.language, QString("zh-CN"));
    QCOMPARE(parsed.hotkeysEnabled, false);
    QCOMPARE(parsed.themeMode, QString("system"));
}

void CoreTests::conflictWarnings()
{
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

    const QStringList warnings = store.warningsForRule(second, {first});
    QVERIFY(!warnings.isEmpty());

    HotkeyRule noHotkey = first;
    noHotkey.id = "three";
    noHotkey.hotkey = {};
    QVERIFY(store.warningsForRule(noHotkey, {first}).isEmpty());
}

QTEST_MAIN(CoreTests)
#include "test_core.moc"
