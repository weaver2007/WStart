#include "ActionRunner.h"

#include "PathUtils.h"

#include <QDir>
#include <QDesktopServices>
#include <QFileInfo>
#include <QThread>
#include <QUrl>

#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#define PSAPI_VERSION 1
#include <psapi.h>
#include <shellapi.h>
#endif

#ifdef Q_OS_WIN
namespace {

struct WindowSearch {
    QString targetPath;
    HWND window = nullptr;
};

QString fromWideString(const wchar_t* value, int length = -1) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QString::fromUtf16(reinterpret_cast<const char16_t*>(value), length);
#else
    return QString::fromUtf16(reinterpret_cast<const ushort*>(value), length);
#endif
}

std::wstring toWideString(const QString& value) {
    return std::wstring(reinterpret_cast<const wchar_t*>(value.utf16()), static_cast<size_t>(value.length()));
}

QString processImagePath(DWORD processId) {
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!process) {
        return {};
    }

    wchar_t buffer[MAX_PATH * 4] = {};
    QString path;
    const DWORD size =
        GetModuleFileNameExW(process, nullptr, buffer, static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0])));
    if (size > 0) {
        path = fromWideString(buffer, static_cast<int>(size));
    }
    CloseHandle(process);
    return QDir::toNativeSeparators(path);
}

BOOL CALLBACK enumWindowsForProcessPath(HWND window, LPARAM parameter) {
    auto* search = reinterpret_cast<WindowSearch*>(parameter);
    if (!search || !IsWindowVisible(window)) {
        return TRUE;
    }

    HWND owner = GetWindow(window, GW_OWNER);
    if (owner) {
        return TRUE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId == 0) {
        return TRUE;
    }

    const QString path = processImagePath(processId);
    if (path.compare(search->targetPath, Qt::CaseInsensitive) != 0) {
        return TRUE;
    }

    search->window = window;
    return FALSE;
}

bool forceForegroundWindow(HWND window) {
    if (!window) {
        return false;
    }

    if (IsIconic(window)) {
        ShowWindow(window, SW_RESTORE);
    } else {
        ShowWindow(window, SW_SHOW);
    }

    const DWORD foregroundThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    const DWORD targetThread = GetWindowThreadProcessId(window, nullptr);
    const DWORD currentThread = GetCurrentThreadId();

    if (foregroundThread != 0) {
        AttachThreadInput(currentThread, foregroundThread, TRUE);
    }
    if (targetThread != 0) {
        AttachThreadInput(currentThread, targetThread, TRUE);
    }

    BringWindowToTop(window);
    SetForegroundWindow(window);
    SetFocus(window);

    if (targetThread != 0) {
        AttachThreadInput(currentThread, targetThread, FALSE);
    }
    if (foregroundThread != 0) {
        AttachThreadInput(currentThread, foregroundThread, FALSE);
    }

    return GetForegroundWindow() == window;
}

int showCommandForWindowState(LaunchWindowState windowState) {
    switch (windowState) {
    case LaunchWindowState::Minimized:
        return SW_SHOWMINIMIZED;
    case LaunchWindowState::Maximized:
        return SW_SHOWMAXIMIZED;
    case LaunchWindowState::Normal:
        return SW_SHOWNORMAL;
    }
    return SW_SHOWNORMAL;
}

bool activateExistingApplicationWindow(const QString& target) {
    const QFileInfo fileInfo(target);
    if (!fileInfo.exists() || fileInfo.suffix().compare("exe", Qt::CaseInsensitive) != 0) {
        return false;
    }

    const QString targetPath = QDir::toNativeSeparators(
        fileInfo.canonicalFilePath().isEmpty() ? fileInfo.absoluteFilePath() : fileInfo.canonicalFilePath());

    for (int attempt = 0; attempt < 8; ++attempt) {
        WindowSearch search;
        search.targetPath = targetPath;
        search.window = nullptr;
        EnumWindows(enumWindowsForProcessPath, reinterpret_cast<LPARAM>(&search));
        if (search.window) {
            return forceForegroundWindow(search.window);
        }
        Sleep(80);
    }
    return false;
}

} // namespace
#endif

ActionRunner::ActionRunner(QObject* parent) : QObject(parent) {}

bool ActionRunner::run(const LaunchAction& action, QString* error) const {
    const LaunchAction resolvedAction = PathUtils::toAbsoluteAction(action);
    if (!resolvedAction.isValid()) {
        if (error) {
            *error = "Action target is empty.";
        }
        return false;
    }

#ifdef Q_OS_WIN
    if (resolvedAction.singleInstance && resolvedAction.type == LaunchActionType::Application &&
        activateExistingApplicationWindow(resolvedAction.target)) {
        return true;
    }

    const std::wstring verb = L"open";
    const std::wstring target = toWideString(resolvedAction.target);
    const std::wstring arguments = toWideString(resolvedAction.arguments);
    const std::wstring workingDirectory = toWideString(QDir::toNativeSeparators(resolvedAction.workingDirectory));
    const int showCommand = showCommandForWindowState(resolvedAction.windowState);

    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = verb.c_str();
    info.lpFile = target.c_str();
    info.lpParameters = arguments.empty() ? nullptr : arguments.c_str();
    info.lpDirectory = workingDirectory.empty() ? nullptr : workingDirectory.c_str();
    info.nShow = showCommand;

    if (!ShellExecuteExW(&info)) {
        if (error) {
            *error = QString("ShellExecuteEx failed with error %1.").arg(GetLastError());
        }
        return false;
    }
    if (info.hProcess) {
        CloseHandle(info.hProcess);
    }
    if (resolvedAction.type == LaunchActionType::Application && resolvedAction.windowState == LaunchWindowState::Normal) {
        activateExistingApplicationWindow(resolvedAction.target);
    }
    return true;
#else
    const QUrl url = resolvedAction.type == LaunchActionType::Url ? QUrl(resolvedAction.target) : QUrl::fromLocalFile(resolvedAction.target);
    if (QDesktopServices::openUrl(url)) {
        return true;
    }
    if (error) {
        *error = QString("Failed to open target: %1").arg(resolvedAction.target);
    }
    return false;
#endif
}
