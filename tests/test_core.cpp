#include "../src/HotkeyConflictDetector.h"
#include "../src/HotkeyTypes.h"
#include "../src/PathUtils.h"
#include "../src/RuleStore.h"
#include "../src/UpdateChecker.h"

#include <QFile>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class CoreTests : public QObject {
    Q_OBJECT

private slots:
    void hotkeyDisplayText();
    void launchActionWindowOptionsRoundTrip();
    void launchActionWindowOptionsDefaultsAndFallback();
    void portablePathConversion();
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
    void defaultSystemToolsAreSeededWithoutHotkeys();
    void conflictWarnings();
    void updateVersionComparison();
    void updateCheckerUsesDefaultManifestUrl();
    void updateManifestSelectsCurrentAsset();
    void updateManifestLegacyDownloadUrl();
    void updateManifestAssetUrlFromReleasePayload();
    void updateManifestDecodesGithubContentsPayload();
    void updateManifestOlderThanCurrentIsNotUpdate();
    void updateManifestWithoutMatchingAssetReturnsError();
    void updateSha256Verification();
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

void CoreTests::launchActionWindowOptionsRoundTrip() {
    LaunchAction action;
    action.type = LaunchActionType::Application;
    action.target = "notepad.exe";
    action.arguments = "/A";
    action.workingDirectory = "C:/Windows";
    action.windowState = LaunchWindowState::Maximized;
    action.singleInstance = true;

    const QJsonObject json = action.toJson();
    QCOMPARE(json.value("windowState").toString(), QString("Maximized"));
    QCOMPARE(json.value("singleInstance").toBool(), true);

    const LaunchAction parsed = LaunchAction::fromJson(json);
    QCOMPARE(static_cast<int>(parsed.type), static_cast<int>(LaunchActionType::Application));
    QCOMPARE(parsed.target, action.target);
    QCOMPARE(parsed.arguments, action.arguments);
    QCOMPARE(parsed.workingDirectory, action.workingDirectory);
    QCOMPARE(static_cast<int>(parsed.windowState), static_cast<int>(LaunchWindowState::Maximized));
    QCOMPARE(parsed.singleInstance, true);
}

void CoreTests::launchActionWindowOptionsDefaultsAndFallback() {
    QJsonObject legacy;
    legacy["type"] = "Application";
    legacy["target"] = "notepad.exe";

    const LaunchAction defaults = LaunchAction::fromJson(legacy);
    QCOMPARE(static_cast<int>(defaults.windowState), static_cast<int>(LaunchWindowState::Normal));
    QCOMPARE(defaults.singleInstance, false);

    QJsonObject invalid = legacy;
    invalid["windowState"] = "Fullscreen";
    invalid["singleInstance"] = true;
    const LaunchAction parsed = LaunchAction::fromJson(invalid);
    QCOMPARE(static_cast<int>(parsed.windowState), static_cast<int>(LaunchWindowState::Normal));
    QCOMPARE(parsed.singleInstance, true);
}

