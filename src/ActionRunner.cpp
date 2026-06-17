#include "ActionRunner.h"

#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

#ifdef Q_OS_WIN
namespace {

struct WindowSearch {
    QString targetPath;
    HWND window = nullptr;
};

QString processImagePath(DWORD processId)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!process) {
        return {};
    }

    wchar_t buffer[MAX_PATH * 4] = {};
    DWORD size = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
    QString path;
    if (QueryFullProcessImageNameW(process, 0, buffer, &size)) {
        path = QString::fromWCharArray(buffer, static_cast<int>(size));
    }
    CloseHandle(process);
    return QDir::toNativeSeparators(path);
}

BOOL CALLBACK enumWindowsForProcessPath(HWND window, LPARAM parameter)
{
    auto *search = reinterpret_cast<WindowSearch *>(parameter);
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

bool forceForegroundWindow(HWND window)
{
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

bool activateExistingApplicationWindow(const QString &target)
{
    const QFileInfo fileInfo(target);
    if (!fileInfo.exists() || fileInfo.suffix().compare("exe", Qt::CaseInsensitive) != 0) {
        return false;
    }

    const QString targetPath = QDir::toNativeSeparators(fileInfo.canonicalFilePath().isEmpty()
        ? fileInfo.absoluteFilePath()
        : fileInfo.canonicalFilePath());

    for (int attempt = 0; attempt < 8; ++attempt) {
        WindowSearch search{targetPath, nullptr};
        EnumWindows(enumWindowsForProcessPath, reinterpret_cast<LPARAM>(&search));
        if (search.window) {
            return forceForegroundWindow(search.window);
        }
        Sleep(80);
    }
    return false;
}

}
#endif

ActionRunner::ActionRunner(QObject *parent)
    : QObject(parent)
{
}

bool ActionRunner::run(const LaunchAction &action, QString *error) const
{
    if (!action.isValid()) {
        if (error) {
            *error = "Action target is empty.";
        }
        return false;
    }

#ifdef Q_OS_WIN
    const std::wstring verb = L"open";
    const std::wstring target = action.target.toStdWString();
    const std::wstring arguments = action.arguments.toStdWString();
    const std::wstring workingDirectory = QDir::toNativeSeparators(action.workingDirectory).toStdWString();

    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = verb.c_str();
    info.lpFile = target.c_str();
    info.lpParameters = arguments.empty() ? nullptr : arguments.c_str();
    info.lpDirectory = workingDirectory.empty() ? nullptr : workingDirectory.c_str();
    info.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&info)) {
        if (error) {
            *error = QString("ShellExecuteEx failed with error %1.").arg(GetLastError());
        }
        return false;
    }
    if (info.hProcess) {
        CloseHandle(info.hProcess);
    }
    if (action.type == LaunchActionType::Application) {
        activateExistingApplicationWindow(action.target);
    }
    return true;
#else
    Q_UNUSED(action)
    if (error) {
        *error = "ActionRunner is implemented for Windows only.";
    }
    return false;
#endif
}
