#include "UiText.h"

namespace {

bool isEnglish(const QString &language)
{
    return UiText::normalizeLanguage(language) == "en-US";
}

}

namespace UiText {

QString normalizeLanguage(const QString &language)
{
    if (language.compare("en", Qt::CaseInsensitive) == 0 ||
        language.compare("en-US", Qt::CaseInsensitive) == 0) {
        return "en-US";
    }
    return "zh-CN";
}

QString text(const QString &language, Key key)
{
    const bool en = isEnglish(language);
    switch (key) {
    case Key::SearchPlaceholder: return en ? "Search items" : QString::fromUtf8("搜索条目");
    case Key::Settings: return en ? "Settings" : QString::fromUtf8("设置");
    case Key::Minimize: return en ? "Minimize" : QString::fromUtf8("最小化");
    case Key::Close: return en ? "Close" : QString::fromUtf8("关闭");
    case Key::HotkeysEnabled: return en ? "Enable global hotkeys" : QString::fromUtf8("启用全局快捷键");
    case Key::Language: return en ? "Language" : QString::fromUtf8("语言");
    case Key::Theme: return en ? "Theme" : QString::fromUtf8("皮肤");
    case Key::ThemeSystem: return en ? "Follow system" : QString::fromUtf8("跟随系统");
    case Key::ThemeLight: return en ? "Light" : QString::fromUtf8("亮色");
    case Key::ThemeDark: return en ? "Dark" : QString::fromUtf8("暗色");
    case Key::ItemAppearance: return en ? "Item appearance" : QString::fromUtf8("条目外观");
    case Key::SectionAppearance: return en ? "Section appearance" : QString::fromUtf8("分栏外观");
    case Key::CategoryAppearance: return en ? "Category appearance" : QString::fromUtf8("分组外观");
    case Key::HotkeyList: return en ? "Hotkey list" : QString::fromUtf8("快捷键列表");
    case Key::HotkeyListEmpty: return en ? "No hotkeys are configured in HStart." : QString::fromUtf8("HStart 中尚未配置快捷键。");
    case Key::HotkeyListCategory: return en ? "Category" : QString::fromUtf8("分类");
    case Key::HotkeyListItem: return en ? "Item" : QString::fromUtf8("条目");
    case Key::HotkeyListHotkey: return en ? "Hotkey" : QString::fromUtf8("快捷键");
    case Key::HotkeyListTarget: return en ? "Target" : QString::fromUtf8("目标");
    case Key::SectionHeight: return en ? "Section height" : QString::fromUtf8("分栏高度");
    case Key::CategoryHeight: return en ? "Category height" : QString::fromUtf8("分组高度");
    case Key::IconWidth: return en ? "Icon width" : QString::fromUtf8("图标宽度");
    case Key::IconHeight: return en ? "Icon height" : QString::fromUtf8("图标高度");
    case Key::ItemWidth: return en ? "Item width" : QString::fromUtf8("条目宽度");
    case Key::ItemHeight: return en ? "Item height" : QString::fromUtf8("条目高度");
    case Key::FontFamily: return en ? "Font" : QString::fromUtf8("字体");
    case Key::FontPointSize: return en ? "Font size" : QString::fromUtf8("字号");
    case Key::TextColor: return en ? "Text color" : QString::fromUtf8("文字颜色");
    case Key::DefaultColor: return en ? "Default color" : QString::fromUtf8("默认颜色");
    case Key::ChooseColor: return en ? "Choose color" : QString::fromUtf8("选择颜色");
    case Key::HorizontalSpacing: return en ? "Horizontal spacing" : QString::fromUtf8("水平间隔");
    case Key::VerticalSpacing: return en ? "Vertical spacing" : QString::fromUtf8("垂直间隔");
    case Key::MultilineText: return en ? "Multiline text" : QString::fromUtf8("多行文字");
    case Key::ShowEllipsis: return en ? "Show ellipsis" : QString::fromUtf8("显示省略号");
    case Key::Chinese: return QString::fromUtf8("中文");
    case Key::English: return "English";
    case Key::Ok: return en ? "OK" : QString::fromUtf8("确定");
    case Key::Cancel: return en ? "Cancel" : QString::fromUtf8("取消");
    case Key::Yes: return en ? "Yes" : QString::fromUtf8("是");
    case Key::No: return en ? "No" : QString::fromUtf8("否");
    case Key::OpenMainWindow: return en ? "Open main window" : QString::fromUtf8("打开主窗口");
    case Key::Exit: return en ? "Exit" : QString::fromUtf8("退出");
    case Key::NewSection: return en ? "New section" : QString::fromUtf8("新建栏目");
    case Key::EditSection: return en ? "Edit section" : QString::fromUtf8("修改栏目");
    case Key::DeleteSection: return en ? "Delete section" : QString::fromUtf8("删除栏目");
    case Key::EncryptSection: return en ? "Encrypt section" : QString::fromUtf8("栏目加密");
    case Key::UnlockSection: return en ? "Unlock section" : QString::fromUtf8("解锁栏目");
    case Key::AddItem: return en ? "Add item" : QString::fromUtf8("添加条目");
    case Key::Run: return en ? "Run" : QString::fromUtf8("运行");
    case Key::Edit: return en ? "Edit" : QString::fromUtf8("编辑");
    case Key::Delete: return en ? "Delete" : QString::fromUtf8("删除");
    case Key::LockedHint: return en ? "This section is encrypted. Right-click the section to unlock, edit, or delete it." : QString::fromUtf8("栏目已加密。右键栏目可解锁、修改或删除。");
    case Key::UnboundHotkey: return en ? "No hotkey" : QString::fromUtf8("未绑定快捷键");
    case Key::Disabled: return en ? "Disabled" : QString::fromUtf8("已禁用");
    case Key::EncryptedItemCount: return en ? "Encrypted · %1 items" : QString::fromUtf8("已加密 · %1 项");
    case Key::ItemCount: return en ? "%1 items" : QString::fromUtf8("%1 项");
    case Key::HotkeyHookFailed: return en ? "Hotkey hook failed" : QString::fromUtf8("热键钩子失败");
    case Key::HookRunning: return en ? "Global keyboard hook is running." : QString::fromUtf8("全局键盘钩子已运行。");
    case Key::HotkeysPaused: return en ? "Hotkeys paused." : QString::fromUtf8("热键已暂停。");
    case Key::HotkeysResumed: return en ? "Hotkeys resumed." : QString::fromUtf8("热键已恢复。");
    case Key::SectionLockedCancelled: return en ? "Section is locked. Launch cancelled." : QString::fromUtf8("栏目未解锁，已取消启动。");
    case Key::LaunchFailed: return en ? "Launch failed: %1" : QString::fromUtf8("启动失败：%1");
    case Key::Launched: return en ? "Launched: %1" : QString::fromUtf8("已启动：%1");
    case Key::SaveFailed: return en ? "Save failed" : QString::fromUtf8("保存失败");
    case Key::SavedItems: return en ? "Saved %1 item(s)." : QString::fromUtf8("已保存 %1 个条目。");
    case Key::NewSectionTitle: return en ? "New section" : QString::fromUtf8("新建栏目");
    case Key::EditSectionTitle: return en ? "Edit section" : QString::fromUtf8("修改栏目");
    case Key::Name: return en ? "Name" : QString::fromUtf8("名称");
    case Key::IconKey: return en ? "Icon key" : QString::fromUtf8("图标键");
    case Key::SortOrder: return en ? "Order" : QString::fromUtf8("顺序");
    case Key::InvalidSection: return en ? "Invalid section" : QString::fromUtf8("无效栏目");
    case Key::SectionNameRequired: return en ? "Section name is required." : QString::fromUtf8("栏目名称不能为空。");
    case Key::DeleteSectionTitle: return en ? "Delete section" : QString::fromUtf8("删除栏目");
    case Key::DeleteSectionWithItems: return en ? "Section \"%1\" has %2 item(s). Deleting it will also delete those items. Continue?" : QString::fromUtf8("栏目“%1”中有 %2 个条目，删除栏目会同时删除这些条目。是否继续？");
    case Key::DeleteSectionConfirm: return en ? "Delete section \"%1\"?" : QString::fromUtf8("确定删除栏目“%1”？");
    case Key::EncryptSectionTitle: return en ? "Encrypt section" : QString::fromUtf8("栏目加密");
    case Key::EncryptSectionPrompt: return en ? "Enter a new password. Leave empty to remove encryption." : QString::fromUtf8("输入新密码，留空则取消加密");
    case Key::InvalidItem: return en ? "Invalid item" : QString::fromUtf8("无效条目");
    case Key::ItemTargetRequired: return en ? "Target path or URL is required." : QString::fromUtf8("条目必须填写目标路径或网址。");
    case Key::HotkeyWarning: return en ? "Hotkey warning" : QString::fromUtf8("快捷键提示");
    case Key::HotkeyDuplicateWarning: return en ? "This hotkey is already used by another enabled item." : QString::fromUtf8("该快捷键已被另一个启用条目使用。");
    case Key::HotkeySystemShortcutWarning: return en ? "This looks like a Windows system shortcut. It is allowed, but user-mode interception is best effort." : QString::fromUtf8("该快捷键类似 Windows 系统快捷键，允许保存，但用户态拦截只能尽力优先处理。");
    case Key::HotkeyCheck: return en ? "Check occupancy" : QString::fromUtf8("检测占用");
    case Key::HotkeyCheckInvalid: return en ? "No valid hotkey has been recorded." : QString::fromUtf8("尚未录入有效快捷键。");
    case Key::HotkeyCheckAvailable: return en ? "This hotkey is not registered by another app through RegisterHotKey." : QString::fromUtf8("未检测到其它程序通过 RegisterHotKey 注册该快捷键。");
    case Key::HotkeyCheckSystemReserved: return en ? "This is a known Windows or Shell shortcut." : QString::fromUtf8("这是常见 Windows 或 Shell 系统快捷键。");
    case Key::HotkeyCheckRegisteredByOtherApp: return en ? "RegisterHotKey probe failed. It may already be registered by another app. Windows error: %1." : QString::fromUtf8("RegisterHotKey 探测失败，可能已被其它程序注册。Windows 错误码：%1。");
    case Key::HotkeyCheckBestEffort: return en ? "HSTART can still try to intercept it while running, but secure or shell-reserved shortcuts cannot be guaranteed." : QString::fromUtf8("HSTART 运行时仍会尽量拦截，但安全或 Shell 保留快捷键无法保证完全覆盖。");
    case Key::HotkeyCheckCannotForceDisable: return en ? "Windows does not provide a reliable public API to force-unregister another process's hotkey." : QString::fromUtf8("Windows 没有可靠公开接口可强制取消其它进程注册的快捷键。");
    case Key::HotkeyCheckWindowsOnly: return en ? "Hotkey occupancy probing is implemented for Windows only." : QString::fromUtf8("快捷键占用探测仅支持 Windows。");
    case Key::DeleteItemTitle: return en ? "Delete item" : QString::fromUtf8("删除条目");
    case Key::DeleteItemConfirm: return en ? "Delete \"%1\"?" : QString::fromUtf8("确定删除“%1”？");
    case Key::UnlockSectionTitle: return en ? "Unlock section" : QString::fromUtf8("解锁栏目");
    case Key::UnlockSectionPrompt: return en ? "Enter section password" : QString::fromUtf8("请输入栏目密码");
    case Key::WrongPasswordTitle: return en ? "Wrong password" : QString::fromUtf8("密码错误");
    case Key::WrongPasswordMessage: return en ? "The section password is incorrect." : QString::fromUtf8("栏目密码不正确。");
    case Key::EnabledCount: return en ? "%1 enabled / %2 total" : QString::fromUtf8("%1 启用 / %2 总计");
    case Key::CategoryProgram: return en ? "Programs" : QString::fromUtf8("程序");
    case Key::CategoryFolder: return en ? "Directories" : QString::fromUtf8("目录");
    case Key::CategoryWebsite: return en ? "Websites" : QString::fromUtf8("网址");
    case Key::SectionSystemTools: return en ? "System tools" : QString::fromUtf8("系统功能");
    case Key::SectionMyPrograms: return en ? "My programs" : QString::fromUtf8("我的程序");
    case Key::SectionSystemFolders: return en ? "System folders" : QString::fromUtf8("系统文件夹");
    case Key::SectionMyFolders: return en ? "My folders" : QString::fromUtf8("我的目录");
    case Key::SectionCommonWebsites: return en ? "Common websites" : QString::fromUtf8("常用网址");
    case Key::SectionMyWebsites: return en ? "My websites" : QString::fromUtf8("我的网址");
    case Key::RuleDialogProgramTitle: return en ? "Program item" : QString::fromUtf8("程序条目");
    case Key::RuleDialogFolderTitle: return en ? "Folder item" : QString::fromUtf8("文件夹条目");
    case Key::RuleDialogWebsiteTitle: return en ? "Website item" : QString::fromUtf8("网址条目");
    case Key::Description: return en ? "Name" : QString::fromUtf8("名称");
    case Key::Target: return en ? "Target" : QString::fromUtf8("目标");
    case Key::Arguments: return en ? "Arguments" : QString::fromUtf8("参数");
    case Key::WorkingDirectory: return en ? "Working directory" : QString::fromUtf8("工作目录");
    case Key::Hotkey: return en ? "Hotkey" : QString::fromUtf8("快捷键");
    case Key::Record: return en ? "Record" : QString::fromUtf8("录制");
    case Key::Browse: return en ? "Browse" : QString::fromUtf8("浏览");
    case Key::SelectFolder: return en ? "Select folder" : QString::fromUtf8("选择文件夹");
    case Key::SelectProgram: return en ? "Select program" : QString::fromUtf8("选择程序");
    case Key::ProgramTargetPlaceholder: return en ? "Choose or enter a program path, e.g. C:/Windows/System32/notepad.exe" : QString::fromUtf8("选择或输入程序路径，例如 C:/Windows/System32/notepad.exe");
    case Key::FolderTargetPlaceholder: return en ? "Choose or enter a folder path" : QString::fromUtf8("选择或输入文件夹路径");
    case Key::WebsiteTargetPlaceholder: return en ? "Enter a URL, e.g. https://example.com" : QString::fromUtf8("输入网址，例如 https://example.com");
    case Key::DescriptionPlaceholder: return en ? "Display name. Leave empty to use target name." : QString::fromUtf8("显示名称，可留空自动使用目标名称");
    case Key::ArgumentsPlaceholder: return en ? "Optional launch arguments" : QString::fromUtf8("可选启动参数");
    case Key::WorkingDirectoryPlaceholder: return en ? "Optional working directory" : QString::fromUtf8("可选工作目录");
    case Key::HotkeyPlaceholder: return en ? "Optional. You can leave this empty for now." : QString::fromUtf8("可选，当前阶段可不填写");
    case Key::TrayTooltip: return "HotKeyManager";
    case Key::SystemTrayUnavailable: return en ? "System tray is not available." : QString::fromUtf8("系统托盘不可用。");
    }
    return {};
}

QString categoryName(const QString &language, LauncherCategory category)
{
    switch (category) {
    case LauncherCategory::Program:
        return text(language, Key::CategoryProgram);
    case LauncherCategory::Folder:
        return text(language, Key::CategoryFolder);
    case LauncherCategory::Website:
        return text(language, Key::CategoryWebsite);
    }
    return text(language, Key::CategoryProgram);
}

}
