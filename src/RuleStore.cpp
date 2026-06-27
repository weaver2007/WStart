#include "RuleStore.h"

#include "HotkeyConflictDetector.h"
#include "QtCompat.h"
#include "UiText.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

namespace {

HotkeyRule makeProgramRule(const QString& sectionId, const QString& name, const QString& target,
                           const QString& arguments = QString()) {
    HotkeyRule rule;
    rule.id = QtCompat::uuidWithoutBraces();
    rule.category = LauncherCategory::Program;
    rule.sectionId = sectionId;
    rule.action.type = LaunchActionType::Application;
    rule.action.target = target;
    rule.action.arguments = arguments;
    rule.description = name;
    return rule;
}

HotkeyRule makeFolderRule(const QString& sectionId, const QString& name, const QString& target) {
    HotkeyRule rule;
    rule.id = QtCompat::uuidWithoutBraces();
    rule.category = LauncherCategory::Folder;
    rule.sectionId = sectionId;
    rule.action.type = LaunchActionType::Folder;
    rule.action.target = target;
    rule.description = name;
    return rule;
}

HotkeyRule makeWebsiteRule(const QString& sectionId, const QString& name, const QString& target) {
    HotkeyRule rule;
    rule.id = QtCompat::uuidWithoutBraces();
    rule.category = LauncherCategory::Website;
    rule.sectionId = sectionId;
    rule.action.type = LaunchActionType::Url;
    rule.action.target = target;
    rule.description = name;
    return rule;
}

} // namespace

QVector<HotkeyRule> RuleStore::defaultSystemProgramRules() {
    const QString systemSection = "program-system";
    struct DefaultProgramRule {
        const char* name;
        const char* target;
        const char* arguments;
    };

    const DefaultProgramRule specs[] = {
        {"我的文档", "explorer.exe", "shell:Personal"},
        {"我的电脑", "explorer.exe", "shell:MyComputerFolder"},
        {"网络连接", "control.exe", "ncpa.cpl"},
        {"控制面板", "control.exe", ""},
        {"回收站", "explorer.exe", "shell:RecycleBinFolder"},
        {"打印机", "control.exe", "printers"},
        {"显示", "control.exe", "desk.cpl"},
        {"截图", "ms-screenclip:", ""},
        {"日期时间", "control.exe", "timedate.cpl"},
        {"Internet", "control.exe", "inetcpl.cpl"},
        {"系统属性", "control.exe", "sysdm.cpl"},
        {"系统信息", "msinfo32.exe", ""},
        {"系统配置", "msconfig.exe", ""},
        {"文件夹选项", "control.exe", "folders"},
        {"设备管理器", "devmgmt.msc", ""},
        {"添加删除程序", "control.exe", "appwiz.cpl"},
        {"记事本", "notepad.exe", ""},
        {"磁盘清理", "cleanmgr.exe", ""},
        {"磁盘管理", "diskmgmt.msc", ""},
        {"计算机管理", "compmgmt.msc", ""},
        {"服务", "services.msc", ""},
        {"组策略", "gpedit.msc", ""},
        {"计算器", "calc.exe", ""},
        {"注册表", "regedit.exe", ""},
        {"命令提示符", "cmd.exe", ""},
        {"远程桌面", "mstsc.exe", ""},
        {"任务管理器", "taskmgr.exe", ""},
        {"鼠标", "control.exe", "mouse"},
        {"键盘", "control.exe", "keyboard"},
        {"屏幕键盘", "osk.exe", ""},
        {"声音", "control.exe", "mmsys.cpl"},
        {"音量", "sndvol.exe", ""},
        {"电源选项", "control.exe", "powercfg.cpl"},
        {"防火墙", "control.exe", "firewall.cpl"},
        {"UAC", "UserAccountControlSettings.exe", ""},
        {"关闭计算机", "shutdown.exe", "/s /t 0"},
        {"重启计算机", "shutdown.exe", "/r /t 0"},
        {"关闭显示器", "powershell.exe", R"wstart(-NoProfile -ExecutionPolicy Bypass -Command "(Add-Type '[DllImport(\"user32.dll\")] public static extern int SendMessage(int hWnd, int hMsg, int wParam, int lParam);' -Name Native -Namespace WStart -PassThru)::SendMessage(-1, 0x0112, 0xF170, 2)")wstart"},
    };

    QVector<HotkeyRule> rules;
    for (const DefaultProgramRule& spec : specs) {
        rules.push_back(makeProgramRule(systemSection, QString::fromUtf8(spec.name), QString::fromLatin1(spec.target),
                                        QString::fromLatin1(spec.arguments)));
    }
    return rules;
}

RuleStore::RuleStore(QObject* parent) : QObject(parent) {}

LauncherDocument RuleStore::loadDocument(QString* error) const {
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
    for (const QJsonValue& value : sectionArray) {
        LauncherSection section = LauncherSection::fromJson(value.toObject());
        if (section.isValid()) {
            document.sections.push_back(section);
        }
    }

    ensureDefaultSections(&document);

    const QJsonArray ruleArray = root.value("rules").toArray();
    for (const QJsonValue& value : ruleArray) {
        HotkeyRule rule = HotkeyRule::fromJson(value.toObject());
        if (rule.sectionId.isEmpty()) {
            rule.sectionId = defaultSectionId(rule.category, 1);
        }
        if (rule.isValid()) {
            document.rules.push_back(rule);
        }
    }

    ensureDefaultRules(&document);

    return document;
}