void CoreTests::portablePathConversion() {
    const QString base = QString::fromLatin1("D:/Tools/WStart");
    QCOMPARE(PathUtils::toAbsolutePath(QString::fromLatin1("notepad.exe"), base), QString::fromLatin1("notepad.exe"));
    QCOMPARE(PathUtils::toAbsolutePath(QString::fromLatin1("ms-settings:"), base), QString::fromLatin1("ms-settings:"));

#ifdef Q_OS_WIN
    QCOMPARE(PathUtils::toPortablePath(QString::fromLatin1("D:/Tools/WStart/Foo.exe"), base),
             QString::fromLatin1("./Foo.exe"));
    QCOMPARE(PathUtils::toPortablePath(QString::fromLatin1("D:/Tools/WStart/apps/Foo.exe"), base),
             QString::fromLatin1("apps/Foo.exe"));
    QCOMPARE(PathUtils::toPortablePath(QString::fromLatin1("E:/Apps/Foo.exe"), base),
             QString::fromLatin1("E:/Apps/Foo.exe"));
    QCOMPARE(PathUtils::toAbsolutePath(QString::fromLatin1("./Foo.exe"), base),
             QString::fromLatin1("D:/Tools/WStart/Foo.exe"));
    QCOMPARE(PathUtils::toAbsolutePath(QString::fromLatin1("apps/Foo.exe"), base),
             QString::fromLatin1("D:/Tools/WStart/apps/Foo.exe"));

    LaunchAction action;
    action.type = LaunchActionType::Application;
    action.target = QString::fromLatin1("D:/Tools/WStart/apps/Foo.exe");
    action.workingDirectory = QString::fromLatin1("D:/Tools/WStart/apps");
    const LaunchAction portableAction = PathUtils::toPortableAction(action, base);
    QCOMPARE(portableAction.target, QString::fromLatin1("apps/Foo.exe"));
    QCOMPARE(portableAction.workingDirectory, QString::fromLatin1("./apps"));
#endif
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
    rule.action.windowState = LaunchWindowState::Minimized;
    rule.action.singleInstance = true;
    rule.description = "Example";

    const HotkeyRule parsed = HotkeyRule::fromJson(rule.toJson());
    QCOMPARE(parsed.id, rule.id);
    QCOMPARE(parsed.enabled, rule.enabled);
    QCOMPARE(static_cast<int>(parsed.category), static_cast<int>(LauncherCategory::Website));
    QCOMPARE(parsed.sectionId, rule.sectionId);
    QCOMPARE(parsed.hotkey.displayText(), rule.hotkey.displayText());
    QCOMPARE(parsed.action.typeName(), QString("Url"));
    QCOMPARE(parsed.action.target, rule.action.target);
    QCOMPARE(static_cast<int>(parsed.action.windowState), static_cast<int>(LaunchWindowState::Minimized));
    QCOMPARE(parsed.action.singleInstance, true);
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
    settings.updatesEnabled = false;
    settings.startupEnabled = true;
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
    QCOMPARE(parsed.updatesEnabled, false);
    QCOMPARE(parsed.startupEnabled, true);
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
    QCOMPARE(defaults.updatesEnabled, true);
    QCOMPARE(defaults.startupEnabled, false);
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
    invalid["updatesEnabled"] = false;
    invalid["startupEnabled"] = true;
    invalid["themeMode"] = "sepia";
    const AppSettings parsed = AppSettings::fromJson(invalid);
    QCOMPARE(parsed.language, QString("zh-CN"));
    QCOMPARE(parsed.hotkeysEnabled, false);
    QCOMPARE(parsed.updatesEnabled, false);
    QCOMPARE(parsed.startupEnabled, true);
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
    QVERIFY(parsedSection.headerHeight >= 12);
    QVERIFY(parsedSection.fontPointSize <= 18);
    QCOMPARE(parsedSection.textColor, QString());

    const LauncherCategoryAppearance parsedCategory = LauncherCategoryAppearance::fromJson(invalid);
    QVERIFY(parsedCategory.iconWidth >= 12);
    QVERIFY(parsedCategory.iconHeight <= 96);
    QVERIFY(parsedCategory.buttonHeight <= 96);
    QVERIFY(parsedCategory.fontPointSize <= 18);
    QCOMPARE(parsedCategory.textColor, QString());

    QJsonObject tooSmall;
    tooSmall["headerHeight"] = 1;
    tooSmall["buttonHeight"] = 1;
    QVERIFY(LauncherSectionAppearance::fromJson(tooSmall).headerHeight >= 12);
    QVERIFY(LauncherCategoryAppearance::fromJson(tooSmall).buttonHeight >= 12);
}

