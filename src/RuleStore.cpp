#include "RuleStore.h"

#include "BuiltInActions.h"
#include "HotkeyConflictDetector.h"
#include "PathUtils.h"
#include "UiText.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
#include <QSaveFile>
#endif
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

#if QT_VERSION < QT_VERSION_CHECK(5, 1, 0) && defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace {

const int kDefaultRulesVersion = 2;

QString standardFolderPath(QStandardPaths::StandardLocation location, const QString& fallbackName) {
    const QString path = QStandardPaths::writableLocation(location);
    return path.isEmpty() ? QDir::home().filePath(fallbackName) : path;
}

HotkeyRule makeProgramRule(const QString& sectionId, const QString& id, const QString& name, const QString& target,
                           const QString& arguments = QString()) {
    HotkeyRule rule;
    rule.id = id;
    rule.category = LauncherCategory::Program;
    rule.sectionId = sectionId;
    rule.action.type = LaunchActionType::Application;
    rule.action.target = target;
    rule.action.arguments = arguments;
    rule.description = name;
    return rule;
}

HotkeyRule makeFolderRule(const QString& sectionId, const QString& id, const QString& name, const QString& target) {
    HotkeyRule rule;
    rule.id = id;
    rule.category = LauncherCategory::Folder;
    rule.sectionId = sectionId;
    rule.action.type = LaunchActionType::Folder;
    rule.action.target = target;
    rule.description = name;
    return rule;
}

HotkeyRule makeWebsiteRule(const QString& sectionId, const QString& id, const QString& name, const QString& target) {
    HotkeyRule rule;
    rule.id = id;
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
        {"环境变量", "rundll32.exe", "sysdm.cpl,EditEnvironmentVariables"},
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
        {"关闭显示器", "powershell.exe",
         R"wstart(-NoProfile -ExecutionPolicy Bypass -Command "(Add-Type '[DllImport(\"user32.dll\")] public static extern int SendMessage(int hWnd, int hMsg, int wParam, int lParam);' -Name Native -Namespace WStart -PassThru)::SendMessage(-1, 0x0112, 0xF170, 2)")wstart"},
    };

    QVector<HotkeyRule> rules;
    const int specCount = static_cast<int>(sizeof(specs) / sizeof(specs[0]));
    for (int index = 0; index < specCount; ++index) {
        const DefaultProgramRule& spec = specs[index];
        rules.push_back(makeProgramRule(systemSection, QString("default-program-system-%1").arg(index + 1),
                                        QString::fromUtf8(spec.name), QString::fromLatin1(spec.target),
                                        QString::fromLatin1(spec.arguments)));
    }
    rules.push_back(makeProgramRule(systemSection, QString::fromLatin1("default-program-system-move-window-monitor"),
                                    QString::fromUtf8("活动窗口移至另一屏幕"),
                                    BuiltInActions::moveActiveWindowToNextMonitorTarget()));
    return rules;
}

RuleStore::RuleStore(QObject* parent) : QObject(parent) {}

RuleStore::RuleStore(const QString& configPathOverride, QObject* parent)
    : QObject(parent), m_configPathOverride(QDir::cleanPath(configPathOverride)) {}

LauncherDocument RuleStore::loadDocument(QString* error) const {
    const QString path = configPath();
    QFile file(path);
    if (!file.exists()) {
        return createDefaultDocument();
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return createDefaultDocument();
    }

    const QByteArray contents = file.readAll();
    file.close();
    const QJsonDocument json = QJsonDocument::fromJson(contents);
    if (!json.isObject()) {
        const QString backupPath = path + QString::fromLatin1(".corrupt");
        const bool backupExists = QFileInfo(backupPath).exists();
        const bool backupCreated = backupExists || QFile::copy(path, backupPath);
        if (error) {
            *error = backupCreated
                         ? QString::fromLatin1("Configuration root must be an object. The original file was preserved "
                                               "at %1.")
                               .arg(backupPath)
                         : QString::fromLatin1("Configuration root must be an object and the original file could not "
                                               "be backed up.");
        }
        return createDefaultDocument();
    }

    LauncherDocument document;
    const QJsonObject root = json.object();
    document.defaultRulesVersion = root.value("defaultRulesVersion").toInt(0);
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

    if (document.defaultRulesVersion < kDefaultRulesVersion) {
        ensureDefaultRules(&document);
        document.defaultRulesVersion = kDefaultRulesVersion;
    }

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
            HotkeyRule storedRule = rule;
            storedRule.action = document.settings.pathStorageMode.compare("absolute", Qt::CaseInsensitive) == 0
                                    ? PathUtils::toAbsoluteAction(storedRule.action)
                                    : PathUtils::toPortableAction(storedRule.action);
            rules.append(storedRule.toJson());
        }
    }

    QJsonObject root;
    root["version"] = 2;
    root["defaultRulesVersion"] = document.defaultRulesVersion;
    root["settings"] = document.settings.toJson();
    root["sections"] = sections;
    root["rules"] = rules;

    const QByteArray contents = QJsonDocument(root).toJson(QJsonDocument::Indented);
    const QString path = configPath();

