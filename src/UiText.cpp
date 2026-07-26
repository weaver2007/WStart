#include "UiText.h"

namespace {

bool isEnglish(const QString& language) {
    return UiText::normalizeLanguage(language) == "en-US";
}

} // namespace

namespace UiText {

QString normalizeLanguage(const QString& language) {
    if (language.compare("en", Qt::CaseInsensitive) == 0 || language.compare("en-US", Qt::CaseInsensitive) == 0) {
        return "en-US";
    }
    return "zh-CN";
}

QString text(const QString& language, Key key) {
    const bool en = isEnglish(language);
    switch (key) {
    case Key::SearchPlaceholder:
        return en ? "Search items" : QString::fromUtf8("搜索条目");
    case Key::Settings:
        return en ? "Settings" : QString::fromUtf8("设置");
    case Key::Minimize:
        return en ? "Minimize" : QString::fromUtf8("最小化");
    case Key::Close:
        return en ? "Close" : QString::fromUtf8("关闭");
    case Key::HotkeysEnabled:
        return en ? "Enable global hotkeys" : QString::fromUtf8("启用全局快捷键");
    case Key::UpdatesEnabled:
        return en ? "Check for updates automatically" : QString::fromUtf8("自动检查更新");
    case Key::StartupEnabled:
        return en ? "Start WStart with system (minimized)" : QString::fromUtf8("开机启动 WStart（最小化）");
    case Key::StartupSettingFailed:
        return en ? "Failed to update startup setting: %1" : QString::fromUtf8("更新开机启动设置失败：%1");
    case Key::StartupUnsupported:
        return en ? "This platform does not support WStart startup registration."
                  : QString::fromUtf8("当前平台不支持注册 WStart 开机启动。");
    case Key::GithubToken:
        return en ? "GitHub token..." : QString::fromUtf8("GitHub Token...");
    case Key::CheckForUpdates:
        return en ? "Check for updates" : QString::fromUtf8("检查更新");
    case Key::About:
        return en ? "About WStart" : QString::fromUtf8("关于 WStart");
    case Key::AboutTitle:
        return "WStart";
    case Key::AboutMessage:
        return en ? "WStart\nVersion: %1" : QString::fromUtf8("WStart\n版本：%1");
    case Key::UpdateChecking:
        return en ? "Checking for updates..." : QString::fromUtf8("正在检查更新...");
    case Key::UpdateAvailableTitle:
        return en ? "Update available" : QString::fromUtf8("发现新版本");
    case Key::UpdateAvailableMessage:
        return en ? "Version %1 is available. Current version: %2.\n\n%3"
                  : QString::fromUtf8("发现版本 %1，当前版本：%2。\n\n%3");
    case Key::UpdateNotAvailable:
        return en ? "You are already using the latest version. Current version: %1."
                  : QString::fromUtf8("当前已是最新版本，当前版本：%1。");
    case Key::UpdateFailed:
        return en ? "Update check failed: %1" : QString::fromUtf8("检查更新失败：%1");
    case Key::UpdateDownloadInstall:
        return en ? "Download and install" : QString::fromUtf8("下载并安装");
    case Key::UpdateDownloadAndUpdate:
        return en ? "Download and update" : QString::fromUtf8("下载并更新");
    case Key::UpdateDownloading:
        return en ? "Downloading update..." : QString::fromUtf8("正在下载更新...");
    case Key::UpdateDownloadFailed:
        return en ? "Update download failed: %1" : QString::fromUtf8("更新下载失败：%1");
    case Key::UpdateDownloaded:
        return en ? "Update package downloaded." : QString::fromUtf8("更新包已下载。");
    case Key::UpdateInstallLaunchFailed:
        return en ? "Unable to open update package: %1" : QString::fromUtf8("无法打开更新包：%1");
    case Key::GithubTokenTitle:
        return en ? "GitHub token" : QString::fromUtf8("GitHub Token");
    case Key::GithubTokenPrompt:
        return en ? "Enter a fine-grained PAT with read-only Contents permission for this private repository."
                  : QString::fromUtf8("请输入对当前私仓具备 Contents 只读权限的 fine-grained PAT。");
    case Key::GithubTokenSaved:
        return en ? "GitHub token saved." : QString::fromUtf8("GitHub Token 已保存。");
    case Key::GithubTokenSaveFailed:
        return en ? "Failed to save GitHub token: %1" : QString::fromUtf8("保存 GitHub Token 失败：%1");
    case Key::OpenDownloadPage:
        return en ? "Open download page" : QString::fromUtf8("打开下载页面");
    case Key::Language:
        return en ? "Language" : QString::fromUtf8("语言");
    case Key::Theme:
        return en ? "Theme" : QString::fromUtf8("皮肤");
    case Key::ThemeSystem:
        return en ? "Follow system" : QString::fromUtf8("跟随系统");
    case Key::ThemeLight:
        return en ? "Light" : QString::fromUtf8("亮色");
    case Key::ThemeDark:
        return en ? "Dark" : QString::fromUtf8("暗色");
    case Key::ItemAppearance:
        return en ? "Item appearance" : QString::fromUtf8("条目外观");
    case Key::SectionAppearance:
        return en ? "Section appearance" : QString::fromUtf8("分栏外观");
    case Key::CategoryAppearance:
        return en ? "Category appearance" : QString::fromUtf8("分组外观");
    case Key::HotkeyList:
        return en ? "Hotkey list" : QString::fromUtf8("快捷键列表");
    case Key::HotkeyListEmpty:
        return en ? "No hotkeys are configured in WStart." : QString::fromUtf8("WStart 中尚未配置快捷键。");
    case Key::HotkeyListCategory:
        return en ? "Category" : QString::fromUtf8("分类");
    case Key::HotkeyListItem:
        return en ? "Item" : QString::fromUtf8("条目");
    case Key::HotkeyListHotkey:
        return en ? "Hotkey" : QString::fromUtf8("快捷键");
    case Key::HotkeyListTarget:
        return en ? "Target" : QString::fromUtf8("目标");
    case Key::SectionHeight:
        return en ? "Section height" : QString::fromUtf8("分栏高度");
    case Key::CategoryHeight:
        return en ? "Category height" : QString::fromUtf8("分组高度");
    case Key::IconWidth:
        return en ? "Icon width" : QString::fromUtf8("图标宽度");
    case Key::IconHeight:
        return en ? "Icon height" : QString::fromUtf8("图标高度");
    case Key::ItemWidth:
        return en ? "Item width" : QString::fromUtf8("条目宽度");
    case Key::ItemHeight:
        return en ? "Item height" : QString::fromUtf8("条目高度");
    case Key::FontFamily:
        return en ? "Font" : QString::fromUtf8("字体");
    case Key::FontPointSize:
        return en ? "Font size" : QString::fromUtf8("字号");
    case Key::TextColor:
        return en ? "Text color" : QString::fromUtf8("文字颜色");
    case Key::DefaultColor:
        return en ? "Default color" : QString::fromUtf8("默认颜色");
    case Key::ChooseColor:
        return en ? "Choose color" : QString::fromUtf8("选择颜色");
    case Key::HorizontalSpacing:
        return en ? "Horizontal spacing" : QString::fromUtf8("水平间隔");
    case Key::VerticalSpacing:
        return en ? "Vertical spacing" : QString::fromUtf8("垂直间隔");
    case Key::MultilineText:
        return en ? "Multiline text" : QString::fromUtf8("多行文字");
    case Key::ShowEllipsis:
        return en ? "Show ellipsis" : QString::fromUtf8("显示省略号");
    case Key::Chinese:
        return QString::fromUtf8("中文");
    case Key::English:
        return "English";
    case Key::Ok:
        return en ? "OK" : QString::fromUtf8("确定");
    case Key::Cancel:
        return en ? "Cancel" : QString::fromUtf8("取消");
    case Key::Yes:
        return en ? "Yes" : QString::fromUtf8("是");
    case Key::No:
        return en ? "No" : QString::fromUtf8("否");
    case Key::OpenMainWindow:
        return en ? "Open main window" : QString::fromUtf8("打开主窗口");
    case Key::Exit:
        return en ? "Exit" : QString::fromUtf8("退出");
    case Key::NewSection:
        return en ? "New section" : QString::fromUtf8("新建栏目");
    case Key::EditSection:
        return en ? "Edit section" : QString::fromUtf8("修改栏目");
    case Key::DeleteSection:
        return en ? "Delete section" : QString::fromUtf8("删除栏目");
    case Key::EncryptSection:
        return en ? "Encrypt section" : QString::fromUtf8("栏目加密");
    case Key::UnlockSection:
        return en ? "Unlock section" : QString::fromUtf8("解锁栏目");
    case Key::AddItem:
        return en ? "Add item" : QString::fromUtf8("添加条目");
    case Key::Run:
        return en ? "Run" : QString::fromUtf8("运行");
    case Key::RunAsAdmin:
        return en ? "Run as administrator" : QString::fromUtf8("以管理员权限运行");
    case Key::ExplorerContextMenu:
        return en ? "Explorer context menu" : QString::fromUtf8("显示资源管理器菜单");
    case Key::BrowseTarget:
        return en ? "Browse" : QString::fromUtf8("浏览");
    case Key::CreateDesktopShortcut:
        return en ? "Desktop shortcut" : QString::fromUtf8("桌面快捷方式");
    case Key::SetStartup:
        return en ? "Run at startup" : QString::fromUtf8("设置开机自启动");
    case Key::Edit:
        return en ? "Edit" : QString::fromUtf8("编辑");
    case Key::Delete:
        return en ? "Delete" : QString::fromUtf8("删除");
    case Key::LockedHint:
        return en ? "This section is encrypted. Right-click the section to unlock, edit, or delete it."
                  : QString::fromUtf8("栏目已加密。右键栏目可解锁、修改或删除。");
    case Key::UnboundHotkey:
        return en ? "No hotkey" : QString::fromUtf8("未绑定快捷键");
    case Key::Disabled:
        return en ? "Disabled" : QString::fromUtf8("已禁用");
    case Key::EncryptedItemCount:
        return en ? "Encrypted · %1 items" : QString::fromUtf8("已加密 · %1 项");
    case Key::ItemCount:
        return en ? "%1 items" : QString::fromUtf8("%1 项");
    case Key::HotkeyHookFailed:
        return en ? "Hotkey hook failed" : QString::fromUtf8("热键钩子失败");
    case Key::HotkeyHookUnavailable:
        return en ? "Global hotkeys are not enabled on this platform yet."
                  : QString::fromUtf8("当前平台暂未启用全局快捷键。");
    case Key::HookRunning:
        return en ? "Global keyboard hook is running." : QString::fromUtf8("全局键盘钩子已运行。");
    case Key::HotkeysPaused:
        return en ? "Hotkeys paused." : QString::fromUtf8("热键已暂停。");
    case Key::HotkeysResumed:
        return en ? "Hotkeys resumed." : QString::fromUtf8("热键已恢复。");
    case Key::SectionLockedCancelled:
        return en ? "Section is locked. Launch cancelled." : QString::fromUtf8("栏目未解锁，已取消启动。");
    case Key::LaunchFailed:
        return en ? "Launch failed: %1" : QString::fromUtf8("启动失败：%1");
    case Key::Launched:
        return en ? "Launched: %1" : QString::fromUtf8("已启动：%1");
    case Key::SaveFailed:
        return en ? "Save failed" : QString::fromUtf8("保存失败");
    case Key::SavedItems:
        return en ? "Saved %1 item(s)." : QString::fromUtf8("已保存 %1 个条目。");
    case Key::NewSectionTitle:
        return en ? "New section" : QString::fromUtf8("新建栏目");
    case Key::EditSectionTitle:
        return en ? "Edit section" : QString::fromUtf8("修改栏目");
    case Key::Name:
        return en ? "Name" : QString::fromUtf8("名称");
    case Key::IconKey:
        return en ? "Icon key" : QString::fromUtf8("图标键");
    case Key::SortOrder:
        return en ? "Order" : QString::fromUtf8("顺序");
    case Key::InvalidSection:
        return en ? "Invalid section" : QString::fromUtf8("无效栏目");
    case Key::SectionNameRequired:
        return en ? "Section name is required." : QString::fromUtf8("栏目名称不能为空。");
    case Key::DeleteSectionTitle:
        return en ? "Delete section" : QString::fromUtf8("删除栏目");
    case Key::DeleteSectionWithItems:
        return en ? "Section \"%1\" has %2 item(s). Deleting it will also delete those items. Continue?"
                  : QString::fromUtf8("栏目“%1”中有 %2 个条目，删除栏目会同时删除这些条目。是否继续？");
    case Key::DeleteSectionConfirm:
        return en ? "Delete section \"%1\"?" : QString::fromUtf8("确定删除栏目“%1”？");
    case Key::EncryptSectionTitle:
        return en ? "Encrypt section" : QString::fromUtf8("栏目加密");
    case Key::EncryptSectionPrompt:
        return en ? "Enter a new password. Leave empty to remove encryption."
                  : QString::fromUtf8("输入新密码，留空则取消加密");
    case Key::InvalidItem:
        return en ? "Invalid item" : QString::fromUtf8("无效条目");
    case Key::ItemTargetRequired:
        return en ? "Target path or URL is required." : QString::fromUtf8("条目必须填写目标路径或网址。");
    case Key::HotkeyWarning:
        return en ? "Hotkey warning" : QString::fromUtf8("快捷键提示");
    case Key::HotkeyDuplicateWarning:
        return en ? "This hotkey is already used by another enabled item."
                  : QString::fromUtf8("该快捷键已被另一个启用条目使用。");
    case Key::HotkeySystemShortcutWarning:
        return en ? "This looks like a Windows system shortcut. It is allowed, but user-mode interception is best "
                    "effort."
                  : QString::fromUtf8("该快捷键类似 Windows 系统快捷键，允许保存，但用户态拦截只能尽力优先处理。");
    case Key::HotkeyCheck:
        return en ? "Check occupancy" : QString::fromUtf8("检测占用");
    case Key::HotkeyCheckInvalid:
        return en ? "No valid hotkey has been set." : QString::fromUtf8("尚未设置有效快捷键。");
    case Key::HotkeyCheckAvailable:
        return en ? "This hotkey is not registered by another app through RegisterHotKey."
                  : QString::fromUtf8("未检测到其它程序通过 RegisterHotKey 注册该快捷键。");
    case Key::HotkeyCheckSystemReserved:
        return en ? "This is a known Windows or Shell shortcut."
                  : QString::fromUtf8("这是常见 Windows 或 Shell 系统快捷键。");
    case Key::HotkeyCheckRegisteredByOtherApp:
        return en ? "RegisterHotKey probe failed. It may already be registered by another app. Windows error: %1."
                  : QString::fromUtf8("RegisterHotKey 探测失败，可能已被其它程序注册。Windows 错误码：%1。");
    case Key::HotkeyCheckBestEffort:
        return en ? "WStart can still try to intercept it while running, but secure or shell-reserved shortcuts cannot "
                    "be guaranteed."
                  : QString::fromUtf8("WStart 运行时仍会尽量拦截，但安全或 Shell 保留快捷键无法保证完全覆盖。");
    case Key::HotkeyCheckCannotForceDisable:
        return en ? "Windows does not provide a reliable public API to force-unregister another process's hotkey."
                  : QString::fromUtf8("Windows 没有可靠公开接口可强制取消其它进程注册的快捷键。");
    case Key::HotkeyCheckWindowsOnly:
        return en ? "Hotkey occupancy probing is implemented for Windows only."
                  : QString::fromUtf8("快捷键占用探测仅支持 Windows。");
    case Key::DeleteItemTitle:
        return en ? "Delete item" : QString::fromUtf8("删除条目");
    case Key::DeleteItemConfirm:
        return en ? "Delete \"%1\"?" : QString::fromUtf8("确定删除“%1”？");
    case Key::DangerousActionTitle:
        return en ? "Confirm system action" : QString::fromUtf8("确认系统操作");
    case Key::DangerousActionConfirm:
        return en ? "\"%1\" may shut down or restart this computer. Continue?"
                  : QString::fromUtf8("“%1”可能会关闭或重启电脑，确定继续吗？");
    case Key::UnlockSectionTitle:
        return en ? "Unlock section" : QString::fromUtf8("解锁栏目");
    case Key::UnlockSectionPrompt:
        return en ? "Enter section password" : QString::fromUtf8("请输入栏目密码");
    case Key::WrongPasswordTitle:
        return en ? "Wrong password" : QString::fromUtf8("密码错误");
    case Key::WrongPasswordMessage:
        return en ? "The section password is incorrect." : QString::fromUtf8("栏目密码不正确。");
    case Key::EnabledCount:
        return en ? "%1 enabled / %2 total" : QString::fromUtf8("%1 启用 / %2 总计");
    case Key::CategoryProgram:
        return en ? "Programs" : QString::fromUtf8("程序");
    case Key::CategoryFolder:
        return en ? "Directories" : QString::fromUtf8("目录");
    case Key::CategoryWebsite:
        return en ? "Websites" : QString::fromUtf8("网址");
    case Key::SectionSystemTools:
        return en ? "System tools" : QString::fromUtf8("系统功能");
    case Key::SectionMyPrograms:
        return en ? "My programs" : QString::fromUtf8("我的程序");
    case Key::SectionSystemFolders:
        return en ? "System folders" : QString::fromUtf8("系统文件夹");
    case Key::SectionMyFolders:
        return en ? "My folders" : QString::fromUtf8("我的目录");
    case Key::SectionCommonWebsites:
        return en ? "Common websites" : QString::fromUtf8("常用网址");
    case Key::SectionMyWebsites:
        return en ? "My websites" : QString::fromUtf8("我的网址");
    case Key::RuleDialogProgramTitle:
        return en ? "Program item" : QString::fromUtf8("程序条目");
    case Key::RuleDialogFolderTitle:
        return en ? "Folder item" : QString::fromUtf8("文件夹条目");
    case Key::RuleDialogWebsiteTitle:
        return en ? "Website item" : QString::fromUtf8("网址条目");
    case Key::Description:
        return en ? "Name" : QString::fromUtf8("名称");
    case Key::Target:
        return en ? "Target" : QString::fromUtf8("目标");
    case Key::Arguments:
        return en ? "Arguments" : QString::fromUtf8("参数");
    case Key::WorkingDirectory:
        return en ? "Working directory" : QString::fromUtf8("工作目录");
    case Key::WindowState:
        return en ? "Window size" : QString::fromUtf8("窗口大小");
    case Key::WindowStateNormal:
        return en ? "Normal" : QString::fromUtf8("正常");
    case Key::WindowStateMinimized:
        return en ? "Minimized" : QString::fromUtf8("最小化");
    case Key::WindowStateMaximized:
        return en ? "Maximized" : QString::fromUtf8("最大化");
    case Key::SingleInstance:
        return en ? "Single instance" : QString::fromUtf8("单一实例");
    case Key::Hotkey:
        return en ? "Hotkey" : QString::fromUtf8("快捷键");
    case Key::Record:
        return en ? "Record" : QString::fromUtf8("录制");
    case Key::Apply:
        return en ? "Apply" : QString::fromUtf8("应用");
    case Key::Browse:
        return en ? "Browse" : QString::fromUtf8("浏览");
    case Key::SelectFolder:
        return en ? "Select folder" : QString::fromUtf8("选择文件夹");
    case Key::SelectProgram:
        return en ? "Select program" : QString::fromUtf8("选择程序");
    case Key::ProgramTargetPlaceholder:
        return en ? "Choose or enter a program path, e.g. C:/Windows/System32/notepad.exe"
                  : QString::fromUtf8("选择或输入程序路径，例如 C:/Windows/System32/notepad.exe");
    case Key::FolderTargetPlaceholder:
        return en ? "Choose or enter a folder path" : QString::fromUtf8("选择或输入文件夹路径");
    case Key::WebsiteTargetPlaceholder:
        return en ? "Enter a URL, e.g. https://example.com" : QString::fromUtf8("输入网址，例如 https://example.com");
    case Key::DescriptionPlaceholder:
        return en ? "Display name. Leave empty to use target name."
                  : QString::fromUtf8("显示名称，可留空自动使用目标名称");
    case Key::ArgumentsPlaceholder:
        return en ? "Optional launch arguments" : QString::fromUtf8("可选启动参数");
    case Key::WorkingDirectoryPlaceholder:
        return en ? "Optional working directory" : QString::fromUtf8("可选工作目录");
    case Key::HotkeyPlaceholder:
        return en ? "Optional. You can leave this empty for now." : QString::fromUtf8("可选，当前阶段可不填写");
    case Key::HotkeyKeyPlaceholder:
        return en ? "Key" : QString::fromUtf8("主键");
    case Key::HotkeyManualInvalid:
        return en ? "Enter a valid main key, such as S, F1, Enter, or Space."
                  : QString::fromUtf8("请输入有效主键，例如 S、F1、Enter 或 Space。");
    case Key::TrayTooltip:
        return "WStart";
    case Key::SystemTrayUnavailable:
        return en ? "System tray is not available." : QString::fromUtf8("系统托盘不可用。");
    }
    return {};
}

QString categoryName(const QString& language, LauncherCategory category) {
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

} // namespace UiText
