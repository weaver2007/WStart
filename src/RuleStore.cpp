#include "RuleStore.h"

#include "HotkeyConflictDetector.h"
#include "UiText.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

RuleStore::RuleStore(QObject *parent)
    : QObject(parent)
{
}

LauncherDocument RuleStore::loadDocument(QString *error) const
{
    QFile file(configPath());
    if (!file.exists()) {
        return createDefaultDocument();
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return createDefaultDocument();
    }

    const QJsonDocument json = QJsonDocument::fromJson(file.readAll());
    if (!json.isObject()) {
        if (error) {
            *error = "Configuration root must be an object.";
        }
        return createDefaultDocument();
    }

    LauncherDocument document;
    const QJsonObject root = json.object();
    document.settings = AppSettings::fromJson(root.value("settings").toObject());

    const QJsonArray sectionArray = root.value("sections").toArray();
    for (const QJsonValue &value : sectionArray) {
        LauncherSection section = LauncherSection::fromJson(value.toObject());
        if (section.isValid()) {
            document.sections.push_back(section);
        }
    }

    ensureDefaultSections(&document);

    const QJsonArray ruleArray = root.value("rules").toArray();
    for (const QJsonValue &value : ruleArray) {
        HotkeyRule rule = HotkeyRule::fromJson(value.toObject());
        if (rule.sectionId.isEmpty()) {
            rule.sectionId = defaultSectionId(rule.category, 1);
        }
        if (rule.isValid()) {
            document.rules.push_back(rule);
        }
    }

    return document;
}