bool RuleStore::saveDocument(const LauncherDocument& document, QString* error) const {
    QDir directory(configDirectory());
    if (!directory.exists() && !directory.mkpath(".")) {
        if (error) {
            *error = "Unable to create configuration directory.";
        }
        return false;
    }

    QJsonArray sections;
    for (const LauncherSection& section : document.sections) {
        if (section.isValid()) {
            sections.append(section.toJson());
        }
    }

    QJsonArray rules;
    for (const HotkeyRule& rule : document.rules) {
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

QVector<HotkeyRule> RuleStore::load(QString* error) const {
    return loadDocument(error).rules;
}

bool RuleStore::save(const QVector<HotkeyRule>& rules, QString* error) const {
    LauncherDocument document = loadDocument();
    document.rules = rules;
    ensureDefaultSections(&document);
    return saveDocument(document, error);
}

QString RuleStore::configPath() const {
    return QDir(configDirectory()).filePath("rules.json");
}

QStringList RuleStore::warningsForRule(const HotkeyRule& rule, const QVector<HotkeyRule>& rules,
                                       const QString& language) const {
    QStringList warnings;
    if (rule.hotkey.isValid()) {
        for (const HotkeyRule& existing : rules) {
            if (existing.id != rule.id && existing.enabled && existing.hotkey.isValid() &&
                existing.hotkey.stableId() == rule.hotkey.stableId()) {
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

LauncherDocument RuleStore::createDefaultDocument() const {
    LauncherDocument document;
    ensureDefaultSections(&document);
    ensureDefaultRules(&document);
    return document;
}

void RuleStore::ensureDefaultSections(LauncherDocument* document) const {
    auto hasSection = [document](const QString& id) {
        return std::any_of(document->sections.begin(), document->sections.end(),
                           [&id](const LauncherSection& section) { return section.id == id; });
    };

    auto addSection = [document, &hasSection](LauncherCategory category, int order, const QString& id,
                                              const QString& name, const QString& iconKey) {
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

    addSection(LauncherCategory::Program, 0, defaultSectionId(LauncherCategory::Program, 0),
               QString::fromUtf8("系统功能"), "system");
    addSection(LauncherCategory::Program, 1, defaultSectionId(LauncherCategory::Program, 1),
               QString::fromUtf8("我的程序"), "program");
    addSection(LauncherCategory::Folder, 0, defaultSectionId(LauncherCategory::Folder, 0),
               QString::fromUtf8("系统文件夹"), "folder-system");
    addSection(LauncherCategory::Folder, 1, defaultSectionId(LauncherCategory::Folder, 1),
               QString::fromUtf8("我的目录"), "folder");
    addSection(LauncherCategory::Website, 0, defaultSectionId(LauncherCategory::Website, 0),
               QString::fromUtf8("常用网址"), "website");
    addSection(LauncherCategory::Website, 1, defaultSectionId(LauncherCategory::Website, 1),
               QString::fromUtf8("我的网址"), "website-user");
}

void RuleStore::ensureDefaultRules(LauncherDocument* document) const {
    if (!document) {
        return;
    }

    auto sectionHasRules = [document](const QString& sectionId) {
        return std::any_of(document->rules.begin(), document->rules.end(), [&sectionId](const HotkeyRule& rule) {
            return rule.sectionId == sectionId && rule.isValid();
        });
    };

    const QString systemSection = defaultSectionId(LauncherCategory::Program, 0);
    const QVector<HotkeyRule> systemRules = defaultSystemProgramRules();

    auto hasSystemRule = [document, &systemSection](const HotkeyRule& expected) {
        return std::any_of(document->rules.begin(), document->rules.end(), [&](const HotkeyRule& rule) {
            if (rule.sectionId != systemSection || !rule.isValid()) {
                return false;
            }
            if (rule.description == expected.description) {
                return true;
            }
            return rule.action.target.compare(expected.action.target, Qt::CaseInsensitive) == 0 &&
                   rule.action.arguments.trimmed().compare(expected.action.arguments.trimmed(), Qt::CaseInsensitive) == 0;
        });
    };

    for (HotkeyRule rule : systemRules) {
        rule.sectionId = systemSection;
        if (!hasSystemRule(rule)) {
            document->rules.push_back(rule);
        }
    }

    const QString folderSection = defaultSectionId(LauncherCategory::Folder, 0);
    if (!sectionHasRules(folderSection)) {
        document->rules.push_back(
            makeFolderRule(folderSection, QString::fromUtf8("我的文档"), QDir::home().filePath("Documents")));
        document->rules.push_back(
            makeFolderRule(folderSection, QString::fromUtf8("桌面"), QDir::home().filePath("Desktop")));
        document->rules.push_back(
            makeFolderRule(folderSection, QString::fromUtf8("下载"), QDir::home().filePath("Downloads")));
    }

    const QString websiteSection = defaultSectionId(LauncherCategory::Website, 0);
    if (!sectionHasRules(websiteSection)) {
        document->rules.push_back(makeWebsiteRule(websiteSection, QString::fromUtf8("百度"), "https://www.baidu.com"));
        document->rules.push_back(makeWebsiteRule(websiteSection, QString::fromUtf8("必应"), "https://www.bing.com"));
    }
}

QString RuleStore::defaultSectionId(LauncherCategory category, int index) const {
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

QString RuleStore::configDirectory() const {
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!appData.isEmpty()) {
        return appData;
    }
    return QDir::home().filePath(".HotKeyManager");
}
