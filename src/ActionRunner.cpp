#include "ActionRunner.h"

#include "PathUtils.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QUrl>

#include <string>
#include <vector>

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
    int score = -1;
    QHash<DWORD, QString> processPaths;
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
    typedef BOOL(WINAPI * QueryFullProcessImageNameWFunction)(HANDLE, DWORD, LPWSTR, PDWORD);
    const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    const QueryFullProcessImageNameWFunction queryFullProcessImageName =
        kernel32 ? reinterpret_cast<QueryFullProcessImageNameWFunction>(
                       GetProcAddress(kernel32, "QueryFullProcessImageNameW"))
                 : nullptr;
    if (queryFullProcessImageName) {
        const DWORD processQueryLimitedInformation = 0x1000;
        HANDLE process = OpenProcess(processQueryLimitedInformation, FALSE, processId);
        if (process) {
            std::vector<wchar_t> buffer(32768, L'\0');
            DWORD size = static_cast<DWORD>(buffer.size());
            const bool queried = queryFullProcessImageName(process, 0, buffer.data(), &size) != FALSE;
            CloseHandle(process);
            if (queried && size > 0) {
                return QDir::toNativeSeparators(fromWideString(buffer.data(), static_cast<int>(size)));
            }
        }
    }

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
    if (!search) {
        return TRUE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId == 0) {
        return TRUE;
    }

    QString path;
    if (search->processPaths.contains(processId)) {
        path = search->processPaths.value(processId);
    } else {
        path = processImagePath(processId);
        search->processPaths.insert(processId, path);
    }
    if (path.compare(search->targetPath, Qt::CaseInsensitive) != 0) {
        return TRUE;
    }

    int score = IsWindowVisible(window) ? 100 : 0;
    score += GetWindow(window, GW_OWNER) == nullptr ? 30 : 0;
    score += (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) == 0 ? 20 : 0;
    score += GetWindowTextLengthW(window) > 0 ? 10 : 0;
    score += IsIconic(window) ? 5 : 0;
    if (score > search->score) {
        search->score = score;
        search->window = window;
    }
    return TRUE;
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

    bool attachedForeground = false;
    bool attachedTarget = false;
    if (foregroundThread != 0 && foregroundThread != currentThread) {
        attachedForeground = AttachThreadInput(currentThread, foregroundThread, TRUE) != FALSE;
    }
    if (targetThread != 0 && targetThread != currentThread && targetThread != foregroundThread) {
        attachedTarget = AttachThreadInput(currentThread, targetThread, TRUE) != FALSE;
    }

    BringWindowToTop(window);
    SetForegroundWindow(window);
    SetFocus(window);

    if (attachedTarget) {
        AttachThreadInput(currentThread, targetThread, FALSE);
    }
    if (attachedForeground) {
        AttachThreadInput(currentThread, foregroundThread, FALSE);
    }

    return GetForegroundWindow() == window;
}

bool applicationProcessExists(const QString& targetPath) {
    std::vector<DWORD> processIds(1024, 0);
    DWORD bytesReturned = 0;
    for (;;) {
        if (!EnumProcesses(processIds.data(), static_cast<DWORD>(processIds.size() * sizeof(DWORD)), &bytesReturned)) {
            return false;
        }
        if (static_cast<size_t>(bytesReturned) < processIds.size() * sizeof(DWORD)) {
            break;
        }
        if (processIds.size() >= 65536) {
            return false;
        }
        processIds.resize(processIds.size() * 2, 0);
    }

    const size_t processCount = bytesReturned / sizeof(DWORD);
    for (size_t index = 0; index < processCount; ++index) {
        const DWORD processId = processIds[index];
        if (processId != 0 && processImagePath(processId).compare(targetPath, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
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

    WindowSearch search;
    search.targetPath = targetPath;
    EnumWindows(enumWindowsForProcessPath, reinterpret_cast<LPARAM>(&search));
    if (search.window) {
        // Finding the existing process satisfies the single-instance contract.
        // Foreground activation is best effort because Windows may reject it
        // under foreground-lock policy; that must not cause a duplicate launch.
        forceForegroundWindow(search.window);
        return true;
    }
    // A process may be starting, tray-only, or temporarily have no top-level
    // window. It still satisfies the single-instance rule and must not be
    // duplicated merely because there is nothing to activate yet.
    return applicationProcessExists(targetPath);
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
    return true;
#else
    const QUrl url = resolvedAction.type == LaunchActionType::Url ? QUrl(resolvedAction.target)
                                                                  : QUrl::fromLocalFile(resolvedAction.target);
    if (QDesktopServices::openUrl(url)) {
        return true;
    }
    if (error) {
        *error = QString("Failed to open target: %1").arg(resolvedAction.target);
    }
    return false;
#endif
}