void CoreTests::defaultSystemToolsAreSeededWithoutHotkeys() {
    const QVector<HotkeyRule> rules = RuleStore::defaultSystemProgramRules();
    QStringList actualNames;
    QHash<QString, HotkeyRule> rulesByName;
    QSet<QString> ruleIds;
    for (const HotkeyRule& rule : rules) {
        actualNames << rule.description;
        rulesByName.insert(rule.description, rule);
        QVERIFY2(!ruleIds.contains(rule.id), qPrintable(QString("Duplicate default rule id: %1").arg(rule.id)));
        ruleIds.insert(rule.id);
        QVERIFY(!rule.hotkey.isValid());
        QCOMPARE(rule.sectionId, QString("program-system"));
        QCOMPARE(static_cast<int>(rule.category), static_cast<int>(LauncherCategory::Program));
        QCOMPARE(static_cast<int>(rule.action.type), static_cast<int>(LaunchActionType::Application));
        QVERIFY(!rule.action.target.trimmed().isEmpty());
    }

    QStringList expectedNames;
    expectedNames << QString::fromUtf8("我的文档") << QString::fromUtf8("我的电脑") << QString::fromUtf8("网络连接")
                  << QString::fromUtf8("控制面板") << QString::fromUtf8("回收站") << QString::fromUtf8("打印机")
                  << QString::fromUtf8("显示") << QString::fromUtf8("截图") << QString::fromUtf8("日期时间")
                  << QString::fromUtf8("Internet") << QString::fromUtf8("系统属性") << QString::fromUtf8("环境变量")
                  << QString::fromUtf8("系统信息")
                  << QString::fromUtf8("系统配置") << QString::fromUtf8("文件夹选项") << QString::fromUtf8("设备管理器")
                  << QString::fromUtf8("添加删除程序") << QString::fromUtf8("记事本") << QString::fromUtf8("磁盘清理")
                  << QString::fromUtf8("磁盘管理") << QString::fromUtf8("计算机管理") << QString::fromUtf8("服务")
                  << QString::fromUtf8("组策略") << QString::fromUtf8("计算器") << QString::fromUtf8("注册表")
                  << QString::fromUtf8("命令提示符") << QString::fromUtf8("远程桌面") << QString::fromUtf8("任务管理器")
                  << QString::fromUtf8("鼠标") << QString::fromUtf8("键盘") << QString::fromUtf8("屏幕键盘")
                  << QString::fromUtf8("声音") << QString::fromUtf8("音量") << QString::fromUtf8("电源选项")
                  << QString::fromUtf8("防火墙") << QString::fromUtf8("UAC") << QString::fromUtf8("关闭计算机")
                  << QString::fromUtf8("重启计算机") << QString::fromUtf8("关闭显示器");

    for (const QString& name : expectedNames) {
        QVERIFY2(actualNames.contains(name), qPrintable(QString("Missing default system tool: %1").arg(name)));
    }
    QCOMPARE(rulesByName.value(QString::fromUtf8("截图")).action.target, QString("ms-screenclip:"));
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

void CoreTests::updateVersionComparison() {
    QVERIFY(UpdateChecker::versionGreaterThan("0.3.10", "0.3.9"));
    QVERIFY(UpdateChecker::versionGreaterThan("v0.4.0", "0.3.9"));
    QVERIFY(!UpdateChecker::versionGreaterThan("0.3.5", "0.3.5"));
    QVERIFY(!UpdateChecker::versionGreaterThan("0.3.4", "0.3.5"));
}

void CoreTests::updateCheckerUsesDefaultManifestUrl() {
    UpdateChecker checker;
    QCOMPARE(checker.manifestUrl(), QString("https://api.github.com/repos/weaver2007/WStart/releases/latest"));
}

void CoreTests::updateManifestSelectsCurrentAsset() {
    const QString platform = UpdateChecker::currentPlatformKey();
    const QString arch = UpdateChecker::currentArchKey();
    const QByteArray json = QString(R"({
        "version": "9.9.9",
        "releaseNotes": "Test release",
        "assets": [
          {"platform": "%1", "arch": "%2", "type": "installer", "url": "https://example.com/setup.exe", "apiUrl": "https://api.example.com/assets/1"},
          {"platform": "%1", "arch": "%2", "type": "portable", "url": "https://example.com/portable.zip"}
        ]
    })")
                                  .arg(platform, arch)
                                  .toUtf8();

    QString error;
    const UpdateInfo installed =
        UpdateChecker::parseManifest(json, "https://example.com/update.json", "0.1.0", false, &error);
    QVERIFY(error.isEmpty());
    QVERIFY(installed.updateAvailable);
    QCOMPARE(installed.asset.type, QString("installer"));
    QCOMPARE(installed.asset.url, QString("https://api.example.com/assets/1"));

    const UpdateInfo portable =
        UpdateChecker::parseManifest(json, "https://example.com/update.json", "0.1.0", true, &error);
    QVERIFY(error.isEmpty());
    QVERIFY(portable.updateAvailable);
    QCOMPARE(portable.asset.type, QString("portable"));
}