bool RuleStore::saveDocument(const LauncherDocument &document, QString *error) const
{
    QDir directory(configDirectory());
    if (!directory.exists() && !directory.mkpath(".")) {
        if (error) {
            *error = "Unable to create configuration directory.";
        }
        return false;
    }

    QJsonArray sections;
    for (const LauncherSection &section : document.sections) {
        if (section.isValid()) {
            sections.append(section.toJson());
        }
    }

    QJsonArray rules;
    for (const HotkeyRule &rule : document.rules) {
        if (rule.isValid()) {
            rules.append(rule.toJson());
        }
    }

    QJsonObject root;
    root["version"] = 2;
    root["settings"] = document.settings.toJson();
    root["sections"] = sections;
    root["rules"] = rules;

    QFile file(configPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

QVector<HotkeyRule> RuleStore::load(QString *error) const
{
    return loadDocument(error).rules;
}

bool RuleStore::save(const QVector<HotkeyRule> &rules, QString *error) const
{
    LauncherDocument document = loadDocument();
    document.rules = rules;
    ensureDefaultSections(&document);
    return saveDocument(document, error);
}

QString RuleStore::configPath() const
{
    return QDir(configDirectory()).filePath("rules.json");
}

QStringList RuleStore::warningsForRule(const HotkeyRule &rule, const QVector<HotkeyRule> &rules, const QString &language) const
{
    QStringList warnings;
    if (rule.hotkey.isValid()) {
        for (const HotkeyRule &existing : rules) {
            if (existing.id != rule.id && existing.enabled && existing.hotkey.isValid() && existing.hotkey.stableId() == rule.hotkey.stableId()) {
                warnings << UiText::text(language, UiText::Key::HotkeyDuplicateWarning);
                break;
            }
        }

        if (HotkeyConflictDetector::isKnownSystemHotkey(rule.hotkey)) {
            warnings << UiText::text(language, UiText::Key::HotkeySystemShortcutWarning);
        }
    }
    return warnings;
}

LauncherDocument RuleStore::createDefaultDocument() const
{
    LauncherDocument document;
    ensureDefaultSections(&document);

    auto makeRule = [](const QString &sectionId, const QString &name, const QString &target, const QString &arguments = QString()) {
        HotkeyRule rule;
        rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        rule.category = LauncherCategory::Program;
        rule.sectionId = sectionId;
        rule.action.type = LaunchActionType::Application;
        rule.action.target = target;
        rule.action.arguments = arguments;
        rule.description = name;
        return rule;
    };

    const QString systemSection = defaultSectionId(LauncherCategory::Program, 0);
    document.rules.push_back(makeRule(systemSection, QString::fromUtf8("控制面板"), "control.exe"));
    document.rules.push_back(makeRule(systemSection, QString::fromUtf8("任务管理器"), "taskmgr.exe"));
    document.rules.push_back(makeRule(systemSection, QString::fromUtf8("命令提示符"), "cmd.exe"));
    document.rules.push_back(makeRule(systemSection, QString::fromUtf8("注册表"), "regedit.exe"));
    document.rules.push_back(makeRule(systemSection, QString::fromUtf8("服务"), "services.msc"));
    document.rules.push_back(makeRule(systemSection, QString::fromUtf8("设备管理器"), "devmgmt.msc"));
    document.rules.push_back(makeRule(systemSection, QString::fromUtf8("计算器"), "calc.exe"));
    document.rules.push_back(makeRule(systemSection, QString::fromUtf8("系统信息"), "msinfo32.exe"));

    auto makeFolderRule = [](const QString &sectionId, const QString &name, const QString &target) {
        HotkeyRule rule;
        rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        rule.category = LauncherCategory::Folder;
        rule.sectionId = sectionId;
        rule.action.type = LaunchActionType::Folder;
        rule.action.target = target;
        rule.description = name;
        return rule;
    };

    const QString folderSection = defaultSectionId(LauncherCategory::Folder, 0);
    document.rules.push_back(makeFolderRule(folderSection, QString::fromUtf8("我的文档"), QDir::home().filePath("Documents")));
    document.rules.push_back(makeFolderRule(folderSection, QString::fromUtf8("桌面"), QDir::home().filePath("Desktop")));
    document.rules.push_back(makeFolderRule(folderSection, QString::fromUtf8("下载"), QDir::home().filePath("Downloads")));

    auto makeWebsiteRule = [](const QString &sectionId, const QString &name, const QString &target) {
        HotkeyRule rule;
        rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        rule.category = LauncherCategory::Website;
        rule.sectionId = sectionId;
        rule.action.type = LaunchActionType::Url;
        rule.action.target = target;
        rule.description = name;
        return rule;
    };

    const QString websiteSection = defaultSectionId(LauncherCategory::Website, 0);
    document.rules.push_back(makeWebsiteRule(websiteSection, QString::fromUtf8("百度"), "https://www.baidu.com"));
    document.rules.push_back(makeWebsiteRule(websiteSection, QString::fromUtf8("必应"), "https://www.bing.com"));

    return document;
}

void RuleStore::ensureDefaultSections(LauncherDocument *document) const
{
    auto hasSection = [document](const QString &id) {
        return std::any_of(document->sections.cbegin(), document->sections.cend(), [&id](const LauncherSection &section) {
            return section.id == id;
        });
    };

    auto addSection = [document, &hasSection](LauncherCategory category, int order, const QString &id, const QString &name, const QString &iconKey) {
        if (hasSection(id)) {
            return;
        }
        LauncherSection section;
        section.id = id;
        section.category = category;
        section.name = name;
        section.iconKey = iconKey;
        section.sortOrder = order;
        document->sections.push_back(section);
    };

    addSection(LauncherCategory::Program, 0, defaultSectionId(LauncherCategory::Program, 0), QString::fromUtf8("系统功能"), "system");
    addSection(LauncherCategory::Program, 1, defaultSectionId(LauncherCategory::Program, 1), QString::fromUtf8("我的程序"), "program");
    addSection(LauncherCategory::Folder, 0, defaultSectionId(LauncherCategory::Folder, 0), QString::fromUtf8("系统文件夹"), "folder-system");
    addSection(LauncherCategory::Folder, 1, defaultSectionId(LauncherCategory::Folder, 1), QString::fromUtf8("我的目录"), "folder");
    addSection(LauncherCategory::Website, 0, defaultSectionId(LauncherCategory::Website, 0), QString::fromUtf8("常用网址"), "website");
    addSection(LauncherCategory::Website, 1, defaultSectionId(LauncherCategory::Website, 1), QString::fromUtf8("我的网址"), "website-user");
}

QString RuleStore::defaultSectionId(LauncherCategory category, int index) const
{
    switch (category) {
    case LauncherCategory::Program:
        return index == 0 ? "program-system" : "program-user";
    case LauncherCategory::Folder:
        return index == 0 ? "folder-system" : "folder-user";
    case LauncherCategory::Website:
        return index == 0 ? "website-common" : "website-user";
    }
    return "program-user";
}

QString RuleStore::configDirectory() const
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!appData.isEmpty()) {
        return appData;
    }
    return QDir::home().filePath(".HotKeyManager");
}