#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    if (file.write(contents) != contents.size()) {
        if (error) {
            *error = file.errorString();
        }
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    return true;
#else
    // QSaveFile was introduced after Qt 4. Write beside the destination and
    // replace only after the complete JSON payload has reached disk.
    const QString temporaryPath = path + QString::fromLatin1(".tmp");
    QFile::remove(temporaryPath);
    QFile file(temporaryPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(contents) != contents.size() ||
        !file.flush()) {
        if (error) {
            *error = file.errorString();
        }
        file.close();
        QFile::remove(temporaryPath);
        return false;
    }
    file.close();

#ifdef Q_OS_WIN
    const QString nativePath = QDir::toNativeSeparators(path);
    const QString nativeTemporaryPath = QDir::toNativeSeparators(temporaryPath);
    BOOL replaced = FALSE;
    if (QFileInfo(path).exists()) {
        replaced = ReplaceFileW(reinterpret_cast<LPCWSTR>(nativePath.utf16()),
                                reinterpret_cast<LPCWSTR>(nativeTemporaryPath.utf16()), nullptr,
                                REPLACEFILE_WRITE_THROUGH, nullptr, nullptr);
    } else {
        replaced = MoveFileExW(reinterpret_cast<LPCWSTR>(nativeTemporaryPath.utf16()),
                               reinterpret_cast<LPCWSTR>(nativePath.utf16()), MOVEFILE_WRITE_THROUGH);
    }
    if (!replaced) {
        const DWORD lastError = GetLastError();
        QFile::remove(temporaryPath);
        if (error) {
            *error = QString::fromLatin1("Unable to replace configuration file (Windows error %1).").arg(lastError);
        }
        return false;
    }
#else
    const QString backupPath = path + QString::fromLatin1(".bak");
    QFile::remove(backupPath);
    const bool hadOriginal = QFileInfo(path).exists();
    if (hadOriginal && !QFile::rename(path, backupPath)) {
        QFile::remove(temporaryPath);
        if (error) {
            *error = QString::fromLatin1("Unable to preserve the existing configuration file.");
        }
        return false;
    }
    if (!QFile::rename(temporaryPath, path)) {
        if (hadOriginal) {
            QFile::rename(backupPath, path);
        }
        if (error) {
            *error = QString::fromLatin1("Unable to replace the configuration file.");
        }
        return false;
    }
    QFile::remove(backupPath);
#endif
    return true;
#endif
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
    if (!m_configPathOverride.isEmpty()) {
        return m_configPathOverride;
    }
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
    document.defaultRulesVersion = kDefaultRulesVersion;
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
                   rule.action.arguments.trimmed().compare(expected.action.arguments.trimmed(), Qt::CaseInsensitive) ==
                       0;
        });
    };

    auto hasRuleId = [document](const QString& id) {
        return std::any_of(document->rules.begin(), document->rules.end(),
                           [&id](const HotkeyRule& rule) { return rule.id == id; });
    };

    auto uniqueRuleId = [&hasRuleId](const QString& preferredId) {
        if (!hasRuleId(preferredId)) {
            return preferredId;
        }
        QString id = preferredId + QString::fromLatin1("-") + QUuid::createUuid().toString();
        id.remove(QLatin1Char('{'));
        id.remove(QLatin1Char('}'));
        return id;
    };

    for (HotkeyRule rule : systemRules) {
        rule.sectionId = systemSection;
        if (!hasSystemRule(rule)) {
            // Default rule ids were historically index-based, so inserted tools must not collide with existing configs.
            rule.id = uniqueRuleId(rule.id);
            document->rules.push_back(rule);
        }
    }

    const QString folderSection = defaultSectionId(LauncherCategory::Folder, 0);
    if (!sectionHasRules(folderSection)) {
        document->rules.push_back(
            makeFolderRule(folderSection, "default-folder-documents", QString::fromUtf8("我的文档"),
                           standardFolderPath(QStandardPaths::DocumentsLocation, QString::fromLatin1("Documents"))));
        document->rules.push_back(
            makeFolderRule(folderSection, "default-folder-desktop", QString::fromUtf8("桌面"),
                           standardFolderPath(QStandardPaths::DesktopLocation, QString::fromLatin1("Desktop"))));
        document->rules.push_back(
            makeFolderRule(folderSection, "default-folder-downloads", QString::fromUtf8("下载"),
                           standardFolderPath(QStandardPaths::DownloadLocation, QString::fromLatin1("Downloads"))));
    }

    const QString websiteSection = defaultSectionId(LauncherCategory::Website, 0);
    if (!sectionHasRules(websiteSection)) {
        document->rules.push_back(makeWebsiteRule(websiteSection, "default-website-baidu", QString::fromUtf8("百度"),
                                                  "https://www.baidu.com"));
        document->rules.push_back(
            makeWebsiteRule(websiteSection, "default-website-bing", QString::fromUtf8("必应"), "https://www.bing.com"));
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
    if (!m_configPathOverride.isEmpty()) {
        return QFileInfo(m_configPathOverride).absolutePath();
    }
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!appData.isEmpty()) {
        return appData;
    }
    return QDir::home().filePath(".WStart");
}