void CoreTests::updateManifestLegacyDownloadUrl() {
    const QByteArray json = R"({
        "version": "1.0.0",
        "downloadUrl": "https://example.com/WStart.zip",
        "sha256": "abc",
        "releaseNotes": "Legacy"
    })";

    QString error;
    const UpdateInfo info = UpdateChecker::parseManifest(json, "https://example.com/update.json", "0.9.0", true, &error);
    QVERIFY(error.isEmpty());
    QVERIFY(info.updateAvailable);
    QCOMPARE(info.asset.url, QString("https://example.com/WStart.zip"));
    QCOMPARE(info.asset.sha256, QString("abc"));
}

void CoreTests::updateManifestAssetUrlFromReleasePayload() {
    const QByteArray json = R"({
        "tag_name": "v0.3.8",
        "assets": [
          {"name": "WStart-0.3.8-windows-x64-setup.exe", "url": "https://api.github.com/assets/1"},
          {"name": "update.json", "url": "https://api.github.com/assets/2"}
        ]
    })";

    QCOMPARE(UpdateChecker::updateManifestAssetUrlFromReleasePayload(json), QString("https://api.github.com/assets/2"));
}

void CoreTests::updateManifestDecodesGithubContentsPayload() {
    const QString platform = UpdateChecker::currentPlatformKey();
    const QString arch = UpdateChecker::currentArchKey();
    const QByteArray manifest = QString(R"({
        "version": "0.3.7",
        "assets": [
          {"platform": "%1", "arch": "%2", "type": "installer", "url": "https://example.com/setup.exe"}
        ]
    })")
                                      .arg(platform, arch)
                                      .toUtf8();
    const QString encoded = QString::fromLatin1(manifest.toBase64());
    const QByteArray payload = QString(R"({
        "name": "update.json",
        "encoding": "base64",
        "content": "%1"
    })")
                                   .arg(encoded)
                                   .toUtf8();

    QString error;
    const UpdateInfo info = UpdateChecker::parseManifest(
        payload, "https://api.github.com/repos/weaver2007/HotKeyManager/contents/update.json?ref=main", "0.3.6",
        false, &error);
    QVERIFY(error.isEmpty());
    QVERIFY(info.updateAvailable);
    QCOMPARE(info.latestVersion, QString("0.3.7"));
    QCOMPARE(info.asset.url, QString("https://example.com/setup.exe"));
}

void CoreTests::updateManifestOlderThanCurrentIsNotUpdate() {
    const QByteArray json = R"({
        "version": "0.3.5",
        "downloadUrl": "https://example.com/WStart.zip"
    })";

    QString error;
    const UpdateInfo info = UpdateChecker::parseManifest(json, "https://example.com/update.json", "0.3.6", true, &error);
    QVERIFY(error.isEmpty());
    QVERIFY(!info.updateAvailable);
    QCOMPARE(info.latestVersion, QString("0.3.5"));
}

void CoreTests::updateManifestWithoutMatchingAssetReturnsError() {
    const QByteArray json = R"({
        "version": "9.9.9",
        "pageUrl": "https://github.com/weaver2007/HotKeyManager/releases/tag/v9.9.9",
        "assets": []
    })";

    QString error;
    const UpdateInfo info = UpdateChecker::parseManifest(json, "https://example.com/update.json", "0.3.8", false, &error);
    QVERIFY(!error.isEmpty());
    QVERIFY(!info.updateAvailable);
    QVERIFY(!info.asset.isValid());
    QVERIFY(!info.asset.url.contains("/releases/tag/"));
}

void CoreTests::updateSha256Verification() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QDir(dir.path()).filePath("payload.txt");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("abc");
    file.close();

    QString error;
    QVERIFY(UpdateChecker::verifySha256(path, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                                        &error));
    QVERIFY(!UpdateChecker::verifySha256(path, "deadbeef", &error));
    QVERIFY(!error.isEmpty());
}

QTEST_APPLESS_MAIN(CoreTests)
#include "test_core.moc"
